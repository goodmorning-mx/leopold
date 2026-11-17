#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "local_endpoint_policy.h"

using leopold::local_endpoint::IpAddress;
using leopold::local_endpoint::ResolverLifecycle;

static void deadline_and_independent_reaping() {
  ResolverLifecycle resolver;
  assert(resolver.request("leopold-dev.local", 100, 4) == ResolverLifecycle::Decision::kStart);
  assert(resolver.poll(false, {}, 1099, 4) == false);
  assert(resolver.request("leopold-dev.local", 1100, 4) == ResolverLifecycle::Decision::kTimedOut);
  assert(resolver.takeTimeout());
  assert(resolver.queryActive());
  // No new request is made. The lifecycle owner still polls and reaps late work.
  assert(resolver.poll(true, IpAddress{0x01020304}, 1110, 4));
  assert(!resolver.queryActive());
  assert(!resolver.cacheValid());
  assert(resolver.request("leopold-dev.local", 1111, 4) == ResolverLifecycle::Decision::kBackoff);
  assert(resolver.request("leopold-dev.local", 5100, 4) == ResolverLifecycle::Decision::kStart);
}

static void cancellation_and_same_network_rejoin() {
  ResolverLifecycle resolver;
  assert(resolver.request("leopold-dev.local", 10, 77) == ResolverLifecycle::Decision::kStart);
  assert(!resolver.poll(false, {}, 20, 78));  // disconnect/rejoin, same BSSID and IP
  assert(!resolver.cacheValid());
  assert(resolver.queryActive());             // cannot delete before mDNS finishes
  assert(resolver.poll(true, IpAddress{0x01020304}, 40, 78));
  assert(!resolver.cacheValid());             // late old-generation answer discarded
  assert(!resolver.queryActive());
  assert(resolver.request("leopold-dev.local", 41, 78) == ResolverLifecycle::Decision::kStart);
}

static void cache_expiry_is_bounded() {
  ResolverLifecycle resolver;
  assert(resolver.request("leopold-dev.local", 1000, 3) == ResolverLifecycle::Decision::kStart);
  assert(resolver.poll(true, IpAddress{0x01020304}, 1010, 3));
  IpAddress cached;
  assert(resolver.cached("leopold-dev.local", 31009, 3, cached));
  assert(cached.value == 0x01020304);
  assert(!resolver.cached("leopold-dev.local", 31010, 3, cached));
  assert(resolver.request("leopold-dev.local", 31010, 3) == ResolverLifecycle::Decision::kStart);
}

static void hostname_dispatch_uses_only_resolved_ip() {
  IpAddress configured{0x0a00002a};
  unsigned dispatches = 0;
  const int result = leopold::local_endpoint::connectResolvedHost(
      "leopold-dev.local", "leopold-dev.local", configured, 8443, 8443,
      9, 9, 10000, 9999,
      [&](IpAddress ip, uint16_t port, const char* tlsName) {
        ++dispatches;
        assert(ip.value == configured.value);
        assert(port == 8443);
        assert(strcmp(tlsName, "leopold-dev.local") == 0);
        return 1; // the firmware adapter calls NetworkClientSecure::connect(IP, ..., host,...)
      });
  assert(result == 1 && dispatches == 1);
  assert(leopold::local_endpoint::connectResolvedHost(
      "redirect.example", "leopold-dev.local", configured, 8443, 8443,
      9, 9, 10000, 9999,
      [&](IpAddress, uint16_t, const char*) { ++dispatches; return 1; }) == 0);
  assert(leopold::local_endpoint::connectResolvedHost(
      "leopold-dev.local", "leopold-dev.local", configured, 8443, 443,
      9, 9, 10000, 9999,
      [&](IpAddress, uint16_t, const char*) { ++dispatches; return 1; }) == 0);
  assert(leopold::local_endpoint::connectResolvedHost(
      "leopold-dev.local", "leopold-dev.local", configured, 8443, 8443,
      9, 10, 10000, 9999,
      [&](IpAddress, uint16_t, const char*) { ++dispatches; return 1; }) == 0);
  assert(leopold::local_endpoint::connectResolvedHost(
      "leopold-dev.local", "leopold-dev.local", configured, 8443, 8443,
      9, 9, 10000, 10000,
      [&](IpAddress, uint16_t, const char*) { ++dispatches; return 1; }) == 0);
  assert(dispatches == 1);
}

static void diagnostics_and_backoff_are_capped() {
  leopold::local_endpoint::TimeoutLogLimiter limiter;
  assert(!limiter.allowTimeout(false, 100));
  assert(limiter.allowTimeout(true, 100));
  assert(!limiter.allowTimeout(false, 60099));
  assert(!limiter.allowTimeout(true, 60099));
  assert(limiter.allowTimeout(true, 60100));
  assert(leopold::local_endpoint::retryBaseMs(1) == 4000);
  assert(leopold::local_endpoint::retryBaseMs(2) == 8000);
  assert(leopold::local_endpoint::retryBaseMs(3) == 16000);
  assert(leopold::local_endpoint::retryBaseMs(4) == 32000);
  assert(leopold::local_endpoint::retryBaseMs(5) == 60000);
  assert(leopold::local_endpoint::retryBaseMs(255) == 60000);
}

int main() {
  deadline_and_independent_reaping();
  cancellation_and_same_network_rejoin();
  cache_expiry_is_bounded();
  hostname_dispatch_uses_only_resolved_ip();
  diagnostics_and_backoff_are_capped();
}
