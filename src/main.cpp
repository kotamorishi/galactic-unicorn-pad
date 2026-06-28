// main.cpp — galactic-unicorn-pad entry point.
//
// Boot order matters: display (panel + LVGL) -> touch -> WiFi -> board client
// -> UI. Mirrors the resilience ethos of galactic-unicorn-leg: every subsystem
// failure is logged and survivable rather than fatal.
#include <Arduino.h>
#include <lvgl.h>
#include <time.h>

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

  // Bring the UI up immediately with the configured host, then connect WiFi in
  // the background so a slow/failed association never delays boot. resolveHost
  // returns an IP unchanged, so the board is usable right away when host is an IP.
  g_board.begin(g_settings.boardHost, g_settings.boardPort);
  ui::init(&g_board);
  Serial.println("[boot] UI ready");

  wifimgr::beginAsync(g_settings.wifiSsid, g_settings.wifiPass);
  Serial.println("[boot] ready (WiFi connecting in background)");
}

void loop() {
  lv_timer_handler();   // LVGL render + input
  ui::tick();           // status polling

  // One-time setup once WiFi associates: NTP (America/Toronto), mDNS, and
  // resolve the board host to its IP. Runs in loop so boot isn't blocked.
  static bool g_net_ready = false;
  if (!g_net_ready && wifimgr::isConnected()) {
    g_net_ready = true;
    configTzTime("EST5EDT,M3.2.0,M11.1.0", "pool.ntp.org", "time.nist.gov");
    wifimgr::startMdns();
    String host = wifimgr::resolveHost(g_settings.boardHost);
    g_board.setHost(host, g_settings.boardPort);
    Serial.printf("[wifi] up; board base = %s\n", g_board.base().c_str());
  }

  wifimgr::loop();      // reconnect monitor (M4)
  delay(5);
}
