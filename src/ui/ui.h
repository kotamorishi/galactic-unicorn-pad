// ui.h — top-level screen manager.
#pragma once

class BoardClient;

namespace ui {
// Build the UI and load the main (word-picker) screen.
void init(BoardClient* board);
// Call from loop(): drives periodic /api/status polling into the status bar.
void tick();
}  // namespace ui
