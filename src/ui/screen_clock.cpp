#include "screen_clock.h"
#include <Arduino.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <esp_heap_caps.h>

// Design fonts (generated from Space Grotesk / Spectral via lv_font_conv,
// src/ui/fonts/*.c) — reproduces the Clock & Calendar design's typography.
LV_FONT_DECLARE(sg_big);  // Space Grotesk 120px digits — the big date
LV_FONT_DECLARE(sg_20);   // Space Grotesk 20px
LV_FONT_DECLARE(sg_15);   // Space Grotesk 15px
LV_FONT_DECLARE(sp_28);   // Spectral 28px (serif)
LV_FONT_DECLARE(sp_30);   // Spectral 30px (serif)

namespace {

// Horizontal scale for the clock face. 1.0 = perfect circle in LVGL coords. If
// the panel's pixels aren't square (clock looks stretched wide), drop below 1.0
// to squeeze it horizontally.
constexpr float CLK_ASPECT_X = 1.0f;

lv_obj_t* s_scr = nullptr;
lv_timer_t* s_timer = nullptr;

// Hand-drawn analog clock on an lv_canvas (lv_meter only draws true circles and
// rendered as a wide ellipse on this panel; a canvas gives full coordinate
// control so the face is a real circle).
lv_obj_t* s_clock_canvas = nullptr;
void* s_clock_buf = nullptr;
int s_clock_size = 0;
lv_color_t s_clock_face, s_clock_tick, s_clock_hand, s_clock_sec;
bool s_clock_face_on = false;

lv_obj_t* s_lbl_weekday = nullptr;
lv_obj_t* s_lbl_big = nullptr;        // big day number (night) / month name (day)
lv_obj_t* s_lbl_monthyear = nullptr;

int s_is_day = -1;     // -1 = not built yet
int s_last_mday = -1;

const char* WD_FULL[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                         "Thursday", "Friday", "Saturday"};
const char* MO_FULL[] = {"January", "February", "March", "April", "May", "June",
                         "July", "August", "September", "October", "November",
                         "December"};

void get_now(struct tm* out) {
  time_t t = time(nullptr);
  localtime_r(&t, out);
}

void to_upper(const char* src, char* dst, size_t n) {
  size_t i = 0;
  for (; src[i] && i < n - 1; i++) dst[i] = (char)toupper((unsigned char)src[i]);
  dst[i] = 0;
}

lv_obj_t* make_clock(lv_obj_t* parent, int size, lv_color_t face, bool draw_face,
                     lv_color_t tick, lv_color_t hand, lv_color_t sec) {
  if (s_clock_buf) { heap_caps_free(s_clock_buf); s_clock_buf = nullptr; }
  s_clock_buf = heap_caps_malloc(LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(size, size),
                                 MALLOC_CAP_SPIRAM);

  lv_obj_t* cv = lv_canvas_create(parent);
  lv_canvas_set_buffer(cv, s_clock_buf, size, size, LV_IMG_CF_TRUE_COLOR_ALPHA);
  lv_obj_set_size(cv, size, size);
  lv_obj_set_style_min_width(cv, size, 0);
  lv_obj_set_style_max_width(cv, size, 0);
  lv_obj_set_style_min_height(cv, size, 0);
  lv_obj_set_style_max_height(cv, size, 0);

  s_clock_canvas = cv;
  s_clock_size = size;
  s_clock_face = face;
  s_clock_face_on = draw_face;
  s_clock_tick = tick;
  s_clock_hand = hand;
  s_clock_sec = sec;
  return cv;
}

void draw_clock(const struct tm& tm) {
  lv_obj_t* cv = s_clock_canvas;
  if (!cv) return;
  int size = s_clock_size;
  int cx = size / 2, cy = size / 2;

  lv_canvas_fill_bg(cv, lv_color_black(), LV_OPA_TRANSP);

  if (s_clock_face_on) {
    lv_draw_rect_dsc_t rd;
    lv_draw_rect_dsc_init(&rd);
    rd.bg_color = s_clock_face;
    rd.bg_opa = LV_OPA_COVER;
    rd.radius = LV_RADIUS_CIRCLE;
    lv_canvas_draw_rect(cv, 0, 0, size, size, &rd);
  }

  lv_draw_line_dsc_t ld;
  lv_draw_line_dsc_init(&ld);
  ld.round_start = ld.round_end = 1;

  // minute ticks (60), every 5th major
  for (int i = 0; i < 60; i++) {
    float a = i * 6.0f * (float)M_PI / 180.0f;
    bool major = (i % 5 == 0);
    float Rout = size * 0.47f;
    float Rin = major ? size * 0.40f : size * 0.435f;
    float sx = sinf(a) * CLK_ASPECT_X, cz = cosf(a);
    lv_point_t p[2];
    p[0].x = cx + (lv_coord_t)(Rout * sx);
    p[0].y = cy - (lv_coord_t)(Rout * cz);
    p[1].x = cx + (lv_coord_t)(Rin * sx);
    p[1].y = cy - (lv_coord_t)(Rin * cz);
    ld.color = s_clock_tick;
    ld.width = major ? 3 : 1;
    lv_canvas_draw_line(cv, p, 2, &ld);
  }

  // hands
  float h = tm.tm_hour % 12, m = tm.tm_min, s = tm.tm_sec;
  float hourA = (h * 30 + m * 0.5f) * (float)M_PI / 180.0f;
  float minA = (m * 6 + s * 0.1f) * (float)M_PI / 180.0f;
  float secA = (s * 6.0f) * (float)M_PI / 180.0f;
  struct Hand { float ang; float len; lv_coord_t w; lv_color_t c; };
  Hand hands[] = {
      {hourA, size * 0.27f, 6, s_clock_hand},
      {minA, size * 0.40f, 4, s_clock_hand},
      {secA, size * 0.44f, 2, s_clock_sec},
  };
  for (auto& hd : hands) {
    lv_point_t p[2];
    p[0].x = cx;
    p[0].y = cy;
    p[1].x = cx + (lv_coord_t)(hd.len * sinf(hd.ang) * CLK_ASPECT_X);
    p[1].y = cy - (lv_coord_t)(hd.len * cosf(hd.ang));
    ld.color = hd.c;
    ld.width = hd.w;
    lv_canvas_draw_line(cv, p, 2, &ld);
  }

  // centre hub
  lv_draw_rect_dsc_t hub;
  lv_draw_rect_dsc_init(&hub);
  hub.bg_color = s_clock_sec;
  hub.bg_opa = LV_OPA_COVER;
  hub.radius = LV_RADIUS_CIRCLE;
  int hr = size / 38;
  lv_canvas_draw_rect(cv, cx - hr, cy - hr, hr * 2, hr * 2, &hub);
}

bool is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

lv_obj_t* make_row(lv_obj_t* parent, int h) {
  lv_obj_t* r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(r, LV_SIZE_CONTENT);
  lv_obj_set_height(r, h);
  return r;
}

// One calendar cell. `today` draws a filled circle behind the number.
void add_cell(lv_obj_t* row, int cell, const lv_font_t* font, const char* txt,
              lv_color_t color, bool today, lv_color_t todayBg, lv_color_t todayTxt) {
  if (today) {
    lv_obj_t* box = lv_obj_create(row);
    lv_obj_remove_style_all(box);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(box, cell, cell);
    int d = cell * 4 / 5;
    lv_obj_t* circ = lv_obj_create(box);
    lv_obj_remove_style_all(circ);
    lv_obj_set_size(circ, d, d);
    lv_obj_center(circ);
    lv_obj_set_style_radius(circ, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(circ, todayBg, 0);
    lv_obj_set_style_bg_opa(circ, LV_OPA_COVER, 0);
    lv_obj_t* l = lv_label_create(circ);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, todayTxt, 0);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_center(l);
  } else {
    lv_obj_t* l = lv_label_create(row);
    lv_obj_set_size(l, cell, cell);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_set_style_text_font(l, font, 0);
    lv_label_set_text(l, txt);
    lv_obj_set_style_pad_top(l, (cell - lv_font_get_line_height(font)) / 2, 0);
  }
}

// Hand-built month grid that matches the design: no borders, single-letter
// weekday header, muted leading/trailing days, today as a filled circle.
lv_obj_t* make_calendar(lv_obj_t* parent, int cell, const lv_font_t* font,
                        const struct tm& tm, lv_color_t header, lv_color_t dayc,
                        lv_color_t muted, lv_color_t todayBg, lv_color_t todayTxt) {
  int year = tm.tm_year + 1900, mon = tm.tm_mon, mday = tm.tm_mday;
  static const int DIM[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int daysIn = DIM[mon]; if (mon == 1 && is_leap(year)) daysIn = 29;
  int prevMon = (mon + 11) % 12, prevYear = mon == 0 ? year - 1 : year;
  int prevDays = DIM[prevMon]; if (prevMon == 1 && is_leap(prevYear)) prevDays = 29;
  struct tm f = tm;
  f.tm_mday = 1; f.tm_hour = 12; f.tm_min = 0; f.tm_sec = 0; f.tm_isdst = -1;
  mktime(&f);
  int startDow = f.tm_wday;

  lv_obj_t* g = lv_obj_create(parent);
  lv_obj_remove_style_all(g);
  lv_obj_clear_flag(g, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(g, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_size(g, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

  static const char* H[] = {"S", "M", "T", "W", "T", "F", "S"};
  lv_obj_t* hr = make_row(g, cell * 4 / 5);
  for (int i = 0; i < 7; i++)
    add_cell(hr, cell, font, H[i], header, false, header, header);

  int cells[42]; bool mut[42]; int n = 0;
  for (int i = startDow - 1; i >= 0; i--) { cells[n] = prevDays - i; mut[n] = true; n++; }
  for (int d = 1; d <= daysIn; d++) { cells[n] = d; mut[n] = false; n++; }
  int nd = 1; while (n % 7 != 0) { cells[n] = nd++; mut[n] = true; n++; }

  char buf[4];
  for (int wk = 0; wk * 7 < n; wk++) {
    lv_obj_t* r = make_row(g, cell);
    for (int c = 0; c < 7; c++) {
      int idx = wk * 7 + c;
      snprintf(buf, sizeof buf, "%d", cells[idx]);
      bool today = !mut[idx] && cells[idx] == mday;
      add_cell(r, cell, font, buf, mut[idx] ? muted : dayc, today, todayBg, todayTxt);
    }
  }
  return g;
}

void update_labels(const struct tm& tm, bool day) {
  char buf[64];
  int dow = tm.tm_wday, mday = tm.tm_mday, mon = tm.tm_mon, year = tm.tm_year + 1900;

  if (!day) {
    if (s_lbl_weekday) { to_upper(WD_FULL[dow], buf, sizeof buf); lv_label_set_text(s_lbl_weekday, buf); }
    if (s_lbl_big) { snprintf(buf, sizeof buf, "%d", mday); lv_label_set_text(s_lbl_big, buf); }
    if (s_lbl_monthyear) { snprintf(buf, sizeof buf, "%s %d", MO_FULL[mon], year); lv_label_set_text(s_lbl_monthyear, buf); }
  } else {
    if (s_lbl_weekday) lv_label_set_text(s_lbl_weekday, WD_FULL[dow]);
    if (s_lbl_monthyear) { snprintf(buf, sizeof buf, "%s %d", MO_FULL[mon], year); lv_label_set_text(s_lbl_monthyear, buf); }
    if (s_lbl_big) lv_label_set_text(s_lbl_big, MO_FULL[mon]);
  }
}

lv_obj_t* make_col(lv_obj_t* parent, lv_flex_align_t main_align) {
  lv_obj_t* c = lv_obj_create(parent);
  lv_obj_remove_style_all(c);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(c, main_align, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  return c;
}

void build_layout(const struct tm& tm, bool day) {
  lv_obj_clean(s_scr);
  s_clock_canvas = nullptr;
  s_lbl_weekday = s_lbl_big = s_lbl_monthyear = nullptr;

  lv_obj_set_flex_flow(s_scr, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_scr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  if (!day) {
    // V1 Midnight — dark, clock left, big date + mini calendar right.
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x0b0d12), 0);
    lv_obj_set_style_pad_all(s_scr, 40, 0);
    lv_obj_set_style_pad_column(s_scr, 44, 0);

    make_clock(s_scr, 340, lv_color_black(), false, lv_color_hex(0x3c4252),
               lv_color_hex(0xe8eaf0), lv_color_hex(0xf0a04b));

    lv_obj_t* col = make_col(s_scr, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_PCT(100));
    lv_obj_set_style_pad_row(col, 6, 0);

    s_lbl_weekday = lv_label_create(col);
    lv_obj_set_style_text_color(s_lbl_weekday, lv_color_hex(0xf0a04b), 0);
    lv_obj_set_style_text_font(s_lbl_weekday, &sg_15, 0);
    lv_obj_set_style_text_letter_space(s_lbl_weekday, 3, 0);

    s_lbl_big = lv_label_create(col);
    lv_obj_set_style_text_color(s_lbl_big, lv_color_hex(0xe8eaf0), 0);
    lv_obj_set_style_text_font(s_lbl_big, &sg_big, 0);

    s_lbl_monthyear = lv_label_create(col);
    lv_obj_set_style_text_color(s_lbl_monthyear, lv_color_hex(0x8a93a6), 0);
    lv_obj_set_style_text_font(s_lbl_monthyear, &sg_20, 0);

    make_calendar(col, 40, &sg_15, tm, lv_color_hex(0x5b6478),
                  lv_color_hex(0xc3c9d6), lv_color_hex(0x3a4150),
                  lv_color_hex(0xf0a04b), lv_color_hex(0x0b0d12));
  } else {
    // V2 Daylight — light, clock + weekday left, month + calendar right.
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0xf4f1ea), 0);
    lv_obj_set_style_pad_all(s_scr, 36, 0);
    lv_obj_set_style_pad_column(s_scr, 36, 0);

    lv_obj_t* left = make_col(s_scr, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(left, LV_SIZE_CONTENT);
    lv_obj_set_height(left, LV_PCT(100));
    lv_obj_set_style_pad_row(left, 14, 0);

    make_clock(left, 300, lv_color_hex(0xfbf9f4), true, lv_color_hex(0xcdc6b5),
               lv_color_hex(0x1c1a17), lv_color_hex(0xc84b31));

    s_lbl_weekday = lv_label_create(left);
    lv_obj_set_style_text_color(s_lbl_weekday, lv_color_hex(0x1c1a17), 0);
    lv_obj_set_style_text_font(s_lbl_weekday, &sp_28, 0);

    s_lbl_monthyear = lv_label_create(left);
    lv_obj_set_style_text_color(s_lbl_monthyear, lv_color_hex(0x8a8377), 0);
    lv_obj_set_style_text_font(s_lbl_monthyear, &sg_15, 0);

    lv_obj_t* right = make_col(s_scr, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(right, 1);
    lv_obj_set_height(right, LV_PCT(100));
    lv_obj_set_style_pad_row(right, 10, 0);

    s_lbl_big = lv_label_create(right);  // month name
    lv_obj_set_style_text_color(s_lbl_big, lv_color_hex(0x1c1a17), 0);
    lv_obj_set_style_text_font(s_lbl_big, &sp_30, 0);

    make_calendar(right, 56, &sg_20, tm, lv_color_hex(0xb0432c),
                  lv_color_hex(0x2b2823), lv_color_hex(0xc3bdae),
                  lv_color_hex(0xc84b31), lv_color_white());
  }
}

void timer_cb(lv_timer_t* /*t*/) {
  struct tm tm;
  get_now(&tm);
  bool day = (tm.tm_hour >= 7 && tm.tm_hour < 19);
  if ((int)day != s_is_day || tm.tm_mday != s_last_mday) {
    s_is_day = day;
    s_last_mday = tm.tm_mday;
    build_layout(tm, day);
    update_labels(tm, day);
  }
  draw_clock(tm);
}

}  // namespace

namespace screen_clock {

lv_obj_t* create() {
  s_scr = lv_obj_create(NULL);
  lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

  struct tm tm;
  get_now(&tm);
  bool day = (tm.tm_hour >= 7 && tm.tm_hour < 19);
  s_is_day = day;
  s_last_mday = tm.tm_mday;
  build_layout(tm, day);
  update_labels(tm, day);
  draw_clock(tm);

  s_timer = lv_timer_create(timer_cb, 1000, nullptr);
  return s_scr;
}

void destroy() {
  if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
  if (s_scr) { lv_obj_del(s_scr); s_scr = nullptr; }
  s_clock_canvas = nullptr;
  if (s_clock_buf) { heap_caps_free(s_clock_buf); s_clock_buf = nullptr; }
  s_lbl_weekday = s_lbl_big = s_lbl_monthyear = nullptr;
  s_is_day = -1;
  s_last_mday = -1;
}

}  // namespace screen_clock
