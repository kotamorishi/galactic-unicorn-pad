// display.h — CrowPanel 7" RGB panel + LVGL display driver bring-up.
#pragma once
#include <stdint.h>

namespace display {
// Initialise panel + backlight + LVGL display driver (partial PSRAM buffers).
// Returns false if the panel or buffers fail to allocate.
bool begin();

// 0..100 backlight (simple on/off on this model; kept as a percentage API so
// PWM dimming can be added without changing callers).
void setBacklight(uint8_t pct);
}  // namespace display
