// wifi_manager.h — STA connection + mDNS host resolution.
//
// Minimal for M1/M2: connect once at boot, expose status, resolve the board's
// .local hostname to an IP. M4 adds reconnect/backoff + an offline indicator.
#pragma once
#include <Arduino.h>

namespace wifimgr {
// Start STA connection WITHOUT blocking; poll isConnected() from loop(). Empty
// ssid => skip (offline). Keeps boot fast so the UI appears immediately.
void beginAsync(const String& ssid, const String& pass);
bool isConnected();
void loop();  // call periodically; placeholder for reconnect logic (M4)

// Start the mDNS responder. Call once, after the connection comes up.
void startMdns();

// Resolve "foo.local" to a dotted IP string. Returns the input unchanged if it
// is already an IP or resolution fails.
String resolveHost(const String& hostOrMdns);

String localIp();
}  // namespace wifimgr
