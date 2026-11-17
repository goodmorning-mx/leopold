#pragma once
#include <stdint.h>
#include <string>
#include "NetworkClientSecure.h"

class HTTPClient {
 public:
  void setConnectTimeout(int32_t timeout) { connectTimeout_ = timeout; }
  void setTimeout(uint16_t) {}
  bool begin(NetworkClient& client, String url) {
    client_ = &client;
    std::string value(url.c_str());
    const size_t scheme = value.find("://");
    if (scheme == std::string::npos) return false;
    value.erase(0, scheme + 3);
    const size_t slash = value.find('/');
    host_ = value.substr(0, slash);
    const size_t colon = host_.find(':');
    if (colon != std::string::npos) {
      port_ = static_cast<uint16_t>(std::stoi(host_.substr(colon + 1)));
      host_.resize(colon);
    } else {
      port_ = scheme == 5 ? 443 : 80;
    }
    return true;
  }
  int GET() {
    if (!client_) return -1;
    if (client_->connected()) return 200;
    return client_->connect(host_.c_str(), port_, connectTimeout_) ? 200 : -1;
  }
 private:
  NetworkClient* client_ = nullptr;
  std::string host_;
  uint16_t port_ = 0;
  int32_t connectTimeout_ = 5000;
};
