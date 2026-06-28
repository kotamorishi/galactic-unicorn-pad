#include "touch.h"
#include "board_pins.h"
#include <Arduino.h>
#include <Wire.h>
#include <TAMC_GT911.h>

namespace {

// INT/RST are -1 (unused) on this model; GT911 is polled over I2C.
TAMC_GT911 g_tp(PIN_TOUCH_SDA, PIN_TOUCH_SCL, PIN_TOUCH_INT, PIN_TOUCH_RST,
                PANEL_WIDTH, PANEL_HEIGHT);

uint8_t g_addr = TOUCH_GT911_ADDR;  // resolved by the probe in begin()

// Probe the I2C bus for a device that ACKs at `addr`.
bool i2c_present(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void gt_read(uint16_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(g_addr);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  Wire.endTransmission();
  Wire.requestFrom(g_addr, n);
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.available() ? Wire.read() : 0;
}

}  // namespace

namespace touch {

bool begin() {
  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);

  // The GT911 latches its I2C address from the INT pin level at reset. INT is
  // not wired on this board (-1), so the address comes up non-deterministically
  // at 0x5D OR 0x14 across power cycles. Hard-coding one means touch silently
  // dies on every boot where the latch picked the other (i2cRead errors flood).
  // Probe both and use whichever responds.
  g_addr = TOUCH_GT911_ADDR;
  if (i2c_present(0x5D)) g_addr = 0x5D;
  else if (i2c_present(0x14)) g_addr = 0x14;
  else Serial.println("[touch] WARNING: GT911 ACKed at neither 0x5D nor 0x14");
  Serial.printf("[touch] using GT911 address 0x%02X\n", g_addr);

  g_tp.begin(g_addr);
  g_tp.setRotation(ROTATION_NORMAL);

  // Confirm the controller identity (should read "911") even on boots where it
  // later reports no touches — distinguishes a dead bus from a non-scanning chip.
  char pid[5] = {0};
  gt_read(GT911_PRODUCT_ID, (uint8_t*)pid, 4);
  Serial.printf("[touch] product id='%s'\n", pid);

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
