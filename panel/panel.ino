/*
 * iPinch Huisje Control Panel
 * CrowPanel Advance 7" V3.0 (DIS08070H) — ESP32-S3 800×480
 *
 * Required libraries:
 *   LVGL 8.3.3, LovyanGFX 1.2.19+, ArduinoJson 6.x
 *
 * Board: ESP32S3 Dev Module, OPI PSRAM, huge_app, upload 460800
 *
 * Copy secrets.h.example → secrets.h and fill in credentials.
 */

#include "pins_config.h"
#include "LovyanGFX_Driver.h"
#include "crab.h"
#include "secrets.h"

#include <Arduino.h>
#include <lvgl.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------
#define HDR_H   55
#define FTR_H   68
#define BODY_H  (LCD_V_RES - HDR_H - FTR_H)   // 357
#define LEFT_W  215
#define RIGHT_W (LCD_H_RES - LEFT_W)            // 585

// Colours
#define C_BG       0x0d1117
#define C_SURFACE  0x161b22
#define C_CARD     0x21262d
#define C_BORDER   0x30363d
#define C_TEXT     0xc9d1d9
#define C_MUTED    0x6e7681
#define C_ACCENT   0x1f6feb
#define C_ON_LIGHT 0xe6a817
#define C_ON_HEAT  0xf85149
#define C_SUCCESS  0x3fb950
#define C_ERROR    0xf85149

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
LGFX gfx;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf_a;
static lv_color_t *buf_b;

// UI handles
static lv_obj_t *lbl_temp, *lbl_weather, *lbl_time_hdr;
static lv_obj_t *dot_conn,  *lbl_conn;
static lv_obj_t *lbl_scene_title;
static lv_obj_t *scene_btns[4];
static lv_obj_t *lbl_scene_name[4];
static lv_obj_t *device_cards[6];
static lv_obj_t *lbl_dev_name[6];
static lv_obj_t *lbl_dev_state[6];
static lv_obj_t *ta_chat, *kb_chat;
static lv_obj_t *lbl_crab_msg;

// State
static bool wifi_ok   = false;
static bool api_ok    = false;

struct DeviceState { const char *id; const char *label; bool on; bool is_heat; };
static DeviceState devices[6] = {
  {"living_lights",  "Living",  false, false},
  {"kitchen_lights", "Kitchen", false, false},
  {"bedroom_lights", "Bedroom", false, false},
  {"decking_lights", "Decking", false, false},
  {"living_heat",    "Living",  false, true },
  {"bedroom_heat",   "Bedroom", false, true },
};

struct SceneState { const char *id; const char *label; const char *icon; bool active; };
static SceneState scenes[4] = {
  {"cozy_evening",  "Cozy Evening",   "~ Cozy",    false},
  {"morning_bright","Morning Bright", "* Morning", false},
  {"nightlight",    "Nightlight",     "- Night",   false},
  {"away",          "Away",           "< Away",    false},
};

static uint32_t last_status_ms = 0;
static bool     fetching       = false;

// ---------------------------------------------------------------------------
// PCA9557 helper
// ---------------------------------------------------------------------------
static void pca_write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(0x18);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

// ---------------------------------------------------------------------------
// LVGL callbacks
// ---------------------------------------------------------------------------
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  gfx.pushImageDMA(area->x1, area->y1,
                   area->x2 - area->x1 + 1, area->y2 - area->y1 + 1,
                   (lgfx::rgb565_t *)&color_p->full);
  lv_disp_flush_ready(disp);
}

void my_touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  uint16_t x, y;
  if (gfx.getTouch(&x, &y)) {
    data->state   = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// ---------------------------------------------------------------------------
// HTTP helpers
// ---------------------------------------------------------------------------
static String http_get(const char *path) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String(IPINCH_BASE_URL) + path;
  http.begin(client, url);
  http.setTimeout(5000);
  int code = http.GET();
  String body = (code == 200) ? http.getString() : "";
  http.end();
  return body;
}

static String http_post(const char *path, const String &payload) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String(IPINCH_BASE_URL) + path;
  http.begin(client, url);
  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(payload);
  String body = (code == 200) ? http.getString() : "";
  http.end();
  return body;
}

// ---------------------------------------------------------------------------
// UI update helpers
// ---------------------------------------------------------------------------
static void set_conn(bool ok) {
  api_ok = ok;
  lv_obj_set_style_bg_color(dot_conn,
    ok ? lv_color_hex(C_SUCCESS) : lv_color_hex(C_ERROR), LV_PART_MAIN);
  lv_label_set_text(lbl_conn, ok ? "Connected" : "Offline");
}

static void update_device_card(int i) {
  bool on      = devices[i].on;
  bool is_heat = devices[i].is_heat;
  uint32_t col = on ? (is_heat ? C_ON_HEAT : C_ON_LIGHT) : C_CARD;
  lv_obj_set_style_bg_color(device_cards[i], lv_color_hex(col), LV_PART_MAIN);
  lv_label_set_text(lbl_dev_state[i], on ? (is_heat ? "On" : "On") : "Off");
  lv_obj_set_style_text_color(lbl_dev_state[i],
    on ? lv_color_hex(0xffffff) : lv_color_hex(C_MUTED), LV_PART_MAIN);
}

static void update_scene_btn(int i) {
  bool active = scenes[i].active;
  lv_obj_set_style_bg_color(scene_btns[i],
    active ? lv_color_hex(C_ACCENT) : lv_color_hex(C_CARD), LV_PART_MAIN);
  lv_obj_set_style_border_color(scene_btns[i],
    active ? lv_color_hex(C_ACCENT) : lv_color_hex(C_BORDER), LV_PART_MAIN);
}

static void show_crab_msg(const char *msg) {
  lv_label_set_text(lbl_crab_msg, msg);
}

// ---------------------------------------------------------------------------
// API actions (called from event handlers)
// ---------------------------------------------------------------------------
static void do_toggle_device(int idx) {
  crab_processing();
  show_crab_msg("On it...");

  StaticJsonDocument<64> doc;
  doc["device"] = devices[idx].id;
  doc["action"] = "toggle";
  String body;
  serializeJson(doc, body);

  String resp = http_post("/huisje", body);
  if (resp.length()) {
    devices[idx].on = !devices[idx].on;
    update_device_card(idx);
    crab_excited();
    show_crab_msg("Done!");
    set_conn(true);
  } else {
    crab_error();
    show_crab_msg("Oops, couldn't reach iPinch");
    set_conn(false);
  }
  delay(800);
  crab_idle();
  show_crab_msg("Hey, tap something!");
}

static void do_activate_scene(int idx) {
  crab_processing();
  show_crab_msg("Setting the scene...");

  StaticJsonDocument<64> doc;
  doc["scene"] = scenes[idx].id;
  String body;
  serializeJson(doc, body);

  String resp = http_post("/huisje/scene", body);
  if (resp.length()) {
    for (int i = 0; i < 4; i++) scenes[i].active = (i == idx);
    for (int i = 0; i < 4; i++) update_scene_btn(i);
    crab_excited();
    show_crab_msg(scenes[idx].label);
    set_conn(true);
  } else {
    crab_error();
    show_crab_msg("Scene failed :(");
    set_conn(false);
  }
  delay(800);
  crab_idle();
}

static void do_send_chat(const char *msg) {
  if (!msg || strlen(msg) == 0) return;
  crab_processing();
  show_crab_msg("Thinking...");

  StaticJsonDocument<256> doc;
  doc["message"] = msg;
  String body;
  serializeJson(doc, body);

  String resp = http_post("/huisje/text", body);
  if (resp.length()) {
    StaticJsonDocument<512> rdoc;
    if (!deserializeJson(rdoc, resp)) {
      const char *reply = rdoc["response"] | "Done!";
      show_crab_msg(reply);
    }
    crab_excited();
    set_conn(true);
  } else {
    crab_error();
    show_crab_msg("Couldn't reach iPinch :(");
    set_conn(false);
  }
  delay(1200);
  crab_idle();
}

static void fetch_status() {
  String resp = http_get("/huisje/status");
  if (!resp.length()) { set_conn(false); return; }

  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, resp)) { set_conn(false); return; }

  set_conn(true);

  // Devices
  JsonObject devs = doc["devices"];
  for (int i = 0; i < 6; i++) {
    if (devs.containsKey(devices[i].id)) {
      bool on = strcmp(devs[devices[i].id]["state"], "on") == 0;
      devices[i].on = on;
      update_device_card(i);
    }
  }

  // Scenes
  JsonObject sc = doc["scenes"];
  for (int i = 0; i < 4; i++) {
    if (sc.containsKey(scenes[i].id)) {
      scenes[i].active = sc[scenes[i].id]["active"] | false;
      update_scene_btn(i);
    }
  }

  // Info
  JsonObject info = doc["info"];
  if (!info.isNull()) {
    lv_label_set_text(lbl_temp,    info["temperature"] | "--°C");
    lv_label_set_text(lbl_weather, info["weather"]     | "");
    lv_label_set_text(lbl_time_hdr, info["time"]       | "--:--");
  }
}

// ---------------------------------------------------------------------------
// LVGL event callbacks
// ---------------------------------------------------------------------------
static void on_scene_btn(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  do_activate_scene(idx);
}

static void on_device_card(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  do_toggle_device(idx);
}

static void on_crab_tap(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  const char *msgs[] = {
    "*happy pincer clacking*",
    "Scuttling with joy!",
    "Ready to control all the things!",
    "*sideways dance of happiness*",
    "What can I do for you?",
  };
  crab_excited();
  show_crab_msg(msgs[millis() % 5]);
}

static void on_ta_focused(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_FOCUSED) {
    lv_keyboard_set_textarea(kb_chat, ta_chat);
    lv_obj_clear_flag(kb_chat, LV_OBJ_FLAG_HIDDEN);
  }
}

static void on_send_btn(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  const char *msg = lv_textarea_get_text(ta_chat);
  lv_textarea_set_text(ta_chat, "");
  lv_obj_add_flag(kb_chat, LV_OBJ_FLAG_HIDDEN);
  do_send_chat(msg);
}

static void on_kb_ready(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    if (code == LV_EVENT_READY) {
      const char *msg = lv_textarea_get_text(ta_chat);
      lv_textarea_set_text(ta_chat, "");
      lv_obj_add_flag(kb_chat, LV_OBJ_FLAG_HIDDEN);
      do_send_chat(msg);
    } else {
      lv_obj_add_flag(kb_chat, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

// ---------------------------------------------------------------------------
// UI builder
// ---------------------------------------------------------------------------
static lv_obj_t *make_card(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_remove_style_all(card);
  lv_obj_set_size(card, w, h);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_style_bg_color(card,   lv_color_hex(C_CARD),   LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card,     LV_OPA_COVER,           LV_PART_MAIN);
  lv_obj_set_style_radius(card,     10,                     LV_PART_MAIN);
  lv_obj_set_style_border_color(card, lv_color_hex(C_BORDER), LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 1,                    LV_PART_MAIN);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  return card;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                             const lv_font_t *font, uint32_t color,
                             lv_align_t align, int ox, int oy) {
  lv_obj_t *lbl = lv_label_create(parent);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl,  font,                  LV_PART_MAIN);
  lv_obj_set_style_text_color(lbl, lv_color_hex(color),   LV_PART_MAIN);
  lv_obj_align(lbl, align, ox, oy);
  return lbl;
}

static void build_ui() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // ── Header ────────────────────────────────────────────────────────────
  lv_obj_t *hdr = lv_obj_create(scr);
  lv_obj_remove_style_all(hdr);
  lv_obj_set_size(hdr, LCD_H_RES, HDR_H);
  lv_obj_set_pos(hdr, 0, 0);
  lv_obj_set_style_bg_color(hdr, lv_color_hex(C_SURFACE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
  lv_obj_set_style_border_color(hdr, lv_color_hex(C_BORDER), LV_PART_MAIN);
  lv_obj_set_style_border_width(hdr, 1, LV_PART_MAIN);
  lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

  // Crab in header
  crab_init(hdr, 8, 4);
  lv_obj_add_event_cb(crab.cont, on_crab_tap, LV_EVENT_CLICKED, NULL);

  // Title
  make_label(hdr, "iPinch Control Panel",
             &lv_font_montserrat_20, 0xffffff, LV_ALIGN_LEFT_MID, 108, 0);
  make_label(hdr, "Huisje  •  Loch Eck",
             &lv_font_montserrat_14, C_MUTED, LV_ALIGN_LEFT_MID, 108, 13);

  // Status strip (right side of header)
  lbl_temp    = make_label(hdr, "--\xc2\xb0""C", &lv_font_montserrat_20,
                           C_TEXT, LV_ALIGN_RIGHT_MID, -180, 0);
  lbl_weather = make_label(hdr, "",          &lv_font_montserrat_14,
                           C_MUTED, LV_ALIGN_RIGHT_MID, -105, 0);
  lbl_time_hdr= make_label(hdr, "--:--",     &lv_font_montserrat_20,
                           C_TEXT, LV_ALIGN_RIGHT_MID, -30, 0);

  // Connection dot
  dot_conn = lv_obj_create(hdr);
  lv_obj_remove_style_all(dot_conn);
  lv_obj_set_size(dot_conn, 10, 10);
  lv_obj_align(dot_conn, LV_ALIGN_RIGHT_MID, -12, -10);
  lv_obj_set_style_bg_color(dot_conn, lv_color_hex(C_MUTED), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(dot_conn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(dot_conn, LV_RADIUS_CIRCLE, LV_PART_MAIN);

  lbl_conn = make_label(hdr, "Connecting", &lv_font_montserrat_14,
                        C_MUTED, LV_ALIGN_RIGHT_MID, -18, 10);

  // ── Body container ────────────────────────────────────────────────────
  lv_obj_t *body = lv_obj_create(scr);
  lv_obj_remove_style_all(body);
  lv_obj_set_size(body, LCD_H_RES, BODY_H);
  lv_obj_set_pos(body, 0, HDR_H);
  lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

  // Vertical divider
  lv_obj_t *div = lv_obj_create(body);
  lv_obj_remove_style_all(div);
  lv_obj_set_size(div, 1, BODY_H - 16);
  lv_obj_set_pos(div, LEFT_W, 8);
  lv_obj_set_style_bg_color(div, lv_color_hex(C_BORDER), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(div, LV_OPA_COVER, LV_PART_MAIN);

  // ── Left: Scenes ──────────────────────────────────────────────────────
  make_label(body, "SCENES", &lv_font_montserrat_14, C_MUTED,
             LV_ALIGN_TOP_LEFT, 12, 12);

  int sc_x = 10, sc_y = 38, sc_w = LEFT_W - 20, sc_h = 64, sc_gap = 8;
  for (int i = 0; i < 4; i++) {
    scene_btns[i] = make_card(body, sc_x, sc_y + i * (sc_h + sc_gap), sc_w, sc_h);
    lv_obj_add_flag(scene_btns[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scene_btns[i], on_scene_btn, LV_EVENT_CLICKED, (void*)(intptr_t)i);

    // Pressed style
    lv_obj_set_style_bg_color(scene_btns[i], lv_color_hex(0x2d333b),
                              LV_STATE_PRESSED | LV_PART_MAIN);

    lbl_scene_name[i] = make_label(scene_btns[i], scenes[i].label,
                                   &lv_font_montserrat_16, C_TEXT,
                                   LV_ALIGN_CENTER, 0, 0);
  }

  // ── Right: Lighting + Heating ─────────────────────────────────────────
  int rx = LEFT_W + 10;
  int card_w = (RIGHT_W - 30) / 2;   // ~277px
  int card_h = 74;

  // Lighting label
  make_label(body, "LIGHTING", &lv_font_montserrat_14, C_MUTED,
             LV_ALIGN_TOP_LEFT, rx + 2, 12);

  // 2×2 light grid
  int light_y = 38;
  for (int i = 0; i < 4; i++) {
    int col = i % 2, row = i / 2;
    int cx = rx + col * (card_w + 10);
    int cy = light_y + row * (card_h + 8);

    device_cards[i] = make_card(body, cx, cy, card_w, card_h);
    lv_obj_add_flag(device_cards[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(device_cards[i], on_device_card, LV_EVENT_CLICKED,
                        (void*)(intptr_t)i);
    lv_obj_set_style_bg_color(device_cards[i], lv_color_hex(0x2d333b),
                              LV_STATE_PRESSED | LV_PART_MAIN);

    lbl_dev_name[i]  = make_label(device_cards[i], devices[i].label,
                                  &lv_font_montserrat_16, C_TEXT,
                                  LV_ALIGN_CENTER, 0, -10);
    lbl_dev_state[i] = make_label(device_cards[i], "Off",
                                  &lv_font_montserrat_14, C_MUTED,
                                  LV_ALIGN_CENTER, 0, 12);
  }

  // Heating label
  int heat_lbl_y = light_y + 2 * (card_h + 8) + 10;
  make_label(body, "HEATING", &lv_font_montserrat_14, C_MUTED,
             LV_ALIGN_TOP_LEFT, rx + 2, heat_lbl_y);

  // 1×2 heat grid
  int heat_y = heat_lbl_y + 26;
  for (int i = 0; i < 2; i++) {
    int cx = rx + i * (card_w + 10);
    device_cards[4 + i] = make_card(body, cx, heat_y, card_w, card_h);
    lv_obj_add_flag(device_cards[4 + i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(device_cards[4 + i], on_device_card, LV_EVENT_CLICKED,
                        (void*)(intptr_t)(4 + i));
    lv_obj_set_style_bg_color(device_cards[4 + i], lv_color_hex(0x2d333b),
                              LV_STATE_PRESSED | LV_PART_MAIN);

    lbl_dev_name[4 + i]  = make_label(device_cards[4 + i], devices[4 + i].label,
                                      &lv_font_montserrat_16, C_TEXT,
                                      LV_ALIGN_CENTER, 0, -10);
    lbl_dev_state[4 + i] = make_label(device_cards[4 + i], "Off",
                                      &lv_font_montserrat_14, C_MUTED,
                                      LV_ALIGN_CENTER, 0, 12);
  }

  // ── Footer: Chat ──────────────────────────────────────────────────────
  lv_obj_t *ftr = lv_obj_create(scr);
  lv_obj_remove_style_all(ftr);
  lv_obj_set_size(ftr, LCD_H_RES, FTR_H);
  lv_obj_set_pos(ftr, 0, HDR_H + BODY_H);
  lv_obj_set_style_bg_color(ftr, lv_color_hex(C_SURFACE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ftr, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_side(ftr, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
  lv_obj_set_style_border_color(ftr, lv_color_hex(C_BORDER), LV_PART_MAIN);
  lv_obj_set_style_border_width(ftr, 1, LV_PART_MAIN);
  lv_obj_clear_flag(ftr, LV_OBJ_FLAG_SCROLLABLE);

  // Crab message bubble
  lbl_crab_msg = make_label(ftr, "Hey, tap something!",
                            &lv_font_montserrat_14, C_MUTED,
                            LV_ALIGN_TOP_LEFT, 10, 6);

  // Text area
  ta_chat = lv_textarea_create(ftr);
  lv_textarea_set_one_line(ta_chat, true);
  lv_textarea_set_placeholder_text(ta_chat, "Ask iPinch...");
  lv_obj_set_size(ta_chat, 600, 36);
  lv_obj_align(ta_chat, LV_ALIGN_BOTTOM_LEFT, 10, -8);
  lv_obj_set_style_bg_color(ta_chat,     lv_color_hex(C_CARD),   LV_PART_MAIN);
  lv_obj_set_style_text_color(ta_chat,   lv_color_hex(C_TEXT),   LV_PART_MAIN);
  lv_obj_set_style_border_color(ta_chat, lv_color_hex(C_BORDER), LV_PART_MAIN);
  lv_obj_set_style_border_width(ta_chat, 1,                      LV_PART_MAIN);
  lv_obj_set_style_radius(ta_chat, 8,                            LV_PART_MAIN);
  lv_obj_set_style_text_font(ta_chat,    &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_add_event_cb(ta_chat, on_ta_focused, LV_EVENT_FOCUSED, NULL);

  // Send button
  lv_obj_t *btn_send = lv_btn_create(ftr);
  lv_obj_set_size(btn_send, 150, 36);
  lv_obj_align(btn_send, LV_ALIGN_BOTTOM_RIGHT, -10, -8);
  lv_obj_set_style_bg_color(btn_send, lv_color_hex(C_ACCENT), LV_PART_MAIN);
  lv_obj_set_style_radius(btn_send, 8, LV_PART_MAIN);
  lv_obj_add_event_cb(btn_send, on_send_btn, LV_EVENT_CLICKED, NULL);
  lv_obj_t *btn_lbl = lv_label_create(btn_send);
  lv_label_set_text(btn_lbl, "Send to iPinch");
  lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_center(btn_lbl);

  // Keyboard (hidden by default)
  kb_chat = lv_keyboard_create(scr);
  lv_obj_set_size(kb_chat, LCD_H_RES, 220);
  lv_obj_align(kb_chat, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(kb_chat, ta_chat);
  lv_obj_add_flag(kb_chat, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(kb_chat, on_kb_ready, LV_EVENT_ALL, NULL);
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("iPinch Huisje Panel — booting...");

  // GPIO38 low (required on V3.0 before I2C)
  pinMode(38, OUTPUT);
  digitalWrite(38, LOW);

  // PCA9557 GT911 reset — IO0=RST, IO1=INT, IO2-IO7 HIGH
  Wire.begin(19, 20);
  pca_write(0x03, 0x00);
  pca_write(0x01, 0xFC);
  delay(20);
  pca_write(0x01, 0xFD);
  delay(100);
  pca_write(0x03, 0x02);
  Wire.end();
  delay(50);

  // Display + touch
  gfx.init();
  gfx.initDMA();
  gfx.startWrite();
  gfx.fillScreen(TFT_BLACK);

  // Backlight
  ledcAttach(2, 300, 8);
  ledcWrite(2, 255);

  // LVGL
  lv_init();
  size_t buf_sz = sizeof(lv_color_t) * LCD_H_RES * 40;
  buf_a = (lv_color_t *)heap_caps_malloc(buf_sz, MALLOC_CAP_SPIRAM);
  buf_b = (lv_color_t *)heap_caps_malloc(buf_sz, MALLOC_CAP_SPIRAM);
  lv_disp_draw_buf_init(&draw_buf, buf_a, buf_b, LCD_H_RES * 40);

  static lv_disp_drv_t dd;
  lv_disp_drv_init(&dd);
  dd.hor_res  = LCD_H_RES;
  dd.ver_res  = LCD_V_RES;
  dd.flush_cb = my_disp_flush;
  dd.draw_buf = &draw_buf;
  lv_disp_drv_register(&dd);

  static lv_indev_drv_t td;
  lv_indev_drv_init(&td);
  td.type    = LV_INDEV_TYPE_POINTER;
  td.read_cb = my_touch_read;
  lv_indev_drv_register(&td);

  // Build UI
  build_ui();
  crab_idle();
  lv_timer_handler();

  // WiFi
  show_crab_msg("Connecting to WiFi...");
  lv_timer_handler();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(500);
    lv_timer_handler();
    Serial.print(".");
    tries++;
  }
  wifi_ok = (WiFi.status() == WL_CONNECTED);
  Serial.println(wifi_ok ? " OK" : " FAILED");

  if (wifi_ok) {
    show_crab_msg("Fetching status...");
    lv_timer_handler();
    fetch_status();
    crab_excited();
    show_crab_msg("Ready!");
    delay(600);
    crab_idle();
    show_crab_msg("Hey, tap something!");
  } else {
    crab_error();
    show_crab_msg("No WiFi — check secrets.h");
  }

  Serial.println("Setup done.");
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {
  lv_timer_handler();

  // Poll status every 10 seconds
  if (wifi_ok && !fetching && (millis() - last_status_ms > 10000)) {
    last_status_ms = millis();
    fetch_status();
  }

  delay(5);
}
