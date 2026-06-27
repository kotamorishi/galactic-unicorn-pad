// screen_main.h — the word-picker screen (core UX).
#pragma once
#include <lvgl.h>

class BoardClient;
class MessageBuilder;
struct BoardStatus;

namespace screen_main {
// Build the screen. `board` and `builder` are owned by the caller (ui.cpp).
lv_obj_t* create(BoardClient* board, MessageBuilder* builder);

// Update the top status bar from a polled board status / a transient message.
void setStatus(const BoardStatus& st);
void setStatusText(const char* text);
}  // namespace screen_main
