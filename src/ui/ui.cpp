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
uint32_t g_last_poll = 0;
}  // namespace

namespace ui {

void init(BoardClient* board) {
  g_board = board;
  lv_obj_t* scr = screen_main::create(board, &g_builder);
  lv_scr_load(scr);
}

void tick() {
  if (!g_board) return;
  uint32_t now = millis();
  if (now - g_last_poll < STATUS_POLL_MS) return;
  g_last_poll = now;

  BoardStatus st;
  if (g_board->getStatus(st)) {
    screen_main::setStatus(st);
  } else {
    screen_main::setStatusText("board offline");
  }
}

}  // namespace ui
