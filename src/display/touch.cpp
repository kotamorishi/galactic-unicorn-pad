#include "touch.h"
#include "board_pins.h"
#include <Arduino.h>
#include <Wire.h>
#include <TAMC_GT911.h>

namespace {

// INT/RST are -1 (unused) on this model; GT911 is polled over I2C.
TAMC_GT911 g_tp(PIN_TOUCH_SDA, PIN_TOUCH_SCL, PIN_TOUCH_INT, PIN_TOUCH_RST,
                PANEL_WIDTH, PANEL_HEIGHT);

}  // namespace

namespace touch {

bool begin() {
  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
  g_tp.begin(TOUCH_GT911_ADDR);   // if dead, try 0x14 (see board_pins.h)
  g_tp.setRotation(ROTATION_NORMAL);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = read_cb;
  lv_indev_drv_register(&indev_drv);

  Serial.println("[touch] GT911 initialised");
  return true;
}

void read_cb(lv_indev_drv_t* /*drv*/, lv_indev_data_t* data) {
  g_tp.read();
  if (g_tp.isTouched && g_tp.touches > 0) {
    data->state = LV_INDEV_STATE_PRESSED;
    // GT911 is mounted 180 degrees relative to the panel on this board, so the
    // raw coordinates are mirrored on both axes. Invert X and Y to match the
    // LVGL display orientation.
    int16_t x = (int16_t)PANEL_WIDTH - 1 - g_tp.points[0].x;
    int16_t y = (int16_t)PANEL_HEIGHT - 1 - g_tp.points[0].y;
    data->point.x = x < 0 ? 0 : (x >= PANEL_WIDTH ? PANEL_WIDTH - 1 : x);
    data->point.y = y < 0 ? 0 : (y >= PANEL_HEIGHT ? PANEL_HEIGHT - 1 : y);
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

}  // namespace touch
