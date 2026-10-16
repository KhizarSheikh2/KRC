#include <Arduino.h>

#define DEBUG

// #include <PCA9554.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <PubSubClient.h>  // for MQTT
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "time.h"
#include <WiFi.h>
#include <Preferences.h>
#include <SPI.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <math.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include "esp_task_wdt.h"
#include "esp_idf_version.h"

#define SSL_CLIENT_BUFFER_SIZE 2048
#include <SSLClient.h>


// Arduino-ESP32 2.x / 3.x compatible task watchdog initialization.
void initTaskWatchdogSafe() {
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t wdtConfig = {};
  wdtConfig.timeout_ms = 10000;
  wdtConfig.idle_core_mask = (1U << portNUM_PROCESSORS) - 1U;
  wdtConfig.trigger_panic = true;

  esp_err_t wdtResult = esp_task_wdt_init(&wdtConfig);
  if (wdtResult == ESP_ERR_INVALID_STATE) {
    esp_task_wdt_reconfigure(&wdtConfig);
  }
#else
  esp_task_wdt_init(10, true);
#endif
}

// ── W5500 SPI Pins ────────────────────────────────────────────────
#define W5500_CS    5
#define W5500_MOSI  23
#define W5500_MISO  19
#define W5500_SCLK  18
#define W5500_RST   17  // GPIO4 is Pump relay; GPIO17 is dedicated to W5500 reset

// ── SSLClient entropy: GPIO 34 (floating input-only ADC pin) ─────
#define SSL_ENTROPY_PIN 34

#include "certificates.h"
#include "aws_certificates.h"
#include "variables.h"
#include "wifithread.h"

// ── Controller I/O remapped to avoid the JC2432W328 built-in TFT/touch pins ──
#define FAN_CONTROL_PIN 22
#define PUMP_CONTROL_PIN 4
#define WATER_LEVEL_PIN 16
#define ONE_WIRE_BUS 21

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

enum Mode {
  TEMPERATURE,
  FAN,
  PUMP,
  SCHEDULER,
  NUM_MODES
};

int current_mode = TEMPERATURE;

void beep(int duration = 50, int freq = 2000) {
  // The previous keypad buzzer shared GPIO resources now required by the TFT/W5500.
  // Keep the call sites intact but make audio feedback optional/no-op.
  (void)duration;
  (void)freq;
}

// ============================================================
// JC2432W328 2.8" DISPLAY STACK
// TFT_eSPI = ST7789 transport / framebuffer flush
// LVGL 8.x = complete UI/UX and touch interaction
// Native panel: 240x320, used as 320x240 landscape
// ============================================================

TFT_eSPI tft = TFT_eSPI();

static constexpr uint16_t SCREEN_W = 320;
static constexpr uint16_t SCREEN_H = 240;
static constexpr uint8_t TFT_BACKLIGHT_PIN = 27;

// CST820 / CST816-compatible capacitive touch controller
static constexpr uint8_t TOUCH_ADDR = 0x15;
static constexpr int TOUCH_SDA = 33;
static constexpr int TOUCH_SCL = 32;
static constexpr int TOUCH_RST = 25;

// Landscape touch transform for TFT rotation = 1.
// If a particular panel revision is mirrored, only these three constants need adjustment.
static constexpr bool TOUCH_SWAP_XY = true;
static constexpr bool TOUCH_INVERT_X = false;
static constexpr bool TOUCH_INVERT_Y = true;

static lv_disp_draw_buf_t lvDrawBuf;
static lv_color_t lvBuffer[SCREEN_W * 20];  // 20-line partial buffer = 12.8 KB
static lv_disp_drv_t lvDispDrv;
static lv_indev_drv_t lvInputDrv;
static lv_obj_t* uiScreen = nullptr;
static lv_obj_t* splashScreen = nullptr;
static lv_obj_t* splashProgressBar = nullptr;
static lv_obj_t* splashProgressLabel = nullptr;
static lv_obj_t* splashStatusLabel = nullptr;
static bool uiBuilt = false;
static bool touchDetected = false;
static uint32_t lvLastTickMs = 0;

// ──────────────────────────────────────────────────────────────
// Industrial dark palette
// ──────────────────────────────────────────────────────────────
static const lv_color_t C_BG        = lv_color_hex(0x050B12);
static const lv_color_t C_SURFACE   = lv_color_hex(0x0B1622);
static const lv_color_t C_CARD      = lv_color_hex(0x0D1924);
static const lv_color_t C_CARD_ALT  = lv_color_hex(0x122638);
static const lv_color_t C_BORDER    = lv_color_hex(0x1B3B50);
static const lv_color_t C_GRID      = lv_color_hex(0x193247);
static const lv_color_t C_CYAN      = lv_color_hex(0x24C7E8);
static const lv_color_t C_BLUE      = lv_color_hex(0x2E86FF);
static const lv_color_t C_GREEN     = lv_color_hex(0x2FD69A);
static const lv_color_t C_AMBER     = lv_color_hex(0xF5B74A);
static const lv_color_t C_RED       = lv_color_hex(0xEF6672);
static const lv_color_t C_TEXT      = lv_color_hex(0xF1F7FC);
static const lv_color_t C_MUTED     = lv_color_hex(0x91A9BC);
static const lv_color_t C_DIM       = lv_color_hex(0x587184);
static const lv_color_t C_OFF       = lv_color_hex(0x334657);

static const char MANUAL_MODE_PASSWORD[] = "1234";

enum PasswordTarget : uint8_t {
  PASSWORD_NONE = 0,
  PASSWORD_FAN_MANUAL,
  PASSWORD_PUMP_MANUAL
};

PasswordTarget passwordTarget = PASSWORD_NONE;
String enteredPassword = "";
unsigned long passwordErrorUntil = 0;
static bool setpointHoldDirty = false;

// Persistent UI objects
static lv_obj_t* headerNetworkChip = nullptr;
static lv_obj_t* headerNetworkLabel = nullptr;
static lv_obj_t* headerPowerBtn = nullptr;
static lv_obj_t* headerPowerLabel = nullptr;
static lv_obj_t* pageRoot[NUM_MODES] = {nullptr, nullptr, nullptr, nullptr};
static lv_obj_t* powerOverlay = nullptr;
static lv_obj_t* navBtn[NUM_MODES] = {nullptr, nullptr, nullptr, nullptr};
static lv_obj_t* navLabel[NUM_MODES] = {nullptr, nullptr, nullptr, nullptr};

// Temperature page
static lv_obj_t* tempValueLabel = nullptr;
static lv_obj_t* tempStateLabel = nullptr;
static lv_obj_t* tempSetpointLabel = nullptr;

// Fan page
static lv_obj_t* fanModeBtn = nullptr;
static lv_obj_t* fanModeLabel = nullptr;
static lv_obj_t* fanStateDot = nullptr;
static lv_obj_t* fanStateLabel = nullptr;
static lv_obj_t* fanActionBtn = nullptr;
static lv_obj_t* fanActionLabel = nullptr;
static lv_obj_t* fanInfoLabel = nullptr;

// Pump page
static lv_obj_t* pumpModeBtn = nullptr;
static lv_obj_t* pumpModeLabel = nullptr;
static lv_obj_t* pumpInfoLabel = nullptr;
static lv_obj_t* pumpStateDot = nullptr;
static lv_obj_t* pumpStateLabel = nullptr;
static lv_obj_t* pumpActionBtn = nullptr;
static lv_obj_t* pumpActionLabel = nullptr;
static lv_obj_t* waterLevelLabel = nullptr;
static lv_obj_t* waterLevelBar = nullptr;

// Scheduler page
static lv_obj_t* scheduleStatusDot = nullptr;
static lv_obj_t* scheduleStatusLabel = nullptr;
static lv_obj_t* scheduleMatchLabel = nullptr;
static lv_obj_t* scheduleBtn = nullptr;
static lv_obj_t* scheduleBtnLabel = nullptr;
static lv_obj_t* scheduleSummaryLabel = nullptr;

// Password modal
static lv_obj_t* passwordOverlay = nullptr;
static lv_obj_t* passwordTitle = nullptr;
static lv_obj_t* passwordSubtitle = nullptr;
static lv_obj_t* passwordDots = nullptr;
static lv_obj_t* passwordError = nullptr;
static lv_obj_t* passwordMatrix = nullptr;

// Feedback overlay
static lv_obj_t* feedbackOverlay = nullptr;
static lv_obj_t* feedbackTitle = nullptr;

// ------------------------------------------------------------
// Forward declarations required by a real PlatformIO .cpp build.
// Arduino .ino preprocessing used to generate these automatically;
// main.cpp intentionally declares them explicitly.
// ------------------------------------------------------------
void updateDisplay();
void updateControlOutputs();
void publishJson();
void hidePasswordModal();
void requestManualPassword(PasswordTarget target);
void showCenteredText(const char* text, uint8_t textSize, int yOffset);
void saveMainPower(bool state);
void setFanControlLocal(bool wantAuto);
void setPumpControlLocal(bool wantAuto);
bool adjustSetpoint(int delta);
void commitSetpoint();
void toggleFanManualState();
void togglePumpManualState();
void toggleSchedulerLocal();
void NTP_TIME();
void inputs();
void fan_auto_control();
void pump_auto_control();
void fan_manual_control();
void pump_manual_control();
void fanScheduler();
void updateStartupProgress(uint8_t progress, const char* statusText);

// ──────────────────────────────────────────────────────────────
// Low-level display + touch
// ──────────────────────────────────────────────────────────────
void lvFlushCb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* colorP) {
  const uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
  const uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t*)colorP, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

bool readTouchPortrait(uint16_t& rawX, uint16_t& rawY) {
  if (!touchDetected) return false;

  Wire.beginTransmission((uint8_t)TOUCH_ADDR);
  Wire.write((uint8_t)0x02);  // finger count register
  if (Wire.endTransmission(false) != 0) return false;

  const uint8_t needed = 5;
  const uint8_t received = Wire.requestFrom((uint8_t)TOUCH_ADDR, needed, (uint8_t)true);
  if (received != needed) {
    while (Wire.available()) Wire.read();
    return false;
  }

  const uint8_t fingers = Wire.read();
  const uint8_t xh = Wire.read();
  const uint8_t xl = Wire.read();
  const uint8_t yh = Wire.read();
  const uint8_t yl = Wire.read();

  if ((fingers & 0x0F) == 0) return false;

  rawX = (uint16_t)(((xh & 0x0F) << 8) | xl);
  rawY = (uint16_t)(((yh & 0x0F) << 8) | yl);
  return true;
}

bool readTouchLandscape(uint16_t& x, uint16_t& y) {
  uint16_t rawX = 0;
  uint16_t rawY = 0;
  if (!readTouchPortrait(rawX, rawY)) return false;

  int32_t tx = rawX;
  int32_t ty = rawY;

  if (TOUCH_SWAP_XY) {
    const int32_t tmp = tx;
    tx = ty;
    ty = tmp;
  }

  const int32_t maxX = TOUCH_SWAP_XY ? 319 : 239;
  const int32_t maxY = TOUCH_SWAP_XY ? 239 : 319;

  if (TOUCH_INVERT_X) tx = maxX - tx;
  if (TOUCH_INVERT_Y) ty = maxY - ty;

  tx = constrain(tx, 0, (int32_t)SCREEN_W - 1);
  ty = constrain(ty, 0, (int32_t)SCREEN_H - 1);

  x = (uint16_t)tx;
  y = (uint16_t)ty;
  return true;
}

void lvTouchCb(lv_indev_drv_t* indev, lv_indev_data_t* data) {
  static uint16_t lastX = SCREEN_W / 2;
  static uint16_t lastY = SCREEN_H / 2;

  uint16_t x = 0;
  uint16_t y = 0;
  if (readTouchLandscape(x, y)) {
    lastX = x;
    lastY = y;
    data->state = LV_INDEV_STATE_PR;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }

  data->point.x = lastX;
  data->point.y = lastY;
}

bool touchReadRegister(uint8_t reg, uint8_t& value) {
  Wire.beginTransmission((uint8_t)TOUCH_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;

  const uint8_t received = Wire.requestFrom((uint8_t)TOUCH_ADDR, (uint8_t)1, (uint8_t)true);
  if (received != 1 || !Wire.available()) return false;
  value = (uint8_t)Wire.read();
  return true;
}

bool touchWriteRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission((uint8_t)TOUCH_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission(true) == 0;
}

bool initTouchController() {
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, HIGH);
  delay(5);
  digitalWrite(TOUCH_RST, LOW);
  delay(10);
  digitalWrite(TOUCH_RST, HIGH);
  delay(60);

  Wire.begin(TOUCH_SDA, TOUCH_SCL, 400000);
  Wire.setTimeOut(20);
  Wire.beginTransmission((uint8_t)TOUCH_ADDR);
  const bool ok = (Wire.endTransmission() == 0);

  Serial.printf("[TOUCH] CST controller at 0x%02X: %s\n", TOUCH_ADDR, ok ? "detected" : "not detected");
  if (ok) {
    uint8_t chipId = 0;
    if (touchReadRegister(0xA7, chipId)) {
      Serial.printf("[TOUCH] Chip ID: 0x%02X%s\n", chipId, chipId == 0xB7 ? " (CST820)" : "");
    }
    // Keep the controller awake because this UI intentionally polls I2C and does not use INT.
    touchWriteRegister(0xFE, 0xFF);
  }
  return ok;
}

void serviceDisplayUi() {
  if (!uiScreen) return;

  const uint32_t now = millis();
  if (lvLastTickMs == 0) lvLastTickMs = now;
  const uint32_t elapsed = now - lvLastTickMs;
  if (elapsed > 0) {
    lv_tick_inc(elapsed);
    lvLastTickMs = now;
  }
  lv_timer_handler();

  if (passwordErrorUntil != 0 && now >= passwordErrorUntil) {
    passwordErrorUntil = 0;
    if (passwordError) lv_label_set_text(passwordError, "");
  }
}

void initDisplaySystem() {
  pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(TFT_BACKLIGHT_PIN, LOW);

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  touchDetected = initTouchController();

  lv_init();
  lv_disp_draw_buf_init(&lvDrawBuf, lvBuffer, nullptr, SCREEN_W * 20);

  lv_disp_drv_init(&lvDispDrv);
  lvDispDrv.hor_res = SCREEN_W;
  lvDispDrv.ver_res = SCREEN_H;
  lvDispDrv.flush_cb = lvFlushCb;
  lvDispDrv.draw_buf = &lvDrawBuf;
  lv_disp_drv_register(&lvDispDrv);

  lv_indev_drv_init(&lvInputDrv);
  lvInputDrv.type = LV_INDEV_TYPE_POINTER;
  lvInputDrv.read_cb = lvTouchCb;
  lv_indev_drv_register(&lvInputDrv);

  lvLastTickMs = millis();
  digitalWrite(TFT_BACKLIGHT_PIN, HIGH);
}

// ──────────────────────────────────────────────────────────────
// LVGL styling helpers
// ──────────────────────────────────────────────────────────────
void noScroll(lv_obj_t* obj) {
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_outline_width(obj, 0, LV_PART_MAIN);
}

void stylePanel(lv_obj_t* obj, lv_color_t fill = C_CARD, int radius = 10) {
  noScroll(obj);
  lv_obj_set_style_bg_color(obj, fill, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(obj, C_BORDER, LV_PART_MAIN);
  lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
  lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, int x, int y,
                    const lv_font_t* font, lv_color_t color) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
  return label;
}

lv_obj_t* makeCard(lv_obj_t* parent, int x, int y, int w, int h, const char* caption) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  stylePanel(card, C_CARD, 10);

  lv_obj_t* accent = lv_obj_create(card);
  lv_obj_set_pos(accent, 0, 0);
  lv_obj_set_size(accent, 4, h);
  lv_obj_set_style_bg_color(accent, C_CYAN, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(accent, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(accent, 0, LV_PART_MAIN);
  noScroll(accent);

  lv_obj_t* title = makeLabel(card, caption, 12, 8, &lv_font_montserrat_12, C_TEXT);
  lv_obj_set_width(title, w - 20);
  return card;
}

void setButtonColors(lv_obj_t* btn, lv_color_t fill, lv_color_t border, lv_color_t pressed) {
  lv_obj_set_style_bg_color(btn, fill, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(btn, pressed, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(btn, border, LV_PART_MAIN);
  lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
  noScroll(btn);
}

lv_obj_t* makeButton(lv_obj_t* parent, int x, int y, int w, int h,
                     const char* text, lv_color_t fill, lv_color_t textColor,
                     lv_event_cb_t cb, void* userData = nullptr) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, w, h);
  setButtonColors(btn, fill, C_BORDER, C_CARD_ALT);
  if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, userData);

  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, textColor, LV_PART_MAIN);
  lv_obj_center(label);
  return btn;
}

lv_obj_t* makeStatusDot(lv_obj_t* parent, int x, int y, lv_color_t color) {
  lv_obj_t* dot = lv_obj_create(parent);
  lv_obj_set_pos(dot, x, y);
  lv_obj_set_size(dot, 12, 12);
  lv_obj_set_style_bg_color(dot, color, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(dot, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_border_opa(dot, LV_OPA_20, LV_PART_MAIN);
  lv_obj_set_style_border_width(dot, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  noScroll(dot);
  return dot;
}

// ──────────────────────────────────────────────────────────────
// UI event callbacks
// ──────────────────────────────────────────────────────────────
void onPowerPressed(lv_event_t* e) {
  saveMainPower(!main_control);
}

void onNavPressed(lv_event_t* e) {
  const intptr_t idx = (intptr_t)lv_event_get_user_data(e);
  if (idx >= 0 && idx < NUM_MODES) {
    current_mode = (int)idx;
    beep(25, 2100);
    updateStartupProgress(100, "System ready");
  delay(250);
  updateDisplay();
  }
}

void onSetpointMinus(lv_event_t* e) {
  const lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_SHORT_CLICKED) {
    if (adjustSetpoint(-1)) commitSetpoint();
  } else if (code == LV_EVENT_LONG_PRESSED_REPEAT) {
    if (adjustSetpoint(-1)) setpointHoldDirty = true;
  } else if (code == LV_EVENT_RELEASED) {
    if (setpointHoldDirty) {
      commitSetpoint();
      setpointHoldDirty = false;
    }
  }
}

void onSetpointPlus(lv_event_t* e) {
  const lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_SHORT_CLICKED) {
    if (adjustSetpoint(+1)) commitSetpoint();
  } else if (code == LV_EVENT_LONG_PRESSED_REPEAT) {
    if (adjustSetpoint(+1)) setpointHoldDirty = true;
  } else if (code == LV_EVENT_RELEASED) {
    if (setpointHoldDirty) {
      commitSetpoint();
      setpointHoldDirty = false;
    }
  }
}

void onFanModePressed(lv_event_t* e) {
  // fan_control: true=AUTO, false=MANUAL. Entering MANUAL is protected.
  if (fan_control) {
    if (timeschen == 1) {
      showCenteredText("DISABLE SCHEDULER FIRST", 1, 0);
      beep(120, 1500);
      delay(450);
      updateDisplay();
      return;
    }
    requestManualPassword(PASSWORD_FAN_MANUAL);
  } else {
    setFanControlLocal(true);
  }
}

void onFanActionPressed(lv_event_t* e) { toggleFanManualState(); }

void onPumpModePressed(lv_event_t* e) {
  // pump_control: true=AUTO, false=MANUAL. Entering MANUAL is protected.
  if (pump_control) requestManualPassword(PASSWORD_PUMP_MANUAL);
  else setPumpControlLocal(true);
}

void onPumpActionPressed(lv_event_t* e) { togglePumpManualState(); }
void onSchedulePressed(lv_event_t* e) { toggleSchedulerLocal(); }

void refreshPasswordDots() {
  if (!passwordDots) return;
  String dots;
  for (int i = 0; i < 4; ++i) dots += (i < (int)enteredPassword.length()) ? "* " : "o ";
  dots.trim();
  lv_label_set_text(passwordDots, dots.c_str());
}

void validateManualPassword() {
  if (enteredPassword == MANUAL_MODE_PASSWORD) {
    const PasswordTarget target = passwordTarget;
    hidePasswordModal();
    showCenteredText("ACCESS GRANTED", 2, 0);
    beep(80, 2200);
    if (target == PASSWORD_FAN_MANUAL) setFanControlLocal(false);
    else if (target == PASSWORD_PUMP_MANUAL) setPumpControlLocal(false);
    return;
  }

  enteredPassword = "";
  refreshPasswordDots();
  passwordErrorUntil = millis() + 1400;
  lv_label_set_text(passwordError, "Incorrect PIN. Try again.");
  lv_obj_set_style_text_color(passwordError, C_RED, LV_PART_MAIN);
  beep(160, 1000);
  lv_refr_now(nullptr);
}

void onPasswordKey(lv_event_t* e) {
  const uint16_t id = lv_btnmatrix_get_selected_btn(passwordMatrix);
  if (id == LV_BTNMATRIX_BTN_NONE) return;
  const char* txt = lv_btnmatrix_get_btn_text(passwordMatrix, id);
  if (!txt) return;

  beep(20, 2200);
  lv_label_set_text(passwordError, "");
  passwordErrorUntil = 0;

  if (strcmp(txt, "C") == 0) {
    hidePasswordModal();
    updateDisplay();
    return;
  }

  if (strcmp(txt, "<") == 0) {
    if (enteredPassword.length() > 0) enteredPassword.remove(enteredPassword.length() - 1);
    refreshPasswordDots();
    return;
  }

  if (enteredPassword.length() < 4 && txt[0] >= '0' && txt[0] <= '9') {
    enteredPassword += txt[0];
    refreshPasswordDots();
    if (enteredPassword.length() == 4) validateManualPassword();
  }
}

// ──────────────────────────────────────────────────────────────
// Screen construction
// ──────────────────────────────────────────────────────────────
void buildHeader() {
  lv_obj_t* header = lv_obj_create(uiScreen);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_size(header, SCREEN_W, 40);
  stylePanel(header, C_SURFACE, 0);
  lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);

  lv_obj_t* stripe = lv_obj_create(header);
  lv_obj_set_pos(stripe, 0, 38);
  lv_obj_set_size(stripe, SCREEN_W, 2);
  lv_obj_set_style_bg_color(stripe, C_CYAN, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(stripe, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(stripe, 0, LV_PART_MAIN);
  noScroll(stripe);

  makeLabel(header, "WATER TANK", 8, 2, &lv_font_montserrat_20, C_TEXT);
  makeLabel(header, "COOLING SYSTEM", 11, 22, &lv_font_montserrat_12, C_CYAN);

  headerNetworkChip = lv_obj_create(header);
  lv_obj_set_pos(headerNetworkChip, 186, 8);
  lv_obj_set_size(headerNetworkChip, 58, 24);
  stylePanel(headerNetworkChip, C_CARD, 12);

  makeStatusDot(headerNetworkChip, 7, 6, C_DIM);
  headerNetworkLabel = makeLabel(headerNetworkChip, "OFF", 22, 5, &lv_font_montserrat_12, C_MUTED);

  headerPowerBtn = lv_btn_create(header);
  lv_obj_set_pos(headerPowerBtn, 252, 7);
  lv_obj_set_size(headerPowerBtn, 60, 26);
  setButtonColors(headerPowerBtn, C_OFF, C_BORDER, C_CARD_ALT);
  lv_obj_add_event_cb(headerPowerBtn, onPowerPressed, LV_EVENT_CLICKED, nullptr);
  headerPowerLabel = lv_label_create(headerPowerBtn);
  lv_label_set_text(headerPowerLabel, "OFF");
  lv_obj_set_style_text_font(headerPowerLabel, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(headerPowerLabel, C_TEXT, LV_PART_MAIN);
  lv_obj_center(headerPowerLabel);
}

void buildTemperaturePage() {
  lv_obj_t* page = lv_obj_create(uiScreen);
  pageRoot[TEMPERATURE] = page;
  lv_obj_set_pos(page, 0, 40);
  lv_obj_set_size(page, 320, 160);
  stylePanel(page, C_BG, 0);
  lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);

  lv_obj_t* tempCard = makeCard(page, 6, 6, 151, 150, "WATER TEMP");
  tempValueLabel = makeLabel(tempCard, "--.- C", 10, 42, &lv_font_montserrat_28, C_TEXT);
  lv_obj_set_width(tempValueLabel, 131);
  lv_obj_set_style_text_align(tempValueLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  tempStateLabel = makeLabel(tempCard, "SENSOR CHECK", 10, 88, &lv_font_montserrat_12, C_MUTED);
  lv_obj_set_width(tempStateLabel, 131);
  lv_obj_set_style_text_align(tempStateLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  lv_obj_t* tempLine = lv_obj_create(tempCard);
  lv_obj_set_pos(tempLine, 16, 122);
  lv_obj_set_size(tempLine, 119, 4);
  lv_obj_set_style_bg_color(tempLine, C_GRID, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(tempLine, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(tempLine, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(tempLine, 2, LV_PART_MAIN);
  noScroll(tempLine);

  lv_obj_t* spCard = makeCard(page, 163, 6, 151, 150, "COOLING SETPOINT");
  tempSetpointLabel = makeLabel(spCard, "25 C", 10, 40, &lv_font_montserrat_28, C_CYAN);
  lv_obj_set_width(tempSetpointLabel, 131);
  lv_obj_set_style_text_align(tempSetpointLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  // Setpoint buttons: tap = one step, hold = continuous repeat.
  lv_obj_t* setpointMinusBtn = makeButton(spCard, 18, 86, 50, 38, "-", C_CARD_ALT, C_TEXT, nullptr);
  lv_obj_add_event_cb(setpointMinusBtn, onSetpointMinus, LV_EVENT_SHORT_CLICKED, nullptr);
  lv_obj_add_event_cb(setpointMinusBtn, onSetpointMinus, LV_EVENT_LONG_PRESSED_REPEAT, nullptr);
  lv_obj_add_event_cb(setpointMinusBtn, onSetpointMinus, LV_EVENT_RELEASED, nullptr);

  lv_obj_t* setpointPlusBtn = makeButton(spCard, 82, 86, 50, 38, "+", C_BLUE, C_TEXT, nullptr);
  lv_obj_add_event_cb(setpointPlusBtn, onSetpointPlus, LV_EVENT_SHORT_CLICKED, nullptr);
  lv_obj_add_event_cb(setpointPlusBtn, onSetpointPlus, LV_EVENT_LONG_PRESSED_REPEAT, nullptr);
  lv_obj_add_event_cb(setpointPlusBtn, onSetpointPlus, LV_EVENT_RELEASED, nullptr);
  lv_obj_t* rangeLabel = makeLabel(spCard, "RANGE 10-50 C", 10, 129, &lv_font_montserrat_12, C_DIM);
  lv_obj_set_width(rangeLabel, 131);
  lv_obj_set_style_text_align(rangeLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

void buildFanPage() {
  lv_obj_t* page = lv_obj_create(uiScreen);
  pageRoot[FAN] = page;
  lv_obj_set_pos(page, 0, 40);
  lv_obj_set_size(page, 320, 160);
  stylePanel(page, C_BG, 0);
  lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);

  lv_obj_t* modeCard = makeCard(page, 6, 6, 151, 150, "FAN MODE");
  fanModeBtn = makeButton(modeCard, 15, 42, 121, 44, "AUTO", C_BLUE, C_TEXT, onFanModePressed);
  fanModeLabel = lv_obj_get_child(fanModeBtn, 0);
  fanInfoLabel = makeLabel(modeCard, "MANUAL needs PIN", 15, 107, &lv_font_montserrat_12, C_MUTED);
  lv_label_set_long_mode(fanInfoLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(fanInfoLabel, 121);
  lv_obj_set_style_text_align(fanInfoLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  lv_obj_t* stateCard = makeCard(page, 163, 6, 151, 150, "FAN STATUS");
  fanStateDot = makeStatusDot(stateCard, 18, 45, C_OFF);
  fanStateLabel = makeLabel(stateCard, "OFF", 42, 35, &lv_font_montserrat_20, C_MUTED);
  fanActionBtn = makeButton(stateCard, 15, 94, 121, 40, "TURN ON", C_GREEN, C_BG, onFanActionPressed);
  fanActionLabel = lv_obj_get_child(fanActionBtn, 0);
}

void buildPumpPage() {
  lv_obj_t* page = lv_obj_create(uiScreen);
  pageRoot[PUMP] = page;
  lv_obj_set_pos(page, 0, 40);
  lv_obj_set_size(page, 320, 160);
  stylePanel(page, C_BG, 0);
  lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);

  lv_obj_t* modeCard = makeCard(page, 6, 6, 151, 150, "PUMP MODE");
  pumpModeBtn = makeButton(modeCard, 15, 42, 121, 44, "AUTO", C_BLUE, C_TEXT, onPumpModePressed);
  pumpModeLabel = lv_obj_get_child(pumpModeBtn, 0);
  pumpInfoLabel = makeLabel(modeCard, "MANUAL needs PIN", 15, 107, &lv_font_montserrat_12, C_MUTED);
  lv_label_set_long_mode(pumpInfoLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(pumpInfoLabel, 121);
  lv_obj_set_style_text_align(pumpInfoLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  lv_obj_t* stateCard = makeCard(page, 163, 6, 151, 150, "PUMP STATUS");
  pumpStateDot = makeStatusDot(stateCard, 18, 39, C_OFF);
  pumpStateLabel = makeLabel(stateCard, "OFF", 42, 29, &lv_font_montserrat_20, C_MUTED);
  waterLevelLabel = makeLabel(stateCard, "LEVEL --", 18, 66, &lv_font_montserrat_12, C_MUTED);

  waterLevelBar = lv_bar_create(stateCard);
  lv_obj_set_pos(waterLevelBar, 18, 84);
  lv_obj_set_size(waterLevelBar, 115, 7);
  lv_bar_set_range(waterLevelBar, 0, 100);
  lv_obj_set_style_bg_color(waterLevelBar, C_GRID, LV_PART_MAIN);
  lv_obj_set_style_bg_color(waterLevelBar, C_CYAN, LV_PART_INDICATOR);
  lv_obj_set_style_radius(waterLevelBar, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(waterLevelBar, 4, LV_PART_INDICATOR);

  pumpActionBtn = makeButton(stateCard, 15, 103, 121, 33, "TURN ON", C_GREEN, C_BG, onPumpActionPressed);
  pumpActionLabel = lv_obj_get_child(pumpActionBtn, 0);
}

void buildSchedulerPage() {
  lv_obj_t* page = lv_obj_create(uiScreen);
  pageRoot[SCHEDULER] = page;
  lv_obj_set_pos(page, 0, 40);
  lv_obj_set_size(page, 320, 160);
  stylePanel(page, C_BG, 0);
  lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);

  lv_obj_t* card = makeCard(page, 6, 6, 308, 150, "FAN SCHEDULER");
  scheduleStatusDot = makeStatusDot(card, 18, 41, C_OFF);
  scheduleStatusLabel = makeLabel(card, "DISABLED", 42, 30, &lv_font_montserrat_20, C_MUTED);
  scheduleMatchLabel = makeLabel(card, "CURRENT SLOT: --", 18, 66, &lv_font_montserrat_12, C_MUTED);
  scheduleSummaryLabel = makeLabel(card, "Scheduler keeps fan in AUTO", 18, 86, &lv_font_montserrat_12, C_DIM);

  scheduleBtn = makeButton(card, 18, 108, 272, 32, "ENABLE SCHEDULER", C_GREEN, C_BG, onSchedulePressed);
  scheduleBtnLabel = lv_obj_get_child(scheduleBtn, 0);
}

void buildBottomNav() {
  lv_obj_t* nav = lv_obj_create(uiScreen);
  lv_obj_set_pos(nav, 0, 200);
  lv_obj_set_size(nav, 320, 40);
  stylePanel(nav, C_SURFACE, 0);
  lv_obj_set_style_border_width(nav, 0, LV_PART_MAIN);

  static const char* names[NUM_MODES] = {"TEMP", "FAN", "PUMP", "SCHED"};
  for (int i = 0; i < NUM_MODES; ++i) {
    navBtn[i] = lv_btn_create(nav);
    lv_obj_set_pos(navBtn[i], i * 80, 0);
    lv_obj_set_size(navBtn[i], 80, 40);
    lv_obj_set_style_bg_color(navBtn[i], C_SURFACE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(navBtn[i], LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(navBtn[i], 0, LV_PART_MAIN);
    lv_obj_set_style_radius(navBtn[i], 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(navBtn[i], 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(navBtn[i], 0, LV_PART_MAIN);
    lv_obj_add_event_cb(navBtn[i], onNavPressed, LV_EVENT_CLICKED, (void*)(intptr_t)i);

    navLabel[i] = lv_label_create(navBtn[i]);
    lv_label_set_text(navLabel[i], names[i]);
    lv_obj_set_style_text_font(navLabel[i], &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(navLabel[i], C_MUTED, LV_PART_MAIN);
    lv_obj_center(navLabel[i]);
  }
}

void buildPowerOverlay() {
  powerOverlay = lv_obj_create(uiScreen);
  lv_obj_set_pos(powerOverlay, 0, 40);
  lv_obj_set_size(powerOverlay, 320, 160);
  lv_obj_set_style_bg_color(powerOverlay, C_BG, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(powerOverlay, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(powerOverlay, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(powerOverlay, 0, LV_PART_MAIN);
  noScroll(powerOverlay);
  lv_obj_add_flag(powerOverlay, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* card = lv_obj_create(powerOverlay);
  lv_obj_set_pos(card, 34, 22);
  lv_obj_set_size(card, 252, 116);
  stylePanel(card, C_CARD, 12);
  lv_obj_set_style_border_color(card, C_RED, LV_PART_MAIN);

  lv_obj_t* ring = lv_obj_create(card);
  lv_obj_set_pos(ring, 17, 28);
  lv_obj_set_size(ring, 50, 50);
  lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_color(ring, C_RED, LV_PART_MAIN);
  lv_obj_set_style_border_width(ring, 3, LV_PART_MAIN);
  lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  noScroll(ring);

  lv_obj_t* stem = lv_obj_create(card);
  lv_obj_set_pos(stem, 39, 18);
  lv_obj_set_size(stem, 6, 30);
  lv_obj_set_style_bg_color(stem, C_RED, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(stem, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(stem, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(stem, 3, LV_PART_MAIN);
  noScroll(stem);

  makeLabel(card, "SYSTEM OFF", 84, 25, &lv_font_montserrat_20, C_TEXT);
  makeLabel(card, "Fan and pump relays forced LOW", 84, 57, &lv_font_montserrat_12, C_MUTED);
  makeLabel(card, "Tap ON in header to resume", 84, 78, &lv_font_montserrat_12, C_CYAN);

  lv_obj_add_flag(powerOverlay, LV_OBJ_FLAG_HIDDEN);
}

void buildPasswordModal() {
  passwordOverlay = lv_obj_create(uiScreen);
  lv_obj_set_pos(passwordOverlay, 0, 0);
  lv_obj_set_size(passwordOverlay, 320, 240);
  lv_obj_set_style_bg_color(passwordOverlay, lv_color_hex(0x02070B), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(passwordOverlay, LV_OPA_90, LV_PART_MAIN);
  lv_obj_set_style_border_width(passwordOverlay, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(passwordOverlay, 0, LV_PART_MAIN);
  noScroll(passwordOverlay);
  lv_obj_add_flag(passwordOverlay, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* panel = lv_obj_create(passwordOverlay);
  lv_obj_set_pos(panel, 28, 10);
  lv_obj_set_size(panel, 264, 220);
  stylePanel(panel, C_CARD, 12);

  passwordTitle = makeLabel(panel, "MANUAL MODE ACCESS", 16, 9, &lv_font_montserrat_14, C_TEXT);
  passwordSubtitle = makeLabel(panel, "FAN AUTO > MANUAL", 16, 32, &lv_font_montserrat_12, C_CYAN);
  passwordDots = makeLabel(panel, "o o o o", 82, 51, &lv_font_montserrat_20, C_CYAN);
  passwordError = makeLabel(panel, "", 16, 76, &lv_font_montserrat_12, C_RED);

  static const char* keyMap[] = {
    "1", "2", "3", "\n",
    "4", "5", "6", "\n",
    "7", "8", "9", "\n",
    "C", "0", "<", ""
  };

  passwordMatrix = lv_btnmatrix_create(panel);
  lv_btnmatrix_set_map(passwordMatrix, keyMap);
  lv_obj_set_pos(passwordMatrix, 16, 96);
  lv_obj_set_size(passwordMatrix, 232, 110);
  lv_obj_set_style_bg_color(passwordMatrix, C_CARD, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(passwordMatrix, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(passwordMatrix, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(passwordMatrix, 3, LV_PART_MAIN);
  lv_obj_set_style_pad_gap(passwordMatrix, 4, LV_PART_MAIN);
  lv_obj_set_style_bg_color(passwordMatrix, C_CARD_ALT, LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(passwordMatrix, C_BLUE, LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_border_color(passwordMatrix, C_BORDER, LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(passwordMatrix, 1, LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(passwordMatrix, 6, LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(passwordMatrix, C_TEXT, LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(passwordMatrix, &lv_font_montserrat_14, LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_add_event_cb(passwordMatrix, onPasswordKey, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_add_flag(passwordOverlay, LV_OBJ_FLAG_HIDDEN);
}

void buildFeedbackOverlay() {
  feedbackOverlay = lv_obj_create(uiScreen);
  lv_obj_set_pos(feedbackOverlay, 0, 0);
  lv_obj_set_size(feedbackOverlay, 320, 240);
  lv_obj_set_style_bg_color(feedbackOverlay, lv_color_hex(0x02070B), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(feedbackOverlay, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_border_width(feedbackOverlay, 0, LV_PART_MAIN);
  noScroll(feedbackOverlay);
  lv_obj_add_flag(feedbackOverlay, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* box = lv_obj_create(feedbackOverlay);
  lv_obj_set_pos(box, 38, 78);
  lv_obj_set_size(box, 244, 84);
  stylePanel(box, C_CARD_ALT, 12);
  lv_obj_set_style_border_color(box, C_CYAN, LV_PART_MAIN);

  feedbackTitle = lv_label_create(box);
  lv_obj_set_width(feedbackTitle, 220);
  lv_obj_set_style_text_align(feedbackTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_font(feedbackTitle, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_color(feedbackTitle, C_TEXT, LV_PART_MAIN);
  lv_obj_center(feedbackTitle);

  lv_obj_add_flag(feedbackOverlay, LV_OBJ_FLAG_HIDDEN);
}

void buildMainUi() {
  if (uiBuilt) return;

  uiScreen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(uiScreen, C_BG, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(uiScreen, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(uiScreen, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(uiScreen, 0, LV_PART_MAIN);
  noScroll(uiScreen);

  buildHeader();
  buildTemperaturePage();
  buildFanPage();
  buildPumpPage();
  buildSchedulerPage();
  buildBottomNav();
  buildPowerOverlay();
  buildPasswordModal();
  buildFeedbackOverlay();

  lv_scr_load(uiScreen);
  uiBuilt = true;

  if (splashScreen && splashScreen != uiScreen) {
    lv_obj_del_async(splashScreen);
    splashScreen = nullptr;
  }
}

// ──────────────────────────────────────────────────────────────
// UI state refresh
// ──────────────────────────────────────────────────────────────
void setObjectHidden(lv_obj_t* obj, bool hidden) {
  if (!obj) return;
  if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

void refreshHeader() {
  if (!uiBuilt) return;

  const bool wifiUp = (WiFi.status() == WL_CONNECTED);
  const char* networkText = eth_connected ? "ETH" : (wifiUp ? "WIFI" : "OFF");
  const lv_color_t networkColor = (eth_connected || wifiUp) ? C_GREEN : C_RED;

  lv_label_set_text(headerNetworkLabel, networkText);
  lv_obj_set_style_text_color(headerNetworkLabel, networkColor, LV_PART_MAIN);
  lv_obj_set_style_border_color(headerNetworkChip, networkColor, LV_PART_MAIN);

  // First child is the status dot created in buildHeader().
  lv_obj_t* networkDot = lv_obj_get_child(headerNetworkChip, 0);
  if (networkDot) lv_obj_set_style_bg_color(networkDot, networkColor, LV_PART_MAIN);

  lv_label_set_text(headerPowerLabel, main_control ? "ON" : "OFF");
  setButtonColors(headerPowerBtn, main_control ? C_GREEN : C_OFF,
                  main_control ? C_GREEN : C_BORDER,
                  main_control ? lv_color_hex(0x229C71) : C_CARD_ALT);
  lv_obj_set_style_text_color(headerPowerLabel, main_control ? C_BG : C_TEXT, LV_PART_MAIN);
}

void refreshTemperaturePage() {
  if (!uiBuilt) return;

  String tempText;
  if (temper > -100 && temper < 125 && temp1 != "0") tempText = String((float)temper, 1) + " C";
  else tempText = "--.- C";
  lv_label_set_text(tempValueLabel, tempText.c_str());

  if (temp1 == "0") {
    lv_label_set_text(tempStateLabel, "SENSOR CHECK");
    lv_obj_set_style_text_color(tempStateLabel, C_AMBER, LV_PART_MAIN);
  } else if (temper > (int)sp + hyst) {
    lv_label_set_text(tempStateLabel, "COOLING DEMAND");
    lv_obj_set_style_text_color(tempStateLabel, C_CYAN, LV_PART_MAIN);
  } else {
    lv_label_set_text(tempStateLabel, "TEMPERATURE STABLE");
    lv_obj_set_style_text_color(tempStateLabel, C_GREEN, LV_PART_MAIN);
  }

  const String spText = String(sp) + " C";
  lv_label_set_text(tempSetpointLabel, spText.c_str());
}

void refreshFanPage() {
  if (!uiBuilt) return;

  lv_label_set_text(fanModeLabel, fan_control ? "AUTO" : "MANUAL");
  setButtonColors(fanModeBtn, fan_control ? C_BLUE : C_AMBER,
                  fan_control ? C_BLUE : C_AMBER, C_CARD_ALT);
  lv_obj_set_style_text_color(fanModeLabel, fan_control ? C_TEXT : C_BG, LV_PART_MAIN);

  lv_obj_set_style_bg_color(fanStateDot, fanstate ? C_GREEN : C_OFF, LV_PART_MAIN);
  lv_label_set_text(fanStateLabel, fanstate ? "ON" : "OFF");
  lv_obj_set_style_text_color(fanStateLabel, fanstate ? C_GREEN : C_MUTED, LV_PART_MAIN);

  const bool manualAllowed = (!fan_control && timeschen == 0);
  if (manualAllowed) {
    lv_obj_clear_state(fanActionBtn, LV_STATE_DISABLED);
    lv_label_set_text(fanActionLabel, fanstate ? "TURN OFF" : "TURN ON");
    setButtonColors(fanActionBtn, fanstate ? C_RED : C_GREEN,
                    fanstate ? C_RED : C_GREEN, C_CARD_ALT);
    lv_obj_set_style_text_color(fanActionLabel, C_BG, LV_PART_MAIN);
  } else {
    lv_obj_add_state(fanActionBtn, LV_STATE_DISABLED);
    lv_label_set_text(fanActionLabel, timeschen ? "SCHEDULED" : "AUTO MODE");
    setButtonColors(fanActionBtn, C_CARD_ALT, C_BORDER, C_CARD_ALT);
    lv_obj_set_style_text_color(fanActionLabel, C_MUTED, LV_PART_MAIN);
  }

  if (timeschen) {
    lv_label_set_text(fanInfoLabel, "Scheduler active");
    lv_obj_set_style_text_color(fanInfoLabel, C_AMBER, LV_PART_MAIN);
  } else {
    lv_label_set_text(fanInfoLabel, fan_control ? "MANUAL needs PIN" : "Tap to return AUTO");
    lv_obj_set_style_text_color(fanInfoLabel, C_MUTED, LV_PART_MAIN);
  }
}

void refreshPumpPage() {
  if (!uiBuilt) return;

  lv_label_set_text(pumpModeLabel, pump_control ? "AUTO" : "MANUAL");
  setButtonColors(pumpModeBtn, pump_control ? C_BLUE : C_AMBER,
                  pump_control ? C_BLUE : C_AMBER, C_CARD_ALT);
  lv_obj_set_style_text_color(pumpModeLabel, pump_control ? C_TEXT : C_BG, LV_PART_MAIN);

  lv_obj_set_style_bg_color(pumpStateDot, pumpstate ? C_GREEN : C_OFF, LV_PART_MAIN);
  lv_label_set_text(pumpStateLabel, pumpstate ? "ON" : "OFF");
  lv_obj_set_style_text_color(pumpStateLabel, pumpstate ? C_GREEN : C_MUTED, LV_PART_MAIN);

  lv_label_set_text(waterLevelLabel, waterlevel ? "LEVEL FULL" : "LEVEL LOW");
  lv_obj_set_style_text_color(waterLevelLabel, waterlevel ? C_GREEN : C_AMBER, LV_PART_MAIN);

  if (pumpInfoLabel) {
    lv_label_set_text(pumpInfoLabel, pump_control ? "MANUAL needs PIN" : "Tap to return AUTO");
    lv_obj_set_style_text_color(pumpInfoLabel, C_MUTED, LV_PART_MAIN);
  }
  lv_bar_set_value(waterLevelBar, waterlevel ? 100 : 15, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(waterLevelBar, waterlevel ? C_GREEN : C_AMBER, LV_PART_INDICATOR);

  if (!pump_control) {
    lv_obj_clear_state(pumpActionBtn, LV_STATE_DISABLED);
    lv_label_set_text(pumpActionLabel, pumpstate ? "TURN OFF" : "TURN ON");
    setButtonColors(pumpActionBtn, pumpstate ? C_RED : C_GREEN,
                    pumpstate ? C_RED : C_GREEN, C_CARD_ALT);
    lv_obj_set_style_text_color(pumpActionLabel, C_BG, LV_PART_MAIN);
  } else {
    lv_obj_add_state(pumpActionBtn, LV_STATE_DISABLED);
    lv_label_set_text(pumpActionLabel, "AUTO MODE");
    setButtonColors(pumpActionBtn, C_CARD_ALT, C_BORDER, C_CARD_ALT);
    lv_obj_set_style_text_color(pumpActionLabel, C_MUTED, LV_PART_MAIN);
  }
}

void refreshSchedulerPage() {
  if (!uiBuilt) return;

  lv_obj_set_style_bg_color(scheduleStatusDot, timeschen ? C_GREEN : C_OFF, LV_PART_MAIN);
  lv_label_set_text(scheduleStatusLabel, timeschen ? "ENABLED" : "DISABLED");
  lv_obj_set_style_text_color(scheduleStatusLabel, timeschen ? C_GREEN : C_MUTED, LV_PART_MAIN);

  if (timeschen) {
    lv_label_set_text(scheduleMatchLabel, tmatched ? "CURRENT SLOT: ACTIVE" : "CURRENT SLOT: INACTIVE");
    lv_obj_set_style_text_color(scheduleMatchLabel, tmatched ? C_GREEN : C_AMBER, LV_PART_MAIN);
    lv_label_set_text(scheduleSummaryLabel, "Fan stays in AUTO while scheduler runs");
  } else {
    lv_label_set_text(scheduleMatchLabel, "CURRENT SLOT: --");
    lv_obj_set_style_text_color(scheduleMatchLabel, C_MUTED, LV_PART_MAIN);
    lv_label_set_text(scheduleSummaryLabel, "Scheduler keeps fan in AUTO");
  }

  lv_label_set_text(scheduleBtnLabel, timeschen ? "DISABLE SCHEDULER" : "ENABLE SCHEDULER");
  setButtonColors(scheduleBtn, timeschen ? C_RED : C_GREEN,
                  timeschen ? C_RED : C_GREEN, C_CARD_ALT);
  lv_obj_set_style_text_color(scheduleBtnLabel, C_BG, LV_PART_MAIN);
}

void refreshNavigation() {
  if (!uiBuilt) return;
  for (int i = 0; i < NUM_MODES; ++i) {
    const bool active = (i == current_mode);
    lv_obj_set_style_bg_color(navBtn[i], active ? C_CARD_ALT : C_SURFACE, LV_PART_MAIN);
    lv_obj_set_style_text_color(navLabel[i], active ? C_CYAN : C_MUTED, LV_PART_MAIN);
    lv_obj_set_style_border_width(navBtn[i], active ? 2 : 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(navBtn[i], active ? C_CYAN : C_SURFACE, LV_PART_MAIN);
    lv_obj_set_style_border_side(navBtn[i], LV_BORDER_SIDE_TOP, LV_PART_MAIN);
  }
}

void hidePasswordModal() {
  passwordTarget = PASSWORD_NONE;
  enteredPassword = "";
  passwordErrorUntil = 0;
  if (passwordOverlay) lv_obj_add_flag(passwordOverlay, LV_OBJ_FLAG_HIDDEN);
  if (passwordError) lv_label_set_text(passwordError, "");
}

void requestManualPassword(PasswordTarget target) {
  if (!uiBuilt) return;
  passwordTarget = target;
  enteredPassword = "";
  passwordErrorUntil = 0;
  lv_label_set_text(passwordSubtitle, target == PASSWORD_FAN_MANUAL ? "FAN AUTO TO MANUAL" : "PUMP AUTO TO MANUAL");
  lv_label_set_text(passwordError, "Enter PIN to unlock manual control");
  lv_obj_set_style_text_color(passwordError, C_MUTED, LV_PART_MAIN);
  refreshPasswordDots();
  lv_obj_clear_flag(passwordOverlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(passwordOverlay);
  beep(45, 1900);
  lv_refr_now(nullptr);
}

void showCenteredText(const char* text, uint8_t textSize = 2, int yOffset = 0) {
  if (!uiBuilt || !feedbackOverlay || !feedbackTitle) return;

  String msg(text);
  // The original MQTT power-off path intentionally leaves its power-off message
  // on screen. On the touch UI we render the dedicated standby page instead so
  // the header ON button remains reachable.
  if (!main_control && msg == "POWER OFF") {
    updateDisplay();
    return;
  }

  lv_label_set_text(feedbackTitle, text);
  lv_color_t c = C_CYAN;
  if (msg.indexOf("OFF") >= 0 || msg.indexOf("Disable") >= 0 || msg.indexOf("DENIED") >= 0) c = C_RED;
  else if (msg.indexOf("ON") >= 0 || msg.indexOf("AUTO") >= 0 || msg.indexOf("GRANTED") >= 0) c = C_GREEN;
  lv_obj_set_style_text_color(feedbackTitle, c, LV_PART_MAIN);

  lv_obj_clear_flag(feedbackOverlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(feedbackOverlay);
  lv_refr_now(nullptr);
}

void updateStartupProgress(uint8_t progress, const char* statusText) {
  if (!splashScreen || !splashProgressBar) return;
  if (progress > 100) progress = 100;
  lv_bar_set_value(splashProgressBar, progress, LV_ANIM_OFF);
  if (splashProgressLabel) {
    String pct = String(progress) + "%";
    lv_label_set_text(splashProgressLabel, pct.c_str());
  }
  if (splashStatusLabel && statusText) {
    lv_label_set_text(splashStatusLabel, statusText);
  }
  lv_refr_now(nullptr);
}

void showStartupScreen() {
  splashScreen = lv_obj_create(nullptr);
  lv_obj_t* splash = splashScreen;
  lv_obj_set_style_bg_color(splash, C_BG, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(splash, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(splash, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(splash, 0, LV_PART_MAIN);
  noScroll(splash);

  // Outer circular badge
  lv_obj_t* mark = lv_obj_create(splash);
  lv_obj_set_pos(mark, 119, 28);
  lv_obj_set_size(mark, 82, 82);
  stylePanel(mark, C_CARD_ALT, LV_RADIUS_CIRCLE);
  lv_obj_set_style_border_color(mark, C_CYAN, LV_PART_MAIN);
  lv_obj_set_style_border_width(mark, 2, LV_PART_MAIN);

  // Tank body
  lv_obj_t* tank = lv_obj_create(mark);
  lv_obj_set_pos(tank, 22, 28);
  lv_obj_set_size(tank, 38, 30);
  lv_obj_set_style_bg_opa(tank, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_color(tank, C_TEXT, LV_PART_MAIN);
  lv_obj_set_style_border_width(tank, 3, LV_PART_MAIN);
  lv_obj_set_style_radius(tank, 7, LV_PART_MAIN);
  noScroll(tank);

  // Tank neck
  lv_obj_t* neck = lv_obj_create(mark);
  lv_obj_set_pos(neck, 34, 21);
  lv_obj_set_size(neck, 14, 8);
  lv_obj_set_style_bg_color(neck, C_TEXT, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(neck, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(neck, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(neck, 3, LV_PART_MAIN);
  noScroll(neck);

  // Top cap / inlet
  lv_obj_t* cap = lv_obj_create(mark);
  lv_obj_set_pos(cap, 37, 14);
  lv_obj_set_size(cap, 8, 6);
  lv_obj_set_style_bg_color(cap, C_CYAN, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(cap, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(cap, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(cap, 3, LV_PART_MAIN);
  noScroll(cap);

  // Water fill inside tank
  lv_obj_t* fill = lv_obj_create(tank);
  lv_obj_set_pos(fill, 4, 16);
  lv_obj_set_size(fill, 24, 8);
  lv_obj_set_style_bg_color(fill, C_CYAN, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(fill, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(fill, 3, LV_PART_MAIN);
  noScroll(fill);

  // Small level sight mark
  lv_obj_t* level = lv_obj_create(tank);
  lv_obj_set_pos(level, 30, 7);
  lv_obj_set_size(level, 2, 15);
  lv_obj_set_style_bg_color(level, C_TEXT, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(level, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(level, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(level, 1, LV_PART_MAIN);
  noScroll(level);

  lv_obj_t* title = makeLabel(splash, "WATER TANK", 0, 124, &lv_font_montserrat_20, C_TEXT);
  lv_obj_set_width(title, SCREEN_W);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  lv_obj_t* sub = makeLabel(splash, "COOLING SYSTEM", 0, 153, &lv_font_montserrat_14, C_CYAN);
  lv_obj_set_width(sub, SCREEN_W);
  lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  splashStatusLabel = makeLabel(splash, "Starting system...", 0, 185, &lv_font_montserrat_12, C_MUTED);
  lv_obj_set_width(splashStatusLabel, SCREEN_W);
  lv_obj_set_style_text_align(splashStatusLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  splashProgressBar = lv_bar_create(splash);
  lv_obj_set_pos(splashProgressBar, 52, 206);
  lv_obj_set_size(splashProgressBar, 216, 12);
  lv_bar_set_range(splashProgressBar, 0, 100);
  lv_bar_set_value(splashProgressBar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(splashProgressBar, C_CARD_ALT, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(splashProgressBar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(splashProgressBar, C_BORDER, LV_PART_MAIN);
  lv_obj_set_style_border_width(splashProgressBar, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(splashProgressBar, 6, LV_PART_MAIN);
  lv_obj_set_style_bg_color(splashProgressBar, C_CYAN, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(splashProgressBar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(splashProgressBar, 6, LV_PART_INDICATOR);

  splashProgressLabel = makeLabel(splash, "0%", 0, 221, &lv_font_montserrat_12, C_DIM);
  lv_obj_set_width(splashProgressLabel, SCREEN_W);
  lv_obj_set_style_text_align(splashProgressLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  lv_scr_load(splash);
  updateStartupProgress(4, touchDetected ? "Display and touch ready" : "Display ready");
}


void updateDisplay() {
  if (!uiBuilt) {
    buildMainUi();
  }

  // Hide transient overlays whenever normal state is explicitly refreshed.
  if (feedbackOverlay) lv_obj_add_flag(feedbackOverlay, LV_OBJ_FLAG_HIDDEN);

  refreshHeader();
  refreshTemperaturePage();
  refreshFanPage();
  refreshPumpPage();
  refreshSchedulerPage();
  refreshNavigation();

  if (!main_control) {
    for (int i = 0; i < NUM_MODES; ++i) {
      setObjectHidden(pageRoot[i], true);
      lv_obj_add_state(navBtn[i], LV_STATE_DISABLED);
    }
    setObjectHidden(powerOverlay, false);
    lv_obj_move_foreground(powerOverlay);
  } else {
    setObjectHidden(powerOverlay, true);
    for (int i = 0; i < NUM_MODES; ++i) {
      setObjectHidden(pageRoot[i], i != current_mode);
      lv_obj_clear_state(navBtn[i], LV_STATE_DISABLED);
    }
  }

  if (passwordTarget == PASSWORD_NONE && passwordOverlay) {
    lv_obj_add_flag(passwordOverlay, LV_OBJ_FLAG_HIDDEN);
  }

  lv_refr_now(nullptr);
}

void saveMainPower(bool state) {
  main_control = state;
  preferences.begin("parameters", false);
  preferences.putBool("main_control", main_control);
  preferences.end();

  if (!main_control) {
    fanstate = 0;
    pumpstate = 0;
    updateControlOutputs();
    hidePasswordModal();
  }

  beep(70, 2000);
  showCenteredText(main_control ? "POWER ON" : "POWER OFF", 2, 0);
  delay(350);
  updateDisplay();
  publishJson();
}

void setFanControlLocal(bool wantAuto) {
  if (wantAuto == fan_control) return;

  if (!wantAuto && timeschen == 1) {
    showCenteredText("DISABLE SCHEDULER FIRST", 1, 0);
    beep(160, 1500);
    delay(550);
    updateDisplay();
    return;
  }

  fan_control = wantAuto;
  preferences.begin("parameters", false);
  preferences.putBool("fan_control", fan_control);
  preferences.end();

  beep(70, 2000);
  showCenteredText(fan_control ? "FAN AUTO" : "FAN MANUAL", 2, 0);
  delay(350);
  updateDisplay();
  publishJson();
}

void setPumpControlLocal(bool wantAuto) {
  if (wantAuto == pump_control) return;

  pump_control = wantAuto;
  preferences.begin("parameters", false);
  preferences.putBool("pump_control", pump_control);
  preferences.end();

  beep(70, 2000);
  showCenteredText(pump_control ? "PUMP AUTO" : "PUMP MANUAL", 2, 0);
  delay(350);
  updateDisplay();
  publishJson();
}

bool adjustSetpoint(int delta) {
  const int oldSp = sp;
  int next = (int)sp + delta;
  if (next < MIN_TEMP) next = MIN_TEMP;
  if (next > MAX_TEMP) next = MAX_TEMP;
  sp = (uint8_t)next;

  if (sp == oldSp) return false;

  temp1sp = String(sp);
  beep(20, 2100);
  updateDisplay();
  return true;
}

void commitSetpoint() {
  preferences.begin("parameters", false);
  preferences.putUInt("sp", sp);
  preferences.end();

  temp1sp = String(sp);
  publishJson();
}

void toggleFanManualState() {
  if (fan_control || timeschen == 1) return;

  fansw = !fansw;
  fanstate = fansw;
  preferences.begin("parameters", false);
  preferences.putUInt("fansw", fansw);
  preferences.end();

  updateControlOutputs();
  beep(55, 2000);
  updateDisplay();
  publishJson();
}

void togglePumpManualState() {
  if (pump_control) return;

  pumpsw = !pumpsw;
  pumpstate = pumpsw;
  preferences.begin("parameters", false);
  preferences.putUInt("pumpsw", pumpsw);
  preferences.end();

  updateControlOutputs();
  beep(55, 2000);
  updateDisplay();
  publishJson();
}

void toggleSchedulerLocal() {
  timeschen = !timeschen;
  if (timeschen == 1 && !fan_control) {
    fan_control = true;  // Original scheduler rule: scheduler requires Fan AUTO.
  }

  preferences.begin("parameters", false);
  preferences.putUInt("timeschen", timeschen);
  preferences.putBool("fan_control", fan_control);
  preferences.end();

  beep(80, timeschen ? 2200 : 1600);
  updateDisplay();
  publishJson();
}


void updateControlOutputs() {
  // fanstate and pumpstate are always resolved before this is called.
  // When power is OFF, loopFunction() zeroes both states before calling here.
  digitalWrite(FAN_CONTROL_PIN,  fanstate  ? HIGH : LOW);
  digitalWrite(PUMP_CONTROL_PIN, pumpstate ? HIGH : LOW);
}

void loopFunction() {
  if (datainterrupt) {
    datainterrupt = false;
    if (WiFi.status() == WL_CONNECTED && deviceConnected == false) {
      wifi_status = 1;
      NTP_TIME();
      inputs();
    }
  }

  if (main_control) {

    // ── Fan Control ──────────────────────────────────────────────
    if (timeschen == 1) {
      // fanScheduler() only needs to run once per minute — the schedule
      // is hour-based, and calling it every loop() floods the Serial monitor.
      static unsigned long lastSchedulerRun = 0;
      if (millis() - lastSchedulerRun >= 60000UL || lastSchedulerRun == 0) {
        fanScheduler();
        lastSchedulerRun = millis();
      }
    } else if (fan_control) {
      fan_auto_control();       // Auto mode sets fanstate based on temp
    } else {
      fan_manual_control();     // Manual mode sets fanstate based on fansw
    }

    // ── Pump Control ─────────────────────────────────────────────
    if (pump_control) {
      pump_auto_control();      // Auto mode sets pumpstate based on water level
    } else {
      pump_manual_control();    // Manual mode sets pumpstate based on pumpsw
    }

    // ── Apply outputs once, after all state is resolved ──────────
    updateControlOutputs();

  } else {
    // Power is OFF — force everything off
    fanstate  = 0;
    pumpstate = 0;
    updateControlOutputs();     // Single call handles the LOW writes
  }
}

void getTemperature() {
  if (tmp_req) {
    sensors.setWaitForConversion(false);
    sensors.requestTemperatures();
    sensors.setWaitForConversion(true);
  updateStartupProgress(38, "Temperature sensor ready");
    tmp_req = 0;
    time3 = millis();
    temper = sensors.getTempCByIndex(0);
    if (temper > 0) {
      temp1 = String(temper);
    } else {
      temp1 = "0";
    }
  }
  if (temper != tprev) {
    op = 1;
    tprev = temper;
    time1 = millis();
  }
}

void inputs() {
  if (digitalRead(WATER_LEVEL_PIN) == LOW) {
  #ifdef DEBUG
      Serial.println("==========digitalRead(WATER_LEVEL_PIN) == LOW======");
      Serial.println("Full");
  #endif
      waterlevel = 1;
      waterlevelsp = 1;
    } else {
  #ifdef DEBUG
      Serial.println("==========digitalRead(WATER_LEVEL_PIN) == HIGH======");
      Serial.println("Low");
  #endif
      waterlevel = 0;
      waterlevelsp = 0;
    }
}

void fan_auto_control() {
  if (op != 0) return;  // temp still stabilising

  if (temper > sp + hyst) {
    fanstate = 1;
  } else if (temper <= sp - hyst) {
    fanstate = 0;
  }
  // Inside the band: do nothing — hold current state
}

void pump_auto_control() {
  if (pumpstate == 0 && waterlevel == 0) {
    // Tank empty, pump is off → start filling
    pumpstate = 1;
  } else if (pumpstate == 1 && waterlevel == 1) {
    // Tank full, pump is on → stop filling
    pumpstate = 0;
  }
  // Sensor bouncing at boundary → hold current pumpstate
}

void fan_manual_control() {
  if (fansw == 1) {
    // ioCon1.digitalWrite(4, HIGH);
    // digitalWrite(FAN_CONTROL_PIN, HIGH); // For ESP32 Board
    fanstate = 1;
  } else {
    // ioCon1.digitalWrite(4, LOW);
    // digitalWrite(FAN_CONTROL_PIN, LOW); // For ESP32 Board
    fanstate = 0;
  }
  
}

void pump_manual_control() {
  if (pumpsw == 1) {
    // ioCon1.digitalWrite(5, HIGH);
    // digitalWrite(PUMP_CONTROL_PIN, HIGH); // For ESP32 Board
    pumpstate = 1;
  } else {
    // ioCon1.digitalWrite(5, LOW);
    // digitalWrite(PUMP_CONTROL_PIN, LOW); // For ESP32 Board
    pumpstate = 0;
  }
  
}

void NTP_TIME() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
  #ifdef DEBUG
      Serial.println("Failed to obtain time");
  #endif
      return;
    }
  #ifdef DEBUG
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    Serial.print("   ");
    Serial.print(timeinfo.tm_hour);
    Serial.print(" ");
  #endif
    deviceTime = "";
  #ifdef DEBUG
    Serial.print("day=");
    Serial.println(timeinfo.tm_wday);
  #endif
    tmp = day_app[timeinfo.tm_wday];
    SchScan[0] = tmp;
    SchScan[1] = timeinfo.tm_mday;
    SchScan[2] = timeinfo.tm_mon + 1;
    SchScan[3] = timeinfo.tm_year - 100;
    SchScan[4] = timeinfo.tm_hour;
    SchScan[5] = timeinfo.tm_min;
    SchScan[6] = timeinfo.tm_sec;
  #ifdef DEBUG
    Serial.println("NTP_TIME called");
  #endif
}

bool syncNTPoverEthernet() {
  const int NTP_PACKET_SIZE = 48;
  byte packetBuffer[NTP_PACKET_SIZE];

  // Use direct IPs (NO DNS)
  IPAddress ntpServers[] = {
    IPAddress(216, 239, 35, 0),    // time.google.com
    IPAddress(129, 6, 15, 28),     // time.nist.gov
    IPAddress(162, 159, 200, 1)    // time.cloudflare.com
  };

  for (int s = 0; s < 3; s++) {

    Serial.print("Trying NTP server: ");
    Serial.println(ntpServers[s]);

    ntpUDP.begin(2390);

    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;
    packetBuffer[1] = 0;
    packetBuffer[2] = 6;
    packetBuffer[3] = 0xEC;
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    ntpUDP.beginPacket(ntpServers[s], 123);
    ntpUDP.write(packetBuffer, NTP_PACKET_SIZE);
    ntpUDP.endPacket();

    unsigned long startWait = millis();

    while (millis() - startWait < 3000) {
      int size = ntpUDP.parsePacket();
      if (size >= NTP_PACKET_SIZE) {

        ntpUDP.read(packetBuffer, NTP_PACKET_SIZE);

        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord  = word(packetBuffer[42], packetBuffer[43]);
        unsigned long secsSince1900 = (highWord << 16) | lowWord;

        const unsigned long seventyYears = 2208988800UL;
        unsigned long epoch = secsSince1900 - seventyYears;

        struct timeval tv;
        tv.tv_sec = epoch;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);

        // SET TIMEZONE
        setenv("TZ", "PKT-5", 1);
        tzset();

        struct tm timeinfo;
        getLocalTime(&timeinfo);

        Serial.printf("✓ NTP synced via Ethernet: %02d:%02d:%02d | wday=%d\n",
                      timeinfo.tm_hour,
                      timeinfo.tm_min,
                      timeinfo.tm_sec,
                      timeinfo.tm_wday);

        ntpUDP.stop();
        return true;
      }
      delay(10);
    }

    ntpUDP.stop();
    Serial.println("Timeout, trying next server...");
  }

  Serial.println("✗ All Ethernet NTP servers failed");
  return false;
}

bool syncNTPoverWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("NTP WiFi: not connected, skipping");
    return false;
  }

  configTime(gmtOffset_sec, daylightOffset_sec, "time.google.com", "pool.ntp.org", "time.nist.gov");

  // Wait up to 5 seconds for sync
  struct tm timeinfo;
  unsigned long waitStart = millis();
  while (millis() - waitStart < 5000) {
    if (getLocalTime(&timeinfo)) {
      Serial.printf("✓ NTP synced via WiFi: %02d:%02d:%02d | wday=%d\n",
        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_wday);
      return true;
    }
    delay(200);
  }
  Serial.println("✗ NTP WiFi sync timeout");
  return false;
}

void syncNTP() {
  Serial.println("── NTP Sync ──────────────────────────");
  bool synced = false;

  updateStartupProgress(90, "Preparing MQTT");
  if (eth_connected) {
    Serial.println("Trying NTP over Ethernet...");
    synced = syncNTPoverEthernet();
  }

  if (!synced && WiFi.status() == WL_CONNECTED) {
    Serial.println("Trying NTP over WiFi...");
    synced = syncNTPoverWiFi();
  }

  if (!synced) {
    Serial.println("✗ NTP sync failed on all interfaces");
  }

  Serial.println("──────────────────────────────────────");
}

void IRAM_ATTR onTimer() {
  if (milscount > 1000) {
    milscount = 0;
    datainterrupt = true;
  }
  if (wifi_status == 0 && deviceConnected == false) {
    if (ledcounter > 50) {
      ledcounter = 0;
      if (blink == 0) {
        // digitalWrite(23, HIGH);
        blink = 1;
      } else {
        // digitalWrite(23, LOW);
        blink = 0;
      }
    }
  } else if (wifi_status == 0 && deviceConnected == true) {
    if (ledcounter > 500) {
      ledretrycount += 1;
      if (ledretrycount > 5) {
        ledcounter = 0;
        ledretrycount = 0;
      } else {
        ledcounter = 400;
      }
      if (blink == 0) {
        // digitalWrite(23, HIGH);
        blink = 1;
      } else {
        // digitalWrite(23, LOW);
        blink = 0;
      }
    }
  } else if (wifi_status == 1 && deviceConnected == false) {
    if (ledcounter > 1001) {
      ledcounter = 0;
      if (blink == 0) {
        // digitalWrite(23, HIGH);
        blink = 1;
      } else {
        // digitalWrite(23, LOW);
        blink = 0;
      }
    }
  }
  ledcounter++;
  milscount++;
}

void parse_schedule() {
  // Clear arrays
  memset(days_in_num, 0, sizeof(days_in_num));
  memset(hours_num, 0, sizeof(hours_num));

  // Find the pipe separator
  int pipeIndex = timesch.indexOf('|');
  if (pipeIndex == -1) return;  // Invalid format, do nothing

  String hoursPart = timesch.substring(0, pipeIndex);
  String daysPart = timesch.substring(pipeIndex + 1);

  // Parse hours
  if (hoursPart.startsWith("hoursch=")) {
    String hstr = hoursPart.substring(8);  // After "hoursch="
    if (hstr == "24") {
      // All hours
      for (int i = 0; i < 24; i++) hours_num[i] = 1;
    } else {
      // Comma-separated list, e.g., "0,1,2"
      int start = 0;
      int comma = hstr.indexOf(',', start);
      while (comma != -1) {
        int h = hstr.substring(start, comma).toInt();
        if (h >= 0 && h < 24) hours_num[h] = 1;
        start = comma + 1;
        comma = hstr.indexOf(',', start);
      }
      // Last value
      int h = hstr.substring(start).toInt();
      if (h >= 0 && h < 24) hours_num[h] = 1;
    }
  }

  // Parse days
  if (daysPart.startsWith("daysch=")) {
    String dstr = daysPart.substring(7);  // After "daysch="
    if (dstr == "7") {
      // All days
      for (int i = 0; i < 7; i++) days_in_num[i] = 1;
    } else {
      // Comma-separated list, e.g., "0,1,2"
      int start = 0;
      int comma = dstr.indexOf(',', start);
      while (comma != -1) {
        int d = dstr.substring(start, comma).toInt();
        if (d >= 0 && d < 7) days_in_num[d] = 1;
        start = comma + 1;
        comma = dstr.indexOf(',', start);
      }
      // Last value
      int d = dstr.substring(start).toInt();
      if (d >= 0 && d < 7) days_in_num[d] = 1;
    }
  }

  #ifdef DEBUG
    Serial.println("Parsed schedule:");
    Serial.print("Hours: ");
    for (int i = 0; i < 24; i++) if (hours_num[i]) Serial.print(String(i) + " ");
    Serial.println();
    Serial.print("Days: ");
    for (int i = 0; i < 7; i++) if (days_in_num[i]) Serial.print(String(i) + " ");
    Serial.println();
  #endif
}

void fanScheduler() {
  if (timeschen == 0) {
    // Serial.println("Scheduler disabled (timeschen = 0)");
    return; // Scheduler disabled
  }

  // Fetch current NTP time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // #ifdef DEBUG
      Serial.println("NTP time unavailable - skipping scheduler");
    // #endif
    return; // Skip if NTP fails (e.g., no WiFi or sync issue)
  }

  int currentHour = timeinfo.tm_hour;  // Current hour from NTP
  int currentDay = timeinfo.tm_wday;   // Current day from NTP

  currentDay = (currentDay == 0) ? 6 : currentDay - 1;

  parse_schedule();

  #ifdef DEBUG
  Serial.printf("Scheduler: Day=%d Hour=%d\n", currentDay, currentHour);
  #endif

  if (days_in_num[currentDay] == 1 && hours_num[currentHour] == 1) {
        tmatched = 1;
        fanstate = 1;
        // Serial.println("SCHEDULE MATCH → FAN ON");
    } else {
        tmatched = 0;
        fanstate = 0;
        // Serial.println("NO MATCH → FAN OFF");
  }
  
  // Save to Preferences (only if changed to avoid wear)
  static int prevTmatched = -1;
  if (tmatched != prevTmatched) {
    preferences.begin("parameters", false);
    preferences.putUInt("tmatched", tmatched);
    preferences.end();
    prevTmatched = tmatched;
  }
}

bool initW5500() {
  // Try to read W5500 version register - should return 0x04 for W5500
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(W5500_CS, LOW);
  
  SPI.transfer(0x00);  // Version register address high byte
  SPI.transfer(0x39);  // Version register address low byte  
  SPI.transfer(0x00);  // Read mode, common register
  byte version = SPI.transfer(0x00);
  
  digitalWrite(W5500_CS, HIGH);
  SPI.endTransaction();
  
  Serial.print("W5500 Version Register: 0x");
  Serial.println(version, HEX);
  
  return (version == 0x04);  // W5500 returns 0x04
}

void setup() {
  initTaskWatchdogSafe();  // 10-second watchdog, compatible with Arduino-ESP32 2.x/3.x
  Serial.begin(115200);

  // Safety first: force both relay outputs OFF before network/display startup.
  pinMode(FAN_CONTROL_PIN, OUTPUT);
  pinMode(PUMP_CONTROL_PIN, OUTPUT);
  digitalWrite(FAN_CONTROL_PIN, LOW);
  digitalWrite(PUMP_CONTROL_PIN, LOW);

  pinMode(WATER_LEVEL_PIN, INPUT_PULLUP);

  // JC2432W328: TFT_eSPI transport + LVGL UI + CST820/CST816 touch.
  initDisplaySystem();
  showStartupScreen();
  updateStartupProgress(8, "Display ready");

  #ifdef DEBUG
    Serial.println("\n\nbooting up device...\n\n");
    Serial.print("devicename: ");
    Serial.println(devicename);
  #endif

  updateStartupProgress(16, "Loading parameters");
  DEVICE_INIT();
  updateStartupProgress(24, "Parameters loaded");
  Serial.printf("After DEVICE_INIT: main_control=%d, timeschen=%d\n", main_control, timeschen);

  updateStartupProgress(30, "Starting temperature sensor");
  sensors.begin();
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();
  sensors.setWaitForConversion(true);
  updateStartupProgress(38, "Temperature sensor ready");

  //=========================================//
  macaddress = WiFi.macAddress();

  updateStartupProgress(46, "Checking network settings");
  if (loadCredentials(ssid, password)) {
    #ifdef DEBUG
      Serial.println("Loaded Wi-Fi credentials from memory.");
    #endif
    connectToWiFi(ssid.c_str(), password.c_str());
  } else {
    #ifdef DEBUG
      Serial.println("No Wi-Fi credentials found. Starting in AP mode.");
    #endif
    startAccessPoint();
  }

  updateStartupProgress(56, "Starting web server");
  server_setup();
  updateStartupProgress(62, "Preparing Ethernet");

  // ── Ethernet Init (after WiFi AP) ─────────────────────────────
  Serial.println("\n═══ Ethernet Initialization ═══");
  // GPIO4 is reserved for the Pump relay; W5500 reset is moved to GPIO17.
  pinMode(W5500_RST, OUTPUT);
  digitalWrite(W5500_RST, LOW);
  delay(50);
  digitalWrite(W5500_RST, HIGH);
  delay(1000);

  SPI.begin(W5500_SCLK, W5500_MISO, W5500_MOSI, W5500_CS);
  pinMode(W5500_CS, OUTPUT);
  digitalWrite(W5500_CS, HIGH);

  // Manual W5500 detection
  if (!initW5500()) {
    Serial.println("✗ W5500 chip not detected on SPI bus");
    Serial.println("  Check: CS, MOSI, MISO, SCLK connections");
    eth_connected = false;
  } else {
    Serial.println("✓ W5500 chip detected");
    
    Ethernet.init(W5500_CS);
    
    // Serial.print("Hardware Status: ");
    switch(Ethernet.hardwareStatus()) {
      case EthernetNoHardware: Serial.println("Not detected"); break;
      case EthernetW5100: Serial.println("W5100"); break;
      case EthernetW5200: Serial.println("W5200"); break;
      case EthernetW5500: Serial.println("W5500"); break;
      default: Serial.println("Unknown"); break;
    }
    
    Serial.print("Initial Link: ");
    Serial.println(Ethernet.linkStatus() == LinkON ? "ON" : "OFF");
    
    // Wait for link
    Serial.print("Waiting for link");
    int linkWaitCount = 0;
    while (Ethernet.linkStatus() != LinkON && linkWaitCount < 50) {
      delay(100);
      linkWaitCount++;
      if (linkWaitCount % 5 == 0) Serial.print(".");
    }
    Serial.println();
    
    if (Ethernet.linkStatus() == LinkON) {
      Serial.println("✓ Cable connected, starting DHCP...");
      
      unsigned long dhcpStart = millis();
      bool dhcpSuccess = Ethernet.begin(newMACAddress);
      
      if (!dhcpSuccess) {
        Serial.println("DHCP timeout, using static IP");
        Ethernet.begin(newMACAddress, IPAddress(192, 168, 0, 117));
        delay(500);
      }
      
      if (Ethernet.localIP() != INADDR_NONE) {
        eth_connected = true;
        myIP = Ethernet.localIP().toString();
        Serial.print("✓ IP: ");
        Serial.println(myIP);
      } else {
        Serial.println("✗ Failed to get IP");
        eth_connected = false;
      }
    } else {
      Serial.println("✗ No link (cable unplugged)");
      eth_connected = false;
    }
  }

  updateStartupProgress(76, eth_connected ? "Ethernet connected" : "WiFi active");
  Serial.println(eth_connected ? "→ Using ETHERNET" : "→ Using WiFi");
  if (!eth_connected) myIP = WiFi.localIP().toString();
  Serial.println("═══════════════════════════════\n");

  // ── NTP ──────────────────────────────────────────────────────────
  updateStartupProgress(84, "Syncing time");
  syncNTP();
  Serial.println("NTP configured");

  // ── Verify entropy source ──────────────────────────────────────
  pinMode(SSL_ENTROPY_PIN, INPUT);
  randomSeed(analogRead(SSL_ENTROPY_PIN));
  Serial.printf("SSL Entropy value: %d\n", analogRead(SSL_ENTROPY_PIN));

  // ── MQTT Setup ────────────────────────────────────────────────
  if (eth_connected) {
    // Configure SSLClient for Ethernet
    sslEthClient.setMutualAuthParams(mTLS);
    sslEthClient.setTimeout(30000);  // 30 second timeout
    
    ethMqttClient.setServer(mqtt_server, mqtt_port);
    ethMqttClient.setCallback(callback);
    ethMqttClient.setBufferSize(2048);  // Increase MQTT buffer
    ethMqttClient.setKeepAlive(60);     // Keep-alive interval
    activeMqttClient = &ethMqttClient;
    
    delay(2000);  // Give SSLClient time to initialize
    Serial.println("Using Ethernet for MQTT");
  } else {
    wifiClient.setCACert(root_ca);
    wifiClient.setCertificate(client_cert);
    wifiClient.setPrivateKey(client_key);
    wifiMqttClient.setServer(mqtt_server, mqtt_port);
    wifiMqttClient.setCallback(callback);
    wifiMqttClient.setBufferSize(2048);
    wifiMqttClient.setKeepAlive(60);
    activeMqttClient = &wifiMqttClient;
    Serial.println("Using WiFi for MQTT");
  }

  delay(1000);  // Brief pause before first connection attempt
  reconnect();
  updateStartupProgress(97, "Loading dashboard");

  // ── Build the LVGL dashboard from restored controller state ───
  updateDisplay();
}

void loop() {
  // Service LVGL timers and the capacitive touch input.
  serviceDisplayUi();

  if (millis() - lastInputTime > 2000) {  // Every 2 seconds
    inputs();
    lastInputTime = millis();
  }

  // Ethernet link monitoring and maintenance
  static unsigned long lastMaintain = 0;
  static unsigned long lastLinkCheck = 0;

  // Maintain DHCP lease every 30 seconds
  if (millis() - lastMaintain > 30000UL) {
    if (eth_connected) {
      byte maintainResult = Ethernet.maintain();
      if (maintainResult != 0) {
        #ifdef DEBUG
          Serial.printf("Ethernet maintain returned: %d\n", maintainResult);
        #endif
      }
    }
    lastMaintain = millis();
  }

  // Check link status more frequently (every 1 second instead of on-demand)
  if (millis() - lastLinkCheck > 1000UL) {
    bool currentEthStatus = false;
    
    // Only check if we think we're connected OR if chip was detected at startup
    if (eth_connected || Ethernet.hardwareStatus() == EthernetW5500) {
      currentEthStatus = (Ethernet.linkStatus() == LinkON && Ethernet.localIP() != INADDR_NONE);
    }
    
    if (currentEthStatus != eth_connected) {
      eth_connected = currentEthStatus;
      
      Serial.println("");
      Serial.println("╔═════════════════════════════════════╗");
      if (eth_connected) {
        Serial.println("║  🔄 NETWORK SWITCH: WiFi → Ethernet ║");
      } else {
        Serial.println("║  🔄 NETWORK SWITCH: Ethernet → WiFi ║");
      }
      Serial.println("╚═════════════════════════════════════╝");
      
      // Disconnect and cleanup
      if (activeMqttClient->connected()) {
        activeMqttClient->disconnect();
      }
      delay(200);  // Reduced cleanup delay
      
      if (eth_connected) {
        sslEthClient.flush();
        sslEthClient.setMutualAuthParams(mTLS);
        sslEthClient.setTimeout(30000);
        ethMqttClient.setServer(mqtt_server, mqtt_port);
        ethMqttClient.setCallback(callback);
        ethMqttClient.setBufferSize(2048);
        ethMqttClient.setKeepAlive(60);
        activeMqttClient = &ethMqttClient;
        myIP = Ethernet.localIP().toString();
        Serial.print("New IP (Ethernet): ");
        Serial.println(myIP);
        updateStartupProgress(84, "Syncing time");
  syncNTP();
      } else {
        wifiClient.setCACert(root_ca);
        wifiClient.setCertificate(client_cert);
        wifiClient.setPrivateKey(client_key);
        wifiMqttClient.setServer(mqtt_server, mqtt_port);
        wifiMqttClient.setCallback(callback);
        wifiMqttClient.setBufferSize(2048);
        wifiMqttClient.setKeepAlive(60);
        activeMqttClient = &wifiMqttClient;
        myIP = WiFi.localIP().toString();
        Serial.print("New IP (WiFi): ");
        Serial.println(myIP);
        updateStartupProgress(84, "Syncing time");
  syncNTP();
      }
      
      delay(1000);
      reconnect();
  updateStartupProgress(97, "Loading dashboard");
      updateDisplay();
    }
    
    lastLinkCheck = millis();
  }

  // NTP periodic resync - every hour
  static unsigned long lastNtpResync = 0;
  if (millis() - lastNtpResync > 3600000UL) {
    updateStartupProgress(84, "Syncing time");
  syncNTP();
    lastNtpResync = millis();
  }

  // MQTT handling
  if (!activeMqttClient->connected()) {
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 10000) {  // Retry every 10s
      reconnect();
  updateStartupProgress(97, "Loading dashboard");
      lastReconnect = millis();
    }
  } else {
    // Only call loop() if connected
    activeMqttClient->loop();
    
    static unsigned long lastPublish = 0;
    if (millis() - lastPublish > 3000) {  // Publish every 3s
      publishJson();
      lastPublish = millis();
    }
  }

  // WiFi AP timeout
  if (millis() - wifi_setting_time >= 300000 && wifi_ap_mode == true) {
    WiFi.softAPdisconnect();
    wifi_ap_mode = false;
  }

  // Reset temperature stabilization flag
  if ((millis() - time1) > 8000) {
    op = 0;
  }

  // Temperature reading and display update
  if ((millis() - time3) > 2000) {
    tmp_req = true;
    #ifdef DEBUG
      int numberOfDevices = sensors.getDeviceCount();
      Serial.print("numberOfDevices = ");
      Serial.println(numberOfDevices);
      Serial.println("request temperatures-----------------------------------------");
    #endif
    getTemperature();
    if (current_mode == TEMPERATURE && main_control) {
      updateDisplay();
    }
    time3 = millis();
  }

  // Main control logic
  loopFunction();

  // Update system state
  watertankcoolsw = (pumpstate == 1 || fanstate == 1) ? 2 : 0;

  bool needRefresh = false;
  
  if (prev_fanstate != fanstate) {
    prev_fanstate = fanstate;
    if (current_mode == FAN) needRefresh = true;
  }

  if (prev_pumpstate != pumpstate) {
    prev_pumpstate = pumpstate;
    if (current_mode == PUMP) needRefresh = true;
  }

  if (needRefresh && main_control) {
    updateDisplay();
  }

  delay(15);
}
