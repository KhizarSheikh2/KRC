#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_heap_caps.h>

#include "UI.h"
#include "DisplayConfig.h"
#include "CST820Touch.h"

// Controller state is owned by the original DX-Mini firmware (Def.h in main.cpp).
// These extern declarations let the UI observe that state without redefining it.
extern bool PowerState;
extern bool dischargeTempAlarm;
extern bool returnTempAlarm;
extern bool EVAP_COOL_SYSTEM_STATUS;
extern bool AUTO_WASH_SYSTEM_STATUS;
extern bool UNIT_COMMAND_STATUS;
extern int EVAP_COOL_SYSTEM_PENDING;
extern int AUTO_WASH_SYSTEM_PENDING;
extern int UNIT_COMMAND_PENDING;
extern float SupplyTemp;
extern float ReturnTemp;
extern float dischargeTemp;
extern int dischargeSp;
extern int ReturnSp;
extern int EVAP_COOL_SYSTEM_SWITCH;
extern int AUTO_WASH_SYSTEM_SWITCH;
extern int UNIT_COMMAND_SWITCH;
extern int FAULT_SWITCH;
extern int MODE_SWITCH;

namespace {
constexpr int UI_W = DX_UI_WIDTH;
constexpr int UI_H = DX_UI_HEIGHT;
constexpr float SENSOR_DISCONNECTED_VALUE = 888.0f;
constexpr float SENSOR_NOT_SELECTED_VALUE = 999.0f;

enum UiPage : uint8_t { PAGE_HOME = 0, PAGE_TEMPS, PAGE_OUTPUTS, PAGE_SETTINGS };

TFT_eSPI tft;
CST820Touch touch(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT, TOUCH_I2C_ADDR);
lv_disp_draw_buf_t drawBuf;
lv_color_t *drawMem = nullptr;
uint16_t drawBufferLines = 0;
lv_disp_drv_t dispDrv;
lv_indev_drv_t indevDrv;

lv_point_t lastTouchPoint{0, 0};
#if DX_TOUCH_DEBUG
uint32_t lastTouchDebugMs = 0;
#endif
uint32_t lastLvTick = 0;
uint32_t lastUiRefresh = 0;
uint32_t lastActionMs = 0;
uint32_t toastUntil = 0;
uint32_t splashVisibleSince = 0;

// LVGL/TFT are serviced only by this dedicated task after uiInit().
// This keeps the HMI alive even if MQTT TLS, NTP or Wi-Fi work blocks loop().
TaskHandle_t uiTaskHandle = nullptr;
QueueHandle_t actionQueue = nullptr;
QueueHandle_t bootQueue = nullptr;
QueueHandle_t toastQueue = nullptr;
volatile bool uiDirtyRequested = true;
volatile bool mainScreenRequested = false;
bool mainScreenVisible = false;

struct BootMessage {
  int value;
  char status[32];
};

struct ToastMessage {
  uint32_t durationMs;
  char text[40];
};

UiPage activePage = PAGE_HOME;
uint8_t selectedTemp = 0; // discharge, return, supply

lv_obj_t *screenSplash = nullptr;
lv_obj_t *screenMain = nullptr;
lv_obj_t *splashBar = nullptr;
lv_obj_t *splashStatus = nullptr;
lv_obj_t *pageHome = nullptr;
lv_obj_t *pageTemps = nullptr;
lv_obj_t *pageOutputs = nullptr;
lv_obj_t *pageSettings = nullptr;

// Header
lv_obj_t *lblContext = nullptr;
lv_obj_t *chipPower = nullptr;
lv_obj_t *lblPower = nullptr;
lv_obj_t *chipWifi = nullptr;
lv_obj_t *lblWifi = nullptr;
lv_obj_t *chipMode = nullptr;
lv_obj_t *lblMode = nullptr;

// Home
lv_obj_t *hero = nullptr;
lv_obj_t *heroTitle = nullptr;
lv_obj_t *heroValue = nullptr;
lv_obj_t *heroUnit = nullptr;
lv_obj_t *heroSetCaption = nullptr;
lv_obj_t *heroSetValue = nullptr;
lv_obj_t *heroMinus = nullptr;
lv_obj_t *heroPlus = nullptr;
lv_obj_t *heroStatus = nullptr;
lv_obj_t *outputCard[3] = {nullptr, nullptr, nullptr};
lv_obj_t *outputState[3] = {nullptr, nullptr, nullptr};
lv_obj_t *tempCard[3] = {nullptr, nullptr, nullptr};
lv_obj_t *tempValue[3] = {nullptr, nullptr, nullptr};

// Temps page
lv_obj_t *tempPageValue[3] = {nullptr, nullptr, nullptr};
lv_obj_t *tempPageSet[2] = {nullptr, nullptr};

// Outputs page
lv_obj_t *outActual[3] = {nullptr, nullptr, nullptr};
lv_obj_t *outCommand[3] = {nullptr, nullptr, nullptr};
lv_obj_t *outStatus[3] = {nullptr, nullptr, nullptr};
lv_obj_t *outButton[3] = {nullptr, nullptr, nullptr};
lv_obj_t *outButtonText[3] = {nullptr, nullptr, nullptr};

// Settings
lv_obj_t *settingsPower = nullptr;
lv_obj_t *settingsMode = nullptr;
lv_obj_t *settingsNetwork = nullptr;

// Navigation
lv_obj_t *navBtn[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t *navText[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t *navLine[4] = {nullptr, nullptr, nullptr, nullptr};

// Display-side temperature hold cache. This keeps the UI stable if a read
// briefly returns SENSOR_DISCONNECTED / SENSOR_NOT_SELECTED for a cycle.
float displayedTemps[3] = {SENSOR_DISCONNECTED_VALUE, SENSOR_DISCONNECTED_VALUE, SENSOR_DISCONNECTED_VALUE};
bool displayedTempValid[3] = {false, false, false};

// Toast
lv_obj_t *toast = nullptr;
lv_obj_t *toastText = nullptr;

const lv_color_t C_BG = lv_color_hex(0x03080E);
const lv_color_t C_HEADER = lv_color_hex(0x07121D);
const lv_color_t C_PANEL = lv_color_hex(0x0A1724);
const lv_color_t C_PANEL2 = lv_color_hex(0x0D1C2B);
const lv_color_t C_PANEL3 = lv_color_hex(0x07131F);
const lv_color_t C_BORDER = lv_color_hex(0x244A67);
const lv_color_t C_BORDER_DIM = lv_color_hex(0x163249);
const lv_color_t C_TEXT = lv_color_hex(0xF4F8FC);
const lv_color_t C_MUTED = lv_color_hex(0xA6B9CB);
const lv_color_t C_DIM = lv_color_hex(0x60788E);
const lv_color_t C_CYAN = lv_color_hex(0x20D6FF);
const lv_color_t C_BLUE = lv_color_hex(0x3395FF);
const lv_color_t C_GREEN = lv_color_hex(0x38E58B);
const lv_color_t C_AMBER = lv_color_hex(0xF3C968);
const lv_color_t C_RED = lv_color_hex(0xFF6172);

void base(lv_obj_t *obj) {
  lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void bg(lv_obj_t *obj, lv_color_t color) {
  lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
}

void panel(lv_obj_t *obj, lv_color_t fill, lv_color_t border, int radius = 8, int borderWidth = 1) {
  base(obj);
  bg(obj, fill);
  lv_obj_set_style_border_color(obj, border, LV_PART_MAIN);
  lv_obj_set_style_border_width(obj, borderWidth, LV_PART_MAIN);
  lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
}

lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color) {
  lv_obj_t *obj = lv_label_create(parent);
  lv_label_set_text(obj, text);
  lv_obj_set_style_text_font(obj, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(obj, color, LV_PART_MAIN);
  return obj;
}

void setBorder(lv_obj_t *obj, lv_color_t color, int width = 1) {
  if (!obj) return;
  lv_obj_set_style_border_color(obj, color, LV_PART_MAIN);
  lv_obj_set_style_border_width(obj, width, LV_PART_MAIN);
}

bool validTemp(float value) {
  return value != SENSOR_DISCONNECTED_VALUE && value != SENSOR_NOT_SELECTED_VALUE;
}

float uiTempForDisplay(uint8_t index, float liveValue) {
  if (index > 2) return liveValue;
  if (validTemp(liveValue)) {
    displayedTemps[index] = liveValue;
    displayedTempValid[index] = true;
    return liveValue;
  }
  if (displayedTempValid[index]) return displayedTemps[index];
  return liveValue;
}

void formatTemp(char *out, size_t outLen, float value, uint8_t decimals = 1, bool includeUnit = false) {
  if (!out || outLen == 0) return;
  if (!validTemp(value)) {
    snprintf(out, outLen, "---%s", includeUnit ? " C" : "");
    return;
  }
  snprintf(out, outLen, includeUnit ? "%.*f C" : "%.*f", static_cast<int>(decimals), value);
}

void wakeUiTask() {
  if (uiTaskHandle) xTaskNotifyGive(uiTaskHandle);
}

void requestUiRefresh() {
  uiDirtyRequested = true;
  wakeUiTask();
}

void queueAction(UiAction action) {
  if (action == UI_ACTION_NONE || !actionQueue) return;
  const uint32_t now = millis();
  if (now - lastActionMs < 35) return;
  lastActionMs = now;

  // Queue actions instead of keeping a single pending slot. This prevents
  // quick valid taps from being lost while the controller loop is busy.
  (void)xQueueSend(actionQueue, &action, 0);
}

void actionEvent(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_PRESSED && code != LV_EVENT_LONG_PRESSED_REPEAT) return;
  queueAction((UiAction)(intptr_t)lv_event_get_user_data(e));
}

lv_obj_t *touchButton(lv_obj_t *parent, int x, int y, int w, int h,
                      const char *text, const lv_font_t *font,
                      lv_color_t accent, UiAction action, int radius = 8,
                      bool allowRepeat = false) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, w, h);
  panel(btn, C_PANEL3, accent, radius, 1);
  lv_obj_set_style_bg_color(btn, C_PANEL2, LV_STATE_PRESSED);
  lv_obj_set_style_border_color(btn, C_TEXT, LV_STATE_PRESSED);
  lv_obj_add_event_cb(btn, actionEvent, LV_EVENT_PRESSED, (void *)(intptr_t)action);
  if (allowRepeat) lv_obj_add_event_cb(btn, actionEvent, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)action);
  lv_obj_t *txt = label(btn, text, font, accent);
  lv_obj_center(txt);
  return btn;
}

void setEnabledVisual(lv_obj_t *obj, bool enabled) {
  if (!obj) return;
  lv_obj_set_style_opa(obj, enabled ? LV_OPA_COVER : LV_OPA_80, LV_PART_MAIN);
}

bool allocateDrawBuffer() {
  // Keep the LVGL draw buffer off static .bss so the ESP32 linker retains
  // enough DRAM for Wi-Fi/TLS and the original DX-Mini globals.  8 lines
  // is the preferred low-memory balance for this 320x240 SPI panel; smaller fallbacks
  // keep the HMI bootable under unexpected heap pressure.
  static constexpr uint16_t candidates[] = {8, 6, 4};

  for (uint16_t lines : candidates) {
    const size_t bytes = static_cast<size_t>(UI_W) * lines * sizeof(lv_color_t);
    drawMem = static_cast<lv_color_t *>(
        heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (drawMem != nullptr) {
      drawBufferLines = lines;
      Serial.printf("[DISPLAY] LVGL draw buffer: %u lines / %u bytes\n",
                    drawBufferLines, static_cast<unsigned>(bytes));
      return true;
    }
  }

  drawBufferLines = 0;
  Serial.println("[DISPLAY] ERROR: LVGL draw buffer allocation failed");
  return false;
}

void flushDisplay(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *colorP) {
  const uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
  const uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors(reinterpret_cast<uint16_t *>(&colorP->full), w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(drv);
}

bool mapTouch(uint16_t rawX, uint16_t rawY, lv_point_t &point) {
  // CST820 is fixed in portrait coordinates even when the TFT is rendered in
  // landscape. Convert controller coordinates into LVGL's 320x240 frame.
  int x = static_cast<int>(rawX);
  int y = static_cast<int>(rawY);

#if DX_TOUCH_SWAP_XY
  const int tmp = x;
  x = y;
  y = tmp;
#endif

#if DX_TOUCH_INVERT_X
  x = (DX_UI_WIDTH - 1) - x;
#endif
#if DX_TOUCH_INVERT_Y
  y = (DX_UI_HEIGHT - 1) - y;
#endif

  x += DX_TOUCH_X_OFFSET;
  y += DX_TOUCH_Y_OFFSET;

  // Never clamp an invalid touch into the nearest widget. A bad sample must be
  // ignored; clamping is what can make an out-of-range touch activate another
  // button at an edge of the screen.
  if (x < 0 || x >= DX_UI_WIDTH || y < 0 || y >= DX_UI_HEIGHT) return false;

  point.x = static_cast<lv_coord_t>(x);
  point.y = static_cast<lv_coord_t>(y);
  return true;
}

void readTouch(lv_indev_drv_t *, lv_indev_data_t *data) {
  uint16_t rawX = 0, rawY = 0;
  if (touch.read(rawX, rawY)) {
    lv_point_t mapped{};
    if (mapTouch(rawX, rawY, mapped)) {
      lastTouchPoint = mapped;
      data->point = lastTouchPoint;
      data->state = LV_INDEV_STATE_PR;
#if DX_TOUCH_DEBUG
      const uint32_t now = millis();
      if (now - lastTouchDebugMs > 80) {
        lastTouchDebugMs = now;
        Serial.printf("[TOUCH] raw=(%u,%u) screen=(%d,%d)\n", rawX, rawY, lastTouchPoint.x, lastTouchPoint.y);
      }
#endif
      return;
    }
  }

  // Preserve the last valid point on release, as expected by LVGL pointer
  // devices, but do not synthesize/clamp a new coordinate.
  data->point = lastTouchPoint;
  data->state = LV_INDEV_STATE_REL;
}

void showToast(const char *text, uint32_t ms = 800) {
  if (!toast || !toastText) return;
  lv_label_set_text(toastText, text);
  lv_obj_clear_flag(toast, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(toast);
  toastUntil = millis() + ms;
}

void pageEvent(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_PRESSED) return;
  activePage = (UiPage)(intptr_t)lv_event_get_user_data(e);
  lv_obj_add_flag(pageHome, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(pageTemps, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(pageOutputs, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(pageSettings, LV_OBJ_FLAG_HIDDEN);
  if (activePage == PAGE_HOME) lv_obj_clear_flag(pageHome, LV_OBJ_FLAG_HIDDEN);
  else if (activePage == PAGE_TEMPS) lv_obj_clear_flag(pageTemps, LV_OBJ_FLAG_HIDDEN);
  else if (activePage == PAGE_OUTPUTS) lv_obj_clear_flag(pageOutputs, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(pageSettings, LV_OBJ_FLAG_HIDDEN);
  for (int i = 0; i < 4; ++i) {
    const bool selected = i == (int)activePage;
    lv_obj_set_style_text_color(navText[i], selected ? C_CYAN : C_MUTED, LV_PART_MAIN);
    bg(navLine[i], selected ? C_CYAN : C_HEADER);
  }
  updateDisplay();
}

void selectTempEvent(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_PRESSED) return;
  selectedTemp = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  updateDisplay();
}

void heroMinusEvent(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_PRESSED && lv_event_get_code(e) != LV_EVENT_LONG_PRESSED_REPEAT) return;
  if (selectedTemp == 0) queueAction(UI_ACTION_DISCHARGE_MINUS);
  else if (selectedTemp == 1) queueAction(UI_ACTION_RETURN_MINUS);
}

void heroPlusEvent(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_PRESSED && lv_event_get_code(e) != LV_EVENT_LONG_PRESSED_REPEAT) return;
  if (selectedTemp == 0) queueAction(UI_ACTION_DISCHARGE_PLUS);
  else if (selectedTemp == 1) queueAction(UI_ACTION_RETURN_PLUS);
}

lv_obj_t *headerChip(lv_obj_t *parent, int x, int w, const char *text, lv_color_t accent,
                     lv_obj_t **outLabel, UiAction action = UI_ACTION_NONE) {
  lv_obj_t *chip = action == UI_ACTION_NONE ? lv_obj_create(parent) : lv_btn_create(parent);
  lv_obj_set_pos(chip, x, 5);
  lv_obj_set_size(chip, w, 30);
  panel(chip, C_PANEL3, accent, 7, 1);
  if (action != UI_ACTION_NONE) {
    lv_obj_set_style_bg_color(chip, C_PANEL2, LV_STATE_PRESSED);
    lv_obj_add_event_cb(chip, actionEvent, LV_EVENT_PRESSED, (void *)(intptr_t)action);
  }
  *outLabel = label(chip, text, &lv_font_montserrat_10, accent);
  lv_obj_center(*outLabel);
  return chip;
}

void createHeader() {
  lv_obj_t *header = lv_obj_create(screenMain);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_size(header, UI_W, 40);
  base(header);
  bg(header, C_HEADER);
  lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);

  lv_obj_t *name = label(header, "DX-MINI", &lv_font_montserrat_18, C_TEXT);
  lv_obj_set_pos(name, 8, 5);
  lblContext = label(header, "HOME", &lv_font_montserrat_8, C_CYAN);
  lv_obj_set_pos(lblContext, 9, 27);

  chipPower = headerChip(header, 128, 44, "ON", C_GREEN, &lblPower, UI_ACTION_TOGGLE_POWER);
  chipWifi = headerChip(header, 176, 58, "WIFI", C_CYAN, &lblWifi);
  chipMode = headerChip(header, 238, 77, "MANUAL", C_AMBER, &lblMode, UI_ACTION_TOGGLE_MODE);

  lv_obj_t *line = lv_obj_create(header);
  lv_obj_set_pos(line, 0, 39);
  lv_obj_set_size(line, UI_W, 1);
  base(line);
  bg(line, C_BORDER);
  lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
}

void createHomeOutputCard(int idx, int x, const char *name, UiAction action) {
  outputCard[idx] = lv_btn_create(pageHome);
  lv_obj_set_pos(outputCard[idx], x, 89);
  lv_obj_set_size(outputCard[idx], 100, 42);
  panel(outputCard[idx], C_PANEL, C_BORDER, 8, 2);
  lv_obj_set_style_bg_color(outputCard[idx], C_PANEL2, LV_STATE_PRESSED);
  lv_obj_add_event_cb(outputCard[idx], actionEvent, LV_EVENT_PRESSED, (void *)(intptr_t)action);

  lv_obj_t *title = label(outputCard[idx], name, &lv_font_montserrat_10, C_TEXT);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 6);
  outputState[idx] = label(outputCard[idx], "OFF", &lv_font_montserrat_16, C_TEXT);
  lv_obj_align(outputState[idx], LV_ALIGN_BOTTOM_LEFT, 8, -5);

}

void createTempSummary(int idx, int x, const char *name) {
  tempCard[idx] = lv_btn_create(pageHome);
  lv_obj_set_pos(tempCard[idx], x, 136);
  lv_obj_set_size(tempCard[idx], 100, 27);
  panel(tempCard[idx], C_PANEL3, C_BORDER_DIM, 6, 1);
  lv_obj_add_event_cb(tempCard[idx], selectTempEvent, LV_EVENT_PRESSED, (void *)(intptr_t)idx);

  lv_obj_t *nameLbl = label(tempCard[idx], name, &lv_font_montserrat_8, C_MUTED);
  lv_obj_set_pos(nameLbl, 6, 3);
  tempValue[idx] = label(tempCard[idx], "--- C", &lv_font_montserrat_12, C_TEXT);
  lv_obj_align(tempValue[idx], LV_ALIGN_RIGHT_MID, -6, 3);
}

void createHome() {
  pageHome = lv_obj_create(screenMain);
  lv_obj_set_pos(pageHome, 0, 40);
  lv_obj_set_size(pageHome, UI_W, 164);
  base(pageHome);
  bg(pageHome, C_BG);
  lv_obj_set_style_border_width(pageHome, 0, LV_PART_MAIN);

  hero = lv_obj_create(pageHome);
  lv_obj_set_pos(hero, 5, 4);
  lv_obj_set_size(hero, 310, 82);
  panel(hero, C_PANEL, C_CYAN, 9, 2);

  heroTitle = label(hero, "DISCHARGE AIR", &lv_font_montserrat_12, C_CYAN);
  lv_obj_set_pos(heroTitle, 12, 8);
  heroValue = label(hero, "---", &lv_font_montserrat_32, C_TEXT);
  lv_obj_set_pos(heroValue, 12, 29);
  heroUnit = label(hero, "C", &lv_font_montserrat_18, C_CYAN);
  lv_obj_set_pos(heroUnit, 92, 39);

  heroSetCaption = label(hero, "SETPOINT", &lv_font_montserrat_8, C_MUTED);
  lv_obj_set_pos(heroSetCaption, 145, 10);
  heroSetValue = label(hero, "0 C", &lv_font_montserrat_18, C_TEXT);
  lv_obj_set_pos(heroSetValue, 145, 28);

  heroMinus = lv_btn_create(hero);
  lv_obj_set_pos(heroMinus, 222, 21);
  lv_obj_set_size(heroMinus, 38, 40);
  panel(heroMinus, C_PANEL3, C_CYAN, 19, 2);
  lv_obj_set_style_bg_color(heroMinus, C_PANEL2, LV_STATE_PRESSED);
  lv_obj_add_event_cb(heroMinus, heroMinusEvent, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(heroMinus, heroMinusEvent, LV_EVENT_LONG_PRESSED_REPEAT, nullptr);
  lv_obj_t *minus = label(heroMinus, "-", &lv_font_montserrat_20, C_TEXT);
  lv_obj_center(minus);

  heroPlus = lv_btn_create(hero);
  lv_obj_set_pos(heroPlus, 266, 21);
  lv_obj_set_size(heroPlus, 38, 40);
  panel(heroPlus, C_PANEL3, C_CYAN, 19, 2);
  lv_obj_set_style_bg_color(heroPlus, C_PANEL2, LV_STATE_PRESSED);
  lv_obj_add_event_cb(heroPlus, heroPlusEvent, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(heroPlus, heroPlusEvent, LV_EVENT_LONG_PRESSED_REPEAT, nullptr);
  lv_obj_t *plus = label(heroPlus, "+", &lv_font_montserrat_20, C_TEXT);
  lv_obj_center(plus);

  heroStatus = label(hero, "NORMAL", &lv_font_montserrat_10, C_GREEN);
  lv_obj_set_pos(heroStatus, 145, 59);

  createHomeOutputCard(0, 5, "EVAP COOL", UI_ACTION_TOGGLE_EVAP);
  createHomeOutputCard(1, 110, "AUTO WASH", UI_ACTION_TOGGLE_WASH);
  createHomeOutputCard(2, 215, "UNIT CMD", UI_ACTION_TOGGLE_UNIT);

  createTempSummary(0, 5, "DISCH");
  createTempSummary(1, 110, "RETURN");
  createTempSummary(2, 215, "SUPPLY");
}

void createTempControl(int idx, int x, const char *title, UiAction minusAction, UiAction plusAction) {
  lv_obj_t *card = lv_obj_create(pageTemps);
  lv_obj_set_pos(card, x, 5);
  lv_obj_set_size(card, 152, 104);
  panel(card, C_PANEL, idx == 0 ? C_CYAN : C_BORDER, 9, idx == 0 ? 2 : 1);

  lv_obj_t *titleLbl = label(card, title, &lv_font_montserrat_12, idx == 0 ? C_CYAN : C_TEXT);
  lv_obj_set_pos(titleLbl, 10, 8);
  tempPageValue[idx] = label(card, "--- C", &lv_font_montserrat_24, C_TEXT);
  lv_obj_set_pos(tempPageValue[idx], 10, 31);
  lv_obj_t *sp = label(card, "SETPOINT", &lv_font_montserrat_8, C_MUTED);
  lv_obj_set_pos(sp, 10, 72);
  tempPageSet[idx] = label(card, "0 C", &lv_font_montserrat_14, C_TEXT);
  lv_obj_set_pos(tempPageSet[idx], 10, 85);

  // Minimum ~30 px finger targets for reliable touch on the 2.8" panel.
  touchButton(card, 78, 65, 31, 34, "-", &lv_font_montserrat_18, C_CYAN, minusAction, 15, true);
  touchButton(card, 115, 65, 31, 34, "+", &lv_font_montserrat_18, C_CYAN, plusAction, 15, true);
}

void createTemps() {
  pageTemps = lv_obj_create(screenMain);
  lv_obj_set_pos(pageTemps, 0, 40);
  lv_obj_set_size(pageTemps, UI_W, 164);
  base(pageTemps);
  bg(pageTemps, C_BG);
  lv_obj_set_style_border_width(pageTemps, 0, LV_PART_MAIN);

  createTempControl(0, 5, "DISCHARGE AIR", UI_ACTION_DISCHARGE_MINUS, UI_ACTION_DISCHARGE_PLUS);
  createTempControl(1, 163, "RETURN AIR", UI_ACTION_RETURN_MINUS, UI_ACTION_RETURN_PLUS);

  lv_obj_t *supply = lv_obj_create(pageTemps);
  lv_obj_set_pos(supply, 5, 114);
  lv_obj_set_size(supply, 310, 46);
  panel(supply, C_PANEL3, C_BORDER_DIM, 8, 1);
  lv_obj_t *title = label(supply, "SUPPLY AIR", &lv_font_montserrat_10, C_MUTED);
  lv_obj_set_pos(title, 10, 7);
  tempPageValue[2] = label(supply, "--- C", &lv_font_montserrat_20, C_TEXT);
  lv_obj_set_pos(tempPageValue[2], 10, 21);
  lv_obj_t *monitor = label(supply, "MONITOR ONLY", &lv_font_montserrat_10, C_BLUE);
  lv_obj_align(monitor, LV_ALIGN_RIGHT_MID, -12, 0);
}

void createOutputRow(int idx, int y, const char *name, UiAction action) {
  lv_obj_t *row = lv_obj_create(pageOutputs);
  lv_obj_set_pos(row, 5, y);
  lv_obj_set_size(row, 310, 49);
  panel(row, C_PANEL, C_BORDER_DIM, 8, 1);

  lv_obj_t *nameLbl = label(row, name, &lv_font_montserrat_14, C_TEXT);
  lv_obj_set_pos(nameLbl, 11, 7);
  lv_obj_t *a = label(row, "ACT", &lv_font_montserrat_8, C_MUTED);
  lv_obj_set_pos(a, 11, 31);
  outActual[idx] = label(row, "OFF", &lv_font_montserrat_10, C_DIM);
  lv_obj_set_pos(outActual[idx], 38, 29);
  lv_obj_t *c = label(row, "CMD", &lv_font_montserrat_8, C_MUTED);
  lv_obj_set_pos(c, 77, 31);
  outCommand[idx] = label(row, "OFF", &lv_font_montserrat_10, C_DIM);
  lv_obj_set_pos(outCommand[idx], 108, 29);
  outStatus[idx] = label(row, "READY", &lv_font_montserrat_10, C_GREEN);
  lv_obj_set_pos(outStatus[idx], 147, 29);

  outButton[idx] = touchButton(row, 235, 7, 66, 35, "OFF", &lv_font_montserrat_12, C_DIM, action, 17);
  outButtonText[idx] = lv_obj_get_child(outButton[idx], 0);
}

void createOutputs() {
  pageOutputs = lv_obj_create(screenMain);
  lv_obj_set_pos(pageOutputs, 0, 40);
  lv_obj_set_size(pageOutputs, UI_W, 164);
  base(pageOutputs);
  bg(pageOutputs, C_BG);
  lv_obj_set_style_border_width(pageOutputs, 0, LV_PART_MAIN);

  createOutputRow(0, 5, "EVAP COOL", UI_ACTION_TOGGLE_EVAP);
  createOutputRow(1, 57, "AUTO WASH", UI_ACTION_TOGGLE_WASH);
  createOutputRow(2, 109, "UNIT COMMAND", UI_ACTION_TOGGLE_UNIT);
}

void createSettings() {
  pageSettings = lv_obj_create(screenMain);
  lv_obj_set_pos(pageSettings, 0, 40);
  lv_obj_set_size(pageSettings, UI_W, 164);
  base(pageSettings);
  bg(pageSettings, C_BG);
  lv_obj_set_style_border_width(pageSettings, 0, LV_PART_MAIN);

  lv_obj_t *identity = lv_obj_create(pageSettings);
  lv_obj_set_pos(identity, 5, 5);
  lv_obj_set_size(identity, 310, 38);
  panel(identity, C_PANEL, C_BORDER, 8, 1);
  lv_obj_t *id = label(identity, "DX-MINI  |  DXM-AAA001", &lv_font_montserrat_12, C_TEXT);
  lv_obj_set_pos(id, 10, 6);
  lv_obj_t *desc = label(identity, "INDUSTRIAL HVAC CONTROLLER", &lv_font_montserrat_8, C_MUTED);
  lv_obj_set_pos(desc, 10, 23);

  lv_obj_t *power = lv_obj_create(pageSettings);
  lv_obj_set_pos(power, 5, 48);
  lv_obj_set_size(power, 152, 61);
  panel(power, C_PANEL, C_BORDER_DIM, 8, 1);
  lv_obj_t *pt = label(power, "SYSTEM POWER", &lv_font_montserrat_10, C_MUTED);
  lv_obj_set_pos(pt, 9, 7);
  settingsPower = label(power, "ON", &lv_font_montserrat_18, C_GREEN);
  lv_obj_set_pos(settingsPower, 9, 29);
  touchButton(power, 82, 14, 61, 37, "TOGGLE", &lv_font_montserrat_10, C_GREEN, UI_ACTION_TOGGLE_POWER, 9);

  lv_obj_t *mode = lv_obj_create(pageSettings);
  lv_obj_set_pos(mode, 163, 48);
  lv_obj_set_size(mode, 152, 61);
  panel(mode, C_PANEL, C_BORDER_DIM, 8, 1);
  lv_obj_t *mt = label(mode, "CONTROL MODE", &lv_font_montserrat_10, C_MUTED);
  lv_obj_set_pos(mt, 9, 7);
  settingsMode = label(mode, "MANUAL", &lv_font_montserrat_16, C_AMBER);
  lv_obj_set_pos(settingsMode, 9, 31);
  touchButton(mode, 91, 14, 52, 37, "CHANGE", &lv_font_montserrat_8, C_AMBER, UI_ACTION_TOGGLE_MODE, 9);

  lv_obj_t *network = lv_obj_create(pageSettings);
  lv_obj_set_pos(network, 5, 114);
  lv_obj_set_size(network, 310, 46);
  panel(network, C_PANEL3, C_BORDER_DIM, 8, 1);
  lv_obj_t *nt = label(network, "NETWORK", &lv_font_montserrat_10, C_MUTED);
  lv_obj_set_pos(nt, 10, 7);
  settingsNetwork = label(network, "OFFLINE", &lv_font_montserrat_16, C_DIM);
  lv_obj_set_pos(settingsNetwork, 10, 22);
  lv_obj_t *ns = label(network, "WiFi / MQTT", &lv_font_montserrat_10, C_DIM);
  lv_obj_align(ns, LV_ALIGN_RIGHT_MID, -12, 0);
}

void createNavigation() {
  lv_obj_t *bar = lv_obj_create(screenMain);
  lv_obj_set_pos(bar, 0, 204);
  lv_obj_set_size(bar, UI_W, 36);
  base(bar);
  bg(bar, C_HEADER);
  lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);

  const char *names[4] = {"HOME", "TEMPS", "OUTPUTS", "SETTINGS"};
  for (int i = 0; i < 4; ++i) {
    navBtn[i] = lv_btn_create(bar);
    lv_obj_set_pos(navBtn[i], i * 80, 0);
    lv_obj_set_size(navBtn[i], 80, 36);
    base(navBtn[i]);
    bg(navBtn[i], C_HEADER);
    lv_obj_set_style_border_width(navBtn[i], 0, LV_PART_MAIN);
    lv_obj_set_style_radius(navBtn[i], 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(navBtn[i], C_PANEL2, LV_STATE_PRESSED);
    lv_obj_add_event_cb(navBtn[i], pageEvent, LV_EVENT_PRESSED, (void *)(intptr_t)i);
    navText[i] = label(navBtn[i], names[i], &lv_font_montserrat_10, i == 0 ? C_CYAN : C_MUTED);
    lv_obj_center(navText[i]);

    navLine[i] = lv_obj_create(bar);
    lv_obj_set_pos(navLine[i], i * 80 + 10, 33);
    lv_obj_set_size(navLine[i], 60, 2);
    base(navLine[i]);
    bg(navLine[i], i == 0 ? C_CYAN : C_HEADER);
    lv_obj_set_style_border_width(navLine[i], 0, LV_PART_MAIN);
  }
}

void createSplash() {
  screenSplash = lv_obj_create(nullptr);
  base(screenSplash);
  bg(screenSplash, C_BG);
  lv_obj_set_style_border_width(screenSplash, 0, LV_PART_MAIN);

  lv_obj_t *badge = lv_obj_create(screenSplash);
  lv_obj_set_pos(badge, 28, 53);
  lv_obj_set_size(badge, 72, 72);
  panel(badge, C_PANEL, C_CYAN, 10, 2);
  lv_obj_t *dx = label(badge, "DX", &lv_font_montserrat_28, C_CYAN);
  lv_obj_center(dx);

  lv_obj_t *title = label(screenSplash, "DX-MINI", &lv_font_montserrat_28, C_TEXT);
  lv_obj_set_pos(title, 119, 56);
  lv_obj_t *sub = label(screenSplash, "INDUSTRIAL HVAC CONTROLLER", &lv_font_montserrat_10, C_MUTED);
  lv_obj_set_pos(sub, 120, 91);

  splashBar = lv_bar_create(screenSplash);
  lv_obj_set_pos(splashBar, 28, 157);
  lv_obj_set_size(splashBar, 264, 8);
  lv_bar_set_range(splashBar, 0, 100);
  lv_bar_set_value(splashBar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(splashBar, C_PANEL2, LV_PART_MAIN);
  lv_obj_set_style_bg_color(splashBar, C_CYAN, LV_PART_INDICATOR);
  lv_obj_set_style_radius(splashBar, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(splashBar, 4, LV_PART_INDICATOR);

  splashStatus = label(screenSplash, "STARTING", &lv_font_montserrat_10, C_MUTED);
  lv_obj_align(splashStatus, LV_ALIGN_BOTTOM_MID, 0, -50);
}

void createMain() {
  screenMain = lv_obj_create(nullptr);
  base(screenMain);
  bg(screenMain, C_BG);
  lv_obj_set_style_border_width(screenMain, 0, LV_PART_MAIN);
  createHeader();
  createHome();
  createTemps();
  createOutputs();
  createSettings();
  createNavigation();

  toast = lv_obj_create(screenMain);
  lv_obj_set_size(toast, 190, 40);
  lv_obj_align(toast, LV_ALIGN_CENTER, 0, 0);
  panel(toast, C_PANEL2, C_CYAN, 9, 2);
  lv_obj_clear_flag(toast, LV_OBJ_FLAG_CLICKABLE);
  toastText = label(toast, "", &lv_font_montserrat_14, C_TEXT);
  lv_obj_center(toastText);
  lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);

  lv_obj_clear_flag(pageHome, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(pageTemps, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(pageOutputs, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(pageSettings, LV_OBJ_FLAG_HIDDEN);
}

void updateHeader() {
  const bool wifi = WiFi.status() == WL_CONNECTED;
  const bool autoMode = MODE_SWITCH != 0;

  lv_label_set_text(lblPower, PowerState ? "ON" : "OFF");
  lv_obj_set_style_text_color(lblPower, PowerState ? C_GREEN : C_RED, LV_PART_MAIN);
  setBorder(chipPower, PowerState ? C_GREEN : C_RED);

  lv_label_set_text(lblWifi, wifi ? "WIFI" : "OFFLINE");
  lv_obj_set_style_text_color(lblWifi, wifi ? C_CYAN : C_DIM, LV_PART_MAIN);
  setBorder(chipWifi, wifi ? C_CYAN : C_BORDER_DIM);

  lv_label_set_text(lblMode, autoMode ? "AUTO" : "MANUAL");
  lv_obj_set_style_text_color(lblMode, autoMode ? C_BLUE : C_AMBER, LV_PART_MAIN);
  setBorder(chipMode, autoMode ? C_BLUE : C_AMBER);

  if (activePage == PAGE_HOME) {
    lv_label_set_text(lblContext, selectedTemp == 0 ? "DISCHARGE AIR" : selectedTemp == 1 ? "RETURN AIR" : "SUPPLY AIR");
  } else if (activePage == PAGE_TEMPS) lv_label_set_text(lblContext, "TEMPERATURES");
  else if (activePage == PAGE_OUTPUTS) lv_label_set_text(lblContext, "OUTPUT CONTROL");
  else lv_label_set_text(lblContext, "SETTINGS");
}

void updateHero() {
  float value = uiTempForDisplay(0, dischargeTemp);
  int setpoint = dischargeSp;
  bool alarm = dischargeTempAlarm;
  bool hasSetpoint = true;
  const char *title = "DISCHARGE AIR";

  if (selectedTemp == 1) {
    value = uiTempForDisplay(1, ReturnTemp);
    setpoint = ReturnSp;
    alarm = returnTempAlarm;
    title = "RETURN AIR";
  } else if (selectedTemp == 2) {
    value = uiTempForDisplay(2, SupplyTemp);
    hasSetpoint = false;
    alarm = false;
    title = "SUPPLY AIR";
  }

  lv_label_set_text(heroTitle, title);
  char valueText[16];
  formatTemp(valueText, sizeof(valueText), value);
  lv_label_set_text(heroValue, valueText);
  lv_obj_align_to(heroUnit, heroValue, LV_ALIGN_OUT_RIGHT_MID, 4, 1);

  if (validTemp(value)) lv_obj_clear_flag(heroUnit, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(heroUnit, LV_OBJ_FLAG_HIDDEN);

  if (hasSetpoint) {
    lv_obj_clear_flag(heroSetCaption, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(heroSetValue, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(heroMinus, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(heroPlus, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_fmt(heroSetValue, "%d C", setpoint);
  } else {
    lv_obj_add_flag(heroSetCaption, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(heroSetValue, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(heroMinus, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(heroPlus, LV_OBJ_FLAG_HIDDEN);
  }

  lv_color_t accent = C_CYAN;
  const char *status = "NORMAL";
  lv_color_t statusColor = C_GREEN;
  if (!PowerState) {
    status = "SYSTEM OFF";
    statusColor = C_RED;
    accent = C_RED;
  } else if (FAULT_SWITCH) {
    status = "FAULT ACTIVE";
    statusColor = C_RED;
    accent = C_RED;
  } else if (!validTemp(value)) {
    status = "SENSOR WAIT";
    statusColor = C_AMBER;
    accent = C_AMBER;
  } else if (alarm) {
    status = "TEMP ALARM";
    statusColor = C_RED;
    accent = C_RED;
  }
  lv_label_set_text(heroStatus, status);
  lv_obj_set_style_text_color(heroStatus, statusColor, LV_PART_MAIN);
  setBorder(hero, accent, 2);
}

void updateHomeOutputs() {
  const bool actual[3] = {EVAP_COOL_SYSTEM_STATUS, AUTO_WASH_SYSTEM_STATUS, UNIT_COMMAND_STATUS};
  const int pending[3] = {EVAP_COOL_SYSTEM_PENDING, AUTO_WASH_SYSTEM_PENDING, UNIT_COMMAND_PENDING};
  const bool manual = MODE_SWITCH == 0;
  for (int i = 0; i < 3; ++i) {
    const bool wait = pending[i] != -1;
    const lv_color_t color = wait ? C_AMBER : (actual[i] ? C_GREEN : C_TEXT);
    lv_label_set_text(outputState[i], wait ? "WAIT" : (actual[i] ? "ON" : "OFF"));
    lv_obj_set_style_text_color(outputState[i], color, LV_PART_MAIN);
    lv_obj_set_style_text_color(lv_obj_get_child(outputCard[i], 0), C_TEXT, LV_PART_MAIN);
    setBorder(outputCard[i], wait ? C_AMBER : (actual[i] ? C_GREEN : C_BORDER), wait || actual[i] ? 2 : 2);
    bg(outputCard[i], wait ? C_PANEL2 : C_PANEL);
    setEnabledVisual(outputCard[i], PowerState && manual);
  }
}

void updateTemps() {
  const float liveValues[3] = {dischargeTemp, ReturnTemp, SupplyTemp};
  const float values[3] = {uiTempForDisplay(0, dischargeTemp), uiTempForDisplay(1, ReturnTemp), uiTempForDisplay(2, SupplyTemp)};
  const bool alarms[3] = {dischargeTempAlarm, returnTempAlarm, false};
  char text[16];
  for (int i = 0; i < 3; ++i) {
    formatTemp(text, sizeof(text), values[i], 1, true);
    lv_label_set_text(tempValue[i], text);
    lv_obj_set_style_text_color(tempValue[i], (validTemp(liveValues[i]) && alarms[i]) ? C_RED : C_TEXT, LV_PART_MAIN);
    setBorder(tempCard[i], selectedTemp == i ? C_CYAN : C_BORDER_DIM, selectedTemp == i ? 2 : 1);
  }

  formatTemp(text, sizeof(text), values[0], 1, true);
  lv_label_set_text(tempPageValue[0], text);
  formatTemp(text, sizeof(text), values[1], 1, true);
  lv_label_set_text(tempPageValue[1], text);
  formatTemp(text, sizeof(text), values[2], 1, true);
  lv_label_set_text(tempPageValue[2], text);
  lv_label_set_text_fmt(tempPageSet[0], "%d C", dischargeSp);
  lv_label_set_text_fmt(tempPageSet[1], "%d C", ReturnSp);
  lv_obj_set_style_text_color(tempPageValue[0], (validTemp(liveValues[0]) && dischargeTempAlarm) ? C_RED : C_TEXT, LV_PART_MAIN);
  lv_obj_set_style_text_color(tempPageValue[1], (validTemp(liveValues[1]) && returnTempAlarm) ? C_RED : C_TEXT, LV_PART_MAIN);
}

void updateOutputs() {
  const bool actual[3] = {EVAP_COOL_SYSTEM_STATUS, AUTO_WASH_SYSTEM_STATUS, UNIT_COMMAND_STATUS};
  const int command[3] = {EVAP_COOL_SYSTEM_SWITCH, AUTO_WASH_SYSTEM_SWITCH, UNIT_COMMAND_SWITCH};
  const int pending[3] = {EVAP_COOL_SYSTEM_PENDING, AUTO_WASH_SYSTEM_PENDING, UNIT_COMMAND_PENDING};
  const bool manual = MODE_SWITCH == 0;

  for (int i = 0; i < 3; ++i) {
    lv_label_set_text(outActual[i], actual[i] ? "ON" : "OFF");
    lv_obj_set_style_text_color(outActual[i], actual[i] ? C_GREEN : C_DIM, LV_PART_MAIN);
    lv_label_set_text(outCommand[i], command[i] ? "ON" : "OFF");
    lv_obj_set_style_text_color(outCommand[i], command[i] ? C_CYAN : C_DIM, LV_PART_MAIN);
    if (pending[i] != -1) {
      lv_label_set_text(outStatus[i], pending[i] ? "START" : "STOP");
      lv_obj_set_style_text_color(outStatus[i], C_AMBER, LV_PART_MAIN);
    } else {
      lv_label_set_text(outStatus[i], "READY");
      lv_obj_set_style_text_color(outStatus[i], C_GREEN, LV_PART_MAIN);
    }
    const lv_color_t color = command[i] ? C_GREEN : C_DIM;
    lv_label_set_text(outButtonText[i], command[i] ? "ON" : "OFF");
    lv_obj_set_style_text_color(outButtonText[i], color, LV_PART_MAIN);
    setBorder(outButton[i], color, 1);
    setEnabledVisual(outButton[i], PowerState && manual);
  }
}

void updateSettings() {
  const bool wifi = WiFi.status() == WL_CONNECTED;
  const bool autoMode = MODE_SWITCH != 0;
  lv_label_set_text(settingsPower, PowerState ? "ON" : "OFF");
  lv_obj_set_style_text_color(settingsPower, PowerState ? C_GREEN : C_RED, LV_PART_MAIN);
  lv_label_set_text(settingsMode, autoMode ? "AUTO" : "MANUAL");
  lv_obj_set_style_text_color(settingsMode, autoMode ? C_BLUE : C_AMBER, LV_PART_MAIN);
  lv_label_set_text(settingsNetwork, wifi ? "ONLINE" : "OFFLINE");
  lv_obj_set_style_text_color(settingsNetwork, wifi ? C_CYAN : C_DIM, LV_PART_MAIN);
}
} // namespace

namespace {
void renderDisplay() {
  if (!screenMain) return;
  updateHeader();
  updateHero();
  updateHomeOutputs();
  updateTemps();
  updateOutputs();
  updateSettings();
}

void processQueuedUiMessages() {
  BootMessage boot{};
  while (bootQueue && xQueueReceive(bootQueue, &boot, 0) == pdTRUE) {
    if (screenSplash) {
      lv_bar_set_value(splashBar, constrain(boot.value, 0, 100), LV_ANIM_OFF);
      lv_label_set_text(splashStatus, boot.status);
    }
  }

  ToastMessage message{};
  while (toastQueue && xQueueReceive(toastQueue, &message, 0) == pdTRUE) {
    showToast(message.text, message.durationMs);
  }
}

void uiTask(void *) {
  lastLvTick = millis();
  constexpr uint32_t MIN_SPLASH_MS = 1200;

  for (;;) {
    const uint32_t now = millis();
    const uint32_t elapsed = now - lastLvTick;
    if (elapsed) {
      lv_tick_inc(elapsed);
      lastLvTick = now;
    }

    processQueuedUiMessages();

    if (mainScreenRequested && !mainScreenVisible &&
        (splashVisibleSince == 0 || (now - splashVisibleSince) >= MIN_SPLASH_MS)) {
      lv_scr_load(screenMain);
      mainScreenVisible = true;
      uiDirtyRequested = true;
      Serial.println("[DISPLAY] DX-MINI main HMI active");
    }

    if (uiDirtyRequested || (now - lastUiRefresh) >= 250) {
      uiDirtyRequested = false;
      lastUiRefresh = now;
      renderDisplay();
    }

    if (toast && toastUntil && (int32_t)(now - toastUntil) >= 0) {
      lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
      toastUntil = 0;
    }

    // lv_timer_handler() services refresh, animations and the pointer input.
    // A short task sleep keeps CPU use low while retaining fast touch response.
    lv_timer_handler();
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(4));
  }
}
} // namespace

UiAction uiTakeAction() {
  UiAction action = UI_ACTION_NONE;
  if (actionQueue) (void)xQueueReceive(actionQueue, &action, 0);
  return action;
}

void updateDisplay() {
  requestUiRefresh();
}

void uiInit() {
  pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(TFT_BACKLIGHT_PIN, LOW);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  Serial.print("[TOUCH] CST820: ");
  if (touch.begin()) {
    Serial.printf("detected at 0x%02X | swapXY=%d invertX=%d invertY=%d\n",
                  touch.address(), DX_TOUCH_SWAP_XY, DX_TOUCH_INVERT_X, DX_TOUCH_INVERT_Y);
  } else {
    Serial.println("NOT DETECTED");
  }

  lv_init();
  lastLvTick = millis();

  if (!allocateDrawBuffer()) {
    // Do not continue into LVGL with a null draw buffer.  Keep the panel
    // dark and return cleanly rather than risking a crash/reboot loop.
    digitalWrite(TFT_BACKLIGHT_PIN, LOW);
    return;
  }
  lv_disp_draw_buf_init(&drawBuf, drawMem, nullptr, UI_W * drawBufferLines);

  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = UI_W;
  dispDrv.ver_res = UI_H;
  dispDrv.flush_cb = flushDisplay;
  dispDrv.draw_buf = &drawBuf;
  lv_disp_drv_register(&dispDrv);

  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = readTouch;
  indevDrv.long_press_time = 320;
  indevDrv.long_press_repeat_time = 90;
  lv_indev_drv_register(&indevDrv);

  actionQueue = xQueueCreate(8, sizeof(UiAction));
  bootQueue = xQueueCreate(1, sizeof(BootMessage));
  toastQueue = xQueueCreate(4, sizeof(ToastMessage));
  if (!actionQueue || !bootQueue || !toastQueue) {
    Serial.println("[DISPLAY] ERROR: UI queue allocation failed");
  }

  createSplash();
  createMain();

  lv_mem_monitor_t lvMem{};
  lv_mem_monitor(&lvMem);
  Serial.printf("[DISPLAY] LVGL free=%u bytes | largest=%u | ESP heap=%u\n",
                static_cast<unsigned>(lvMem.free_size),
                static_cast<unsigned>(lvMem.free_biggest_size),
                static_cast<unsigned>(ESP.getFreeHeap()));

  // Draw the splash completely before enabling the backlight.
  lv_scr_load(screenSplash);
  lv_bar_set_value(splashBar, 5, LV_ANIM_OFF);
  lv_label_set_text(splashStatus, "STARTING");
  lv_timer_handler();
  lv_refr_now(nullptr);
  splashVisibleSince = millis();
  digitalWrite(TFT_BACKLIGHT_PIN, HIGH);

  // Keep all LVGL/TFT access on a dedicated higher-priority task. The Arduino
  // loop can block in TLS/NTP/Wi-Fi without freezing the display or touch.
  BaseType_t created = xTaskCreatePinnedToCore(
      uiTask, "dx_ui", 6144, nullptr, 1, &uiTaskHandle, 1);
  if (created != pdPASS) {
    uiTaskHandle = nullptr;
    Serial.println("[DISPLAY] ERROR: UI task creation failed");
  } else {
    Serial.printf("[DISPLAY] DX-MINI splash visible | UI task active | touch poll=%dms\n",
                  LV_INDEV_DEF_READ_PERIOD);
  }
}

void uiLoop() {
  // Normal path: LVGL is serviced by the dedicated UI task.  If task creation
  // ever fails because of unexpected low memory, gracefully fall back to the
  // original single-threaded servicing so the display still remains usable.
  if (uiTaskHandle) return;

  const uint32_t now = millis();
  const uint32_t elapsed = now - lastLvTick;
  if (elapsed) {
    lv_tick_inc(elapsed);
    lastLvTick = now;
  }

  processQueuedUiMessages();

  constexpr uint32_t MIN_SPLASH_MS = 1200;
  if (mainScreenRequested && !mainScreenVisible &&
      (splashVisibleSince == 0 || (now - splashVisibleSince) >= MIN_SPLASH_MS)) {
    lv_scr_load(screenMain);
    mainScreenVisible = true;
    uiDirtyRequested = true;
    Serial.println("[DISPLAY] DX-MINI main HMI active (fallback)");
  }

  if (uiDirtyRequested || (now - lastUiRefresh) >= 250) {
    uiDirtyRequested = false;
    lastUiRefresh = now;
    renderDisplay();
  }

  if (toast && toastUntil && (int32_t)(now - toastUntil) >= 0) {
    lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
    toastUntil = 0;
  }

  lv_timer_handler();
}

void showStartupScreen() {
  BootMessage message{};
  message.value = 10;
  snprintf(message.status, sizeof(message.status), "%s", "INITIALIZING");
  if (bootQueue) xQueueOverwrite(bootQueue, &message);
  wakeUiTask();
}

void uiSetBootProgress(int value, const char *status) {
  BootMessage message{};
  message.value = constrain(value, 0, 100);
  snprintf(message.status, sizeof(message.status), "%s", status ? status : "");
  if (bootQueue) xQueueOverwrite(bootQueue, &message);
  wakeUiTask();
}

void uiShowMain() {
  mainScreenRequested = true;
  requestUiRefresh();
}

void showCenteredText(const char *text, int seconds) {
  if (!text || !toastQueue) return;
  ToastMessage message{};
  message.durationMs = static_cast<uint32_t>(max(seconds, 1)) * 700U;
  snprintf(message.text, sizeof(message.text), "%s", text);
  (void)xQueueSend(toastQueue, &message, 0);
  wakeUiTask();
}
