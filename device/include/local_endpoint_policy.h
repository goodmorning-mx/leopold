#pragma once

#include <stdint.h>
#include <string.h>

namespace leopold::local_endpoint {

constexpr uint32_t kResolveDeadlineMs = 1000;
constexpr uint32_t kCacheLifetimeMs = 30000;
constexpr uint32_t kResolveRetryMs = 4000;

inline bool deadlineReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

inline uint32_t retryBaseMs(uint8_t failures) {
  const uint8_t exponent = failures > 5 ? 5 : failures;
  const uint32_t base = 2000UL << exponent;
  return base > 60000UL ? 60000UL : base;
}

struct IpAddress {
  uint32_t value = 0;
  bool valid() const { return value != 0; }
};

struct EndpointCache {
  char hostname[254] = {};
  IpAddress address;
  uint32_t expiresAt = 0;
  uint32_t generation = 0;
  bool valid = false;
};

class ResolverLifecycle {
 public:
  enum class Decision : uint8_t { kStart, kPending, kReady, kBackoff, kTimedOut };

  Decision request(const char* hostname, uint32_t now, uint32_t generation) {
    observeGeneration(generation);
    if (cache_.valid && cache_.generation == generation &&
        !deadlineReached(now, cache_.expiresAt) && same(cache_.hostname, hostname)) {
      return Decision::kReady;
    }
    if (cache_.valid && deadlineReached(now, cache_.expiresAt)) cache_.valid = false;
    if (active_) {
      if (generation != activeGeneration_ || !same(activeHostname_, hostname)) stale_ = true;
      if (!timedOut_ && !stale_ && deadlineReached(now, deadline_)) {
        timedOut_ = true;
        retryAt_ = now + kResolveRetryMs;
        retryScheduled_ = true;
        timeoutPending_ = true;
      }
      return timedOut_ ? Decision::kTimedOut : Decision::kPending;
    }
    if (retryScheduled_ && !deadlineReached(now, retryAt_)) return Decision::kBackoff;
    if (hostname == nullptr || hostname[0] == '\0' || strlen(hostname) >= sizeof(activeHostname_)) {
      return Decision::kBackoff;
    }
    memcpy(activeHostname_, hostname, strlen(hostname) + 1);
    activeGeneration_ = generation;
    deadline_ = now + kResolveDeadlineMs;
    active_ = true;
    stale_ = false;
    timedOut_ = false;
    return Decision::kStart;
  }

  // Called by the query owner after mdns_query_async_new. A failed start has
  // no asynchronous object to reap, so it is closed here and cooled down.
  void queryStartFailed(uint32_t now) {
    active_ = false;
    stale_ = false;
    timedOut_ = false;
    retryAt_ = now + 1000;
    retryScheduled_ = true;
  }

  // Call for every poll, including after timeout or a Wi-Fi generation change.
  // The caller must free returned mDNS records and delete the query whenever
  // this returns true. Late/cancelled results are always discarded.
  bool poll(bool finished, IpAddress result, uint32_t now, uint32_t generation) {
    observeGeneration(generation);
    if (!active_) return false;
    if (generation != activeGeneration_) stale_ = true;
    if (!timedOut_ && !stale_ && deadlineReached(now, deadline_)) {
      timedOut_ = true;
      retryAt_ = now + kResolveRetryMs;
      retryScheduled_ = true;
      timeoutPending_ = true;
    }
    if (!finished) return false;
    if (!stale_ && !timedOut_ && generation == activeGeneration_ && result.valid()) {
      memcpy(cache_.hostname, activeHostname_, sizeof(cache_.hostname));
      cache_.address = result;
      cache_.expiresAt = now + kCacheLifetimeMs;
      cache_.generation = generation;
      cache_.valid = true;
      retryScheduled_ = false;
    } else if (!timedOut_ && !stale_) {
      retryAt_ = now + kResolveRetryMs;
      retryScheduled_ = true;
    }
    active_ = false;
    activeHostname_[0] = '\0';
    stale_ = false;
    timedOut_ = false;
    return true;
  }

  bool cached(const char* hostname, uint32_t now, uint32_t generation,
              IpAddress& result, uint32_t* expiresAt = nullptr) {
    observeGeneration(generation);
    if (!cache_.valid || cache_.generation != generation ||
        deadlineReached(now, cache_.expiresAt) || !same(cache_.hostname, hostname)) {
      if (cache_.valid && deadlineReached(now, cache_.expiresAt)) cache_.valid = false;
      return false;
    }
    result = cache_.address;
    if (expiresAt) *expiresAt = cache_.expiresAt;
    return true;
  }

  bool takeTimeout() {
    const bool value = timeoutPending_;
    timeoutPending_ = false;
    return value;
  }

  void invalidate(uint32_t generation) { observeGeneration(generation); }
  bool queryActive() const { return active_; }
  bool cacheValid() const { return cache_.valid; }
  uint32_t activeDeadline() const { return deadline_; }

 private:
  static bool same(const char* left, const char* right) {
    return left != nullptr && right != nullptr && strcmp(left, right) == 0;
  }
  void observeGeneration(uint32_t generation) {
    if (generation == observedGeneration_) return;
    observedGeneration_ = generation;
    cache_.valid = false;
    if (active_) stale_ = true;
  }

  EndpointCache cache_;
  char activeHostname_[254] = {};
  uint32_t observedGeneration_ = 0;
  uint32_t activeGeneration_ = 0;
  uint32_t deadline_ = 0;
  uint32_t retryAt_ = 0;
  bool active_ = false;
  bool stale_ = false;
  bool timedOut_ = false;
  bool timeoutPending_ = false;
  bool retryScheduled_ = false;
};

class TimeoutLogLimiter {
 public:
  bool allowTimeout(bool timeout, uint32_t now) {
    return timeout && allow(now);
  }
  bool allow(uint32_t now) {
    if (hasLogged_ && !deadlineReached(now, nextAllowed_)) return false;
    hasLogged_ = true;
    nextAllowed_ = now + 60000;
    return true;
  }
 private:
  uint32_t nextAllowed_ = 0;
  bool hasLogged_ = false;
};

template <typename ConnectResolved>
int connectResolvedHost(const char* requestedHost, const char* expectedHost,
                       IpAddress address, uint16_t expectedPort,
                       uint16_t requestedPort, uint32_t endpointGeneration,
                       uint32_t currentGeneration, uint32_t expiresAt,
                       uint32_t now, ConnectResolved&& connect) {
  if (!address.valid() || expectedPort != requestedPort ||
      endpointGeneration != currentGeneration ||
      deadlineReached(now, expiresAt) ||
      !requestedHost || !expectedHost || strcmp(requestedHost, expectedHost) != 0) {
    return 0;
  }
  return connect(address, expectedPort, expectedHost);
}

}  // namespace leopold::local_endpoint
