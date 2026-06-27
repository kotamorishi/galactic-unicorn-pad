// board_client.h — HTTP client for the galactic-unicorn-leg LED board API.
//
// Pure networking, no UI dependencies. All requests are synchronous and block
// for the duration of the HTTP round-trip (100s of ms). For M1/M2 we call these
// from the UI thread behind a "Sending..." state; M4 moves them to a core-0 task.
#pragma once
#include <Arduino.h>

struct BoardStatus {
  bool ok = false;            // request succeeded and parsed
  bool active = false;        // a schedule is currently displaying
  String message;
  String time;                // "HH:MM:SS"
  String day;                 // "Mon".."Sun"
  String next_start;
  String next_day;
  int brightness_offset = 0;
};

// Mirrors the LED board's /api/message body. Defaults match the board's own
// defaults so the minimal path only needs `text`.
struct MessagePayload {
  String text;
  String display_mode = "scroll";   // "scroll" | "fixed"
  String scroll_speed = "medium";   // "slow" | "medium" | "fast"
  String font = "bitmap8";          // "bitmap6" | "bitmap8"
  uint8_t r = 255, g = 165, b = 0;  // fixed orange (M3 will make this selectable)
  uint8_t bgr = 0, bgg = 0, bgb = 0;
};

class BoardClient {
public:
  void begin(const String& host, uint16_t port = 80);
  void setHost(const String& host, uint16_t port = 80);

  bool getStatus(BoardStatus& out);                 // GET  /api/status
  bool postMessage(const MessagePayload& m);        // POST /api/message
  bool postBrightness(int offset);                  // POST /api/system/brightness
  bool postVolume(int volume0_100);                 // POST /api/system/volume
  bool call(int presetId, int volume, int count);   // POST /api/call

  const String& base() const { return _base; }

private:
  String _base;  // "http://<host>:<port>"

  // Returns true on 2xx. When `respSize`>0 a small response buffer is parsed by
  // the caller; here we only need success/failure for most endpoints.
  bool _postJson(const char* path, const String& body);
  bool _getInto(const char* path, String& outBody);
};
