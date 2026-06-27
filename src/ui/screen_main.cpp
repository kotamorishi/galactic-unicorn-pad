#include "screen_main.h"
#include "model/message_builder.h"
#include "net/board_client.h"
#include "app_config.h"
#include <Arduino.h>

namespace {

BoardClient* s_board = nullptr;
MessageBuilder* s_builder = nullptr;

lv_obj_t* s_status_label = nullptr;
lv_obj_t* s_preview_label = nullptr;
lv_obj_t* s_counter_label = nullptr;
lv_obj_t* s_word_grid = nullptr;

// --- Embedded word tiles (M2). Future: load from data/words.json (M3).
// "\n" starts a new row; "" terminates the map. Order mirrors data/words.json.
const char* MAP_NAMES[]  = {"EMMA", "KOTA", "MUM", "\n", "DAD", "GRAN", "ALL", ""};
const char* MAP_CHORES[] = {"LAUNDRY", "CLEAN", "DISHES", "\n", "TRASH", "TIDY", "ROOM", "BATH", ""};
const char* MAP_WORDS[]  = {"UP", "NOW", "PLEASE", "\n", "DONE", "HELP", "TIME", "\n", "GO", "STOP", "WAIT", ""};
const char* MAP_STATUS[] = {"READY", "DINNER", "HELLO", "\n", "BYE", "GREAT", "JOB", "\n", "LOVE", "YOU", ""};
const char** CATEGORY_MAPS[] = {MAP_NAMES, MAP_CHORES, MAP_WORDS, MAP_STATUS};
constexpr int NUM_CATEGORIES = 4;

void refresh_preview() {
  if (!s_preview_label) return;
  const String& t = s_builder->text();
  lv_label_set_text(s_preview_label,
                    t.length() ? t.c_str() : "(tap words to build a message)");

  char buf[16];
  snprintf(buf, sizeof(buf), "%u/%d", (unsigned)s_builder->length(), MESSAGE_MAX_CHARS);
  lv_label_set_text(s_counter_label, buf);

  bool near = s_builder->length() > (size_t)(MESSAGE_MAX_CHARS * 9 / 10);
  lv_obj_set_style_text_color(
      s_counter_label,
      near ? lv_palette_main(LV_PALETTE_RED) : lv_color_hex(0xAAAAAA), 0);
}

// Force one render pass so transient "sending..." text is visible before a
// blocking HTTP call. (M4 moves HTTP off the UI thread entirely.)
void pump_ui() { lv_refr_now(NULL); }

// --- event handlers ---

void word_event_cb(lv_event_t* e) {
  lv_obj_t* obj = lv_event_get_target(e);
  uint16_t id = lv_btnmatrix_get_selected_btn(obj);
  if (id == LV_BTNMATRIX_BTN_NONE) return;
  const char* txt = lv_btnmatrix_get_btn_text(obj, id);
  if (txt && !s_builder->appendWord(txt)) {
    screen_main::setStatusText("message full (128 chars)");
  }
  refresh_preview();
}

void edit_event_cb(lv_event_t* e) {
  lv_obj_t* obj = lv_event_get_target(e);
  uint16_t id = lv_btnmatrix_get_selected_btn(obj);
  switch (id) {
    case 0: s_builder->appendSpace(); break;     // SPACE
    case 1: s_builder->backspaceToken(); break;  // DEL (whole word)
    case 2: s_builder->clear(); break;           // CLEAR
    default: break;
  }
  refresh_preview();
}

void category_event_cb(lv_event_t* e) {
  lv_obj_t* obj = lv_event_get_target(e);
  uint16_t id = lv_btnmatrix_get_selected_btn(obj);
  if (id < NUM_CATEGORIES && s_word_grid) {
    lv_btnmatrix_set_map(s_word_grid, CATEGORY_MAPS[id]);
  }
}

void send_event_cb(lv_event_t* /*e*/) {
  if (s_builder->empty()) {
    screen_main::setStatusText("nothing to send");
    return;
  }
  screen_main::setStatusText("sending...");
  pump_ui();

  MessagePayload m;
  m.text = s_builder->text();
  // M3 will apply the user-selected color here; default white for now.
  bool ok = s_board->postMessage(m);
  screen_main::setStatusText(ok ? "sent!" : "send failed");
}

void call_event_cb(lv_event_t* /*e*/) {
  screen_main::setStatusText("calling...");
  pump_ui();
  bool ok = s_board->call(/*preset_id=*/1, /*volume=*/50, /*count=*/1);
  screen_main::setStatusText(ok ? "called!" : "call failed");
}

// --- small layout helpers ---

lv_obj_t* make_btnmatrix(lv_obj_t* parent, const char** map, lv_event_cb_t cb,
                         lv_coord_t height, bool one_check) {
  lv_obj_t* bm = lv_btnmatrix_create(parent);
  lv_btnmatrix_set_map(bm, map);
  lv_obj_set_width(bm, LV_PCT(100));
  if (height > 0)
    lv_obj_set_height(bm, height);
  else
    lv_obj_set_flex_grow(bm, 1);
  lv_obj_set_style_pad_all(bm, 4, 0);
  if (one_check) {
    lv_btnmatrix_set_btn_ctrl_all(bm, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(bm, true);
  }
  lv_obj_add_event_cb(bm, cb, LV_EVENT_VALUE_CHANGED, NULL);
  return bm;
}

}  // namespace

namespace screen_main {

lv_obj_t* create(BoardClient* board, MessageBuilder* builder) {
  s_board = board;
  s_builder = builder;

  lv_obj_t* scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x101015), 0);
  lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(scr, 8, 0);
  lv_obj_set_style_pad_row(scr, 8, 0);

  // 1) Status bar
  s_status_label = lv_label_create(scr);
  lv_label_set_text(s_status_label, "starting...");
  lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x88CCFF), 0);

  // 2) Preview row (message-so-far + char counter)
  lv_obj_t* prev = lv_obj_create(scr);
  lv_obj_set_width(prev, LV_PCT(100));
  lv_obj_set_height(prev, 76);
  lv_obj_set_style_bg_color(prev, lv_color_hex(0x1C1C24), 0);
  lv_obj_set_flex_flow(prev, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(prev, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_hor(prev, 12, 0);

  s_preview_label = lv_label_create(prev);
  lv_obj_set_flex_grow(s_preview_label, 1);
  lv_obj_set_style_text_font(s_preview_label, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(s_preview_label, lv_color_white(), 0);
  lv_label_set_long_mode(s_preview_label, LV_LABEL_LONG_SCROLL_CIRCULAR);

  s_counter_label = lv_label_create(prev);
  lv_obj_set_style_text_color(s_counter_label, lv_color_hex(0xAAAAAA), 0);

  // 3) Edit toolbar
  static const char* edit_map[] = {"SPACE", "DEL", "CLEAR", ""};
  make_btnmatrix(scr, edit_map, edit_event_cb, 56, false);

  // 4) Category selector (one-checked)
  static const char* category_map[] = {"NAMES", "CHORES", "WORDS", "STATUS", ""};
  lv_obj_t* cat = make_btnmatrix(scr, category_map, category_event_cb, 52, true);
  lv_btnmatrix_set_btn_ctrl(cat, 0, LV_BTNMATRIX_CTRL_CHECKED);

  // 5) Word grid (fills remaining space)
  s_word_grid = make_btnmatrix(scr, CATEGORY_MAPS[0], word_event_cb, 0, false);
  lv_obj_set_style_text_font(s_word_grid, &lv_font_montserrat_20, 0);

  // 6) Action row: SEND / CALL
  lv_obj_t* actions = lv_obj_create(scr);
  lv_obj_set_width(actions, LV_PCT(100));
  lv_obj_set_height(actions, 70);
  lv_obj_set_style_bg_opa(actions, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(actions, 0, 0);
  lv_obj_set_style_pad_all(actions, 0, 0);
  lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(actions, 8, 0);
  lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t* send = lv_btn_create(actions);
  lv_obj_set_flex_grow(send, 3);
  lv_obj_set_height(send, LV_PCT(100));
  lv_obj_set_style_bg_color(send, lv_palette_main(LV_PALETTE_GREEN), 0);
  lv_obj_add_event_cb(send, send_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* send_lbl = lv_label_create(send);
  lv_label_set_text(send_lbl, "SEND");
  lv_obj_set_style_text_font(send_lbl, &lv_font_montserrat_28, 0);
  lv_obj_center(send_lbl);

  lv_obj_t* call = lv_btn_create(actions);
  lv_obj_set_flex_grow(call, 1);
  lv_obj_set_height(call, LV_PCT(100));
  lv_obj_set_style_bg_color(call, lv_palette_main(LV_PALETTE_AMBER), 0);
  lv_obj_add_event_cb(call, call_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* call_lbl = lv_label_create(call);
  lv_label_set_text(call_lbl, "CALL");
  lv_obj_set_style_text_font(call_lbl, &lv_font_montserrat_28, 0);
  lv_obj_center(call_lbl);

  refresh_preview();
  return scr;
}

void setStatus(const BoardStatus& st) {
  if (!s_status_label) return;
  char buf[120];
  if (st.active) {
    snprintf(buf, sizeof(buf), "%s %s  |  Displaying: %s",
             st.day.c_str(), st.time.c_str(), st.message.c_str());
  } else {
    snprintf(buf, sizeof(buf), "%s %s  |  Off (next %s %s)",
             st.day.c_str(), st.time.c_str(), st.next_day.c_str(),
             st.next_start.c_str());
  }
  lv_label_set_text(s_status_label, buf);
}

void setStatusText(const char* text) {
  if (s_status_label) lv_label_set_text(s_status_label, text);
}

}  // namespace screen_main
