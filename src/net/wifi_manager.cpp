#include "wifi_manager.h"
#include <WiFi.h>
#include <ESPmDNS.h>

namespace wifimgr {

void beginAsync(const String& ssid, const String& pass) {
  if (ssid.length() == 0) {
    Serial.println("[wifi] no SSID configured; skipping (offline mode)");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.printf("[wifi] connecting to %s (background)\n", ssid.c_str());
}

void startMdns() { MDNS.begin("galacticpad"); }

bool isConnected() { return WiFi.status() == WL_CONNECTED; }

void loop() {
  // M4: monitor + exponential-backoff reconnect, mirroring galactic-unicorn-leg.
}

String resolveHost(const String& hostOrMdns) {
  // Already an IP? hand it back.
  IPAddress probe;
  if (probe.fromString(hostOrMdns)) return hostOrMdns;

  // Strip a trailing ".local" and query mDNS.
  String name = hostOrMdns;
  int dot = name.indexOf(".local");
  if (dot >= 0) name = name.substring(0, dot);

  IPAddress ip = MDNS.queryHost(name);
  if (ip != IPAddress((uint32_t)0)) {
    Serial.printf("[wifi] resolved %s -> %s\n", hostOrMdns.c_str(), ip.toString().c_str());
    return ip.toString();
  }
  Serial.printf("[wifi] mDNS resolve failed for %s; using as-is\n", hostOrMdns.c_str());
  return hostOrMdns;
}

String localIp() {
  return WiFi.localIP().toString();
}

}  // namespace wifimgr
