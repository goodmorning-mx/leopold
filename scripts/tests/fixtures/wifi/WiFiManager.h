#pragma once
#include "Arduino.h"
#include "WiFi.h"
struct WiFiManagerParameter {
  std::string value;
  const char* id;
  inline static std::vector<WiFiManagerParameter*> all;
  WiFiManagerParameter(const char* key,const char*,const char* v,int,const char* = nullptr):value(v),id(key){all.push_back(this);}
  const char* getValue()const{return value.c_str();}
  void setValue(const char* v,int){value=v;}
};
struct WiFiManager {
  std::vector<WiFiManagerParameter*> parameters;
  std::function<void()> saved;
  int portalStarts=0;
  void setConfigPortalBlocking(bool){}
  void setDebugOutput(bool){}
  void setConnectTimeout(int){}
  void setShowPassword(bool){}
  void setTitle(const char*){}
  void setCustomHeadElement(const char*){}
  void setDarkMode(bool){}
  void addParameter(WiFiManagerParameter* p){parameters.push_back(p);}
  void setSaveParamsCallback(std::function<void()> f){saved=f;}
  void setMenu(std::vector<const char*> const&){}
  bool getWiFiIsSaved(){return !WiFi.persistedSsid.empty();}
  void stopConfigPortal(){}
  void process(){}
  void startConfigPortal(const char*){++portalStarts;}
  void resetSettings(){}
};
