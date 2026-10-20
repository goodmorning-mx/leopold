#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <functional>
using String = std::string;
inline size_t strlcpy(char* out, const char* in, size_t n) { size_t len=strlen(in); if(n){std::memcpy(out,in,len<n?len:n-1);out[len<n?len:n-1]=0;}return len; }
inline uint32_t testMillis = 100;
inline uint32_t millis(){return testMillis;}
inline void delay(int){}
struct SerialMock {
  std::string output;
  void println(const char* v){output+=v;}
  template<typename... Args> void printf(const char* f, Args... args){char b[512];snprintf(b,sizeof b,f,args...);output+=b;}
};
inline SerialMock Serial;
struct EspMock { void restart(){} };
inline EspMock ESP;
