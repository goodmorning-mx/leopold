#pragma once
#include <stddef.h>
#include <stdint.h>
namespace leopold_settings_icons {
struct Material { const char* id; uint16_t width, height; const uint8_t* bha; };
// Accent and output are sRGB-encoded bytes. Destination is straight RGBA8888.
// Caller owns output, which must have width*height*4 bytes. No allocation.
inline uint8_t channel(uint8_t body, uint8_t highlight, uint8_t accent) {
  const unsigned value = (unsigned(body)*accent + 127u)/255u + highlight;
  return uint8_t(value > 255u ? 255u : value);
}
inline void renderRgba(const Material& material, uint8_t r, uint8_t g, uint8_t b,
                       uint8_t* output, uint8_t opacity=255) {
  const size_t pixels=size_t(material.width)*material.height;
  for(size_t i=0;i<pixels;++i) {
    const uint8_t* p=material.bha+i*3;
    output[i*4]=channel(p[0],p[1],r);
    output[i*4+1]=channel(p[0],p[1],g);
    output[i*4+2]=channel(p[0],p[1],b);
    output[i*4+3]=uint8_t((unsigned(p[2])*opacity+127u)/255u);
  }
}
// LVGL RGB565A8 layout: little-endian packed RGB565 plane, then A8 plane.
// Caller owns width*height*3 bytes. Verify target byte order/config before use.
inline void renderRgb565A8(const Material& m,uint8_t r,uint8_t g,uint8_t b,
                          uint8_t* output,uint8_t opacity=255) {
 const size_t pixels=size_t(m.width)*m.height;
 for(size_t i=0;i<pixels;++i){
  const uint8_t* p=m.bha+i*3;
  const uint8_t rr=channel(p[0],p[1],r),gg=channel(p[0],p[1],g),bb=channel(p[0],p[1],b);
  const uint16_t packed=uint16_t(((rr&248u)<<8)|((gg&252u)<<3)|(bb>>3));
  output[i*2]=uint8_t(packed);output[i*2+1]=uint8_t(packed>>8);
  output[pixels*2+i]=uint8_t((unsigned(p[2])*opacity+127u)/255u);
 }
}
}
