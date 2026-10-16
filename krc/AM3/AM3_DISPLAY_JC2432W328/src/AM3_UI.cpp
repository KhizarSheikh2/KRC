#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <math.h>
#include <string.h>

#include "AM3_UI.h"
#include "BoardConfig.h"
#include "CST820Touch.h"
#include "DisplayLink.h"
#include "DisplayState.h"

namespace {
TFT_eSPI tft;
CST820Touch touch(AM3_TOUCH_SDA, AM3_TOUCH_SCL, AM3_TOUCH_RST, AM3_TOUCH_INT);

// 12 scanlines keeps the LVGL DMA-style transfer buffer compact enough for the
// classic ESP32 while still giving smooth 40 MHz SPI screen updates.
constexpr size_t LVGL_BUFFER_LINES = 12;
constexpr size_t LVGL_BUFFER_PIXELS = AM3_SCREEN_WIDTH * LVGL_BUFFER_LINES;
static lv_color_t drawBuffer[LVGL_BUFFER_PIXELS];
static lv_disp_draw_buf_t drawBufferDescriptor;
static lv_disp_drv_t displayDriver;
static lv_indev_drv_t inputDriver;

lv_obj_t* screenSplash = nullptr;
lv_obj_t* screenHome = nullptr;
lv_obj_t* screenTemps = nullptr;
lv_obj_t* screenDiag = nullptr;

lv_obj_t* lblHeaderStatus[3] = {nullptr, nullptr, nullptr};
lv_obj_t* lblHeaderTime[3] = {nullptr, nullptr, nullptr};

lv_obj_t* objHomeStateCard = nullptr;
lv_obj_t* lblHomeState = nullptr;
lv_obj_t* lblHomeDetail = nullptr;
lv_obj_t* lblHomeLink = nullptr;
lv_obj_t* lblSuctionPsi = nullptr;
lv_obj_t* lblSuctionBar = nullptr;
lv_obj_t* lblDischargePsi = nullptr;
lv_obj_t* lblDischargeBar = nullptr;
lv_obj_t* lblCommand = nullptr;

lv_obj_t* objTempCard[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
lv_obj_t* lblTempValue[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
lv_obj_t* lblTempState[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
lv_obj_t* objTempSummary = nullptr;
lv_obj_t* lblTempSummaryMain = nullptr;
lv_obj_t* lblTempSummarySub = nullptr;

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
lv_obj_t* lblMqtt = nullptr;
lv_obj_t* lblInternet = nullptr;
lv_obj_t* lblShutdown = nullptr;
lv_obj_t* lblDiagMessage = nullptr;

unsigned long bootMs = 0;
unsigned long lastRefreshMs = 0;
unsigned long lastLvTickMs = 0;
bool mainScreensBuilt = false;

// ---------------------------------------------------------------------------
// Dark industrial palette: near-black graphite + cool cyan + safety colors.
// The palette deliberately avoids gradients/shadows to keep the classic ESP32
// fast and deterministic while still giving the 2.8-inch panel strong depth.
// ---------------------------------------------------------------------------
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

  makeLabel(header, "AM3", &lv_font_montserrat_20, C_TEXT,
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

void eventHome(lv_event_t*) { if (screenHome) lv_scr_load(screenHome); }
void eventTemps(lv_event_t*) { if (screenTemps) lv_scr_load(screenTemps); }
void eventDiag(lv_event_t*) { if (screenDiag) lv_scr_load(screenDiag); }

void createNav(lv_obj_t* screen, int activeIndex) {
  lv_obj_t* bar = lv_obj_create(screen);
  lv_obj_set_pos(bar, 4, 207);
  lv_obj_set_size(bar, 312, 29);
  stylePanel(bar, C_BORDER_SOFT, 8, C_PANEL_ALT);

  const char* names[3] = {"OVERVIEW", "TEMPS", "STATUS"};
  lv_event_cb_t callbacks[3] = {eventHome, eventTemps, eventDiag};

  for (int i = 0; i < 3; ++i) {
    const int x = 3 + i * 102;
    lv_obj_t* btn = lv_btn_create(bar);
    lv_obj_set_pos(btn, x, 2);
    lv_obj_set_size(btn, 100, 25);
    const lv_color_t accent = (i == activeIndex) ? C_CYAN : C_DIM;
    lv_obj_set_style_bg_color(btn, accent, 0);
    lv_obj_set_style_bg_opa(btn, i == activeIndex ? LV_OPA_20 : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(btn, accent, 0);
    lv_obj_set_style_border_width(btn, i == activeIndex ? 1 : 0, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, callbacks[i], LV_EVENT_CLICKED, nullptr);
    makeLabel(btn, names[i], &lv_font_montserrat_10, accent,
              LV_ALIGN_CENTER, 0, 0);
  }
}

void eventStart(lv_event_t*) {
  const bool sent = sendAM3StartCommand();
  if (lblCommand) {
    lv_label_set_text(lblCommand, sent ? "START SENT" : "NO LINK");
    lv_obj_set_style_text_color(lblCommand, sent ? C_GREEN : C_RED, 0);
  }
}

void eventStop(lv_event_t*) {
  const bool sent = sendAM3StopCommand();
  if (lblCommand) {
    lv_label_set_text(lblCommand, sent ? "STOP SENT" : "NO LINK");
    lv_obj_set_style_text_color(lblCommand, sent ? C_AMBER : C_RED, 0);
  }
}

void eventReset(lv_event_t*) {
  const bool sent = sendAM3ResetCommand();
  if (lblDiagMessage) {
    lv_label_set_text(lblDiagMessage, sent ? "RESET REQUEST SENT" : "LINK OFFLINE");
    lv_obj_set_style_text_color(lblDiagMessage, sent ? C_AMBER : C_RED, 0);
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

void buildHomeScreen() {
  screenHome = lv_obj_create(nullptr);
  setObjBg(screenHome, C_BG);
  lv_obj_clear_flag(screenHome, LV_OBJ_FLAG_SCROLLABLE);
  createHeader(screenHome, "SYSTEM OVERVIEW", 0);

  objHomeStateCard = lv_obj_create(screenHome);
  lv_obj_set_pos(objHomeStateCard, 4, 40);
  lv_obj_set_size(objHomeStateCard, 312, 45);
  stylePanel(objHomeStateCard, C_AMBER, 9, C_PANEL_ALT);

  lblHomeState = makeLabel(objHomeStateCard, "WAITING", &lv_font_montserrat_16,
                           C_AMBER, LV_ALIGN_LEFT_MID, 10, -6);
  constrainLabel(lblHomeState, 169, LV_TEXT_ALIGN_LEFT);

  lblHomeDetail = makeLabel(objHomeStateCard, "Waiting for AM3 hardware",
                            &lv_font_montserrat_10, C_MUTED,
                            LV_ALIGN_BOTTOM_LEFT, 10, -5);
  constrainLabel(lblHomeDetail, 202, LV_TEXT_ALIGN_LEFT);

  lblHomeLink = makeLabel(objHomeStateCard, "LINK OFFLINE", &lv_font_montserrat_10,
                          C_RED, LV_ALIGN_RIGHT_MID, -10, 1);
  constrainLabel(lblHomeLink, 94, LV_TEXT_ALIGN_RIGHT);

  createPressureCard(screenHome, "SUCTION PRESSURE", C_CYAN, 4,
                     &lblSuctionPsi, &lblSuctionBar);
  createPressureCard(screenHome, "DISCHARGE PRESSURE", C_AMBER, 163,
                     &lblDischargePsi, &lblDischargeBar);

  makeButton(screenHome, "START", C_GREEN, 4, 158, 98, 43, eventStart,
             &lv_font_montserrat_14);
  makeButton(screenHome, "STOP", C_RED, 108, 158, 98, 43, eventStop,
             &lv_font_montserrat_14);

  lv_obj_t* commandPanel = lv_obj_create(screenHome);
  lv_obj_set_pos(commandPanel, 212, 158);
  lv_obj_set_size(commandPanel, 104, 43);
  stylePanel(commandPanel, C_BORDER, 8, C_PANEL_ALT);
  makeLabel(commandPanel, "CONTROL", &lv_font_montserrat_10, C_DIM,
            LV_ALIGN_TOP_MID, 0, 5);
  lblCommand = makeLabel(commandPanel, "READY", &lv_font_montserrat_10, C_MUTED,
                         LV_ALIGN_BOTTOM_MID, 0, -6);
  constrainLabel(lblCommand, 94, LV_TEXT_ALIGN_CENTER);

  createNav(screenHome, 0);
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

  objTempSummary = lv_obj_create(screenTemps);
  lv_obj_set_pos(objTempSummary, 163, 146);
  lv_obj_set_size(objTempSummary, 153, 55);
  stylePanel(objTempSummary, C_BORDER, 8, C_PANEL_ALT);
  makeLabel(objTempSummary, "SENSOR HEALTH", &lv_font_montserrat_10, C_MUTED,
            LV_ALIGN_TOP_LEFT, 8, 5);
  lblTempSummaryMain = makeLabel(objTempSummary, "0 / 5 LIVE", &lv_font_montserrat_16,
                                 C_AMBER, LV_ALIGN_LEFT_MID, 8, 5);
  constrainLabel(lblTempSummaryMain, 136, LV_TEXT_ALIGN_LEFT);
  lblTempSummarySub = makeLabel(objTempSummary, "Waiting for data", &lv_font_montserrat_10,
                                C_DIM, LV_ALIGN_BOTTOM_LEFT, 8, -5);
  constrainLabel(lblTempSummarySub, 136, LV_TEXT_ALIGN_LEFT);

  createNav(screenTemps, 1);
}

lv_obj_t* createDiagCell(lv_obj_t* parent, const char* title, int x, int y,
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

void buildDiagScreen() {
  screenDiag = lv_obj_create(nullptr);
  setObjBg(screenDiag, C_BG);
  lv_obj_clear_flag(screenDiag, LV_OBJ_FLAG_SCROLLABLE);
  createHeader(screenDiag, "STATUS / SAFETIES", 2);

  objHps = createDiagCell(screenDiag, "HIGH PRESS", 4, 40, 100, 47, &lblHps);
  objLps = createDiagCell(screenDiag, "LOW PRESS", 110, 40, 100, 47, &lblLps);
  objOps = createDiagCell(screenDiag, "OIL PRESS", 216, 40, 100, 47, &lblOps);

  objRun = createDiagCell(screenDiag, "RUN OUTPUT", 4, 92, 100, 47, &lblRun);
  objAlarm = createDiagCell(screenDiag, "ALARM", 110, 92, 100, 47, &lblAlarm);
  objShutdown = createDiagCell(screenDiag, "SHUTDOWN", 216, 92, 100, 47, &lblShutdown);

  objMqtt = createDiagCell(screenDiag, "MQTT", 4, 144, 100, 47, &lblMqtt);
  objInternet = createDiagCell(screenDiag, "INTERNET", 110, 144, 100, 47, &lblInternet);
  makeButton(screenDiag, "RESET", C_AMBER, 216, 144, 100, 47, eventReset,
             &lv_font_montserrat_14);

  lblDiagMessage = makeLabel(screenDiag, "SAFETY STATE FROM HARDWARE",
                             &lv_font_montserrat_10, C_MUTED,
                             LV_ALIGN_TOP_LEFT, 6, 194);
  constrainLabel(lblDiagMessage, 308, LV_TEXT_ALIGN_LEFT);

  createNav(screenDiag, 2);
}

void buildMainScreens() {
  // Load Home first, then release the splash before allocating the two
  // remaining pages. This keeps peak LVGL heap use lower on the classic ESP32.
  buildHomeScreen();
  lv_scr_load(screenHome);
  if (screenSplash != nullptr) {
    lv_obj_del(screenSplash);
    screenSplash = nullptr;
  }
  buildTempScreen();
  buildDiagScreen();
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

  makeLabel(frame, "AM3", &lv_font_montserrat_28, C_TEXT,
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

void flushDisplay(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color) {
  const uint32_t width = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  const uint32_t height = static_cast<uint32_t>(area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, width, height);
  tft.pushColors(reinterpret_cast<uint16_t*>(&color->full), width * height, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

void readTouch(lv_indev_drv_t*, lv_indev_data_t* data) {
  uint16_t rawX = 0;
  uint16_t rawY = 0;
  uint8_t gesture = 0;

  if (!touch.getTouch(rawX, rawY, gesture)) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  int32_t x = rawX;
  int32_t y = rawY;

  if (AM3_TOUCH_SWAP_XY) {
    const int32_t tmp = x;
    x = y;
    y = tmp;
  }
  if (AM3_TOUCH_INVERT_X) {
    x = (AM3_SCREEN_WIDTH - 1) - x;
  }
  if (AM3_TOUCH_INVERT_Y) {
    y = (AM3_SCREEN_HEIGHT - 1) - y;
  }

  x = constrain(x, 0L, static_cast<long>(AM3_SCREEN_WIDTH - 1));
  y = constrain(y, 0L, static_cast<long>(AM3_SCREEN_HEIGHT - 1));

  data->point.x = static_cast<lv_coord_t>(x);
  data->point.y = static_cast<lv_coord_t>(y);
  data->state = LV_INDEV_STATE_PRESSED;
}

String pressureText(const String& value, const char* suffix, uint8_t decimals) {
  if (value.length() == 0 || value == "999") {
    return String("-- ") + suffix;
  }
  if (value == "888") {
    return String("ERR ") + suffix;
  }
  const float parsed = value.toFloat();
  if (!isfinite(parsed)) {
    return String("-- ") + suffix;
  }
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

void setBinaryLabel(lv_obj_t* label, lv_obj_t* card, bool active,
                    const char* onText, const char* offText,
                    lv_color_t onColor = C_GREEN, lv_color_t offColor = C_OFF,
                    bool activeIsWarning = false) {
  if (!label) return;
  lv_label_set_text(label, active ? onText : offText);
  const lv_color_t color = active ? onColor : offColor;
  lv_obj_set_style_text_color(label, color, 0);

  if (card) {
    const bool highlight = activeIsWarning ? active : !active;
    const lv_color_t cardColor = highlight ? color : C_BORDER;
    styleDynamicCard(card, cardColor, highlight);
  }
}

void refreshHeader() {
  char timeText[10] = "--:--";
  struct tm timeInfo;
  if (am3GetRemotePakistanTime(timeInfo)) {
    strftime(timeText, sizeof(timeText), "%H:%M", &timeInfo);
  }

  const char* statusText = "OFFLINE";
  lv_color_t statusColor = C_RED;
  if (am3DisplayLinkConnected) {
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

void refreshHome() {
  if (!lblHomeState) return;

  const StateAppearance state = stateAppearance(remoteCompStatus);
  lv_label_set_text(lblHomeState, state.title);
  lv_obj_set_style_text_color(lblHomeState, state.color, 0);
  lv_label_set_text(lblHomeDetail, state.detail);
  lv_obj_set_style_text_color(lblHomeDetail, state.fault ? C_RED : C_MUTED, 0);
  styleDynamicCard(objHomeStateCard, state.color, state.fault || remoteCompStatus == 1);

  if (am3DisplayLinkConnected) {
    String link = remoteDeviceName;
    if (link.length() == 0) link = "AM3";
    link += remoteInternetConnected ? "  NET" : "  LOCAL";
    lv_label_set_text(lblHomeLink, link.c_str());
    lv_obj_set_style_text_color(lblHomeLink,
                                remoteInternetConnected ? C_GREEN : C_AMBER, 0);
  } else {
    lv_label_set_text(lblHomeLink, "LINK OFFLINE");
    lv_obj_set_style_text_color(lblHomeLink, C_RED, 0);
  }

  const String sPsi = pressureText(suctionPressure, "PSI", 1);
  const String sBar = pressureText(suctionPressureB, "bar", 1);
  const String dPsi = pressureText(dischargePressure, "PSI", 0);
  const String dBar = pressureText(dischargePressureB, "bar", 1);
  lv_label_set_text(lblSuctionPsi, sPsi.c_str());
  lv_label_set_text(lblSuctionBar, sBar.c_str());
  lv_label_set_text(lblDischargePsi, dPsi.c_str());
  lv_label_set_text(lblDischargeBar, dBar.c_str());

  if (am3LastCommandAckMs != 0 && millis() - am3LastCommandAckMs < 5000UL) {
    lv_label_set_text(lblCommand, am3LastCommandAccepted ? "ACCEPTED" : "REJECTED");
    lv_obj_set_style_text_color(lblCommand,
                                am3LastCommandAccepted ? C_GREEN : C_RED, 0);
  } else if (!am3DisplayLinkConnected) {
    lv_label_set_text(lblCommand, "NO LINK");
    lv_obj_set_style_text_color(lblCommand, C_RED, 0);
  } else {
    lv_label_set_text(lblCommand, "READY");
    lv_obj_set_style_text_color(lblCommand, C_CYAN, 0);
  }
}

void refreshTemperatures() {
  const float values[5] = {ReturnTempC, SuctionTempC, dischargeTempC, SupplyTempC, OilTempC};
  const lv_color_t liveColors[5] = {C_BLUE, C_CYAN, C_RED, C_GREEN, C_AMBER};

  int liveCount = 0;
  int faultCount = 0;
  int notSetCount = 0;

  for (int i = 0; i < 5; ++i) {
    if (!lblTempValue[i]) continue;
    const char* state = temperatureState(values[i]);
    const String value = temperatureText(values[i]);
    lv_label_set_text(lblTempValue[i], value.c_str());
    lv_label_set_text(lblTempState[i], state);

    if (strcmp(state, "LIVE") == 0) {
      ++liveCount;
      lv_obj_set_style_text_color(lblTempValue[i], liveColors[i], 0);
      lv_obj_set_style_text_color(lblTempState[i], C_GREEN, 0);
      styleDynamicCard(objTempCard[i], liveColors[i], false);
    } else if (strcmp(state, "NOT SET") == 0) {
      ++notSetCount;
      lv_obj_set_style_text_color(lblTempValue[i], C_OFF, 0);
      lv_obj_set_style_text_color(lblTempState[i], C_AMBER, 0);
      styleDynamicCard(objTempCard[i], C_AMBER, false);
    } else {
      ++faultCount;
      lv_obj_set_style_text_color(lblTempValue[i], C_RED, 0);
      lv_obj_set_style_text_color(lblTempState[i], C_RED, 0);
      styleDynamicCard(objTempCard[i], C_RED, true);
    }
  }

  if (lblTempSummaryMain && lblTempSummarySub) {
    char mainText[18];
    snprintf(mainText, sizeof(mainText), "%d / 5 LIVE", liveCount);
    lv_label_set_text(lblTempSummaryMain, mainText);

    if (faultCount > 0) {
      char subText[24];
      snprintf(subText, sizeof(subText), "%d fault%s detected", faultCount,
               faultCount == 1 ? "" : "s");
      lv_label_set_text(lblTempSummarySub, subText);
      lv_obj_set_style_text_color(lblTempSummaryMain, C_RED, 0);
      lv_obj_set_style_text_color(lblTempSummarySub, C_RED, 0);
      styleDynamicCard(objTempSummary, C_RED, true);
    } else if (notSetCount > 0) {
      char subText[24];
      snprintf(subText, sizeof(subText), "%d channel%s not set", notSetCount,
               notSetCount == 1 ? "" : "s");
      lv_label_set_text(lblTempSummarySub, subText);
      lv_obj_set_style_text_color(lblTempSummaryMain, C_AMBER, 0);
      lv_obj_set_style_text_color(lblTempSummarySub, C_AMBER, 0);
      styleDynamicCard(objTempSummary, C_AMBER, false);
    } else {
      lv_label_set_text(lblTempSummarySub, "All channels healthy");
      lv_obj_set_style_text_color(lblTempSummaryMain, C_GREEN, 0);
      lv_obj_set_style_text_color(lblTempSummarySub, C_MUTED, 0);
      styleDynamicCard(objTempSummary, C_GREEN, false);
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

void refreshDiagnostics() {
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
  setBinaryLabel(lblInternet, objInternet, remoteInternetConnected != 0, "ONLINE", "OFFLINE",
                 C_GREEN, C_RED, false);

  if (am3LastCommandAckMs != 0 && millis() - am3LastCommandAckMs < 5000UL) {
    lv_label_set_text(lblDiagMessage, am3LastCommandMessage.c_str());
    lv_obj_set_style_text_color(lblDiagMessage,
                                am3LastCommandAccepted ? C_GREEN : C_RED, 0);
  } else {
    const StateAppearance state = stateAppearance(remoteCompStatus);
    lv_label_set_text(lblDiagMessage, state.detail);
    lv_obj_set_style_text_color(lblDiagMessage, state.fault ? C_RED : C_MUTED, 0);
  }
}

void refreshUi() {
  refreshHeader();
  refreshHome();
  refreshTemperatures();
  refreshDiagnostics();
}
}  // namespace

void initAM3UI() {
  pinMode(AM3_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(AM3_BACKLIGHT_PIN, AM3_BACKLIGHT_ACTIVE_HIGH ? LOW : HIGH);

  tft.begin();
  tft.setRotation(AM3_TFT_ROTATION);
  tft.fillScreen(TFT_BLACK);

  const bool touchOk = touch.begin(AM3_TOUCH_I2C_HZ);
  Serial.printf("[TOUCH] CST820 %s", touchOk ? "detected" : "not detected");
  if (touchOk) {
    Serial.printf(" at 0x%02X", touch.address());
  }
  Serial.println();

  lv_init();
  lv_disp_draw_buf_init(&drawBufferDescriptor, drawBuffer, nullptr, LVGL_BUFFER_PIXELS);

  lv_disp_drv_init(&displayDriver);
  displayDriver.hor_res = AM3_SCREEN_WIDTH;
  displayDriver.ver_res = AM3_SCREEN_HEIGHT;
  displayDriver.flush_cb = flushDisplay;
  displayDriver.draw_buf = &drawBufferDescriptor;
  lv_disp_drv_register(&displayDriver);

  lv_indev_drv_init(&inputDriver);
  inputDriver.type = LV_INDEV_TYPE_POINTER;
  inputDriver.read_cb = readTouch;
  lv_indev_drv_register(&inputDriver);

  buildSplash();
  lv_timer_handler();
  digitalWrite(AM3_BACKLIGHT_PIN, AM3_BACKLIGHT_ACTIVE_HIGH ? HIGH : LOW);

  bootMs = millis();
  lastLvTickMs = bootMs;
  lastRefreshMs = 0;
  Serial.println("[DISPLAY] JC2432W328 enhanced LVGL UI ready");
}

void updateAM3UI() {
  const unsigned long now = millis();
  const uint32_t elapsed = now - lastLvTickMs;
  if (elapsed > 0) {
    lv_tick_inc(elapsed);
    lastLvTickMs = now;
  }

  if (!mainScreensBuilt && now - bootMs >= AM3_SPLASH_MS) {
    buildMainScreens();
    refreshUi();
  }

  if (mainScreensBuilt && now - lastRefreshMs >= AM3_UI_REFRESH_MS) {
    lastRefreshMs = now;
    refreshUi();
  }

  lv_timer_handler();
}
