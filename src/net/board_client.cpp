#include "board_client.h"
#include "app_config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

void BoardClient::begin(const String& host, uint16_t port) {
  setHost(host, port);
}

void BoardClient::setHost(const String& host, uint16_t port) {
  _base = "http://" + host + ":" + String(port);
}

bool BoardClient::_postJson(const char* path, const String& body) {
  if (WiFi.status() != WL_CONNECTED || _base.length() == 0) return false;
  HTTPClient http;
  if (!http.begin(_base + path)) return false;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  http.end();
  return code >= 200 && code < 300;
}

bool BoardClient::_getInto(const char* path, String& outBody) {
  if (WiFi.status() != WL_CONNECTED || _base.length() == 0) return false;
  HTTPClient http;
  if (!http.begin(_base + path)) return false;
  http.setTimeout(HTTP_TIMEOUT_MS);
  int code = http.GET();
  bool ok = code >= 200 && code < 300;
  if (ok) outBody = http.getString();
  http.end();
  return ok;
}

bool BoardClient::postMessage(const MessagePayload& m) {
  // Enforce the board's 128-char limit defensively.
  String text = m.text;
  if (text.length() > MESSAGE_MAX_CHARS) text = text.substring(0, MESSAGE_MAX_CHARS);

  JsonDocument doc;
  doc["text"] = text;
  doc["display_mode"] = m.display_mode;
  doc["scroll_speed"] = m.scroll_speed;
  doc["font"] = m.font;
  JsonObject color = doc["color"].to<JsonObject>();
  color["r"] = m.r; color["g"] = m.g; color["b"] = m.b;
  JsonObject bg = doc["bg_color"].to<JsonObject>();
  bg["r"] = m.bgr; bg["g"] = m.bgg; bg["b"] = m.bgb;

  String body;
  serializeJson(doc, body);
  return _postJson("/api/message", body);
}

bool BoardClient::postBrightness(int offset) {
  JsonDocument doc;
  doc["brightness_offset"] = offset;
  String body; serializeJson(doc, body);
  return _postJson("/api/system/brightness", body);
}

bool BoardClient::postVolume(int volume0_100) {
  JsonDocument doc;
  doc["volume"] = volume0_100;
  String body; serializeJson(doc, body);
  return _postJson("/api/system/volume", body);
}

bool BoardClient::previewSound(int presetId, int volume, int count) {
  // /api/sound/preview rings the preset WITHOUT flashing an alert on the LED
  // board (unlike /api/call). Same as the web UI's "Test Sound" button.
  JsonDocument doc;
  doc["preset_id"] = presetId;
  doc["volume"] = volume;
  doc["count"] = count;
  String body; serializeJson(doc, body);
  return _postJson("/api/sound/preview", body);
}

bool BoardClient::getStatus(BoardStatus& out) {
  String body;
  if (!_getInto("/api/status", body)) return false;

  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) return false;

  out.active = doc["active"] | false;
  out.message = (const char*)(doc["message"] | "");
  out.time = (const char*)(doc["time"] | "");
  out.day = (const char*)(doc["day"] | "");
  out.next_start = (const char*)(doc["next_start"] | "");
  out.next_day = (const char*)(doc["next_day"] | "");
  out.brightness_offset = doc["brightness_offset"] | 0;
  out.ok = true;
  return true;
}
