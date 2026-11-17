"""Compile the exact production assignment reader against bounded host fixtures.

This exercises firmware_ota.cpp's getAssignment() body and the pinned
ArduinoJson parser. HTTPClient and local_endpoint are API-level fixtures: they
control the values the production function receives, but do not implement TLS,
socket framing, or Arduino-ESP32 HTTPClient's wire parser.

The manifest verifier is also compiled from its production function bodies,
with the existing public-key header. Its small mbedTLS API surface is adapted
to OpenSSL on the host; this is not a target-mbedTLS test. Synthetic envelopes
have no private signer, while the optional exact replay reads a protected
response only when LEOPOLD_OTA_ASSIGNMENT_REPLAY_PATH is explicitly set.
"""
from __future__ import annotations

import os
import re
import shlex
import shutil
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ARDUINO_JSON = ROOT / "device/.pio/libdeps/leopold/ArduinoJson/src"
OTA_SOURCE = ROOT / "device/src/firmware_ota.cpp"
UI_SOURCE = ROOT / "device/include/generated_firmware_ui.h"


CPP_PREFIX = r'''#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include "ota_signing_key.h"

class String {
 public:
  String() = default;
  String(const char* value) : value_(value ? value : "") {}
  String(std::string value) : value_(std::move(value)) {}
  const char* c_str() const { return value_.c_str(); }
  size_t length() const { return value_.length(); }
  char operator[](size_t index) const { return value_[index]; }
  bool operator==(const String& other) const { return value_ == other.value_; }
  String& operator=(const char* value) {
    value_ = value ? value : "";
    return *this;
  }
  bool concat(const char* value) {
    if (value) value_ += value;
    return true;
  }
 private:
  std::string value_;
};

#include <ArduinoJson.h>

constexpr int HTTP_CODE_OK = 200;
constexpr int HTTP_CODE_NO_CONTENT = 204;
constexpr const char* LEOPOLD_API_BASE_URL = "https://fixture.invalid:8443";
constexpr const char* LEOPOLD_TLS_ROOT_CA = "fixture-ca";
constexpr uint32_t kConnectTimeoutMs = 5000;

struct HttpFixture {
  int begin_status = 0;
  int http_status = 200;
  int content_length = 0;
  String body;
  unsigned begin_calls = 0;
  unsigned get_calls = 0;
  unsigned body_calls = 0;
  unsigned end_calls = 0;
};

HttpFixture* active_fixture = nullptr;

class HTTPClient {
 public:
  HTTPClient() : fixture_(active_fixture) { assert(fixture_ != nullptr); }
  void setFollowRedirects(int) {}
  void setConnectTimeout(int32_t) {}
  void setTimeout(uint16_t) {}
  void addHeader(const char*, const String&) {}
  void addHeader(const char*, const char*) {}
  int GET() {
    ++fixture_->get_calls;
    return fixture_->http_status;
  }
  int getSize() const { return fixture_->content_length; }
  String getString() {
    ++fixture_->body_calls;
    return fixture_->body;
  }
  void end() { ++fixture_->end_calls; }
 private:
  HttpFixture* fixture_;
};

namespace local_endpoint {
class SecureClient {};
enum class BeginStatus { kReady, kResolvePending, kResolveUnavailable,
                         kServiceRefused, kTlsFailure, kBeginFailure };
BeginStatus beginRequest(::HTTPClient&, SecureClient&, const char*, const String&,
                         const char*, uint32_t);
BeginStatus beginRequest(::HTTPClient&, SecureClient&, const char*, const String&,
                         const char*, uint32_t) {
  assert(active_fixture != nullptr);
  ++active_fixture->begin_calls;
  return static_cast<BeginStatus>(active_fixture->begin_status);
}
}  // namespace local_endpoint

void noRedirects(HTTPClient& http) {
  http.setFollowRedirects(0);
  http.setConnectTimeout(5000);
  http.setTimeout(1000);
}
void secureHeaders(HTTPClient& http) {
  (void)http;
}
String baseUrl() { return String(LEOPOLD_API_BASE_URL); }
String operator+(const String& left, const char* right) {
  return String(std::string(left.c_str()) + right);
}

// Host adapter for the production verifier's small mbedTLS API surface.
// The production public key macro is included unchanged. ECDSA verification
// uses native OpenSSL EVP, not the ESP target's mbedTLS archive.
constexpr int MBEDTLS_MD_SHA256 = 0;
unsigned decode_calls = 0;
unsigned public_key_parse_calls = 0;
unsigned signature_verify_calls = 0;
struct mbedtls_pk_context { EVP_PKEY* key = nullptr; };
void mbedtls_pk_init(mbedtls_pk_context* context) { context->key = nullptr; }
void mbedtls_pk_free(mbedtls_pk_context* context) { EVP_PKEY_free(context->key); }
int mbedtls_pk_parse_public_key(mbedtls_pk_context* context,
                                const unsigned char* pem, size_t pem_length) {
  ++public_key_parse_calls;
  BIO* input = BIO_new_mem_buf(pem, static_cast<int>(pem_length));
  if (!input) return -1;
  context->key = PEM_read_bio_PUBKEY(input, nullptr, nullptr, nullptr);
  BIO_free(input);
  return context->key ? 0 : -1;
}
int mbedtls_pk_verify(mbedtls_pk_context* context, int digest_type,
                      const unsigned char* hash, size_t hash_length,
                      const unsigned char* signature, size_t signature_length) {
  ++signature_verify_calls;
  if (!context->key || digest_type != MBEDTLS_MD_SHA256) return -1;
  EVP_PKEY_CTX* verifier = EVP_PKEY_CTX_new(context->key, nullptr);
  if (!verifier) return -1;
  const bool initialized = EVP_PKEY_verify_init(verifier) > 0 &&
      EVP_PKEY_CTX_set_signature_md(verifier, EVP_sha256()) > 0;
  const int result = initialized
      ? EVP_PKEY_verify(verifier, signature, signature_length, hash, hash_length)
      : -1;
  EVP_PKEY_CTX_free(verifier);
  return result == 1 ? 0 : -1;
}
int mbedtls_base64_decode(unsigned char* output, size_t output_capacity,
                          size_t* output_length, const unsigned char* input,
                          size_t input_length) {
  ++decode_calls;
  *output_length = 0;
  if (input_length > static_cast<size_t>(INT_MAX))
    return -1;
  // EVP_DecodeBlock writes the full block length before considering the
  // caller's capacity. Decode into temporary storage and bound the copy.
  const size_t temporary_capacity = ((input_length + 3) / 4) * 3;
  std::vector<unsigned char> temporary(temporary_capacity);
  const int decoded = EVP_DecodeBlock(temporary.data(), input, static_cast<int>(input_length));
  if (decoded < 0) return -1;
  size_t length = static_cast<size_t>(decoded);
  if (input_length > 0 && input[input_length - 1] == '=') --length;
  if (input_length > 1 && input[input_length - 2] == '=') --length;
  if (length > output_capacity) return -1;
  std::memcpy(output, temporary.data(), length);
  *output_length = length;
  return 0;
}
int mbedtls_sha256(const unsigned char* input, size_t input_length,
                   unsigned char output[32], int) {
  unsigned int output_length = 0;
  return EVP_Digest(input, input_length, output, &output_length,
                    EVP_sha256(), nullptr) == 1 && output_length == 32 ? 0 : -1;
}

'''


CPP_MAIN = r'''
static const String kValidBody = R"json({"release_id":"rel-offline-fixture","manifest":{"byte_length":1234,"hardware_target":"esp32s3","key_id":"18124c21ca7b5a7b","partition_layout":"leopold-ota-6m25-v1","release_id":"rel-offline-fixture","sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature_algorithm":"ECDSA-P256-SHA256","source_commit":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","ui_revision":17},"signature":"fixture-signature","binary_path":"/v1/device/firmware/releases/rel-offline-fixture/binary"})json";

void reset(HttpFixture& fixture) {
  fixture = HttpFixture{};
  fixture.http_status = HTTP_CODE_OK;
  fixture.body = kValidBody;
  fixture.content_length = static_cast<int>(fixture.body.length());
  active_fixture = &fixture;
}

void verifySyntheticMutation(const char* field, const char* value) {
  JsonDocument changed;
  assert(deserializeJson(changed, kValidBody.c_str()) == DeserializationError::Ok);
  changed["manifest"][field] = value;
  String releaseId, sourceCommit;
  size_t expectedSize = 0;
  uint8_t expectedDigest[32] = {};
  decode_calls = public_key_parse_calls = signature_verify_calls = 0;
  assert(!leopold::firmware_ota::verifyManifest(
      changed["manifest"].as<JsonObjectConst>(), changed["signature"] | "",
      releaseId, sourceCommit, expectedSize, expectedDigest));
  assert(decode_calls == 0 && public_key_parse_calls == 0 && signature_verify_calls == 0);
}

void verifySyntheticNumberMutation(const char* field, uint32_t value) {
  JsonDocument changed;
  assert(deserializeJson(changed, kValidBody.c_str()) == DeserializationError::Ok);
  changed["manifest"][field] = value;
  String releaseId, sourceCommit;
  size_t expectedSize = 0;
  uint8_t expectedDigest[32] = {};
  decode_calls = public_key_parse_calls = signature_verify_calls = 0;
  assert(!leopold::firmware_ota::verifyManifest(
      changed["manifest"].as<JsonObjectConst>(), changed["signature"] | "",
      releaseId, sourceCommit, expectedSize, expectedDigest));
  assert(decode_calls == 0 && public_key_parse_calls == 0 && signature_verify_calls == 0);
}

int main(int argc, char** argv) {
  HttpFixture fixture;
  JsonDocument doc;

  // Production decodeBase64() must reject oversized valid input without
  // touching bytes outside the supplied output region.
  {
    const std::string oversized_base64(172, 'A');  // 129 decoded bytes.
    uint8_t guarded_output[130];
    memset(guarded_output, 0xA5, sizeof(guarded_output));
    size_t decoded_length = 0;
    assert(!leopold::firmware_ota::decodeBase64(
        oversized_base64.c_str(), guarded_output + 1, 128, decoded_length));
    assert(guarded_output[0] == 0xA5 && guarded_output[129] == 0xA5);
    assert(decoded_length == 0);
    const char malformed_base64[] = "%%%=";
    assert(!leopold::firmware_ota::decodeBase64(
        malformed_base64, guarded_output + 1, 128, decoded_length));
  }

  // A fixed-length assignment reaches ArduinoJson and retains its fields.
  reset(fixture);
  assert(getAssignment(doc));
  assert(doc["release_id"] == "rel-offline-fixture");
  assert(doc["manifest"]["source_commit"] == "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  assert(fixture.begin_calls == 1 && fixture.get_calls == 1);
  assert(fixture.body_calls == 1 && fixture.end_calls == 1);

  // An unassigned 204 is success with no JSON document and no body read.
  reset(fixture);
  fixture.http_status = HTTP_CODE_NO_CONTENT;
  fixture.content_length = -1;
  fixture.body = "";
  doc.clear();
  assert(getAssignment(doc));
  assert(doc.isNull());
  assert(fixture.get_calls == 1 && fixture.body_calls == 0 && fixture.end_calls == 1);

  // Unexpected status, failed begin, and invalid declared lengths fail closed.
  reset(fixture);
  fixture.http_status = 401;
  assert(!getAssignment(doc));
  assert(fixture.body_calls == 0 && fixture.end_calls == 1);

  reset(fixture);
  fixture.begin_status = static_cast<int>(local_endpoint::BeginStatus::kTlsFailure);
  assert(!getAssignment(doc));
  assert(fixture.begin_calls == 1 && fixture.get_calls == 0 && fixture.end_calls == 0);

  for (int invalid_length : {0, -1, 4097}) {
    reset(fixture);
    fixture.content_length = invalid_length;
    assert(!getAssignment(doc));
    assert(fixture.body_calls == 0 && fixture.end_calls == 1);
  }

  // Declared/body mismatch and malformed JSON fail after one bounded read.
  reset(fixture);
  fixture.content_length += 1;
  assert(!getAssignment(doc));
  assert(fixture.body_calls == 1 && fixture.end_calls == 1);

  reset(fixture);
  fixture.body = "{\"manifest\":";
  fixture.content_length = static_cast<int>(fixture.body.length());
  assert(!getAssignment(doc));
  assert(fixture.body_calls == 1 && fixture.end_calls == 1);

  // The exact production verifier is compiled below. The synthetic envelope
  // reaches base64 decode after passing all metadata predicates, then fails
  // because no private signer is present in this test.
  JsonDocument synthetic;
  assert(deserializeJson(synthetic, kValidBody.c_str()) == DeserializationError::Ok);
  String releaseId, sourceCommit;
  size_t expectedSize = 0;
  uint8_t expectedDigest[32] = {};
  decode_calls = public_key_parse_calls = signature_verify_calls = 0;
  assert(!leopold::firmware_ota::verifyManifest(
      synthetic["manifest"].as<JsonObjectConst>(), synthetic["signature"] | "",
      releaseId, sourceCommit, expectedSize, expectedDigest));
  assert(releaseId == String("rel-offline-fixture"));
  assert(sourceCommit == String("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  assert(expectedSize == 1234);
  assert(decode_calls == 1 && public_key_parse_calls == 0 && signature_verify_calls == 0);

  for (const auto& mutation : {
           std::pair<const char*, const char*>{"signature_algorithm", "RSA"},
           {"key_id", "wrong-key-id"},
           {"hardware_target", "esp32"},
           {"partition_layout", "wrong-layout"},
           {"release_id", "invalid/release"},
           {"source_commit", "ABC"},
           {"sha256", "not-a-digest"}}) {
    verifySyntheticMutation(mutation.first, mutation.second);
  }
  for (uint32_t revision : {0u, 16u})
    verifySyntheticNumberMutation("ui_revision", revision);
  for (uint32_t length : {0u, 6553601u})
    verifySyntheticNumberMutation("byte_length", length);

  if (argc == 2) {
    // The protected response is read only for this explicit manual replay.
    // No exact signed envelope or response path is embedded in this source.
    std::ifstream input(argv[1], std::ios::binary);
    const std::string exact_body((std::istreambuf_iterator<char>(input)), {});
    assert(input.good() || input.eof());
    assert(exact_body.size() == 595);
    reset(fixture);
    fixture.body = String(exact_body);
    fixture.content_length = static_cast<int>(exact_body.size());
    JsonDocument assignment;
    assert(getAssignment(assignment));
    assert(assignment["release_id"] == "local-b-de9dd5cc5661b317");
    assert(assignment["manifest"]["signature_algorithm"] == "ECDSA-P256-SHA256");
    assert(assignment["manifest"]["key_id"] == "18124c21ca7b5a7b");
    assert(assignment["manifest"]["hardware_target"] == "esp32s3");
    assert(assignment["manifest"]["partition_layout"] == "leopold-ota-6m25-v1");
    assert(assignment["manifest"]["ui_revision"] == 17);
    assert(assignment["manifest"]["byte_length"] == 5008272);
    assert(assignment["binary_path"] ==
        "/v1/device/firmware/releases/local-b-de9dd5cc5661b317/binary");

    releaseId = "";
    sourceCommit = "";
    expectedSize = 0;
    memset(expectedDigest, 0, sizeof(expectedDigest));
    decode_calls = public_key_parse_calls = signature_verify_calls = 0;
    assert(leopold::firmware_ota::verifyManifest(
        assignment["manifest"].as<JsonObjectConst>(), assignment["signature"] | "",
        releaseId, sourceCommit, expectedSize, expectedDigest));
    assert(releaseId == String("local-b-de9dd5cc5661b317"));
    assert(sourceCommit == String("542cd9e9b53f7a6dd2d18af4fe625dca5cb04ed6"));
    assert(expectedSize == 5008272);
    assert(decode_calls == 1 && public_key_parse_calls == 1 && signature_verify_calls == 1);
    char digest[65] = {};
    for (size_t i = 0; i < sizeof(expectedDigest); ++i)
      snprintf(digest + i * 2, 3, "%02x", expectedDigest[i]);
    assert(std::string(digest) ==
        "7e3b9340119e8ed9da971898d247e48948c1cd346d318e913e68670fc465c5ca");

    JsonDocument tampered;
    assert(deserializeJson(tampered, exact_body) == DeserializationError::Ok);
    tampered["signature"] = "AA==";
    releaseId = "";
    sourceCommit = "";
    expectedSize = 0;
    decode_calls = public_key_parse_calls = signature_verify_calls = 0;
    assert(!leopold::firmware_ota::verifyManifest(
        tampered["manifest"].as<JsonObjectConst>(), tampered["signature"] | "",
        releaseId, sourceCommit, expectedSize, expectedDigest));
    assert(decode_calls == 1 && public_key_parse_calls == 1 && signature_verify_calls == 1);
  }
}
'''


def extract_get_assignment(source: str) -> str:
    return extract_function(source, "bool getAssignment(JsonDocument& doc) {")


def extract_function(source: str, marker: str) -> str:
    if source.count(marker) != 1:
        raise AssertionError(f"expected one production definition for {marker!r}")
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 0
    for offset in range(opening, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[start : offset + 1]
    raise AssertionError(f"production function {marker!r} has no closing brace")


def test_production_assignment_reader_with_controlled_http_fixture() -> None:
    compiler = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        raise AssertionError("a C++ compiler is required for the assignment-reader regression")

    ota_source = OTA_SOURCE.read_text()
    production_function = extract_get_assignment(ota_source)
    base64_function = extract_function(
        ota_source,
        "bool decodeBase64(const char* input, uint8_t* output, size_t outputCapacity, size_t& outputLength) {",
    )
    verifier_function = extract_function(
        ota_source,
        "bool verifyManifest(JsonObjectConst manifest, const char* signatureText,",
    )
    layout_match = re.search(r'constexpr char kLayoutId\[\] = "([^"]+)";', ota_source)
    slot_match = re.search(r"constexpr size_t kSlotBytes = (0x[0-9a-fA-F]+);", ota_source)
    ui_match = re.search(r"constexpr uint32_t kUiRevision = (\d+);", UI_SOURCE.read_text())
    if layout_match is None or slot_match is None or ui_match is None:
        raise AssertionError("could not extract OTA compatibility constants from production source")
    verifier_prefix = (
        "namespace leopold { namespace generated_ui { constexpr uint32_t kUiRevision = "
        + ui_match.group(1)
        + "; } namespace firmware_ota { constexpr char kLayoutId[] = \""
        + layout_match.group(1)
        + "\"; constexpr size_t kSlotBytes = "
        + slot_match.group(1)
        + ";\n"
    )
    verifier_body = base64_function + "\n" + verifier_function
    verifier_suffix = "\n} }  // namespace leopold::firmware_ota\n"
    with tempfile.TemporaryDirectory(prefix="leopold-ota-assignment-") as directory:
        temporary = Path(directory)
        (temporary / "Arduino.h").write_text("#pragma once\n")
        harness = temporary / "assignment_reader.cpp"
        executable = temporary / "assignment_reader"
        harness.write_text(
            CPP_PREFIX + production_function + verifier_prefix + verifier_body +
            verifier_suffix + CPP_MAIN
        )
        openssl_flags = shlex.split(
            subprocess.check_output(["pkg-config", "--cflags", "--libs", "openssl"], text=True)
        )
        subprocess.run(
            [
                compiler,
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-DARDUINO",
                "-DARDUINOJSON_ENABLE_ARDUINO_STRING=1",
                "-DARDUINOJSON_ENABLE_ARDUINO_STREAM=0",
                "-DARDUINOJSON_ENABLE_ARDUINO_PRINT=0",
                "-DARDUINOJSON_ENABLE_PROGMEM=0",
                f"-I{temporary}",
                f"-I{ARDUINO_JSON}",
                f"-I{ROOT / 'device/include'}",
                str(harness),
                "-o",
                str(executable),
                *openssl_flags,
            ],
            check=True,
            cwd=ROOT,
        )
        command = [str(executable)]
        replay_path = os.environ.get("LEOPOLD_OTA_ASSIGNMENT_REPLAY_PATH")
        if replay_path:
            command.append(replay_path)
        subprocess.run(command, check=True, cwd=ROOT)
