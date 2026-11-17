#include <assert.h>
#include <stdint.h>
#include <string>

#include "local_endpoint_transport.h"
#include "WiFi.h"

uint32_t fake_millis = 1000;
int fake_mdns_created = 0;
int fake_mdns_deleted = 0;
bool fake_mdns_force_finish = false;
uint32_t fake_mdns_answer = 0;
size_t fake_mdns_max_results = 0;
FakeSerial Serial;
FakeWiFi WiFi;
unsigned fake_hostname_resolver_calls = 0;
unsigned fake_ip_tls_calls = 0;
std::string fake_tls_host;
std::string fake_tls_ca;

int main() {
  using namespace leopold::local_endpoint;
  assert(start());
  const uint32_t generation = host_test::generation();
  Endpoint pending;
  const uint32_t beforeRequest = fake_millis;
  assert(resolveOrigin("https://leopold-dev.local:8443", pending) == ResolveStatus::kPending);
  assert(fake_millis == beforeRequest); // request queues resolution; it never waits for DNS.
  assert(fake_hostname_resolver_calls == 0 && fake_ip_tls_calls == 0);
  host_test::publishResolved("leopold-dev.local", 0x0a00002a, generation, fake_millis);

  SecureClient client;
  HTTPClient http;
  assert(beginRequest(http, client, "https://leopold-dev.local:8443",
      "https://leopold-dev.local:8443/v1/device/connect", "retained-test-ca", 5000)
      == BeginStatus::kReady);
  assert(fake_ip_tls_calls == 0); // HTTPClient.begin occurs without preconnect.
  assert(http.GET() == 200);       // HTTPClient's host+port+timeout virtual dispatch.
  assert(fake_ip_tls_calls == 1);
  assert(fake_hostname_resolver_calls == 0);
  assert(fake_tls_host == "leopold-dev.local");
  assert(fake_tls_ca == "retained-test-ca");

  client.stop();
  assert(client.connect("leopold-dev.local", 8443) == 1); // second hostname overload
  assert(fake_ip_tls_calls == 2);
  assert(fake_hostname_resolver_calls == 0);

  client.stop();
  assert(client.connect("leopold-dev.local", 8443, "explicit-test-ca", nullptr, nullptr) == 1);
  assert(fake_ip_tls_calls == 3);
  assert(fake_tls_ca == "explicit-test-ca");
  assert(fake_hostname_resolver_calls == 0);

  client.stop();
  assert(client.connect(IPAddress(0x0a00002a), 8443, 5000) == 1);
  assert(fake_ip_tls_calls == 4);
  assert(fake_hostname_resolver_calls == 0);
  assert(fake_tls_host == "leopold-dev.local");

  client.stop();
  assert(client.connect("leopold-dev.local", 8443, "psk-id", "psk-key") == 0);
  assert(client.connect("other-name.local", 8443) == 0);
  assert(fake_hostname_resolver_calls == 0);

  client.stop();                  // server closed the prior connection; HTTPClient reconnects.
  assert(http.GET() == 200);
  assert(fake_ip_tls_calls == 5);
  assert(fake_hostname_resolver_calls == 0);

  HTTPClient wrongHostHttp;
  SecureClient wrongHostClient;
  assert(beginRequest(wrongHostHttp, wrongHostClient, "https://leopold-dev.local:8443",
      "https://other-name.local:8443/v1/device/connect", "retained-test-ca", 5000)
      == BeginStatus::kReady);
  assert(wrongHostHttp.GET() < 0);
  assert(fake_hostname_resolver_calls == 0);
  assert(fake_ip_tls_calls == 5);

  // Simulate disconnect/rejoin at the same BSSID/IP. The event callback only
  // increments generation; a stale client binding is refused on reconnect.
  WiFi.emit(ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.emit(ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.local_ = IPAddress();
  WiFi.emit(ARDUINO_EVENT_WIFI_STA_LOST_IP);
  Endpoint lostIp;
  assert(resolveOrigin("https://leopold-dev.local:8443", lostIp) == ResolveStatus::kUnavailable);
  assert(http.GET() < 0);
  assert(fake_hostname_resolver_calls == 0);
  assert(fake_ip_tls_calls == 5);
  WiFi.local_ = IPAddress(0x0a000002); // same BSSID/IP is restored
  WiFi.emit(ARDUINO_EVENT_WIFI_STA_GOT_IP);
  Endpoint afterRejoin;
  assert(resolveOrigin("https://leopold-dev.local:8443", afterRejoin) == ResolveStatus::kPending);
  assert(http.GET() < 0);
  assert(fake_hostname_resolver_calls == 0);
  assert(fake_ip_tls_calls == 5);

  // An unrelated non-local origin keeps the pinned client's normal hostname
  // resolver path; only approved .local origin traffic is resolver-bound.
  HTTPClient publicHttp;
  SecureClient publicClient;
  assert(beginRequest(publicHttp, publicClient, "https://api.example.test:8443",
      "https://api.example.test:8443/health", "public-test-ca", 5000)
      == BeginStatus::kReady);
  assert(publicHttp.GET() == 200);
  assert(fake_hostname_resolver_calls == 1);

  fake_millis = 50000;
  host_test::publishResolved("leopold-dev.local", 0x0a00002a, host_test::generation(), fake_millis);
  HTTPClient expiredHttp;
  SecureClient expiredClient;
  assert(beginRequest(expiredHttp, expiredClient, "https://leopold-dev.local:8443",
      "https://leopold-dev.local:8443/health", "retained-test-ca", 5000)
      == BeginStatus::kReady);
  fake_millis = 80000; // endpoint cache expires; no stale IP can be reused
  assert(expiredHttp.GET() < 0);
  assert(fake_hostname_resolver_calls == 1);
  assert(fake_ip_tls_calls == 6);
}
