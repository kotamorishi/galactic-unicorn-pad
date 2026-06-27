// main.cpp — galactic-unicorn-pad entry point.
//
// Boot order matters: display (panel + LVGL) -> touch -> WiFi -> board client
// -> UI. Mirrors the resilience ethos of galactic-unicorn-leg: every subsystem
// failure is logged and survivable rather than fatal.
#include <Arduino.h>
#include <lvgl.h>

#include "display/display.h"
#include "display/touch.h"
#include "net/wifi_manager.h"
#include "net/board_client.h"
#include "data/settings.h"
#include "ui/ui.h"

static BoardClient g_board;
static AppSettings g_settings;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[boot] galactic-unicorn-pad");

  settings::load(g_settings);

  if (!display::begin()) {
    Serial.println("[boot] display init FAILED — check board_pins.h / PSRAM mode");
    // Continue: serial logs remain available for diagnosis.
  }
  touch::begin();

  bool wifi = wifimgr::begin(g_settings.wifiSsid, g_settings.wifiPass);
  String host = g_settings.boardHost;
  if (wifi) host = wifimgr::resolveHost(g_settings.boardHost);
  g_board.begin(host, g_settings.boardPort);
  Serial.printf("[boot] board base = %s\n", g_board.base().c_str());

  ui::init(&g_board);
  Serial.println("[boot] ready");
}

void loop() {
  lv_timer_handler();   // LVGL render + input
  wifimgr::loop();      // reconnect monitor (M4)
  ui::tick();           // status polling
  delay(5);
}
