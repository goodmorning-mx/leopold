#pragma once
#include "Arduino.h"
constexpr int WL_CONNECTED=3, WL_DISCONNECTED=6, WIFI_STA=1;
struct WiFiMock {
  int state=WL_DISCONNECTED, disconnects=0;
  bool persistentFlag=true, autoReconnect=true;
  String ssid, password, persistedSsid="provisioned", persistedPassword="synthetic-original";
  void persistent(bool v){persistentFlag=v;}
  void setAutoReconnect(bool v){autoReconnect=v;}
  void mode(int){}
  void begin(const char* s=nullptr,const char* p=nullptr){
    ssid=s?s:persistedSsid;password=p?p:persistedPassword;state=WL_DISCONNECTED;
    if(s && persistentFlag){persistedSsid=ssid;persistedPassword=password;}
  }
  void disconnect(bool,bool){++disconnects;state=WL_DISCONNECTED;}
  void reconnect(){state=WL_DISCONNECTED;}
  void softAPdisconnect(bool){}
  int status()const{return state;}
  String SSID()const{return ssid;}
  String psk()const{return password;}
};
inline WiFiMock WiFi;
