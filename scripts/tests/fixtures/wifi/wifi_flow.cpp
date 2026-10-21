#include "Arduino.h"
#include "WiFi.h"
#include "Preferences.h"
#include "wifi_portal.h"
#include <cassert>
#include <iostream>
using leopold::WifiPortal;
int main(){
  WifiPortal portal; portal.begin("synthetic-id");
  assert(std::string(WiFiManagerParameter::all[0]->id)=="backup_ssid_1");
  assert(std::string(WiFiManagerParameter::all[2]->id)=="backup_ssid_2");
  assert(std::string(WiFiManagerParameter::all[4]->id)=="backup_ssid_3");
  WiFi.ssid="working"; WiFi.password="synthetic-working"; WiFi.state=WL_CONNECTED;
  int before=WiFi.disconnects;portal.prepareForScan();
  assert(WiFi.disconnects==before && WiFi.state==WL_CONNECTED);
  assert(testNvs["wifi-backups:ssid0"]=="working");
  // Disconnected/in-flight station attempts stop; credentials/NVS stay intact.
  WiFi.state=WL_DISCONNECTED;before=WiFi.disconnects;
  auto preserved=testNvs;auto driver=WiFi.persistedSsid;
  portal.prepareForScan();assert(WiFi.disconnects==before+1);
  assert(testNvs==preserved && WiFi.persistedSsid==driver && !WiFi.autoReconnect);
  portal.startConfiguration(500);assert(portal.portalActive());
  portal.prepareForScan();assert(!portal.portalActive());
  portal.loop(100000); assert(!portal.portalActive()); // no recovery while choosing
  portal.connectToNetwork("candidate","synthetic-new",1000);
  assert(!portal.hasSavedNetwork("candidate"));
  assert(WiFi.persistedSsid=="provisioned"); // failed password cannot poison boot credentials
  WiFi.state=WL_CONNECTED; portal.loop(1500);
  assert(portal.hasSavedNetwork("candidate"));
  assert(testNvs["wifi-backups:ssid0"]=="candidate"); // stale portal fields did not overwrite it
  assert(testNvs["wifi-backups:pass0"]=="synthetic-new");
  WifiPortal rebooted; rebooted.begin("synthetic-id");
  assert(WiFi.ssid=="candidate" && WiFi.password=="synthetic-new");
  assert(rebooted.connectSavedNetwork("working",2000));
  assert(WiFi.password=="synthetic-working");
  WiFi.state=WL_CONNECTED;rebooted.loop(2100);
  assert(testNvs["wifi-backups:ssid0"]=="working");
  rebooted.connectToNetwork("working","synthetic-wrong",2500);
  rebooted.loop(2600);
  assert(testNvs["wifi-backups:pass0"]=="synthetic-working");
  rebooted.cancelSelection(2700);
  assert(WiFi.password=="synthetic-working");
  rebooted.connectToNetwork("bad","synthetic-bad",3000);
  rebooted.loop(4000);assert(!rebooted.hasSavedNetwork("bad"));
  rebooted.cancelSelection(4500);
  assert(WiFi.ssid=="working" && WiFi.autoReconnect);
  assert(!rebooted.hasSavedNetwork("bad"));
  WiFi.state=WL_CONNECTED;
  rebooted.connectToNetwork("open","",5000);WiFi.state=WL_CONNECTED;rebooted.loop(5100);
  assert(rebooted.hasSavedNetwork("open"));
  rebooted.connectToNetwork("fourth","synthetic-fourth",6000);WiFi.state=WL_CONNECTED;rebooted.loop(6100);
  assert(testNvs["wifi-backups:ssid0"]=="fourth");
  assert(!rebooted.hasSavedNetwork("candidate")); // oldest success evicted from bounded legacy store
  rebooted.prepareForScan();rebooted.cancelSelection(6200);
  assert(WiFi.autoReconnect);
  assert(Serial.output.find("synthetic-new")==std::string::npos);
  assert(Serial.output.find("synthetic-working")==std::string::npos);
  std::cout<<"Wi-Fi host flow passed: scan preserves connection; selection guard; save-after-success; stale fields; boot; saved reuse; failure/cancel rollback; open; eviction; log privacy\n";
}
