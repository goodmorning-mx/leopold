#include "wifi_scan.h"
#include "settings_contact.h"
#include <cassert>
#include <iostream>
int preparations=0;
void prepareStation(){++preparations;WiFi.scanDelete();}
int main(){
  leopold::WifiScan scan;
  // Reproduce old defect: loop sampled 100, start took 1ms, unsigned age wraps.
  uint32_t oldNow=100, oldStart=101;
  assert(uint32_t(oldNow-oldStart)>=30000);
  testMillis=100; WiFi.startDelay=1;
  scan.begin(); assert(scan.poll()==-1); assert(testMillis==101);
  assert(scan.poll()==-1); // same loop, never spuriously times out
  testMillis=30100; assert(scan.poll()==-1); // 29999ms
  testMillis=30101; assert(scan.poll()==-2); assert(scanStops==1);
  int starts=WiFi.scans;scan.begin();assert(scan.poll()==-1);
  assert(WiFi.scans==starts); // cannot reuse a still-running/stopping scan
  WiFi.scanResult=0;assert(scan.poll()==-3);assert(!scan.busy());
  scan.begin();assert(scan.poll()==-1);WiFi.scanResult=4;
  assert(scan.poll()==4);assert(scan.poll()==-3);WiFi.scanDelete();
  scan.begin();assert(scan.poll()==-1);scan.cancel();scan.cancel();
  assert(scanStops==2);assert(scan.poll()==-1);
  WiFi.scanResult=0;assert(scan.poll()==-3); // cancelled zero never rendered as new scan
  scan.begin();assert(scan.poll()==-1);WiFi.scanResult=0;assert(scan.poll()==0); // genuine empty
  WiFi.scanDelete();WiFi.startResult=-2;scan.begin();assert(scan.poll()==-2);
  WiFi.startResult=2;scan.begin();assert(scan.poll()==2); // synchronous completion supported
  WiFi.scanDelete();WiFi.startResult=-1;WiFi.startDelay=0;
  testMillis=0xfffffff0;scan.begin();assert(scan.poll()==-1);
  testMillis=0x10;assert(scan.poll()==-1); // rollover, 32ms
  testMillis=uint32_t(0xfffffff0u+30000u);assert(scan.poll()==-2);
  WiFi.scanResult=0;scan.poll();
  // Portal-owned scan must finish stopping before the requested scan starts.
  WiFi.scanResult=-1;starts=WiFi.scans;scan.begin();assert(scan.poll(prepareStation)==-1);
  assert(preparations==0);
  assert(WiFi.scans==starts);WiFi.scanResult=0;assert(scan.poll(prepareStation)==-1);
  assert(preparations==1);
  assert(WiFi.scans==starts+1);WiFi.scanResult=3;assert(scan.poll()==3);
  leopold::SettingsContact row;
  for(const char* id:{"settings-wifi","settings-bluetooth","settings-display","settings-sound"}){
    row.begin(id);row.move(id);assert(row.selected()==id);assert(row.release(id,true)==id);assert(!row.selected());
    row.begin(id);assert(!row.release(nullptr,true));
    row.begin(id);row.move(nullptr);row.move(id);assert(!row.release(id,true));
    row.begin(id);row.move("different-row");assert(!row.release("different-row",true));
    row.begin(id);assert(!row.release(id,false)); // swipe/debounced/cancel
  }
  row.begin(nullptr);row.move("settings-wifi");assert(!row.release("settings-wifi",true));
  std::cout<<"Scan lifecycle and original-row release passed\n";
}
