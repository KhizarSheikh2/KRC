#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <math.h>
#include <string.h>

#include "AM4_UI.h"
#include "DisplayLink.h"
#include "DisplayState.h"

namespace {
// -----------------------------------------------------------------------------
// JC2432W328 display/touch configuration.
// Hardware-host firmware is intentionally untouched; this file is display-only.
// -----------------------------------------------------------------------------
constexpr uint16_t SCREEN_W = 320;
constexpr uint16_t SCREEN_H = 240;
constexpr uint8_t TFT_BACKLIGHT_PIN = 27;
constexpr uint8_t TOUCH_ADDR = 0x15;
constexpr int TOUCH_SDA = 33;
constexpr int TOUCH_SCL = 32;
constexpr int TOUCH_RST = 25;
constexpr bool TOUCH_SWAP_XY = true;
constexpr bool TOUCH_INVERT_X = false;
constexpr bool TOUCH_INVERT_Y = true;
constexpr uint32_t UI_REFRESH_MS = 250UL;
constexpr uint32_t SPLASH_MS = 900UL;

TFT_eSPI tft;

// Compact partial buffer: smooth enough at 40 MHz while keeping ESP32 RAM free.
constexpr size_t LVGL_BUFFER_LINES = 12;
constexpr size_t LVGL_BUFFER_PIXELS = SCREEN_W * LVGL_BUFFER_LINES;
static lv_color_t drawBuffer[LVGL_BUFFER_PIXELS];
static lv_disp_draw_buf_t drawBufferDescriptor;
static lv_disp_drv_t displayDriver;
static lv_indev_drv_t inputDriver;

bool touchDetected = false;
bool mainScreensBuilt = false;
unsigned long bootMs = 0;
unsigned long lastRefreshMs = 0;
unsigned long lastLvTickMs = 0;

lv_obj_t* screenSplash = nullptr;
lv_obj_t* screenOverview = nullptr;
lv_obj_t* screenTemps = nullptr;
lv_obj_t* screenStatus = nullptr;

lv_obj_t* lblHeaderStatus[3] = {nullptr, nullptr, nullptr};
lv_obj_t* lblHeaderTime[3] = {nullptr, nullptr, nullptr};

lv_obj_t* objOverviewStateCard = nullptr;
lv_obj_t* lblOverviewState = nullptr;
lv_obj_t* lblOverviewDetail = nullptr;
lv_obj_t* lblOverviewLink = nullptr;
lv_obj_t* lblSuctionPsi = nullptr;
lv_obj_t* lblSuctionBar = nullptr;
lv_obj_t* lblDischargePsi = nullptr;
lv_obj_t* lblDischargeBar = nullptr;
lv_obj_t* lblCommand = nullptr;

constexpr int TEMP_COUNT = 6;
lv_obj_t* objTempCard[TEMP_COUNT] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
lv_obj_t* lblTempValue[TEMP_COUNT] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
lv_obj_t* lblTempState[TEMP_COUNT] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

lv_obj_t* objHps = nullptr;
lv_obj_t* objLps = nullptr;
lv_obj_t* objOps = nullptr;
lv_obj_t* objRun = nullptr;
lv_obj_t* objAlarm = nullptr;
lv_obj_t* objShutdown = nullptr;
lv_obj_t* objMqtt = nullptr;
lv_obj_t* objInternet = nullptr;

lv_obj_t* lblHps = nullptr;
lv_obj_t* lblLps = nullptr;
lv_obj_t* lblOps = nullptr;
lv_obj_t* lblRun = nullptr;
lv_obj_t* lblAlarm = nullptr;
lv_obj_t* lblShutdown = nullptr;
lv_obj_t* lblMqtt = nullptr;
lv_obj_t* lblInternet = nullptr;
lv_obj_t* lblStatusMessage = nullptr;

// -----------------------------------------------------------------------------
// Approved V4 visual language, polished for AM4:
// graphite background + cyan system accent + safety green/amber/red.
// No gradients or shadows: sharp industrial HMI styling and low redraw cost.
// -----------------------------------------------------------------------------
const lv_color_t C_BG          = lv_color_hex(0x04090D);
const lv_color_t C_PANEL       = lv_color_hex(0x0A141B);
const lv_color_t C_PANEL_ALT   = lv_color_hex(0x0D1B24);
const lv_color_t C_PANEL_HI    = lv_color_hex(0x112631);
const lv_color_t C_BORDER      = lv_color_hex(0x223744);
const lv_color_t C_BORDER_SOFT = lv_color_hex(0x172A34);
const lv_color_t C_TEXT        = lv_color_hex(0xEEF7FA);
const lv_color_t C_MUTED       = lv_color_hex(0x7D939E);
const lv_color_t C_DIM         = lv_color_hex(0x506570);
const lv_color_t C_CYAN        = lv_color_hex(0x21C8DB);
const lv_color_t C_GREEN       = lv_color_hex(0x35D77D);
const lv_color_t C_AMBER       = lv_color_hex(0xFFB84D);
const lv_color_t C_RED         = lv_color_hex(0xFF5E68);
const lv_color_t C_BLUE        = lv_color_hex(0x5AA8FF);
const lv_color_t C_VIOLET      = lv_color_hex(0xB58CFF);
const lv_color_t C_OFF         = lv_color_hex(0x617682);

struct StateAppearance {
  const char* title;
  const char* detail;
  lv_color_t color;
  bool fault;
};

StateAppearance stateAppearance(int status) {
  switch (status) {
    case 0:  return {"STOPPED", "System idle - ready for command", C_OFF, false};
    case 1:  return {"RUNNING", "Compressor output active", C_GREEN, false};
    case 2:  return {"AUTO STOP", "Return setpoint reached", C_BLUE, false};
    case 4:  return {"RESTARTING", "Automatic restart delay active", C_AMBER, false};
    case 5:  return {"RETURN TEMP", "Return temperature protection", C_RED, true};
    case 6:  return {"SUCTION TEMP", "Suction temperature protection", C_RED, true};
    case 7:  return {"DISCHARGE TEMP", "Discharge temperature protection", C_RED, true};
    case 12: return {"LOW PRESSURE", "Low-pressure safety trip", C_RED, true};
    case 13: return {"HIGH PRESSURE", "High-pressure safety trip", C_RED, true};
    case 14: return {"OIL PRESSURE", "Oil-pressure safety trip", C_RED, true};
    case 15: return {"COMP FAIL", "Compressor feedback failure", C_RED, true};
    case 16: return {"SENSOR ERROR", "Temperature sensor fault", C_RED, true};
    case 17: return {"MC STUCK", "Contactor feedback fault", C_RED, true};
    default: return {"UNKNOWN", "Unknown hardware state", C_AMBER, true};
  }
}

void setObjBg(lv_obj_t* obj, lv_color_t color, lv_opa_t opa = LV_OPA_COVER) {
  lv_obj_set_style_bg_color(obj, color, 0);
  lv_obj_set_style_bg_opa(obj, opa, 0);
}

void stylePanel(lv_obj_t* obj, lv_color_t border = C_BORDER, int radius = 8,
                lv_color_t bg = C_PANEL) {
  setObjBg(obj, bg);
  lv_obj_set_style_border_color(obj, border, 0);
  lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_radius(obj, radius, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_style_shadow_width(obj, 0, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void styleDynamicCard(lv_obj_t* obj, lv_color_t accent, bool highlighted) {
  if (!obj) return;
  lv_obj_set_style_border_color(obj, accent, 0);
  lv_obj_set_style_border_opa(obj, highlighted ? LV_OPA_COVER : LV_OPA_50, 0);
  lv_obj_set_style_border_width(obj, highlighted ? 2 : 1, 0);
  lv_obj_set_style_bg_color(obj, highlighted ? C_PANEL_HI : C_PANEL, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, const lv_font_t* font,
                    lv_color_t color, lv_align_t align, int x, int y) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
  lv_obj_align(label, align, x, y);
  return label;
}

void constrainLabel(lv_obj_t* label, int width, lv_text_align_t alignment) {
  if (!label) return;
  lv_obj_set_width(label, width);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(label, alignment, 0);
}

lv_obj_t* makePill(lv_obj_t* parent, const char* text, lv_color_t color,
                   int width, int height) {
  lv_obj_t* pill = lv_obj_create(parent);
  lv_obj_set_size(pill, width, height);
  lv_obj_set_style_radius(pill, height / 2, 0);
  lv_obj_set_style_bg_color(pill, color, 0);
  lv_obj_set_style_bg_opa(pill, LV_OPA_20, 0);
  lv_obj_set_style_border_color(pill, color, 0);
  lv_obj_set_style_border_opa(pill, LV_OPA_70, 0);
  lv_obj_set_style_border_width(pill, 1, 0);
  lv_obj_set_style_pad_all(pill, 0, 0);
  lv_obj_set_style_shadow_width(pill, 0, 0);
  lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
  makeLabel(pill, text, &lv_font_montserrat_10, color, LV_ALIGN_CENTER, 0, 0);
  return pill;
}

void applyButtonStyle(lv_obj_t* button, lv_color_t accent) {
  lv_obj_set_style_bg_color(button, accent, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(button, LV_OPA_20, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(button, accent, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(button, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(button, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_set_style_bg_color(button, accent, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(button, LV_OPA_40, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_width(button, 2, LV_PART_MAIN | LV_STATE_PRESSED);
}

lv_obj_t* makeButton(lv_obj_t* parent, const char* text, lv_color_t accent,
                     int x, int y, int w, int h, lv_event_cb_t callback,
                     const lv_font_t* font = &lv_font_montserrat_12) {
  lv_obj_t* button = lv_btn_create(parent);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, w, h);
  applyButtonStyle(button, accent);
  if (callback != nullptr) {
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);
  }
  makeLabel(button, text, font, accent, LV_ALIGN_CENTER, 0, 0);
  return button;
}

void createHeader(lv_obj_t* screen, const char* title, int pageIndex) {
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_pos(header, 4, 4);
  lv_obj_set_size(header, 312, 32);
  stylePanel(header, C_BORDER, 8, C_PANEL_ALT);

  makeLabel(header, "AM4", &lv_font_montserrat_20, C_TEXT,
            LV_ALIGN_LEFT_MID, 9, 0);

  lv_obj_t* titleLabel = makeLabel(header, title, &lv_font_montserrat_10, C_CYAN,
                                   LV_ALIGN_LEFT_MID, 65, 0);
  constrainLabel(titleLabel, 135, LV_TEXT_ALIGN_LEFT);

  if (pageIndex >= 0 && pageIndex < 3) {
    lblHeaderTime[pageIndex] = makeLabel(header, "--:--", &lv_font_montserrat_10,
                                         C_MUTED, LV_ALIGN_TOP_RIGHT, -8, 2);
    lblHeaderStatus[pageIndex] = makeLabel(header, "OFFLINE", &lv_font_montserrat_10,
                                           C_RED, LV_ALIGN_BOTTOM_RIGHT, -8, -2);
    constrainLabel(lblHeaderStatus[pageIndex], 78, LV_TEXT_ALIGN_RIGHT);
  }
}

void eventOverview(lv_event_t*) { if (screenOverview) lv_scr_load(screenOverview); }
void eventTemps(lv_event_t*) { if (screenTemps) lv_scr_load(screenTemps); }
void eventStatus(lv_event_t*) { if (screenStatus) lv_scr_load(screenStatus); }

void createNav(lv_obj_t* screen, int activeIndex) {
  lv_obj_t* bar = lv_obj_create(screen);
  lv_obj_set_pos(bar, 4, 207);
  lv_obj_set_size(bar, 312, 29);
  stylePanel(bar, C_BORDER_SOFT, 8, C_PANEL_ALT);

  const char* names[3] = {"OVERVIEW", "TEMPS", "STATUS"};
  lv_event_cb_t callbacks[3] = {eventOverview, eventTemps, eventStatus};

  for (int i = 0; i < 3; ++i) {
    const int x = 3 + i * 102;
    lv_obj_t* button = lv_btn_create(bar);
    lv_obj_set_pos(button, x, 2);
    lv_obj_set_size(button, 100, 25);
    const lv_color_t accent = (i == activeIndex) ? C_CYAN : C_DIM;
    lv_obj_set_style_bg_color(button, accent, 0);
    lv_obj_set_style_bg_opa(button, i == activeIndex ? LV_OPA_20 : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(button, accent, 0);
    lv_obj_set_style_border_width(button, i == activeIndex ? 1 : 0, 0);
    lv_obj_set_style_radius(button, 6, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_add_event_cb(button, callbacks[i], LV_EVENT_CLICKED, nullptr);
    makeLabel(button, names[i], &lv_font_montserrat_10, accent,
              LV_ALIGN_CENTER, 0, 0);
  }
}

void eventStart(lv_event_t*) {
  const bool sent = sendAM4StartCommand();
  if (lblCommand) {
    lv_label_set_text(lblCommand, sent ? "START SENT" : "NO LINK");
    lv_obj_set_style_text_color(lblCommand, sent ? C_GREEN : C_RED, 0);
  }
}

void eventStop(lv_event_t*) {
  const bool sent = sendAM4StopCommand();
  if (lblCommand) {
    lv_label_set_text(lblCommand, sent ? "STOP SENT" : "NO LINK");
    lv_obj_set_style_text_color(lblCommand, sent ? C_AMBER : C_RED, 0);
  }
}

void eventReset(lv_event_t*) {
  const bool sent = sendAM4ResetCommand();
  if (lblStatusMessage) {
    lv_label_set_text(lblStatusMessage, sent ? "RESET REQUEST SENT" : "LINK OFFLINE");
    lv_obj_set_style_text_color(lblStatusMessage, sent ? C_AMBER : C_RED, 0);
  }
}

lv_obj_t* createPressureCard(lv_obj_t* parent, const char* title, lv_color_t accent,
                             int x, lv_obj_t** psiLabel, lv_obj_t** barLabel) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_set_pos(card, x, 90);
  lv_obj_set_size(card, 153, 63);
  stylePanel(card, accent, 9, C_PANEL);
  lv_obj_set_style_border_opa(card, LV_OPA_60, 0);

  makeLabel(card, title, &lv_font_montserrat_10, C_MUTED,
            LV_ALIGN_TOP_LEFT, 9, 6);

  *psiLabel = makeLabel(card, "--- PSI", &lv_font_montserrat_24, accent,
                        LV_ALIGN_LEFT_MID, 9, 3);
  constrainLabel(*psiLabel, 137, LV_TEXT_ALIGN_LEFT);

  *barLabel = makeLabel(card, "--.- bar", &lv_font_montserrat_10, C_MUTED,
                        LV_ALIGN_BOTTOM_RIGHT, -9, -5);
  constrainLabel(*barLabel, 78, LV_TEXT_ALIGN_RIGHT);
  return card;
}

void buildOverviewScreen() {
  screenOverview = lv_obj_create(nullptr);
  setObjBg(screenOverview, C_BG);
  lv_obj_clear_flag(screenOverview, LV_OBJ_FLAG_SCROLLABLE);
  createHeader(screenOverview, "SYSTEM OVERVIEW", 0);

  objOverviewStateCard = lv_obj_create(screenOverview);
  lv_obj_set_pos(objOverviewStateCard, 4, 40);
  lv_obj_set_size(objOverviewStateCard, 312, 45);
  stylePanel(objOverviewStateCard, C_AMBER, 9, C_PANEL_ALT);

  lblOverviewState = makeLabel(objOverviewStateCard, "WAITING", &lv_font_montserrat_16,
                               C_AMBER, LV_ALIGN_LEFT_MID, 10, -6);
  constrainLabel(lblOverviewState, 169, LV_TEXT_ALIGN_LEFT);

  lblOverviewDetail = makeLabel(objOverviewStateCard, "Waiting for AM4 hardware",
                                &lv_font_montserrat_10, C_MUTED,
                                LV_ALIGN_BOTTOM_LEFT, 10, -5);
  constrainLabel(lblOverviewDetail, 202, LV_TEXT_ALIGN_LEFT);

  lblOverviewLink = makeLabel(objOverviewStateCard, "LINK OFFLINE", &lv_font_montserrat_10,
                              C_RED, LV_ALIGN_RIGHT_MID, -10, 1);
  constrainLabel(lblOverviewLink, 94, LV_TEXT_ALIGN_RIGHT);

  createPressureCard(screenOverview, "SUCTION PRESSURE", C_CYAN, 4,
                     &lblSuctionPsi, &lblSuctionBar);
  createPressureCard(screenOverview, "DISCHARGE PRESSURE", C_AMBER, 163,
                     &lblDischargePsi, &lblDischargeBar);

  makeButton(screenOverview, "START", C_GREEN, 4, 158, 98, 43, eventStart,
             &lv_font_montserrat_14);
  makeButton(screenOverview, "STOP", C_RED, 108, 158, 98, 43, eventStop,
             &lv_font_montserrat_14);

  lv_obj_t* commandPanel = lv_obj_create(screenOverview);
  lv_obj_set_pos(commandPanel, 212, 158);
  lv_obj_set_size(commandPanel, 104, 43);
  stylePanel(commandPanel, C_BORDER, 8, C_PANEL_ALT);
  makeLabel(commandPanel, "CONTROL", &lv_font_montserrat_10, C_DIM,
            LV_ALIGN_TOP_MID, 0, 5);
  lblCommand = makeLabel(commandPanel, "READY", &lv_font_montserrat_10, C_MUTED,
                         LV_ALIGN_BOTTOM_MID, 0, -6);
  constrainLabel(lblCommand, 94, LV_TEXT_ALIGN_CENTER);

  createNav(screenOverview, 0);
}

void createTempCard(lv_obj_t* parent, int index, const char* name, lv_color_t accent,
                    int x, int y, int w, int h) {
  lv_obj_t* card = lv_obj_create(parent);
  objTempCard[index] = card;
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  stylePanel(card, accent, 8, C_PANEL);
  lv_obj_set_style_border_opa(card, LV_OPA_50, 0);

  makeLabel(card, name, &lv_font_montserrat_10, C_MUTED,
            LV_ALIGN_TOP_LEFT, 8, 5);
  lblTempValue[index] = makeLabel(card, "---.- C", &lv_font_montserrat_16, accent,
                                  LV_ALIGN_BOTTOM_LEFT, 8, -6);
  constrainLabel(lblTempValue[index], w - 16, LV_TEXT_ALIGN_LEFT);
  lblTempState[index] = makeLabel(card, "WAIT", &lv_font_montserrat_10, C_MUTED,
                                  LV_ALIGN_TOP_RIGHT, -7, 5);
  constrainLabel(lblTempState[index], 53, LV_TEXT_ALIGN_RIGHT);
}

void buildTempScreen() {
  screenTemps = lv_obj_create(nullptr);
  setObjBg(screenTemps, C_BG);
  lv_obj_clear_flag(screenTemps, LV_OBJ_FLAG_SCROLLABLE);
  createHeader(screenTemps, "TEMPERATURES", 1);

  createTempCard(screenTemps, 0, "RETURN", C_BLUE, 4, 40, 153, 48);
  createTempCard(screenTemps, 1, "SUCTION", C_CYAN, 163, 40, 153, 48);
  createTempCard(screenTemps, 2, "DISCHARGE", C_RED, 4, 93, 153, 48);
  createTempCard(screenTemps, 3, "SUPPLY", C_GREEN, 163, 93, 153, 48);
  createTempCard(screenTemps, 4, "OIL", C_AMBER, 4, 146, 153, 55);
  createTempCard(screenTemps, 5, "OTHER SENSOR", C_VIOLET, 163, 146, 153, 55);

  createNav(screenTemps, 1);
}

lv_obj_t* createStatusCell(lv_obj_t* parent, const char* title, int x, int y,
                           int w, int h, lv_obj_t** value) {
  lv_obj_t* cell = lv_obj_create(parent);
  lv_obj_set_pos(cell, x, y);
  lv_obj_set_size(cell, w, h);
  stylePanel(cell, C_BORDER, 8, C_PANEL);
  makeLabel(cell, title, &lv_font_montserrat_10, C_MUTED,
            LV_ALIGN_TOP_LEFT, 7, 5);
  *value = makeLabel(cell, "--", &lv_font_montserrat_14, C_TEXT,
                     LV_ALIGN_BOTTOM_LEFT, 7, -6);
  constrainLabel(*value, w - 14, LV_TEXT_ALIGN_LEFT);
  return cell;
}

void buildStatusScreen() {
  screenStatus = lv_obj_create(nullptr);
  setObjBg(screenStatus, C_BG);
  lv_obj_clear_flag(screenStatus, LV_OBJ_FLAG_SCROLLABLE);
  createHeader(screenStatus, "STATUS / SAFETIES", 2);

  objHps = createStatusCell(screenStatus, "HIGH PRESS", 4, 40, 100, 47, &lblHps);
  objLps = createStatusCell(screenStatus, "LOW PRESS", 110, 40, 100, 47, &lblLps);
  objOps = createStatusCell(screenStatus, "OIL PRESS", 216, 40, 100, 47, &lblOps);

  objRun = createStatusCell(screenStatus, "RUN OUTPUT", 4, 92, 100, 47, &lblRun);
  objAlarm = createStatusCell(screenStatus, "ALARM", 110, 92, 100, 47, &lblAlarm);
  objShutdown = createStatusCell(screenStatus, "SHUTDOWN", 216, 92, 100, 47, &lblShutdown);

  objMqtt = createStatusCell(screenStatus, "MQTT", 4, 144, 100, 47, &lblMqtt);
  objInternet = createStatusCell(screenStatus, "INTERNET", 110, 144, 100, 47, &lblInternet);
  makeButton(screenStatus, "RESET", C_AMBER, 216, 144, 100, 47, eventReset,
             &lv_font_montserrat_14);

  lblStatusMessage = makeLabel(screenStatus, "SAFETY STATE FROM HARDWARE",
                               &lv_font_montserrat_10, C_MUTED,
                               LV_ALIGN_TOP_LEFT, 6, 194);
  constrainLabel(lblStatusMessage, 308, LV_TEXT_ALIGN_LEFT);

  createNav(screenStatus, 2);
}

void buildMainScreens() {
  // Memory-conscious screen construction: load the first page before allocating the rest.
  buildOverviewScreen();
  lv_scr_load(screenOverview);
  if (screenSplash != nullptr) {
    lv_obj_del(screenSplash);
    screenSplash = nullptr;
  }
  buildTempScreen();
  buildStatusScreen();
  mainScreensBuilt = true;
}

void buildSplash() {
  screenSplash = lv_obj_create(nullptr);
  setObjBg(screenSplash, C_BG);
  lv_obj_clear_flag(screenSplash, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* frame = lv_obj_create(screenSplash);
  lv_obj_set_pos(frame, 18, 28);
  lv_obj_set_size(frame, 284, 184);
  stylePanel(frame, C_CYAN, 14, C_PANEL_ALT);
  lv_obj_set_style_border_width(frame, 2, 0);
  lv_obj_set_style_border_opa(frame, LV_OPA_70, 0);

  makeLabel(frame, "AM4", &lv_font_montserrat_28, C_TEXT,
            LV_ALIGN_TOP_MID, 0, 22);
  makeLabel(frame, "INDUSTRIAL CONTROL HMI", &lv_font_montserrat_12, C_CYAN,
            LV_ALIGN_CENTER, 0, -8);
  makeLabel(frame, "HVAC PROTECTION  /  MONITORING", &lv_font_montserrat_10, C_MUTED,
            LV_ALIGN_CENTER, 0, 17);

  lv_obj_t* initPill = makePill(frame, "SYSTEM INITIALIZING", C_GREEN, 142, 24);
  lv_obj_align(initPill, LV_ALIGN_BOTTOM_MID, 0, -20);

  makeLabel(screenSplash, "JC2432W328  |  TFT_eSPI + LVGL", &lv_font_montserrat_10,
            C_DIM, LV_ALIGN_BOTTOM_MID, 0, -8);

  lv_scr_load(screenSplash);
}

// -----------------------------------------------------------------------------
// TFT/LVGL + CST820 touch
// -----------------------------------------------------------------------------
void flushDisplay(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color) {
  const uint32_t width = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  const uint32_t height = static_cast<uint32_t>(area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, width, height);
  tft.pushColors(reinterpret_cast<uint16_t*>(&color->full), width * height, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

bool touchReadRegister(uint8_t reg, uint8_t& value) {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;

  const uint8_t received = Wire.requestFrom(TOUCH_ADDR, static_cast<uint8_t>(1),
                                            static_cast<uint8_t>(true));
  if (received != 1 || !Wire.available()) return false;
  value = static_cast<uint8_t>(Wire.read());
  return true;
}

bool touchWriteRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission(true) == 0;
}

bool initTouchController() {
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);
  delay(12);
  digitalWrite(TOUCH_RST, HIGH);
  delay(120);

  Wire.begin(TOUCH_SDA, TOUCH_SCL, 400000UL);
  Wire.setTimeOut(20);
  Wire.beginTransmission(TOUCH_ADDR);
  const bool ok = Wire.endTransmission() == 0;

  Serial.printf("[TOUCH] CST820 at 0x%02X: %s\n", TOUCH_ADDR,
                ok ? "detected" : "not detected");
  if (ok) {
    uint8_t chipId = 0;
    if (touchReadRegister(0xA7, chipId)) {
      Serial.printf("[TOUCH] Chip ID: 0x%02X\n", chipId);
    }
    touchWriteRegister(0xFE, 0xFF);  // Keep controller awake while polling.
  }
  return ok;
}

bool readTouchPortrait(uint16_t& rawX, uint16_t& rawY) {
  if (!touchDetected) return false;

  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(static_cast<uint8_t>(0x02));
  if (Wire.endTransmission(false) != 0) return false;

  constexpr uint8_t NEEDED = 5;
  const uint8_t received = Wire.requestFrom(TOUCH_ADDR, NEEDED,
                                            static_cast<uint8_t>(true));
  if (received != NEEDED) {
    while (Wire.available()) Wire.read();
    return false;
  }

  const uint8_t fingers = static_cast<uint8_t>(Wire.read());
  const uint8_t xh = static_cast<uint8_t>(Wire.read());
  const uint8_t xl = static_cast<uint8_t>(Wire.read());
  const uint8_t yh = static_cast<uint8_t>(Wire.read());
  const uint8_t yl = static_cast<uint8_t>(Wire.read());
  if ((fingers & 0x0F) == 0) return false;

  rawX = static_cast<uint16_t>(((xh & 0x0F) << 8) | xl);
  rawY = static_cast<uint16_t>(((yh & 0x0F) << 8) | yl);
  return true;
}

void readTouch(lv_indev_drv_t*, lv_indev_data_t* data) {
  uint16_t rawX = 0;
  uint16_t rawY = 0;
  if (!readTouchPortrait(rawX, rawY)) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  int32_t x = rawX;
  int32_t y = rawY;

  if (TOUCH_SWAP_XY) {
    const int32_t tmp = x;
    x = y;
    y = tmp;
  }
  if (TOUCH_INVERT_X) x = (SCREEN_W - 1) - x;
  if (TOUCH_INVERT_Y) y = (SCREEN_H - 1) - y;

  if (x < 0) x = 0;
  if (x >= static_cast<int32_t>(SCREEN_W)) x = static_cast<int32_t>(SCREEN_W) - 1;
  if (y < 0) y = 0;
  if (y >= static_cast<int32_t>(SCREEN_H)) y = static_cast<int32_t>(SCREEN_H) - 1;

  data->point.x = static_cast<lv_coord_t>(x);
  data->point.y = static_cast<lv_coord_t>(y);
  data->state = LV_INDEV_STATE_PRESSED;
}

// -----------------------------------------------------------------------------
// Dynamic state formatting / refresh
// -----------------------------------------------------------------------------
String pressureText(const String& value, const char* suffix, uint8_t decimals) {
  if (value.length() == 0 || value == "999") return String("-- ") + suffix;
  if (value == "888") return String("ERR ") + suffix;

  const float parsed = value.toFloat();
  if (!isfinite(parsed)) return String("-- ") + suffix;
  return String(parsed, static_cast<unsigned int>(decimals)) + " " + suffix;
}

const char* temperatureState(float value) {
  if (fabsf(value - SENSOR_NOT_SELECTED) < 0.01f) return "NOT SET";
  if (fabsf(value - SENSOR_DISCONNECTED) < 0.01f) return "FAULT";
  if (!isfinite(value) || value < -55.0f || value > 125.0f) return "FAULT";
  return "LIVE";
}

String temperatureText(float value) {
  const char* state = temperatureState(value);
  if (strcmp(state, "LIVE") != 0) return "---.- C";
  return String(value, 1U) + " C";
}

float temperatureValue(int index) {
  switch (index) {
    case 0: return ReturnTempC;
    case 1: return SuctionTempC;
    case 2: return dischargeTempC;
    case 3: return SupplyTempC;
    case 4: return OilTempC;
    case 5: return OtherTempC;
    default: return SENSOR_NOT_SELECTED;
  }
}

void setBinaryLabel(lv_obj_t* label, lv_obj_t* card, bool active,
                    const char* onText, const char* offText,
                    lv_color_t onColor = C_GREEN, lv_color_t offColor = C_OFF,
                    bool activeIsWarning = false) {
  if (!label) return;
  lv_label_set_text(label, active ? onText : offText);
  const lv_color_t color = active ? onColor : offColor;
  lv_obj_set_style_text_color(label, color, 0);

  if (card) {
    const bool highlighted = activeIsWarning ? active : !active;
    const lv_color_t cardColor = highlighted ? color : C_BORDER;
    styleDynamicCard(card, cardColor, highlighted);
  }
}

void refreshHeader() {
  char timeText[10] = "--:--";
  struct tm timeInfo;
  if (am4GetRemotePakistanTime(timeInfo)) {
    strftime(timeText, sizeof(timeText), "%H:%M", &timeInfo);
  }

  const char* statusText = "OFFLINE";
  lv_color_t statusColor = C_RED;
  if (am4DisplayLinkConnected) {
    statusText = "LINK LIVE";
    statusColor = C_GREEN;
  } else if (WiFi.status() == WL_CONNECTED) {
    statusText = "SYNCING";
    statusColor = C_AMBER;
  }

  for (int i = 0; i < 3; ++i) {
    if (lblHeaderStatus[i]) {
      lv_label_set_text(lblHeaderStatus[i], statusText);
      lv_obj_set_style_text_color(lblHeaderStatus[i], statusColor, 0);
    }
    if (lblHeaderTime[i]) {
      lv_label_set_text(lblHeaderTime[i], timeText);
    }
  }
}

void refreshOverview() {
  if (!lblOverviewState) return;

  const StateAppearance state = stateAppearance(remoteCompStatus);
  lv_label_set_text(lblOverviewState, state.title);
  lv_obj_set_style_text_color(lblOverviewState, state.color, 0);
  lv_label_set_text(lblOverviewDetail, state.detail);
  lv_obj_set_style_text_color(lblOverviewDetail, state.fault ? C_RED : C_MUTED, 0);
  styleDynamicCard(objOverviewStateCard, state.color,
                   state.fault || remoteCompStatus == 1);

  if (am4DisplayLinkConnected) {
    String link = remoteDeviceName;
    if (link.length() == 0) link = "AM4";
    link += remoteInternetConnected ? "  NET" : "  LOCAL";
    lv_label_set_text(lblOverviewLink, link.c_str());
    lv_obj_set_style_text_color(lblOverviewLink,
                                remoteInternetConnected ? C_GREEN : C_AMBER, 0);
  } else {
    lv_label_set_text(lblOverviewLink, "LINK OFFLINE");
    lv_obj_set_style_text_color(lblOverviewLink, C_RED, 0);
  }

  const String sPsi = pressureText(suctionPressure, "PSI", 1);
  const String sBar = pressureText(suctionPressureB, "bar", 1);
  const String dPsi = pressureText(dischargePressure, "PSI", 0);
  const String dBar = pressureText(dischargePressureB, "bar", 1);
  lv_label_set_text(lblSuctionPsi, sPsi.c_str());
  lv_label_set_text(lblSuctionBar, sBar.c_str());
  lv_label_set_text(lblDischargePsi, dPsi.c_str());
  lv_label_set_text(lblDischargeBar, dBar.c_str());

  if (am4LastAckMs != 0 && millis() - am4LastAckMs < 5000UL) {
    lv_label_set_text(lblCommand, am4LastAckAccepted ? "ACCEPTED" : "REJECTED");
    lv_obj_set_style_text_color(lblCommand, am4LastAckAccepted ? C_GREEN : C_RED, 0);
  } else if (!am4DisplayLinkConnected) {
    lv_label_set_text(lblCommand, "NO LINK");
    lv_obj_set_style_text_color(lblCommand, C_RED, 0);
  } else {
    lv_label_set_text(lblCommand, "READY");
    lv_obj_set_style_text_color(lblCommand, C_CYAN, 0);
  }
}

void refreshTemperatures() {
  const lv_color_t liveColors[TEMP_COUNT] = {
    C_BLUE, C_CYAN, C_RED, C_GREEN, C_AMBER, C_VIOLET
  };

  for (int i = 0; i < TEMP_COUNT; ++i) {
    if (!lblTempValue[i] || !lblTempState[i]) continue;
    const float value = temperatureValue(i);
    const char* state = temperatureState(value);
    const String text = temperatureText(value);

    lv_label_set_text(lblTempValue[i], text.c_str());
    lv_label_set_text(lblTempState[i], state);

    if (strcmp(state, "LIVE") == 0) {
      lv_obj_set_style_text_color(lblTempValue[i], liveColors[i], 0);
      lv_obj_set_style_text_color(lblTempState[i], C_GREEN, 0);
      styleDynamicCard(objTempCard[i], liveColors[i], false);
    } else if (strcmp(state, "NOT SET") == 0) {
      lv_obj_set_style_text_color(lblTempValue[i], C_OFF, 0);
      lv_obj_set_style_text_color(lblTempState[i], C_AMBER, 0);
      styleDynamicCard(objTempCard[i], C_AMBER, false);
    } else {
      lv_obj_set_style_text_color(lblTempValue[i], C_RED, 0);
      lv_obj_set_style_text_color(lblTempState[i], C_RED, 0);
      styleDynamicCard(objTempCard[i], C_RED, true);
    }
  }
}

void setPressureSwitch(lv_obj_t* label, lv_obj_t* card, const String& value,
                       const char* normalValue) {
  if (!label) return;
  const bool hasValue = value.length() > 0;
  const bool normal = hasValue && value.equalsIgnoreCase(normalValue);
  lv_label_set_text(label, hasValue ? value.c_str() : "--");
  lv_obj_set_style_text_color(label, normal ? C_GREEN : (hasValue ? C_RED : C_OFF), 0);

  if (card) {
    if (!hasValue) {
      styleDynamicCard(card, C_BORDER, false);
    } else {
      styleDynamicCard(card, normal ? C_GREEN : C_RED, !normal);
    }
  }
}

void refreshStatus() {
  setPressureSwitch(lblHps, objHps, remoteHps, "LOW");
  setPressureSwitch(lblLps, objLps, remoteLps, "HIGH");
  setPressureSwitch(lblOps, objOps, remoteOps, "HIGH");

  setBinaryLabel(lblRun, objRun, remoteRunOutput != 0, "ON", "OFF",
                 C_GREEN, C_OFF, false);
  setBinaryLabel(lblAlarm, objAlarm, remoteAlarmOutput != 0, "ACTIVE", "CLEAR",
                 C_RED, C_GREEN, true);
  setBinaryLabel(lblShutdown, objShutdown, remoteMachineShutdown != 0, "YES", "NO",
                 C_RED, C_GREEN, true);
  setBinaryLabel(lblMqtt, objMqtt, remoteMqttConnected != 0, "ONLINE", "OFFLINE",
                 C_GREEN, C_RED, false);
  setBinaryLabel(lblInternet, objInternet, remoteInternetConnected != 0,
                 "ONLINE", "OFFLINE", C_GREEN, C_RED, false);

  if (am4LastAckMs != 0 && millis() - am4LastAckMs < 5000UL) {
    const char* message = am4LastAckMessage.length() ? am4LastAckMessage.c_str()
                                                     : (am4LastAckAccepted ? "COMMAND ACCEPTED" : "COMMAND REJECTED");
    lv_label_set_text(lblStatusMessage, message);
    lv_obj_set_style_text_color(lblStatusMessage,
                                am4LastAckAccepted ? C_GREEN : C_RED, 0);
  } else {
    const StateAppearance state = stateAppearance(remoteCompStatus);
    lv_label_set_text(lblStatusMessage, state.detail);
    lv_obj_set_style_text_color(lblStatusMessage, state.fault ? C_RED : C_MUTED, 0);
  }
}

void refreshUi() {
  refreshHeader();
  refreshOverview();
  refreshTemperatures();
  refreshStatus();
}
}  // namespace

void initAM4UI() {
  pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(TFT_BACKLIGHT_PIN, LOW);

  // TOUCH_CS=5 only suppresses TFT_eSPI's resistive-touch warning. The actual
  // JC2432W328 touch controller is capacitive over I2C, so keep GPIO5 deselected.
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  touchDetected = initTouchController();

  lv_init();
  lv_disp_draw_buf_init(&drawBufferDescriptor, drawBuffer, nullptr,
                        LVGL_BUFFER_PIXELS);

  lv_disp_drv_init(&displayDriver);
  displayDriver.hor_res = SCREEN_W;
  displayDriver.ver_res = SCREEN_H;
  displayDriver.flush_cb = flushDisplay;
  displayDriver.draw_buf = &drawBufferDescriptor;
  lv_disp_drv_register(&displayDriver);

  lv_indev_drv_init(&inputDriver);
  inputDriver.type = LV_INDEV_TYPE_POINTER;
  inputDriver.read_cb = readTouch;
  lv_indev_drv_register(&inputDriver);

  buildSplash();
  lv_timer_handler();
  digitalWrite(TFT_BACKLIGHT_PIN, HIGH);

  bootMs = millis();
  lastLvTickMs = bootMs;
  lastRefreshMs = 0;
  Serial.println("[DISPLAY] AM4 JC2432W328 enhanced LVGL UI ready");
}

void updateAM4UI() {
  const unsigned long now = millis();
  const uint32_t elapsed = now - lastLvTickMs;
  if (elapsed > 0) {
    lv_tick_inc(elapsed);
    lastLvTickMs = now;
  }

  if (!mainScreensBuilt && now - bootMs >= SPLASH_MS) {
    buildMainScreens();
    refreshUi();
  }

  if (mainScreensBuilt && now - lastRefreshMs >= UI_REFRESH_MS) {
    lastRefreshMs = now;
    refreshUi();
  }

  lv_timer_handler();
}
