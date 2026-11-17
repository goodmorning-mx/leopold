#include "touch_input.h"
#include "settings_contact.h"
#include "Wire.h"
#include <cassert>
#include <iostream>
using namespace leopold;
int main(){
 TouchInput touch;touch.begin();TouchGesture gesture;TouchPoint point;
 auto sample=[&](bool down,int x,int y,uint32_t at){Wire.down=down;Wire.x=x;Wire.y=y;return touch.pollGesture(gesture,point,at);};
 auto hold=[&](int duration,TouchGesture expected){
  static uint32_t at=1000;at+=2000;
  assert(sample(true,320,150,at)&&gesture==TouchGesture::kDrag);
  assert(touch.contactDown());auto id=touch.contactId();
  sample(true,320,150,at+duration-1);
  assert(sample(false,0,0,at+duration)&&gesture==expected);
  assert(!touch.contactDown()&&touch.contactId()==id&&point.x==320&&point.y==150);
 };
 hold(650,TouchGesture::kTap);hold(651,TouchGesture::kRelease);
 hold(699,TouchGesture::kRelease);hold(700,TouchGesture::kLongPress);
 hold(4000,TouchGesture::kLongPress); // clock keeps its separate gesture
 // Sub-drag-interval excursions are still visible through lastPoint().
 SettingsContact row;sample(true,320,150,20000);row.begin("wifi");
 sample(true,320,200,20001);assert(touch.lastPoint().y==200);row.move("bluetooth");
 sample(true,320,150,20002);row.move("wifi");
 assert(sample(false,0,0,20800));assert(!row.release("wifi",true));
 sample(true,320,150,20820);assert(sample(false,0,0,20850)&&gesture==TouchGesture::kCancel);
 sample(true,320,250,22000);assert(sample(false,0,0,22100)&&gesture==TouchGesture::kTap);
 sample(true,320,250,23000);sample(true,320,100,23200);
 assert(sample(false,0,0,23300)&&gesture==TouchGesture::kSwipeUp);
 std::cout<<"Production touch path passed\n";
}
