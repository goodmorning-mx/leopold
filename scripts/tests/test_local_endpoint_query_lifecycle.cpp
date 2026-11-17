#include <assert.h>
#include <stdint.h>

#include "local_endpoint_transport.h"
#include "WiFi.h"
#include "mdns.h"

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
  Endpoint endpoint;
  assert(resolveOrigin("https://leopold-dev.local:8443", endpoint) == ResolveStatus::kPending);
  host_test::pumpResolverOnce(); // owner starts the actual adapter query
  assert(host_test::hasActiveQuery());
  assert(fake_mdns_created == 1 && fake_mdns_deleted == 0);
  assert(fake_mdns_max_results == LEOPOLD_EXPECTED_MDNS_MAX_RESULTS);

  fake_mdns_answer = 0x0a00002a; // one IPv4 result arrives after query creation
  fake_millis += 100;
  host_test::pumpResolverOnce();

#if LEOPOLD_EXPECTED_MDNS_MAX_RESULTS == 1
  assert(fake_mdns_deleted == 1);
  assert(!host_test::hasActiveQuery());
  assert(host_test::cacheValid());
  assert(resolveOrigin("https://leopold-dev.local:8443", endpoint) == ResolveStatus::kResolved);
  assert(static_cast<uint32_t>(endpoint.address) == fake_mdns_answer);
#else
  // The historical four-result request cannot complete on this single answer.
  // It returns only at the 1000ms deadline, where the real lifecycle rejects it.
  assert(fake_mdns_deleted == 0);
  assert(host_test::hasActiveQuery());
  assert(!host_test::cacheValid());
  assert(host_test::hasActiveQuery());
  fake_millis = 2000;
  host_test::pumpResolverOnce();
  assert(fake_mdns_deleted == 1);
  assert(!host_test::hasActiveQuery());
  assert(!host_test::cacheValid());
#endif
}
