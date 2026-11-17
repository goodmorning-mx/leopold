#pragma once
#include <stdint.h>
#include <string>
#include "IPAddress.h"

extern unsigned fake_hostname_resolver_calls;
extern unsigned fake_ip_tls_calls;
extern std::string fake_tls_host;
extern std::string fake_tls_ca;

class NetworkClient {
 public:
  virtual ~NetworkClient() = default;
  virtual int connect(const char* host, uint16_t port, int32_t timeout) = 0;
  virtual int connect(IPAddress ip, uint16_t port, int32_t timeout) = 0;
  virtual uint8_t connected() = 0;
  virtual void setConnectionTimeout(uint32_t timeout) { timeout_ = timeout; }
  virtual void setTimeout(uint16_t) {}
 protected:
  int32_t timeout_ = 3000;
};

class NetworkClientSecure : public NetworkClient {
 public:
  int connect(IPAddress ip, uint16_t port, int32_t timeout) override {
    timeout_ = timeout;
    return connect(ip, port);
  }
  int connect(IPAddress ip, uint16_t port) {
    return connect(ip, port, nullptr, ca_.c_str(), nullptr, nullptr);
  }
  int connect(IPAddress ip, uint16_t port, const char* ca,
              const char*, const char*) {
    return connect(ip, port, nullptr, ca, nullptr, nullptr);
  }
  int connect(IPAddress, uint16_t port, const char* host, const char* ca,
              const char*, const char*) {
    ++fake_ip_tls_calls;
    fake_tls_host = host ? host : "";
    fake_tls_ca = ca ? ca : "";
    connected_ = true;
    return port == 8443 ? 1 : 0;
  }
  int connect(IPAddress, uint16_t port, const char*, const char*) {
    ++fake_ip_tls_calls;
    fake_tls_host = "IP-PSK";
    connected_ = true;
    return port == 8443 ? 1 : 0;
  }
  int connect(const char* host, uint16_t port, int32_t timeout) override {
    timeout_ = timeout;
    ++fake_hostname_resolver_calls; // models pinned NetworkClientSecure host fallback
    return connect(IPAddress(0x7f000001), port, host, ca_.c_str(), nullptr, nullptr);
  }
  int connect(const char* host, uint16_t port) {
    return connect(host, port, timeout_);
  }
  int connect(const char* host, uint16_t port, const char* ca,
              const char*, const char*) {
    ++fake_hostname_resolver_calls;
    return connect(IPAddress(0x7f000001), port, host, ca, nullptr, nullptr);
  }
  int connect(const char* host, uint16_t port, const char*, const char*) {
    ++fake_hostname_resolver_calls;
    return connect(IPAddress(0x7f000001), port, host, ca_.c_str(), nullptr, nullptr);
  }
  void setCACert(const char* ca) { ca_ = ca ? ca : ""; _CA_cert = ca_.c_str(); }
  void setHandshakeTimeout(unsigned long) {}
  void stop() { connected_ = false; }
  uint8_t connected() override { return connected_; }
  int lastError(char* message, size_t size) {
    if (size) message[0] = '\0';
    return 0;
  }
 private:
  std::string ca_;
  bool connected_ = false;
 protected:
  const char* _CA_cert = nullptr;
  const char* _cert = nullptr;
  const char* _private_key = nullptr;
};

using WiFiClientSecure = NetworkClientSecure;
