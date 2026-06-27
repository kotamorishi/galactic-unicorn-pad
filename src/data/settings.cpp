#include "settings.h"
#include "app_config.h"
#include <Preferences.h>

namespace {
constexpr const char* NS = "pad";
}

namespace settings {

void load(AppSettings& out) {
  // Start from compile-time defaults.
  out.wifiSsid = DEFAULT_WIFI_SSID;
  out.wifiPass = DEFAULT_WIFI_PASS;
  out.boardHost = DEFAULT_BOARD_HOST;
  out.boardPort = DEFAULT_BOARD_PORT;

  // Overlay any NVS-saved values.
  Preferences p;
  if (!p.begin(NS, /*readOnly=*/true)) return;
  out.wifiSsid = p.getString("ssid", out.wifiSsid);
  out.wifiPass = p.getString("pass", out.wifiPass);
  out.boardHost = p.getString("host", out.boardHost);
  out.boardPort = p.getUShort("port", out.boardPort);
  out.lastR = p.getUChar("r", out.lastR);
  out.lastG = p.getUChar("g", out.lastG);
  out.lastB = p.getUChar("b", out.lastB);
  out.brightnessOffset = p.getInt("bright", out.brightnessOffset);
  out.volume = p.getInt("vol", out.volume);
  p.end();
}

void save(const AppSettings& s) {
  Preferences p;
  if (!p.begin(NS, /*readOnly=*/false)) return;
  p.putString("ssid", s.wifiSsid);
  p.putString("pass", s.wifiPass);
  p.putString("host", s.boardHost);
  p.putUShort("port", s.boardPort);
  p.putUChar("r", s.lastR);
  p.putUChar("g", s.lastG);
  p.putUChar("b", s.lastB);
  p.putInt("bright", s.brightnessOffset);
  p.putInt("vol", s.volume);
  p.end();
}

}  // namespace settings
