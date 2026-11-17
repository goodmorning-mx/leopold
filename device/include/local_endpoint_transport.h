#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include "local_endpoint_policy.h"

namespace leopold::local_endpoint {

enum class ResolveStatus : uint8_t { kResolved, kNotLocal, kPending, kUnavailable };
struct Endpoint {
  String origin;
  String hostname;
  uint16_t port = 0;
  uint32_t generation = 0;
  uint32_t expiresAt = 0;
  IPAddress address;
  bool local = false;
};

// Starts a low-priority owner task that continuously polls and reaps mDNS
// queries. These entry points are nonblocking and safe from the UI/OTA callers.
bool start();
void requestResolve(const char* origin);
ResolveStatus resolveOrigin(const char* origin, Endpoint& endpoint);

// Routes HTTPClient's virtual hostname connect through the most recently
// resolved IPv4 address while retaining the DNS hostname for TLS verification.
class SecureClient final : public WiFiClientSecure {
 public:
  using WiFiClientSecure::connect;
  void bindResolved(const Endpoint& endpoint);
  void clearResolved();
  uint8_t connected() override;
  int connect(const char* host, uint16_t port, int32_t timeout) override;
  int connect(const char* host, uint16_t port);
  int connect(const char* host, uint16_t port, const char* ca,
              const char* clientCert, const char* privateKey);
  int connect(const char* host, uint16_t port, const char* pskIdent,
              const char* pskKey);
  int connect(IPAddress ip, uint16_t port, int32_t timeout) override;
  int connect(IPAddress ip, uint16_t port);
  int connect(IPAddress ip, uint16_t port, const char* ca,
              const char* clientCert, const char* privateKey);
  int connect(IPAddress ip, uint16_t port, const char* host, const char* ca,
              const char* clientCert, const char* privateKey);
  int connect(IPAddress ip, uint16_t port, const char* pskIdent,
              const char* pskKey);

 private:
  int connectBound(const char* host, uint16_t port, const char* ca = nullptr,
                   const char* clientCert = nullptr, const char* privateKey = nullptr);
  bool bound_ = false;
  IPAddress address_;
  String hostname_;
  uint16_t port_ = 0;
  uint32_t generation_ = 0;
  uint32_t expiresAt_ = 0;
};

enum class BeginStatus : uint8_t {
  kReady, kResolvePending, kResolveUnavailable, kServiceRefused, kTlsFailure, kBeginFailure
};
BeginStatus beginRequest(HTTPClient& http, SecureClient& client,
                         const char* origin, const String& url, const char* ca,
                         uint32_t timeoutMs);

#ifdef LEOPOLD_LOCAL_ENDPOINT_HOST_TEST
namespace host_test {
void pumpResolverOnce();
bool hasActiveQuery();
bool cacheValid();
void publishResolved(const char* hostname, uint32_t address, uint32_t generation, uint32_t now);
uint32_t generation();
}
#endif

}  // namespace leopold::local_endpoint
