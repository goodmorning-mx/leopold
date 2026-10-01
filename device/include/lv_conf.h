#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_DEF_REFR_PERIOD 16
#define LV_DPI_DEF 326
#define LV_USE_LOG 0

#define LV_MEM_SIZE (128 * 1024U)
#ifndef __ASSEMBLER__
  #include <esp_heap_caps.h>
  static inline void* leopold_lvgl_alloc(size_t size) {
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  #define LV_MEM_POOL_ALLOC leopold_lvgl_alloc
#endif

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

#endif
