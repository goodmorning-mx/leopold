#include "local_endpoint_transport.h"

#include <WiFi.h>
#include <esp_event.h>
#include <mdns.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <string.h>

namespace leopold::local_endpoint {
namespace {
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
ResolverLifecycle lifecycle;
TimeoutLogLimiter timeoutLogLimiter;
std::atomic<uint32_t> connectionGeneration{1};
std::atomic<bool> cacheInvalidationPending{false};
TaskHandle_t resolverTaskHandle = nullptr;
bool mdnsReady = false;
mdns_search_once_t* activeQuery = nullptr;
char activeHost[254] = {};
char requestedHostname[254] = {};
bool hasResolveRequest = false;
bool started = false;

bool parse(const char* origin, Endpoint& out) {
  if (!origin || strncmp(origin, "https://", 8) != 0) return false;
  const char* start = origin + 8;
  const char* end = strpbrk(start, "/?#");
  if (!end) end = start + strlen(start);
  if (start == end || memchr(start, '@', end - start) != nullptr) return false;
  const char* colon = static_cast<const char*>(memchr(start, ':', end - start));
  const char* hostEnd = colon == nullptr ? end : colon;
  if (hostEnd == start || size_t(hostEnd - start) >= 254) return false;
  out.origin = origin;
  out.hostname = String(start).substring(0, hostEnd - start);
  out.local = out.hostname.endsWith(".local");
  if (colon == nullptr) out.port = 443;
  else {
    char* parsedEnd = nullptr;
    const long port = strtol(colon + 1, &parsedEnd, 10);
    if (parsedEnd != end || port < 1 || port > 65535) return false;
    out.port = static_cast<uint16_t>(port);
  }
  return true;
}

void wifiEvent(arduino_event_id_t event, arduino_event_info_t) {
  if (event == ARDUINO_EVENT_WIFI_STA_START ||
      event == ARDUINO_EVENT_WIFI_STA_STOP ||
      event == ARDUINO_EVENT_WIFI_STA_CONNECTED ||
      event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED ||
      event == ARDUINO_EVENT_WIFI_STA_GOT_IP ||
      event == ARDUINO_EVENT_WIFI_STA_LOST_IP) {
    connectionGeneration.fetch_add(1, std::memory_order_acq_rel);
    cacheInvalidationPending.store(true, std::memory_order_release);
    // No lock, allocation, mDNS cleanup, or wait is permitted in this callback.
  }
}

IPAddress toArduinoAddress(IpAddress value) {
  return IPAddress(value.value);
}

IpAddress toPolicyAddress(const mdns_result_t* results) {
  for (const mdns_result_t* item = results; item != nullptr; item = item->next) {
    if (item->addr && item->addr->addr.type == ESP_IPADDR_TYPE_V4) {
      const uint32_t value = item->addr->addr.u_addr.ip4.addr;
      if (value != 0) return IpAddress{value};
    }
  }
  return {};
}

void resolverStep() {
    char requested[254] = {};
    bool request = false;
    portENTER_CRITICAL(&stateMux);
    if (hasResolveRequest) {
      memcpy(requested, requestedHostname, sizeof(requested));
      requestedHostname[0] = '\0';
      hasResolveRequest = false;
      request = true;
    }
    portEXIT_CRITICAL(&stateMux);

    const uint32_t now = millis();
    const uint32_t generation = connectionGeneration.load(std::memory_order_acquire);
    if (cacheInvalidationPending.exchange(false, std::memory_order_acq_rel)) {
      portENTER_CRITICAL(&stateMux);
      lifecycle.invalidate(generation);
      portEXIT_CRITICAL(&stateMux);
    }
    if (request && WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
      ResolverLifecycle::Decision decision;
      portENTER_CRITICAL(&stateMux);
      decision = lifecycle.request(requested, now, generation);
      portEXIT_CRITICAL(&stateMux);
      if (decision == ResolverLifecycle::Decision::kStart) {
        if (!mdnsReady) mdnsReady = mdns_init() == ESP_OK;
        char queryName[254] = {};
        strlcpy(queryName, requested, sizeof(queryName));
        const size_t length = strlen(queryName);
        if (length > 6 && strcmp(queryName + length - 6, ".local") == 0) {
          queryName[length - 6] = '\0';
        }
        activeQuery = mdnsReady ? mdns_query_async_new(queryName, nullptr, nullptr,
            MDNS_TYPE_A, kResolveDeadlineMs, 1, nullptr) : nullptr;
        if (activeQuery) {
          strlcpy(activeHost, requested, sizeof(activeHost));
        } else {
          portENTER_CRITICAL(&stateMux);
          lifecycle.queryStartFailed(now);
          portEXIT_CRITICAL(&stateMux);
        }
      }
    }

    if (activeQuery != nullptr) {
      mdns_result_t* results = nullptr;
      const bool finished = mdns_query_async_get_results(activeQuery, 0, &results, nullptr);
      const IpAddress address = finished ? toPolicyAddress(results) : IpAddress{};
      if (results) mdns_query_results_free(results);
      bool reaped = false;
      bool timedOut = false;
      char timeoutHost[254] = {};
      portENTER_CRITICAL(&stateMux);
      reaped = lifecycle.poll(finished, address, millis(),
          connectionGeneration.load(std::memory_order_acquire));
      timedOut = lifecycle.takeTimeout();
      if (timedOut) strlcpy(timeoutHost, activeHost, sizeof(timeoutHost));
      portEXIT_CRITICAL(&stateMux);
      if (finished) {
        if (!reaped) {
          Serial.printf("LOCAL_MDNS_QUERY_OWNERSHIP_ERROR host=%s\n", activeHost);
        }
        // mDNS documents deletion only after completion; always release the
        // finished object even if the local state invariant was violated.
        mdns_query_async_delete(activeQuery);
        activeQuery = nullptr;
        activeHost[0] = '\0';
      }
      if (timeoutLogLimiter.allowTimeout(timedOut, millis())) {
        Serial.printf("LOCAL_MDNS_TIMEOUT host=%s timeout_ms=1000\n", timeoutHost);
      }
    }
}

void resolverTask(void*) {
  for (;;) {
    resolverStep();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
}  // namespace

bool start() {
  if (started) return true;
  WiFi.onEvent(wifiEvent);
  const BaseType_t result = xTaskCreatePinnedToCore(
      resolverTask, "local-mdns", 4096, nullptr, 1, &resolverTaskHandle, 0);
  started = result == pdPASS;
  return started;
}

void requestResolve(const char* origin) {
  Endpoint endpoint;
  if (!parse(origin, endpoint) || !endpoint.local) return;
  portENTER_CRITICAL(&stateMux);
  strlcpy(requestedHostname, endpoint.hostname.c_str(), sizeof(requestedHostname));
  hasResolveRequest = true;
  portEXIT_CRITICAL(&stateMux);
}

ResolveStatus resolveOrigin(const char* origin, Endpoint& endpoint) {
  if (!parse(origin, endpoint)) return ResolveStatus::kUnavailable;
  if (!endpoint.local) return ResolveStatus::kNotLocal;
  requestResolve(origin);
  IpAddress address;
  uint32_t expiresAt = 0;
  const uint32_t generation = connectionGeneration.load(std::memory_order_acquire);
  bool found = false;
  portENTER_CRITICAL(&stateMux);
  if (cacheInvalidationPending.exchange(false, std::memory_order_acq_rel)) {
    lifecycle.invalidate(generation);
  }
  found = lifecycle.cached(endpoint.hostname.c_str(), millis(), generation, address, &expiresAt);
  portEXIT_CRITICAL(&stateMux);
  if (!found) {
    return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)
        ? ResolveStatus::kPending : ResolveStatus::kUnavailable;
  }
  endpoint.address = toArduinoAddress(address);
  endpoint.generation = generation;
  endpoint.expiresAt = expiresAt;
  return ResolveStatus::kResolved;
}

void SecureClient::bindResolved(const Endpoint& endpoint) {
  bound_ = endpoint.local;
  address_ = endpoint.address;
  hostname_ = endpoint.hostname;
  port_ = endpoint.port;
  generation_ = endpoint.generation;
  expiresAt_ = endpoint.expiresAt;
}

void SecureClient::clearResolved() {
  bound_ = false;
  address_ = IPAddress();
  hostname_ = "";
  port_ = 0;
  generation_ = 0;
  expiresAt_ = 0;
}

uint8_t SecureClient::connected() {
  if (bound_ && (generation_ != connectionGeneration.load(std::memory_order_acquire) ||
      WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0))) {
    stop();
    return 0;
  }
  return WiFiClientSecure::connected();
}

int SecureClient::connectBound(const char* host, uint16_t port, const char* ca,
                               const char* clientCert, const char* privateKey) {
  if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) return 0;
  const IpAddress policyAddress{static_cast<uint32_t>(address_)};
  const uint32_t currentGeneration = connectionGeneration.load(std::memory_order_acquire);
  return connectResolvedHost(host, hostname_.c_str(), policyAddress, port_, port,
      generation_, currentGeneration, expiresAt_, millis(),
      [this, ca, clientCert, privateKey](IpAddress, uint16_t targetPort, const char* tlsHostname) {
        const int result = NetworkClientSecure::connect(
            address_, targetPort, tlsHostname,
            ca ? ca : _CA_cert,
            clientCert ? clientCert : _cert,
            privateKey ? privateKey : _private_key);
        if (generation_ != connectionGeneration.load(std::memory_order_acquire) ||
            WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
          stop();
          return 0;
        }
        if (result != 1) {
          char message[96] = {};
          const int error = lastError(message, sizeof(message));
          if (error == -1) {
            Serial.printf("LOCAL_ENDPOINT_SERVICE_REFUSED host=%s\n", tlsHostname);
          } else {
            Serial.printf("LOCAL_ENDPOINT_TLS_FAILURE host=%s code=%d %s\n",
                          tlsHostname, error, message);
          }
        }
        return result;
      });
}

int SecureClient::connect(const char* host, uint16_t port, int32_t timeout) {
  if (!bound_) return NetworkClientSecure::connect(host, port, timeout);
  setConnectionTimeout(timeout < 0 ? 0U : static_cast<uint32_t>(timeout));
  return connectBound(host, port);
}

int SecureClient::connect(const char* host, uint16_t port) {
  if (!bound_) return NetworkClientSecure::connect(host, port);
  return connectBound(host, port);
}

int SecureClient::connect(const char* host, uint16_t port, const char* ca,
                          const char* clientCert, const char* privateKey) {
  if (!bound_) return NetworkClientSecure::connect(host, port, ca, clientCert, privateKey);
  return connectBound(host, port, ca, clientCert, privateKey);
}

int SecureClient::connect(const char* host, uint16_t port, const char* pskIdent,
                          const char* pskKey) {
  if (!bound_) return NetworkClientSecure::connect(host, port, pskIdent, pskKey);
  // HTTPClient and the local API use CA-verified TLS, never PSK. Refuse this
  // overload for a pinned hostname instead of losing hostname verification.
  return 0;
}

int SecureClient::connect(IPAddress ip, uint16_t port, int32_t timeout) {
  if (!bound_) return NetworkClientSecure::connect(ip, port, timeout);
  if (ip != address_ || port != port_) return 0;
  setConnectionTimeout(timeout < 0 ? 0U : static_cast<uint32_t>(timeout));
  return connectBound(hostname_.c_str(), port_);
}

int SecureClient::connect(IPAddress ip, uint16_t port) {
  if (!bound_) return NetworkClientSecure::connect(ip, port);
  if (ip != address_ || port != port_) return 0;
  return connectBound(hostname_.c_str(), port_);
}

int SecureClient::connect(IPAddress ip, uint16_t port, const char* ca,
                          const char* clientCert, const char* privateKey) {
  if (!bound_) return NetworkClientSecure::connect(ip, port, ca, clientCert, privateKey);
  if (ip != address_ || port != port_) return 0;
  return connectBound(hostname_.c_str(), port_, ca, clientCert, privateKey);
}

int SecureClient::connect(IPAddress ip, uint16_t port, const char* host,
                          const char* ca, const char* clientCert,
                          const char* privateKey) {
  if (!bound_) return NetworkClientSecure::connect(ip, port, host, ca, clientCert, privateKey);
  if (ip != address_ || port != port_ || !host || strcmp(hostname_.c_str(), host) != 0) return 0;
  return connectBound(host, port, ca, clientCert, privateKey);
}

int SecureClient::connect(IPAddress ip, uint16_t port, const char* pskIdent,
                          const char* pskKey) {
  if (!bound_) return NetworkClientSecure::connect(ip, port, pskIdent, pskKey);
  // PSK does not carry hostname verification; this transport is CA-only.
  return 0;
}

BeginStatus beginRequest(HTTPClient& http, SecureClient& client,
                         const char* origin, const String& url, const char* ca,
                         uint32_t timeoutMs) {
  Endpoint endpoint;
  const ResolveStatus status = resolveOrigin(origin, endpoint);
  if (status == ResolveStatus::kPending) return BeginStatus::kResolvePending;
  if (status == ResolveStatus::kUnavailable) return BeginStatus::kResolveUnavailable;
  client.setCACert(ca);
  client.setConnectionTimeout(timeoutMs);
  client.setHandshakeTimeout((timeoutMs + 999) / 1000);
  client.setTimeout(timeoutMs);
  if (status == ResolveStatus::kResolved) client.bindResolved(endpoint);
  else client.clearResolved();
  http.setConnectTimeout(timeoutMs);
  // HTTPClient owns first connection and reconnect dispatch. The local client
  // intercepts each virtual hostname connect and maps it to the cached IP.
  return http.begin(client, url) ? BeginStatus::kReady : BeginStatus::kBeginFailure;
}

#ifdef LEOPOLD_LOCAL_ENDPOINT_HOST_TEST
namespace host_test {
void pumpResolverOnce() { resolverStep(); }
bool hasActiveQuery() { return activeQuery != nullptr; }
bool cacheValid() {
  bool valid = false;
  portENTER_CRITICAL(&stateMux);
  valid = lifecycle.cacheValid();
  portEXIT_CRITICAL(&stateMux);
  return valid;
}
void publishResolved(const char* hostname, uint32_t address, uint32_t generation, uint32_t now) {
  portENTER_CRITICAL(&stateMux);
  lifecycle.request(hostname, now, generation);
  lifecycle.poll(true, IpAddress{address}, now, generation);
  portEXIT_CRITICAL(&stateMux);
}
uint32_t generation() { return connectionGeneration.load(std::memory_order_acquire); }
}  // namespace host_test
#endif

}  // namespace leopold::local_endpoint
