// screen_clock.h — full-screen clock + calendar shown after inactivity.
//
// Two looks driven by the local hour: a dark "Midnight" layout at night and a
// light "Daylight" layout during the day (07:00-19:00). The screen owns a 1 Hz
// LVGL timer that advances the analog clock and rebuilds the layout on a
// day/night or date rollover. Time comes from the system clock (NTP-synced in
// main.cpp for America/Toronto).
#pragma once
#include <lvgl.h>

namespace screen_clock {
// Build the clock screen and start its update timer. Returns the screen object.
lv_obj_t* create();
// Stop the timer and delete the screen. Safe to call when not created.
void destroy();
}  // namespace screen_clock
