#pragma once
#include "Arduino.h"
struct TwoWire {
 bool down=false; uint16_t x=0,y=0; int index=0;
 bool begin(int,int,int){return true;}
 void beginTransmission(uint8_t){} void write(uint8_t){}
 uint8_t endTransmission(bool){return 0;}
 uint8_t requestFrom(uint8_t,uint8_t,bool){index=0;return 5;}
 uint8_t read(){uint8_t data[]={uint8_t(down?1:0),uint8_t(x>>8),uint8_t(x),uint8_t(y>>8),uint8_t(y)};return data[index++];}
};
inline TwoWire Wire;
