#include "display.h"
#include "board_pins.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <esp_heap_caps.h>

namespace {

Arduino_ESP32RGBPanel* g_bus = nullptr;
Arduino_RGB_Display* g_gfx = nullptr;

lv_disp_draw_buf_t g_draw_buf;
lv_color_t* g_buf1 = nullptr;
lv_color_t* g_buf2 = nullptr;

// Full-screen double buffers in PSRAM + LVGL full_refresh: LVGL renders the
// whole frame off-screen, then we copy it to the panel in one shot. This kills
// the per-strip flicker that small partial buffers produced. Kept off-screen
// (not direct-mode) so the RGB DMA never scans a half-drawn buffer — direct-mode
// on the single live framebuffer was tried and corrupts the image.
// (True tear-free rendering needs a hardware second framebuffer via esp_lcd
// num_fbs=2, which neither Arduino_GFX 1.3.x nor LovyanGFX v1 expose.)
constexpr uint32_t kBufLines = PANEL_HEIGHT;  // full screen

void flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  g_gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)color_p, w, h);
  lv_disp_flush_ready(drv);
}

}  // namespace

namespace display {

bool begin() {
#if BOARD_USE_CH422G
  // TODO: initialise the CH422G IO expander here (before backlight/touch) and
  // drive backlight-enable / panel-reset through it. Not used on this model.
#endif

  g_bus = new Arduino_ESP32RGBPanel(
      PIN_DE, PIN_VSYNC, PIN_HSYNC, PIN_PCLK,
      PIN_R0, PIN_R1, PIN_R2, PIN_R3, PIN_R4,
      PIN_G0, PIN_G1, PIN_G2, PIN_G3, PIN_G4, PIN_G5,
      PIN_B0, PIN_B1, PIN_B2, PIN_B3, PIN_B4,
      RGB_HSYNC_POLARITY, RGB_HSYNC_FRONT_PORCH, RGB_HSYNC_PULSE_WIDTH, RGB_HSYNC_BACK_PORCH,
      RGB_VSYNC_POLARITY, RGB_VSYNC_FRONT_PORCH, RGB_VSYNC_PULSE_WIDTH, RGB_VSYNC_BACK_PORCH,
      RGB_PCLK_ACTIVE_NEG, RGB_PREFER_SPEED);

  g_gfx = new Arduino_RGB_Display(PANEL_WIDTH, PANEL_HEIGHT, g_bus);
  if (!g_gfx->begin()) {
    Serial.println("[display] gfx->begin() failed");
    return false;
  }
  g_gfx->fillScreen(0x0000);
  setBacklight(100);

  lv_init();

  uint32_t buf_px = PANEL_WIDTH * kBufLines;
  g_buf1 = (lv_color_t*)heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
  g_buf2 = (lv_color_t*)heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
  if (!g_buf1 || !g_buf2) {
    Serial.println("[display] LVGL draw buffer alloc failed (PSRAM)");
    return false;
  }
  lv_disp_draw_buf_init(&g_draw_buf, g_buf1, g_buf2, buf_px);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = PANEL_WIDTH;
  disp_drv.ver_res = PANEL_HEIGHT;
  disp_drv.flush_cb = flush_cb;
  disp_drv.draw_buf = &g_draw_buf;
  // Render the entire frame each pass and push it in one transfer (no partial
  // strips), which removes the strip-flicker artefacts.
  disp_drv.full_refresh = 1;
  lv_disp_drv_register(&disp_drv);

  Serial.println("[display] initialised (full-refresh, full-screen buffers)");
  return true;
}

void setBacklight(uint8_t pct) {
  pinMode(PIN_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_BACKLIGHT, pct > 0 ? HIGH : LOW);
}

}  // namespace display
