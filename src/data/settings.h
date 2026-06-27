// settings.h — persisted user settings in NVS (Preferences).
//
// Minimal for now: loads compile-time defaults from app_config.h and overlays
// any values previously saved to NVS. The settings *screen* that writes these
// arrives in M4; load()/save() are in place so the rest of the code can already
// depend on a single AppSettings source of truth.
#pragma once
#include <Arduino.h>

struct AppSettings {
  String wifiSsid;
  String wifiPass;
  String boardHost;
  uint16_t boardPort = 80;

  // last-used message styling
  uint8_t lastR = 255, lastG = 255, lastB = 255;
  int brightnessOffset = 0;
  int volume = 50;
};

namespace settings {
void load(AppSettings& out);   // defaults (app_config.h) overlaid with NVS
void save(const AppSettings& s);
}  // namespace settings
