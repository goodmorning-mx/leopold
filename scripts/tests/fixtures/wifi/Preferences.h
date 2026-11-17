#pragma once
#include "Arduino.h"
#include <map>
inline std::map<std::string,std::string> testNvs;
struct Preferences {
  std::string prefix;
  bool begin(const char* p,bool){prefix=std::string(p)+":";return true;}
  void end(){}
  String getString(const char* k,const char* fallback){auto i=testNvs.find(prefix+k);return i==testNvs.end()?fallback:i->second;}
  size_t getString(const char* k,char* out,size_t n){String s=getString(k,"");return strlcpy(out,s.c_str(),n);}
  void putString(const char* k,const char* s){testNvs[prefix+k]=s;}
  void remove(const char* k){testNvs.erase(prefix+k);}
  void clear(){for(auto i=testNvs.begin();i!=testNvs.end();)if(i->first.rfind(prefix,0)==0)i=testNvs.erase(i);else ++i;}
};
