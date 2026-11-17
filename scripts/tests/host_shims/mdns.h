#pragma once
#include <stddef.h>
#include <stdint.h>
constexpr int ESP_OK = 0;
constexpr uint16_t MDNS_TYPE_A = 1;
constexpr int ESP_IPADDR_TYPE_V4 = 0;
struct FakeMdnsIp4 { uint32_t addr; };
struct FakeMdnsAddress { int type; union { FakeMdnsIp4 ip4; } u_addr; };
struct mdns_ip_addr_t { FakeMdnsAddress addr; mdns_ip_addr_t* next; };
struct mdns_result_t { mdns_ip_addr_t* addr; mdns_result_t* next; };
extern uint32_t fake_millis;
extern int fake_mdns_created;
extern int fake_mdns_deleted;
extern bool fake_mdns_force_finish;
extern uint32_t fake_mdns_answer;
extern size_t fake_mdns_max_results;
struct mdns_search_once_s { uint32_t started; uint32_t timeout; size_t max_results; };
using mdns_search_once_t = mdns_search_once_s;
using mdns_query_notify_t = void (*)(mdns_search_once_t*);
inline int mdns_init() { return ESP_OK; }
inline mdns_search_once_t* mdns_query_async_new(const char*, const char*, const char*, uint16_t,
                                                uint32_t timeout, size_t max_results, mdns_query_notify_t) {
  ++fake_mdns_created;
  fake_mdns_max_results = max_results;
  return new mdns_search_once_t{fake_millis, timeout, max_results};
}
inline bool mdns_query_async_get_results(mdns_search_once_t* query, uint32_t, mdns_result_t** results,
                                         uint8_t*) {
  if (results) *results = nullptr;
  const bool gotEnoughAnswers = fake_mdns_answer && query->max_results <= 1;
  const bool finished = fake_mdns_force_finish || gotEnoughAnswers ||
      fake_millis - query->started >= query->timeout;
  if (finished && results && fake_mdns_answer) {
    auto* address = new mdns_ip_addr_t{};
    address->addr.type = ESP_IPADDR_TYPE_V4;
    address->addr.u_addr.ip4.addr = fake_mdns_answer;
    *results = new mdns_result_t{address, nullptr};
  }
  return finished;
}
inline void mdns_query_results_free(mdns_result_t* result) {
  if (result) { delete result->addr; delete result; }
}
inline int mdns_query_async_delete(mdns_search_once_t* search) { ++fake_mdns_deleted; delete search; return 0; }
