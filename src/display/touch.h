// touch.h — GT911 capacitive touch -> LVGL pointer input device.
#pragma once
#include <lvgl.h>

namespace touch {
// Init I2C + GT911 and register an LVGL pointer indev. Call after display::begin().
bool begin();
// LVGL read callback (registered internally by begin(); exposed for testing).
void read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data);
}  // namespace touch
