// app_config.h — compile-time defaults.
//
// These are *defaults only*. At runtime NVS (Preferences) values win once the
// settings screen exists (M4). For now, set your WiFi + board host here.
#pragma once

// ---- WiFi (STA) ----
// TODO: set your network. Empty SSID => WiFi is skipped (UI still runs offline).
#define DEFAULT_WIFI_SSID     ""
#define DEFAULT_WIFI_PASS     ""

// ---- LED board (galactic-unicorn-leg) ----
// mDNS host of the board, or a raw IP like "192.168.1.50".
// The board advertises HTTP on port 80.
#define DEFAULT_BOARD_HOST    "galacticunicorn.local"
#define DEFAULT_BOARD_PORT    80

// ---- Message limits (must match the LED board) ----
#define MESSAGE_MAX_CHARS     128

// ---- Status polling ----
#define STATUS_POLL_MS        5000

// ---- HTTP ----
#define HTTP_TIMEOUT_MS       4000
