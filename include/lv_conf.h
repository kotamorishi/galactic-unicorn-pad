// lv_conf.h — minimal LVGL 8.3 config for CrowPanel 7" (ESP32-S3 + PSRAM).
//
// Only the settings we care about are set explicitly; everything else falls
// back to LVGL's internal defaults (lv_conf_internal.h). Enabled via the
// -DLV_CONF_INCLUDE_SIMPLE build flag in platformio.ini.
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

// ---- Color ----
#define LV_COLOR_DEPTH 16
// RGB565 byte order on this panel: start with 0. If red/blue look swapped on
// the real device, flip this to 1.
#define LV_COLOR_16_SWAP 0

// ---- Memory: route LVGL's allocator to PSRAM so widget trees + JSON-derived
// strings don't pressure the 512KB internal SRAM. ----
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE "esp_heap_caps.h"
#define LV_MEM_CUSTOM_ALLOC(size)        heap_caps_malloc(size, MALLOC_CAP_SPIRAM)
#define LV_MEM_CUSTOM_FREE(ptr)          heap_caps_free(ptr)
#define LV_MEM_CUSTOM_REALLOC(p, new_s)  heap_caps_realloc(p, new_s, MALLOC_CAP_SPIRAM)

// ---- Tick: use Arduino millis() ----
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

// ---- HAL settings ----
#define LV_DPI_DEF 130

// ---- Fonts (big touch targets) ----
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_48 1   // big date on the clock screen
#define LV_FONT_DEFAULT &lv_font_montserrat_20

// ---- Widgets actually used ----
#define LV_USE_BTNMATRIX 1
#define LV_USE_LABEL 1
#define LV_USE_BTN 1
#define LV_USE_SLIDER 1
#define LV_USE_LIST 1
#define LV_USE_COLORWHEEL 1
#define LV_USE_KEYBOARD 1
#define LV_USE_TEXTAREA 1
#define LV_USE_CANVAS 1     // analog clock face, hand-drawn (screen_clock)
#define LV_LABEL_LONG_TXT_HINT 1

// ---- Logging (off in normal builds) ----
#define LV_USE_LOG 0

#endif // LV_CONF_H
