#include "ui.h"
#include "screen_main.h"
#include "model/message_builder.h"
#include "net/board_client.h"
#include "app_config.h"
#include <Arduino.h>
#include <lvgl.h>

namespace {
MessageBuilder g_builder;     // the message being assembled
BoardClient* g_board = nullptr;

// /api/status is fetched on Core 0 so its blocking HTTP (up to HTTP_TIMEOUT_MS)
// never stalls the LVGL/touch loop on Core 1. The task publishes the latest
// result under a mutex; ui::tick() (Core 1) picks it up and is the only place
// that touches LVGL — LVGL is not thread-safe, so the network task must never
// call into it.
SemaphoreHandle_t g_status_mtx = nullptr;
BoardStatus g_status_shared;
bool g_status_ok = false;
bool g_status_fresh = false;

void status_task(void*) {
  for (;;) {
    BoardStatus st;
    bool ok = g_board->getStatus(st);
    if (xSemaphoreTake(g_status_mtx, portMAX_DELAY) == pdTRUE) {
      g_status_shared = st;
      g_status_ok = ok;
      g_status_fresh = true;
      xSemaphoreGive(g_status_mtx);
    }
    vTaskDelay(pdMS_TO_TICKS(STATUS_POLL_MS));
  }
}
}  // namespace

namespace ui {

void init(BoardClient* board) {
  g_board = board;
  lv_obj_t* scr = screen_main::create(board, &g_builder);
  lv_scr_load(scr);

  g_status_mtx = xSemaphoreCreateMutex();
  // Pin to Core 0 (PRO_CPU, where the WiFi stack lives) at low priority.
  // 8 KB stack covers HTTPClient + ArduinoJson + String churn.
  xTaskCreatePinnedToCore(status_task, "status", 8192, nullptr, 1, nullptr, 0);
}

void tick() {
  if (!g_status_mtx) return;
  BoardStatus st;
  bool ok = false;
  bool have = false;
  if (xSemaphoreTake(g_status_mtx, 0) == pdTRUE) {
    if (g_status_fresh) {
      st = g_status_shared;
      ok = g_status_ok;
      g_status_fresh = false;
      have = true;
    }
    xSemaphoreGive(g_status_mtx);
  }
  if (!have) return;
  if (ok) {
    screen_main::setStatus(st);
  } else {
    screen_main::setStatusText("board offline");
  }
}

}  // namespace ui
