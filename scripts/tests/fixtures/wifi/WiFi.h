#pragma once
#include "Arduino.h"
constexpr int WL_CONNECTED=3, WL_DISCONNECTED=6, WIFI_STA=1;
constexpr int WIFI_SCAN_RUNNING=-1, WIFI_SCAN_FAILED=-2;
struct WiFiMock {
  int scanResult=-2, startResult=-1, scans=0, clears=0;
  uint32_t startDelay=0;
  int scanComplete(){return scanResult;}
  int scanNetworks(bool,bool){++scans;testMillis+=startDelay;return scanResult=startResult;}
  void scanDelete(){++clears;scanResult=-2;}
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
