#include "firmware_ota.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <atomic>
#include <memory>
#include <esp_app_desc.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_rom_crc.h>
#include <nvs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

#include "config_guard.h"
#include "boot_health_policy.h"
#include "boot_health_diagnostics.h"
#include "ota_terminal_outbox.h"
#include "device_identity.h"
#include "ota_signing_key.h"
#include "generated_firmware_ui.h"
#include "tls_bundle.h"
#include "local_endpoint_transport.h"

// Arduino ESP32 3.2.0 declares this weak callback in C in esp32-hal-misc.c.
// Returning true defers its default early boot confirmation to local checks.
extern "C" bool verifyRollbackLater() { return true; }

namespace leopold::firmware_ota {
namespace {
constexpr char kLayoutId[] = "leopold-ota-6m25-v1";
constexpr size_t kSlotBytes = 0x640000;
constexpr uint32_t kConnectTimeoutMs = 5000;
constexpr uint32_t kIdleTimeoutMs = 15000;
constexpr uint32_t kTransferTimeoutMs = 15 * 60 * 1000;
constexpr uint8_t kMaxReleaseAttempts = 3;
constexpr size_t kTransferBufferBytes = 4096;
std::atomic<bool> cancelRequested{false};
std::atomic<bool> transferActive{false};
bool pendingVerification = false;
bool pendingIdentityValid = false;
bool localUsbPending = false;
bool rollbackRequested = false;
uint32_t attemptedPartitionAddress = UINT32_MAX;
BootHealthPolicy bootHealth;
BootHealthDiagnosticRecord bootDiagnostic = {};
BootHealthDiagnostics::PersistResult lastDiagnosticWriteResult =
    BootHealthDiagnostics::PersistResult::kNotAttempted;
String pendingReleaseId;
String outcomeCommit;
String terminalEnqueueRelease;
String terminalEnqueueDetail;
TerminalState terminalEnqueueState = TerminalState::kFailed;
bool terminalEnqueuePending = false;
uint32_t nextOutcomeAttemptAt = 0;
class TerminalOutboxLock {
 public:
  TerminalOutboxLock() {
    static SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    handle_ = mutex;
    locked_ = handle_ != nullptr &&
              xSemaphoreTake(handle_, portMAX_DELAY) == pdTRUE;
  }
  ~TerminalOutboxLock() {
    unlock();
  }
  bool locked() const { return locked_; }
  void unlock() {
    if (!locked_) return;
    xSemaphoreGive(handle_);
    locked_ = false;
  }
 private:
  SemaphoreHandle_t handle_ = nullptr;
  bool locked_ = false;
};

String baseUrl() { return String(LEOPOLD_API_BASE_URL); }

void secureHeaders(HTTPClient& http) {
  http.addHeader("X-Leopold-Device-ID", identity::deviceId());
  http.addHeader("Authorization", String("Bearer ") + identity::deviceSecret());
  http.addHeader("Connection", "close");
}

void noRedirects(HTTPClient& http) {
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.setConnectTimeout(kConnectTimeoutMs);
  http.setTimeout(1000);
}

String newEventId(const char* state) {
  char value[96] = {};
  snprintf(value, sizeof(value), "%s-%08lx-%s-%08lx", identity::deviceId(),
           static_cast<unsigned long>(millis()), state,
           static_cast<unsigned long>(esp_random()));
  return String(value);
}

class PreferencesTerminalOutcomeStore final : public TerminalOutcomeStore {
 public:
  size_t read(void* output, size_t capacity) override {
    class OutboxPreferences final : public Preferences {
     public:
      esp_err_t blobLength(const char* key, size_t* length) {
        return _started && key != nullptr ? nvs_get_blob(_handle, key, nullptr, length)
                                          : ESP_ERR_INVALID_STATE;
      }
      esp_err_t blobRead(const char* key, void* data, size_t* length) {
        return _started && key != nullptr ? nvs_get_blob(_handle, key, data, length)
                                          : ESP_ERR_INVALID_STATE;
      }
    } preferences;
    // Opening read/write also creates this dedicated OTA namespace on devices
    // that have never queued an outcome; it does not touch product settings.
    if (!preferences.begin("ota-outbox", false)) return SIZE_MAX;
    size_t length = 0;
    const esp_err_t lengthResult = preferences.blobLength("pending", &length);
    if (lengthResult == ESP_ERR_NVS_NOT_FOUND) { preferences.end(); return 0; }
    if (lengthResult != ESP_OK) { preferences.end(); return SIZE_MAX; }
    if (output == nullptr && capacity == 0) { preferences.end(); return length; }
    if (output == nullptr || length > capacity) { preferences.end(); return SIZE_MAX; }
    size_t copied = capacity;
    const esp_err_t readResult = preferences.blobRead("pending", output, &copied);
    preferences.end();
    return readResult == ESP_OK && copied == length ? copied : SIZE_MAX;
  }

  bool write(const void* input, size_t length) override {
    Preferences preferences;
    if (!preferences.begin("ota-outbox", false)) return false;
    const size_t written = preferences.putBytes("pending", input, length);
    preferences.end();
    return written == length;
  }

  bool clear() override {
    Preferences preferences;
    if (!preferences.begin("ota-outbox", false)) return false;
    const bool removed = preferences.remove("pending");
    preferences.end();
    if (!removed) return read(nullptr, 0) == 0;
    return read(nullptr, 0) == 0;
  }
};

PreferencesTerminalOutcomeStore terminalOutcomeStore;

bool postStatus(const String& releaseId, const char* state, const char* detail,
                const String& eventId) {
  local_endpoint::SecureClient secure;
  HTTPClient http;
  noRedirects(http);
  if (local_endpoint::beginRequest(http, secure, LEOPOLD_API_BASE_URL,
      baseUrl() + "/v1/device/firmware/status", LEOPOLD_TLS_ROOT_CA,
      kConnectTimeoutMs) != local_endpoint::BeginStatus::kReady) return false;
  secureHeaders(http);
  http.addHeader("Content-Type", "application/json");
  JsonDocument payload;
  payload["event_id"] = eventId;
  payload["release_id"] = releaseId;
  payload["state"] = state;
  if (detail != nullptr && *detail != '\0') payload["detail"] = detail;
  String body;
  serializeJson(payload, body);
  const int code = http.POST(body);
  http.end();
  return code >= 200 && code < 300;
}

bool queueTerminalOutcome(const String& releaseId, TerminalState state, const char* detail) {
  TerminalOutcomeRecord record{};
  const char* stateName = state == TerminalState::kHealthy ? "healthy"
      : state == TerminalState::kRolledBack ? "rolled_back" : "failed";
  const String eventId = newEventId(stateName);
  if (!makeTerminalOutcomeRecord(&record, state, releaseId.c_str(), eventId.c_str(), detail)) {
    Serial.printf("OTA_STATUS outbox_record_invalid release=%s\n", releaseId.c_str());
    return false;
  }
  TerminalOutboxLock lock;
  if (!lock.locked()) {
    Serial.println("OTA_STATUS outbox_lock_failed; outcome retained for reconstruction");
    return false;
  }
  const String requestedDetail = detail == nullptr ? "" : detail;
  if (terminalEnqueuePending &&
      (terminalEnqueueRelease != releaseId || terminalEnqueueState != state ||
       terminalEnqueueDetail != requestedDetail)) {
    Serial.printf("OTA_STATUS outbox_memory_pending_conflict release=%s\n", releaseId.c_str());
    return false;
  }
  const OutboxResult result = enqueueTerminalOutcome(terminalOutcomeStore, record);
  if (result == OutboxResult::kQueued || result == OutboxResult::kSameOutcomeAlreadyQueued) {
    if (terminalEnqueuePending && terminalEnqueueRelease == releaseId &&
        terminalEnqueueState == state && terminalEnqueueDetail == requestedDetail)
      terminalEnqueuePending = false;
    return true;
  }
  terminalEnqueueRelease = releaseId;
  terminalEnqueueDetail = requestedDetail;
  terminalEnqueueState = state;
  terminalEnqueuePending = true;
  Serial.printf("OTA_STATUS outbox_enqueue_failed result=%u release=%s\n",
                static_cast<unsigned>(result), releaseId.c_str());
  return false;
}

void saveFailure(const String& releaseId, const char* commit, const char* reason) {
  Preferences prefs;
  if (prefs.begin("ota-state", false)) {
    String previous = prefs.getString("failed_release", "");
    uint8_t attempts = previous == releaseId ? prefs.getUChar("attempts", 0) : 0;
    if (attempts < 255) ++attempts;
    prefs.putString("failed_release", releaseId);
    prefs.putString("attempt_commit", commit);
    prefs.putUChar("attempts", attempts);
    prefs.end();
  }
  (void)queueTerminalOutcome(releaseId, TerminalState::kFailed, reason);
}

bool hasExceededAttempts(const String& releaseId) {
  Preferences prefs;
  if (!prefs.begin("ota-state", true)) return false;
  const bool blocked = prefs.getString("failed_release", "") == releaseId &&
                       prefs.getUChar("attempts", 0) >= kMaxReleaseAttempts;
  prefs.end();
  return blocked;
}

void resetAttempts(const String& releaseId) {
  Preferences prefs;
  if (prefs.begin("ota-state", false)) {
    prefs.putString("failed_release", releaseId);
    prefs.putUChar("attempts", 0);
    prefs.end();
  }
}

bool checkPartitionLayout(const esp_partition_t* target) {
  if (target == nullptr || target->type != ESP_PARTITION_TYPE_APP ||
      (target->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0 &&
       target->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_1) ||
      target->size != kSlotBytes) return false;
  const esp_partition_t* ota0 = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  const esp_partition_t* ota1 = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
  const esp_partition_t* running = esp_ota_get_running_partition();
  return running != nullptr && ota0 != nullptr && ota1 != nullptr && ota0->address == 0x10000 &&
         ota1->address == 0x650000 && ota0->size == kSlotBytes &&
         ota1->size == kSlotBytes && target->address != running->address;
}

bool persistBootDiagnostic() {
  bootDiagnostic.previousWriteResult = static_cast<uint8_t>(lastDiagnosticWriteResult);
  Preferences preferences;
  size_t written = 0;
  const auto result = BootHealthDiagnostics::persist(preferences, bootDiagnostic, &written);
  lastDiagnosticWriteResult = result;
  if (result != BootHealthDiagnostics::PersistResult::kWritten) {
    Serial.printf("OTA_HEALTH diagnostic_persist status=%u written=%lu expected=%lu\n",
                  static_cast<unsigned>(result), static_cast<unsigned long>(written),
                  static_cast<unsigned long>(sizeof(bootDiagnostic)));
    return false;
  }
  return true;
}

bool selectedLocalUsbRecordMatches(const esp_partition_t* running) {
  bootDiagnostic.identityFlags |= BootHealthDiagnostics::kLocalMarkerChecked;
  if (running == nullptr) {
    bootDiagnostic.failureStage = static_cast<uint8_t>(BootHealthDiagnostics::FailureStage::kRunningPartition);
    return false;
  }
  if (strlen(config::kSourceCommit) != 40) {
    bootDiagnostic.failureStage = static_cast<uint8_t>(BootHealthDiagnostics::FailureStage::kNormalAttemptIdentity);
    return false;
  }
  const esp_partition_t* otaData = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
  if (otaData == nullptr) {
    bootDiagnostic.otaRecordReadResult = ESP_ERR_NOT_FOUND;
    bootDiagnostic.failureStage = static_cast<uint8_t>(BootHealthDiagnostics::FailureStage::kOtaDataPartition);
    return false;
  }
  bootDiagnostic.otaDataPartitionFound = 1;
  bootDiagnostic.otaDataAddress = otaData->address;
  if (otaData->size < 64) {
    bootDiagnostic.otaRecordReadResult = ESP_ERR_INVALID_SIZE;
    bootDiagnostic.failureStage = static_cast<uint8_t>(BootHealthDiagnostics::FailureStage::kOtaDataPartition);
    return false;
  }
  esp_ota_select_entry_t entries[2] = {};
  const esp_err_t readResult = esp_partition_read(otaData, 0, entries, sizeof(entries));
  bootDiagnostic.otaRecordReadResult = static_cast<int32_t>(readResult);
  if (readResult != ESP_OK) {
    bootDiagnostic.failureStage = static_cast<uint8_t>(BootHealthDiagnostics::FailureStage::kOtaDataRead);
    return false;
  }
  bootDiagnostic.otaRecordReadMask = 0x03;
  BootHealthPolicy::OtaRecordObservation observations[2] = {};
  for (int index = 0; index < 2; ++index) {
    const esp_ota_select_entry_t& entry = entries[index];
    BootHealthPolicy::OtaRecordObservation& observation = observations[index];
    observation.read = true;
    observation.sequence = entry.ota_seq;
    observation.state = entry.ota_state;
    observation.storedCrc = entry.crc;
    memcpy(observation.label, entry.seq_label, sizeof(observation.label));
    const uint32_t crc = esp_rom_crc32_le(UINT32_MAX,
        reinterpret_cast<const uint8_t*>(&entry.ota_seq), sizeof(entry.ota_seq));
    observation.crcValid = entry.ota_seq != UINT32_MAX && entry.crc == crc;
    bootDiagnostic.otaSequence[index] = entry.ota_seq;
    bootDiagnostic.otaState[index] = entry.ota_state;
    bootDiagnostic.otaCrc[index] = entry.crc;
    if (observation.crcValid) bootDiagnostic.otaRecordCrcValidMask |= 1U << index;
  }
  const bool runningSlotValid = running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
      running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1;
  const uint32_t runningSlot = running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 ? 0U : 1U;
  const auto decision = BootHealthPolicy::evaluateLocalUsbIdentity(
      observations, true, runningSlotValid, runningSlot, config::kSourceCommit);
  bootDiagnostic.identityFlags |= decision.identityFlags;
  bootDiagnostic.selectedRecordIndex = decision.selectedRecordIndex;
  bootDiagnostic.selectionStatus = static_cast<uint8_t>(decision.selectionStatus);
  bootDiagnostic.failureStage = static_cast<uint8_t>(decision.failureStage);
  bootDiagnostic.identityMarkerStatus = static_cast<uint8_t>(decision.markerStatus);
  if (decision.selectedRecordIndex >= 0) {
    memcpy(bootDiagnostic.selectedRecordLabel,
           entries[decision.selectedRecordIndex].seq_label,
           sizeof(bootDiagnostic.selectedRecordLabel));
  }
  return decision.matched;
}

bool decodeBase64(const char* input, uint8_t* output, size_t outputCapacity, size_t& outputLength) {
  outputLength = 0;
  if (input == nullptr || strlen(input) > 1024) return false;
  return mbedtls_base64_decode(output, outputCapacity, &outputLength,
      reinterpret_cast<const unsigned char*>(input), strlen(input)) == 0;
}

bool verifyManifest(JsonObjectConst manifest, const char* signatureText,
                    String& releaseId, String& sourceCommit,
                    size_t& byteLength, uint8_t digestOut[32]) {
  if (manifest["signature_algorithm"] != "ECDSA-P256-SHA256" ||
      manifest["key_id"] != "18124c21ca7b5a7b" ||
      manifest["hardware_target"] != "esp32s3" ||
      manifest["partition_layout"] != kLayoutId) return false;
  releaseId = manifest["release_id"].as<String>();
  sourceCommit = manifest["source_commit"].as<String>();
  if (releaseId.length() == 0 || releaseId.length() > 64 || sourceCommit.length() != 40) return false;
  for (size_t i = 0; i < releaseId.length(); ++i) {
    const char c = releaseId[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return false;
  }
  for (size_t i = 0; i < sourceCommit.length(); ++i) {
    const char c = sourceCommit[i];
    if (!((c >= 'a' && c <= 'f') || (c >= '0' && c <= '9'))) return false;
  }
  if (manifest["ui_revision"].as<uint32_t>() < generated_ui::kUiRevision) return false;
  byteLength = manifest["byte_length"].as<size_t>();
  if (byteLength == 0 || byteLength > kSlotBytes) return false;
  const char* hashText = manifest["sha256"] | "";
  if (strlen(hashText) != 64) return false;
  for (size_t i = 0; i < 32; ++i) {
    unsigned int value = 0;
    if (sscanf(hashText + i * 2, "%2x", &value) != 1) return false;
    digestOut[i] = static_cast<uint8_t>(value);
  }

  // The backend returns the signed manifest in canonical insertion order.
  // Python registered only sorted-key canonical JSON; ArduinoJson serializes
  // the retained sorted key order without whitespace.
  String canonical;
  serializeJson(manifest, canonical);
  uint8_t signature[128] = {};
  size_t signatureLength = 0;
  if (!decodeBase64(signatureText, signature, sizeof(signature), signatureLength)) return false;
  mbedtls_pk_context key;
  mbedtls_pk_init(&key);
  const int parsed = mbedtls_pk_parse_public_key(&key,
      reinterpret_cast<const unsigned char*>(LEOPOLD_OTA_VERIFY_KEY_PEM),
      sizeof(LEOPOLD_OTA_VERIFY_KEY_PEM));
  unsigned char hash[32] = {};
  mbedtls_sha256(reinterpret_cast<const unsigned char*>(canonical.c_str()),
                 canonical.length(), hash, 0);
  const int verified = parsed == 0
      ? mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, hash, sizeof(hash), signature, signatureLength)
      : parsed;
  mbedtls_pk_free(&key);
  return verified == 0;
}

int fail(const String& releaseId, const char* commit, const char* reason,
         esp_ota_handle_t handle, bool otaStarted) {
  if (otaStarted) esp_ota_abort(handle);
  saveFailure(releaseId, commit, reason);
  Serial.printf("OTA failed release=%s reason=%s\n", releaseId.c_str(), reason);
  transferActive.store(false);
  cancelRequested.store(false);
  return -1;
}

bool getAssignment(JsonDocument& doc) {
  local_endpoint::SecureClient secure;
  HTTPClient http;
  noRedirects(http);
  if (local_endpoint::beginRequest(http, secure, LEOPOLD_API_BASE_URL,
      baseUrl() + "/v1/device/firmware/assignment", LEOPOLD_TLS_ROOT_CA,
      kConnectTimeoutMs) != local_endpoint::BeginStatus::kReady) return false;
  secureHeaders(http);
  const int code = http.GET();
  if (code == HTTP_CODE_NO_CONTENT) { http.end(); return true; }
  const int size = http.getSize();
  if (code != HTTP_CODE_OK || size <= 0 || size > 4096) { http.end(); return false; }
  String body = http.getString();
  http.end();
  if (body.length() != static_cast<unsigned int>(size)) return false;
  return deserializeJson(doc, body) == DeserializationError::Ok;
}

bool failPendingBoot(const char* reason) {
  if (!pendingVerification) return false;
  if (rollbackRequested) return true;
  rollbackRequested = true;
  bootDiagnostic.phase = static_cast<uint8_t>(BootHealthDiagnostics::Phase::kRollbackIntent);
  bootDiagnostic.rollbackReason = strcmp(reason, "pending_identity_mismatch") == 0
      ? static_cast<uint8_t>(BootHealthDiagnostics::RollbackReason::kPendingIdentityMismatch)
      : strcmp(reason, "local_health_timeout") == 0
          ? static_cast<uint8_t>(BootHealthDiagnostics::RollbackReason::kLocalHealthTimeout)
          : static_cast<uint8_t>(BootHealthDiagnostics::RollbackReason::kConfirmationApiFailed);
  if (strcmp(reason, "local_health_timeout") == 0)
    bootDiagnostic.failureStage = static_cast<uint8_t>(BootHealthDiagnostics::FailureStage::kLocalHealthTimeout);
  else if (strcmp(reason, "confirmation_api_failed") == 0)
    bootDiagnostic.failureStage = static_cast<uint8_t>(BootHealthDiagnostics::FailureStage::kConfirmationApi);
  bootDiagnostic.elapsedMs = bootHealth.elapsed(millis());
  bootDiagnostic.loopCount = bootHealth.loops();
  bootDiagnostic.phase = static_cast<uint8_t>(BootHealthDiagnostics::Phase::kRollbackIntent);
  (void)persistBootDiagnostic();
  Serial.printf("OTA_HEALTH rollback_requested reason=%s\n", reason);
  const esp_err_t result = esp_ota_mark_app_invalid_rollback_and_reboot();
  bootDiagnostic.rollbackApiResult = static_cast<int32_t>(result);
  bootDiagnostic.phase = static_cast<uint8_t>(BootHealthDiagnostics::Phase::kRollbackApiReturned);
  (void)persistBootDiagnostic();
  Serial.printf("OTA_HEALTH rollback_api result=%s code=%d; rollback_outcome=unknown_until_reboot_observation\n",
                esp_err_to_name(result), static_cast<int>(result));
  return true;
}
}  // namespace

bool beginBootVerification() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
  const esp_err_t stateResult = running == nullptr
      ? ESP_ERR_NOT_FOUND : esp_ota_get_state_partition(running, &state);
  pendingVerification = stateResult == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY;
  pendingIdentityValid = false;
  localUsbPending = false;
  bootHealth.reset(millis());
  rollbackRequested = false;
  BootHealthDiagnostics::initialize(bootDiagnostic);
  lastDiagnosticWriteResult = BootHealthDiagnostics::PersistResult::kNotAttempted;
  bootDiagnostic.runningPartitionValid = running != nullptr;
  bootDiagnostic.runningAddress = running == nullptr ? UINT32_MAX : running->address;
  bootDiagnostic.runningSubtype = running == nullptr
      ? UINT32_MAX : static_cast<uint32_t>(running->subtype);
  bootDiagnostic.stateResult = static_cast<int32_t>(stateResult);
  bootDiagnostic.stateValid = stateResult == ESP_OK;
  if (bootDiagnostic.stateValid) bootDiagnostic.imageState = static_cast<int32_t>(state);
  if (running == nullptr || stateResult != ESP_OK)
    bootDiagnostic.failureStage = static_cast<uint8_t>(BootHealthDiagnostics::FailureStage::kPublicStateLookup);
  const esp_app_desc_t* appDescription = esp_app_get_description();
  BootHealthDiagnostics::setBuildIdentity(bootDiagnostic, config::kSourceCommit,
      appDescription == nullptr ? nullptr : appDescription->app_elf_sha256);
  if (pendingVerification) bootDiagnostic.identityFlags |= BootHealthDiagnostics::kOtaStatePending;
  if (pendingVerification) {
    // Persist the observation point before any identity branch can request
    // rollback. The record contains no credentials or device identifiers.
    (void)persistBootDiagnostic();
  }
  Serial.printf("OTA_HEALTH boot partition=%s address=0x%lx state_result=%s state=%d pending=%s source=%s\n",
                running == nullptr ? "unknown" : running->label,
                static_cast<unsigned long>(running == nullptr ? 0 : running->address),
                esp_err_to_name(stateResult), stateResult == ESP_OK ? static_cast<int>(state) : -1,
                pendingVerification ? "yes" : "no", config::kSourceCommit);
  TerminalState recoveredTerminalState = TerminalState::kFailed;
  bool recoveredTerminalOutcome = false;
  Preferences prefs;
  if (prefs.begin("ota-state", false)) {
    if (pendingVerification) bootDiagnostic.identityFlags |= BootHealthDiagnostics::kPreferencesOpened;
    pendingReleaseId = prefs.getString("attempt_release", "");
    outcomeCommit = prefs.getString("attempt_commit", "");
    attemptedPartitionAddress = prefs.getUInt("attempt_partition", UINT32_MAX);
    bootDiagnostic.attemptedAddress = attemptedPartitionAddress;
    bootDiagnostic.attemptedAddressValid = attemptedPartitionAddress != UINT32_MAX;
    const bool sourceCommitMatches = outcomeCommit == config::kSourceCommit;
    if (pendingVerification) {
      if (pendingReleaseId.length() != 0) bootDiagnostic.identityFlags |= BootHealthDiagnostics::kNvsReleasePresent;
      if (running != nullptr && running->address == attemptedPartitionAddress)
        bootDiagnostic.identityFlags |= BootHealthDiagnostics::kNvsPartitionMatches;
      if (sourceCommitMatches) bootDiagnostic.identityFlags |= BootHealthDiagnostics::kNvsCommitMatches;
      const bool nvsIdentityMatches = BootHealthPolicy::matchesPendingImage(
          true, pendingReleaseId.length() != 0, running == nullptr ? UINT32_MAX : running->address,
          attemptedPartitionAddress, sourceCommitMatches);
      pendingIdentityValid = nvsIdentityMatches;
      if (nvsIdentityMatches) {
        bootDiagnostic.identityFlags |= BootHealthDiagnostics::kNvsIdentityMatched;
        bootDiagnostic.failureStage = static_cast<uint8_t>(BootHealthDiagnostics::FailureStage::kIdentityMatched);
      } else {
        bootDiagnostic.failureStage = static_cast<uint8_t>(BootHealthDiagnostics::FailureStage::kNormalAttemptIdentity);
      }
      if (!pendingIdentityValid) {
        Serial.printf("OTA_HEALTH pending_identity_mismatch running=0x%lx attempted=0x%lx commit_match=%s release_present=%s\n",
                      static_cast<unsigned long>(running == nullptr ? 0 : running->address),
                      static_cast<unsigned long>(attemptedPartitionAddress),
                      outcomeCommit == config::kSourceCommit ? "yes" : "no",
                      pendingReleaseId.length() == 0 ? "no" : "yes");
      }
    } else if (pendingReleaseId.length() != 0) {
      const BootHealthPolicy::PriorOutcome priorOutcome = BootHealthPolicy::classifyPriorImage(
          stateResult == ESP_OK, stateResult == ESP_OK && state == ESP_OTA_IMG_VALID,
          running == nullptr ? UINT32_MAX : running->address, attemptedPartitionAddress,
          sourceCommitMatches);
      if (priorOutcome == BootHealthPolicy::PriorOutcome::kHealthy) {
        recoveredTerminalState = TerminalState::kHealthy;
        recoveredTerminalOutcome = true;
        Serial.printf("OTA_HEALTH previously_confirmed release=%s partition=0x%lx source_match=%s\n",
                      pendingReleaseId.c_str(), static_cast<unsigned long>(running->address),
                      outcomeCommit == config::kSourceCommit ? "yes" : "no");
      } else if (priorOutcome == BootHealthPolicy::PriorOutcome::kRolledBack) {
        recoveredTerminalState = TerminalState::kRolledBack;
        recoveredTerminalOutcome = true;
        Serial.printf("OTA_HEALTH prior_attempt_not_running release=%s running=0x%lx attempted=0x%lx\n",
                      pendingReleaseId.c_str(), static_cast<unsigned long>(running->address),
                      static_cast<unsigned long>(attemptedPartitionAddress));
      } else {
        Serial.printf("OTA_HEALTH prior_attempt_outcome=unknown state_result=%s state=%d attempted=0x%lx source_match=%s\n",
                      esp_err_to_name(stateResult), stateResult == ESP_OK ? static_cast<int>(state) : -1,
                      static_cast<unsigned long>(attemptedPartitionAddress),
                      sourceCommitMatches ? "yes" : "no");
      }
    }
    prefs.end();
  } else {
    if (pendingVerification)
      bootDiagnostic.failureStage = static_cast<uint8_t>(BootHealthDiagnostics::FailureStage::kAttemptPreferences);
    Serial.println("OTA_HEALTH preferences_open_failed; attempt_outcome=unknown");
  }
  if (recoveredTerminalOutcome)
    (void)queueTerminalOutcome(pendingReleaseId, recoveredTerminalState, nullptr);
  if (pendingVerification && !pendingIdentityValid &&
      selectedLocalUsbRecordMatches(running)) {
    localUsbPending = true;
    pendingIdentityValid = true;
    pendingReleaseId = String("usb-") + String(config::kSourceCommit).substring(0, 12);
    Serial.printf("OTA_HEALTH local_usb_identity source_match=yes partition=0x%lx\n",
                  static_cast<unsigned long>(running == nullptr ? 0 : running->address));
  }
  if (pendingVerification) {
    if (pendingIdentityValid) bootDiagnostic.identityFlags |= BootHealthDiagnostics::kPendingIdentityMatched;
    bootDiagnostic.phase = static_cast<uint8_t>(BootHealthDiagnostics::Phase::kIdentity);
    (void)persistBootDiagnostic();
  }
  if (pendingVerification && !pendingIdentityValid) {
    failPendingBoot("pending_identity_mismatch");
  }
  return pendingVerification;
}

void setLocalHealth(bool display, bool touch, bool settings, bool setup,
                    BootHealthDiagnostics::InitStage initStage) {
  bootHealth.setReadiness(display, touch, settings, setup);
#ifdef LEOPOLD_OTA_TEST_MODE
  Serial.printf("LEOPOLD_OTA_TEST_HEALTH stage=%s display=%s touch=%s settings=%s cloud_worker=%s\n",
                config::kSourceCommit,
                display ? "ready" : "failed",
                touch ? "ready" : "failed",
                settings ? "ready" : "failed",
                setup ? "ready" : "failed");
#endif
  if (pendingVerification) {
    bootDiagnostic.phase = static_cast<uint8_t>(BootHealthDiagnostics::Phase::kReadiness);
    bootDiagnostic.initStage = static_cast<uint8_t>(initStage);
    bootDiagnostic.readinessFlags = BootHealthDiagnostics::readiness(display, touch, settings, setup);
    bootDiagnostic.elapsedMs = bootHealth.elapsed(millis());
    bootDiagnostic.loopCount = bootHealth.loops();
    (void)persistBootDiagnostic();
    Serial.printf("OTA_HEALTH readiness display=%s touch=%s settings=%s setup=%s elapsed_ms=%lu loops=%lu\n",
                  display ? "ready" : "failed", touch ? "ready" : "failed",
                  settings ? "ready" : "failed", setup ? "ready" : "failed",
                  static_cast<unsigned long>(bootHealth.elapsed(millis())),
                  static_cast<unsigned long>(bootHealth.loops()));
  }
}

void loopHealth(uint32_t now) {
  if (!pendingVerification) return;
  if (rollbackRequested) return;
  const BootHealthPolicy::Action action = bootHealth.tick(now);
  const uint32_t elapsed = bootHealth.elapsed(now);
  if (bootHealth.loops() == 1 || elapsed >= BootHealthPolicy::kDeadlineMs) {
    bootDiagnostic.phase = static_cast<uint8_t>(BootHealthDiagnostics::Phase::kProgress);
    bootDiagnostic.elapsedMs = elapsed;
    bootDiagnostic.loopCount = bootHealth.loops();
    (void)persistBootDiagnostic();
    Serial.printf("OTA_HEALTH progress elapsed_ms=%lu loops=%lu display=%s touch=%s settings=%s setup=%s\n",
                  static_cast<unsigned long>(elapsed), static_cast<unsigned long>(bootHealth.loops()),
                  bootHealth.displayReady() ? "ready" : "failed",
                  bootHealth.touchReady() ? "ready" : "failed",
                  bootHealth.settingsReady() ? "ready" : "failed",
                  bootHealth.setupReady() ? "ready" : "failed");
  }
  if (action == BootHealthPolicy::Action::kRollback) {
    failPendingBoot("local_health_timeout");
    return;
  }
  if (action == BootHealthPolicy::Action::kConfirm) {
    const esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
    bootDiagnostic.phase = static_cast<uint8_t>(BootHealthDiagnostics::Phase::kConfirmation);
    bootDiagnostic.confirmationResult = static_cast<int32_t>(result);
    bootDiagnostic.elapsedMs = elapsed;
    bootDiagnostic.loopCount = bootHealth.loops();
    if (BootHealthPolicy::afterConfirmationApi(result == ESP_OK) == BootHealthPolicy::Action::kNone) {
      bootDiagnostic.phase = static_cast<uint8_t>(BootHealthDiagnostics::Phase::kConfirmed);
      (void)persistBootDiagnostic();
      pendingVerification = false;
      if (localUsbPending) {
        Preferences prefs;
        if (prefs.begin("ota-state", false)) {
          prefs.putString("active_release", pendingReleaseId);
          prefs.remove("attempt_release");
          prefs.remove("attempt_commit");
          prefs.remove("attempt_partition");
          prefs.putUChar("attempts", 0);
          prefs.end();
        } else {
          Serial.println("OTA_HEALTH local_usb_confirmation_bookkeeping_failed");
        }
      } else {
        (void)queueTerminalOutcome(pendingReleaseId, TerminalState::kHealthy, nullptr);
      }
      const esp_partition_t* running = esp_ota_get_running_partition();
      Serial.printf("OTA_HEALTH confirmed source=%s release=%s partition=0x%lx elapsed_ms=%lu loops=%lu\n",
                    config::kSourceCommit, pendingReleaseId.c_str(),
                    static_cast<unsigned long>(running == nullptr ? 0 : running->address),
                    static_cast<unsigned long>(elapsed), static_cast<unsigned long>(bootHealth.loops()));
    } else {
      bootDiagnostic.failureStage = static_cast<uint8_t>(BootHealthDiagnostics::FailureStage::kConfirmationApi);
      (void)persistBootDiagnostic();
      Serial.printf("OTA_HEALTH confirmation_api_failed result=%s code=%d\n",
                    esp_err_to_name(result), static_cast<int>(result));
      failPendingBoot("confirmation_api_failed");
    }
  }
}

void reportBootOutcomeWhenOnline() {
  String retryRelease;
  String retryDetail;
  TerminalState retryState = TerminalState::kFailed;
  {
    TerminalOutboxLock lock;
    if (!lock.locked()) return;
    if (terminalEnqueuePending) {
      retryRelease = terminalEnqueueRelease;
      retryDetail = terminalEnqueueDetail;
      retryState = terminalEnqueueState;
    }
  }
  if (retryRelease.length())
    (void)queueTerminalOutcome(retryRelease, retryState,
                               retryDetail.length() ? retryDetail.c_str() : nullptr);
  TerminalOutcomeRecord outcome{};
  OutboxResult loaded;
  {
    TerminalOutboxLock lock;
    if (!lock.locked()) return;
    loaded = loadTerminalOutcome(terminalOutcomeStore, &outcome);
  }
  if (loaded == OutboxResult::kEmpty) return;
  if (loaded != OutboxResult::kQueued) {
    Serial.printf("OTA_STATUS outbox_load_failed result=%u\n", static_cast<unsigned>(loaded));
    return;
  }
  if (WiFi.status() != WL_CONNECTED || millis() < nextOutcomeAttemptAt) return;
  nextOutcomeAttemptAt = millis() + 30000;
  Preferences prefs;
  if (!prefs.begin("ota-state", false)) {
    Serial.println("OTA_STATUS bookkeeping_open_failed; outcome retained");
    return;
  }
  bool bookkeepingWritten = true;
  if (outcome.state == static_cast<uint8_t>(TerminalState::kHealthy)) {
    bookkeepingWritten = prefs.putString("active_release", outcome.releaseId) > 0 &&
                         prefs.putUChar("attempts", 0) == sizeof(uint8_t);
  } else if (outcome.state == static_cast<uint8_t>(TerminalState::kRolledBack)) {
    bookkeepingWritten = prefs.putString("failed_release", outcome.releaseId) > 0 &&
                         prefs.putUChar("attempts", kMaxReleaseAttempts) == sizeof(uint8_t);
  }
  const bool clearAttemptMarkers =
      outcome.state == static_cast<uint8_t>(TerminalState::kHealthy) ||
      outcome.state == static_cast<uint8_t>(TerminalState::kRolledBack);
  if (bookkeepingWritten && clearAttemptMarkers) {
    prefs.remove("attempt_release");
    prefs.remove("attempt_commit");
    prefs.remove("attempt_partition");
  }
  bool cleared = bookkeepingWritten && (!clearAttemptMarkers || (
      prefs.getString("attempt_release", "").length() == 0 &&
      prefs.getString("attempt_commit", "").length() == 0 &&
      prefs.getUInt("attempt_partition", UINT32_MAX) == UINT32_MAX));
  if (outcome.state == static_cast<uint8_t>(TerminalState::kHealthy))
    cleared = cleared && prefs.getString("active_release", "") == outcome.releaseId &&
        prefs.getUChar("attempts", 255) == 0;
  else if (outcome.state == static_cast<uint8_t>(TerminalState::kRolledBack))
    cleared = cleared && prefs.getString("failed_release", "") == outcome.releaseId &&
        prefs.getUChar("attempts", 0) == kMaxReleaseAttempts;
  prefs.end();
  if (!cleared) {
    Serial.println("OTA_STATUS attempt_cleanup_failed; outcome retained");
    return;
  }
  const char* state = outcome.state == static_cast<uint8_t>(TerminalState::kHealthy) ? "healthy"
      : outcome.state == static_cast<uint8_t>(TerminalState::kRolledBack) ? "rolled_back" : "failed";
  if (!postStatus(outcome.releaseId, state, outcome.detail[0] ? outcome.detail : nullptr,
                  outcome.eventId)) return;
  OutboxResult clearedOutbox;
  {
    TerminalOutboxLock lock;
    if (!lock.locked()) {
      Serial.println("OTA_STATUS outbox_ack_lock_failed; clear pending verification");
      return;
    }
    clearedOutbox = clearAcknowledgedOutcome(terminalOutcomeStore, outcome);
  }
  if (clearedOutbox != OutboxResult::kEmpty)
    Serial.printf("OTA_STATUS outbox_ack_clear_unverified result=%u release=%s\n",
                  static_cast<unsigned>(clearedOutbox), outcome.releaseId);
}

void cancel() { cancelRequested.store(true); }
bool busy() { return transferActive.load(); }

int checkAndInstall() {
  TerminalOutboxLock lock;
  if (!lock.locked()) return -1;
  const bool memoryPending = terminalEnqueuePending;
  const OutboxResult queuedOutcome = memoryPending ? OutboxResult::kPendingDifferentOutcome
      : loadTerminalOutcome(terminalOutcomeStore, nullptr);
  // Release the store lock before doing any Wi-Fi or HTTP work.
  lock.unlock();
  if (queuedOutcome != OutboxResult::kEmpty) {
    if (queuedOutcome != OutboxResult::kQueued)
      Serial.printf("OTA_STATUS update_blocked_by_outbox result=%u\n", static_cast<unsigned>(queuedOutcome));
    return -1;
  }
  if (transferActive.load() || WiFi.status() != WL_CONNECTED || pendingVerification) return -1;
  JsonDocument assignment;
  if (!getAssignment(assignment)) return -1;
  if (assignment.isNull()) return HTTP_CODE_NO_CONTENT;
  JsonObjectConst manifest = assignment["manifest"].as<JsonObjectConst>();
  String releaseId, sourceCommit;
  size_t expectedSize = 0;
  uint8_t expectedDigest[32] = {};
  if (manifest.isNull() || !verifyManifest(manifest, assignment["signature"] | "",
                                           releaseId, sourceCommit, expectedSize, expectedDigest)) {
    if (releaseId.length() > 0 && !hasExceededAttempts(releaseId)) {
      saveFailure(releaseId, sourceCommit.c_str(), "manifest_signature_or_compatibility_failed");
    }
    return 422;
  }
  if (sourceCommit == String(config::kSourceCommit) || hasExceededAttempts(releaseId)) {
    Serial.printf("OTA release skipped release=%s (already installed or blocked)\n", releaseId.c_str());
    return HTTP_CODE_NO_CONTENT;
  }
  const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
  esp_chip_info_t chipInfo;
  esp_chip_info(&chipInfo);
  if (chipInfo.model != CHIP_ESP32S3 || !checkPartitionLayout(target)) {
    saveFailure(releaseId, sourceCommit.c_str(), "partition_layout_mismatch");
    return 422;
  }
  postStatus(releaseId, "downloading", "begin", newEventId("downloading"));
  local_endpoint::SecureClient secure;
  HTTPClient http;
  noRedirects(http);
  const String expectedPath = "/v1/device/firmware/releases/" + releaseId + "/binary";
  const String returnedPath = assignment["binary_path"] | "";
  if (returnedPath != expectedPath || returnedPath.indexOf("..") >= 0 ||
      local_endpoint::beginRequest(http, secure, LEOPOLD_API_BASE_URL,
          baseUrl() + expectedPath, LEOPOLD_TLS_ROOT_CA,
          kConnectTimeoutMs) != local_endpoint::BeginStatus::kReady) {
    saveFailure(releaseId, sourceCommit.c_str(), "invalid_binary_path");
    return 422;
  }
  secureHeaders(http);
  const int code = http.GET();
  if (code != HTTP_CODE_OK || http.getSize() != static_cast<int>(expectedSize)) {
    http.end();
    saveFailure(releaseId, sourceCommit.c_str(), "download_length_or_http_error");
    return code > 0 ? code : -1;
  }
  // Keep the transfer scratch out of the cloud-worker stack. PSRAM is required
  // for this OTA path so a low internal heap cannot starve TLS or other workers.
  std::unique_ptr<uint8_t, decltype(&heap_caps_free)> buffer(
      static_cast<uint8_t*>(heap_caps_malloc(
          kTransferBufferBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)),
      &heap_caps_free);
  if (!buffer) {
    http.end();
    Serial.printf("OTA_STATUS transfer_buffer_unavailable psram_free=%lu psram_largest=%lu\n",
                  static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)),
                  static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    saveFailure(releaseId, sourceCommit.c_str(), "transfer_buffer_unavailable");
    return -1;
  }
  esp_ota_handle_t handle = 0;
  if (esp_ota_begin(target, expectedSize, &handle) != ESP_OK) {
    http.end();
    saveFailure(releaseId, sourceCommit.c_str(), "ota_begin_failed");
    return -1;
  }
  transferActive.store(true);
  cancelRequested.store(false);
  mbedtls_sha256_context hash;
  mbedtls_sha256_init(&hash);
  mbedtls_sha256_starts(&hash, 0);
  size_t received = 0;
  uint32_t lastDataAt = millis();
  uint32_t transferStart = lastDataAt;
  int progress = -1;
  WiFiClient* stream = http.getStreamPtr();
  while (received < expectedSize) {
    if (cancelRequested.load()) {
      http.end(); mbedtls_sha256_free(&hash);
      return fail(releaseId, sourceCommit.c_str(), "cancelled", handle, true);
    }
    const size_t available = stream->available();
    if (available == 0) {
      if (millis() - lastDataAt > kIdleTimeoutMs || millis() - transferStart > kTransferTimeoutMs) {
        http.end(); mbedtls_sha256_free(&hash);
        return fail(releaseId, sourceCommit.c_str(), "download_timeout", handle, true);
      }
      delay(2);
      continue;
    }
    const size_t amount = stream->readBytes(buffer.get(), min(kTransferBufferBytes, min(available, expectedSize - received)));
    if (amount == 0) continue;
    if (received == 0 && buffer.get()[0] != 0xE9) {
      http.end(); mbedtls_sha256_free(&hash);
      return fail(releaseId, sourceCommit.c_str(), "not_application_image", handle, true);
    }
    if (esp_ota_write(handle, buffer.get(), amount) != ESP_OK) {
      http.end(); mbedtls_sha256_free(&hash);
      return fail(releaseId, sourceCommit.c_str(), "flash_write_failed", handle, true);
    }
    mbedtls_sha256_update(&hash, buffer.get(), amount);
    received += amount;
    lastDataAt = millis();
    const int currentProgress = static_cast<int>((received * 100) / expectedSize);
    if (currentProgress >= progress + 10) {
      progress = currentProgress;
      Serial.printf("OTA download %d%%\n", progress);
    }
    delay(1);
  }
  http.end();
  uint8_t actualDigest[32] = {};
  mbedtls_sha256_finish(&hash, actualDigest);
  mbedtls_sha256_free(&hash);
  if (received != expectedSize || memcmp(actualDigest, expectedDigest, sizeof(actualDigest)) != 0) {
    return fail(releaseId, sourceCommit.c_str(), "sha256_mismatch", handle, true);
  }
  if (esp_ota_end(handle) != ESP_OK) {
    saveFailure(releaseId, sourceCommit.c_str(), "invalid_esp_application_image");
    transferActive.store(false);
    return 422;
  }
  transferActive.store(false);
  postStatus(releaseId, "verified", nullptr, newEventId("verified"));
  Preferences prefs;
  if (!prefs.begin("ota-state", false)) {
    saveFailure(releaseId, sourceCommit.c_str(), "nvs_pending_state_failed");
    return -1;
  }
  const bool attemptRecorded =
      prefs.putString("attempt_release", releaseId) == releaseId.length() &&
      prefs.putString("attempt_commit", sourceCommit) == sourceCommit.length() &&
      prefs.putUInt("attempt_partition", target->address) == sizeof(uint32_t);
  prefs.putString("failed_release", releaseId);
  prefs.putUChar("attempts", 0);
  prefs.end();
  if (!attemptRecorded) {
    Preferences cleanup;
    if (cleanup.begin("ota-state", false)) {
      cleanup.remove("attempt_release");
      cleanup.remove("attempt_commit");
      cleanup.remove("attempt_partition");
      cleanup.end();
    }
    saveFailure(releaseId, sourceCommit.c_str(), "nvs_attempt_partition_failed");
    return -1;
  }
  resetAttempts(releaseId);
  if (esp_ota_set_boot_partition(target) != ESP_OK) {
    Preferences cleanup;
    if (cleanup.begin("ota-state", false)) {
      cleanup.remove("attempt_release");
      cleanup.remove("attempt_commit");
      cleanup.remove("attempt_partition");
      cleanup.end();
    }
    saveFailure(releaseId, sourceCommit.c_str(), "boot_selection_failed");
    return -1;
  }
  postStatus(releaseId, "rebooting", nullptr, newEventId("rebooting"));
  Serial.printf("OTA verified and selected release=%s bytes=%u\n", releaseId.c_str(), static_cast<unsigned>(received));
  delay(300);
  esp_restart();
  return HTTP_CODE_OK;
}

}  // namespace leopold::firmware_ota
