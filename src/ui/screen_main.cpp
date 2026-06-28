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
lv_obj_t* s_cat_selector = nullptr;
int s_current_cat = 0;  // index into CATEGORY_MAPS; mirrors the checked tab

// --- Embedded word tiles (M2). Future: load from data/words.json (M3).
// "\n" starts a new row; "" terminates the map. Order mirrors data/words.json.
const char* MAP_NAMES[]  = {"EMMA", "LEO", "KOTA", "\n", "HARUNA", "EVERYONE", ""};
const char* MAP_CHORES[] = {"CLEAN UP", "SLEEP TIME", "QUIET", "\n", "DO NOT FIGHT", "HOMEWORK TIME", ""};
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

// Switch the active category: swap the word grid map and move the checked tab.
void select_category(int id) {
  if (id < 0 || id >= NUM_CATEGORIES) return;
  if (s_cat_selector) {
    lv_btnmatrix_clear_btn_ctrl(s_cat_selector, s_current_cat, LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_set_btn_ctrl(s_cat_selector, id, LV_BTNMATRIX_CTRL_CHECKED);
  }
  if (s_word_grid) lv_btnmatrix_set_map(s_word_grid, CATEGORY_MAPS[id]);
  s_current_cat = id;
}

// --- event handlers ---

void word_event_cb(lv_event_t* e) {
  lv_obj_t* obj = lv_event_get_target(e);
  uint16_t id = lv_btnmatrix_get_selected_btn(obj);
  if (id == LV_BTNMATRIX_BTN_NONE) return;
  const char* txt = lv_btnmatrix_get_btn_text(obj, id);
  bool was_names = (s_current_cat == 0);
  if (txt && !s_builder->appendWord(txt)) {
    screen_main::setStatusText("message full (128 chars)");
  }
  refresh_preview();
  // Typical flow: pick a name, then a chore. Auto-advance NAMES -> CHORES.
  if (was_names) select_category(1);
}

void clear_event_cb(lv_event_t* /*e*/) {
  s_builder->clear();
  refresh_preview();
}

void category_event_cb(lv_event_t* e) {
  lv_obj_t* obj = lv_event_get_target(e);
  uint16_t id = lv_btnmatrix_get_selected_btn(obj);
  if (id < NUM_CATEGORIES) select_category(id);
}

void send_event_cb(lv_event_t* /*e*/) {
  // An empty SEND clears the board (sends an empty message), since there is no
  // dedicated clear-the-display control.
  bool clearing = s_builder->empty();
  screen_main::setStatusText(clearing ? "clearing board..." : "sending...");
  pump_ui();

  MessagePayload m;
  m.text = s_builder->text();  // empty string -> board shows nothing
  // Colour is fixed orange for now (see MessagePayload defaults); M3 adds a picker.
  bool ok = s_board->postMessage(m);
  screen_main::setStatusText(ok ? (clearing ? "board cleared!" : "sent!")
                                : "send failed");
}

void call_event_cb(lv_event_t* /*e*/) {
  screen_main::setStatusText("playing sound...");
  pump_ui();
  bool ok = s_board->previewSound(/*preset_id=*/1, /*volume=*/50, /*count=*/1);
  screen_main::setStatusText(ok ? "sound played!" : "sound failed");
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
  lv_obj_set_style_pad_ver(prev, 0, 0);
  lv_obj_set_style_pad_column(prev, 10, 0);
  // Plain container scrolls by default, which snaps children to the top edge and
  // breaks the flex cross-axis centering. Disable it so everything sits centred.
  lv_obj_clear_flag(prev, LV_OBJ_FLAG_SCROLLABLE);

  s_preview_label = lv_label_create(prev);
  lv_obj_set_flex_grow(s_preview_label, 1);
  lv_obj_set_style_text_font(s_preview_label, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(s_preview_label, lv_color_white(), 0);
  lv_label_set_long_mode(s_preview_label, LV_LABEL_LONG_SCROLL_CIRCULAR);

  s_counter_label = lv_label_create(prev);
  lv_obj_set_style_text_color(s_counter_label, lv_color_hex(0xAAAAAA), 0);

  // Clear button (x) lives in the preview row now that the edit toolbar is gone.
  lv_obj_t* clear_btn = lv_btn_create(prev);
  lv_obj_set_size(clear_btn, 52, 52);
  lv_obj_set_style_bg_color(clear_btn, lv_palette_main(LV_PALETTE_RED), 0);
  lv_obj_add_event_cb(clear_btn, clear_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* clear_lbl = lv_label_create(clear_btn);
  lv_label_set_text(clear_lbl, LV_SYMBOL_CLOSE);
  lv_obj_center(clear_lbl);

  // 3) Category selector (one-checked)
  static const char* category_map[] = {"NAMES", "CHORES", "WORDS", "STATUS", ""};
  lv_obj_t* cat = make_btnmatrix(scr, category_map, category_event_cb, 78, true);
  lv_btnmatrix_set_btn_ctrl(cat, 0, LV_BTNMATRIX_CTRL_CHECKED);
  s_cat_selector = cat;

  // 4) Word grid (fills remaining space)
  s_word_grid = make_btnmatrix(scr, CATEGORY_MAPS[0], word_event_cb, 0, false);
  lv_obj_set_style_text_font(s_word_grid, &lv_font_montserrat_20, 0);

  // 5) Action row: SEND / CALL
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
