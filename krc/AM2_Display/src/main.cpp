#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include "BoardConfig.h"
#include "CST820Touch.h"
#include "DisplayLink.h"
#include "UI.h"

TFT_eSPI tft;
CST820Touch touch;
DisplayLink displayLink;

static lv_disp_draw_buf_t drawBuf;
static lv_color_t lvBuffer[AM2_SCREEN_W * 16];
static uint32_t lastLvTick = 0;

static void displayFlush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* colorP) {
  const uint32_t w = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  const uint32_t h = static_cast<uint32_t>(area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors(reinterpret_cast<uint16_t*>(colorP), w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

static void touchRead(lv_indev_drv_t*, lv_indev_data_t* data) {
  uint16_t x = 0;
  uint16_t y = 0;
  if (touch.read(x, y)) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = static_cast<lv_coord_t>(x);
    data->point.y = static_cast<lv_coord_t>(y);
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

void setup() {
  Serial.begin(115200);
  delay(80);
  Serial.println("\n[AM2 DISPLAY] JC2432W328 RS485 HMI boot");

  pinMode(AM2_TFT_BL_PIN, OUTPUT);
  digitalWrite(AM2_TFT_BL_PIN, LOW);

  tft.begin();
  tft.setRotation(1);
  tft.invertDisplay(false);
  tft.fillScreen(TFT_BLACK);

  lv_init();
  lv_disp_draw_buf_init(&drawBuf, lvBuffer, nullptr, AM2_SCREEN_W * 16);

  static lv_disp_drv_t dispDrv;
  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = AM2_SCREEN_W;
  dispDrv.ver_res = AM2_SCREEN_H;
  dispDrv.flush_cb = displayFlush;
  dispDrv.draw_buf = &drawBuf;
  lv_disp_drv_register(&dispDrv);

  const bool touchOk = touch.begin();
  Serial.printf("[TOUCH] CST820 %s @ 0x%02X\n", touchOk ? "OK" : "NOT FOUND", AM2_TOUCH_ADDR);

  static lv_indev_drv_t indevDrv;
  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = touchRead;
  lv_indev_drv_register(&indevDrv);

  displayLink.begin();
  uiBegin(displayLink);

  digitalWrite(AM2_TFT_BL_PIN, HIGH);
  lastLvTick = millis();
  Serial.println("[READY] AM2 display UI running; RS485 9600 8N1");
}

void loop() {
  displayLink.loop();

  const uint32_t now = millis();
  const uint32_t elapsed = now - lastLvTick;
  if (elapsed > 0) {
    lv_tick_inc(elapsed);
    lastLvTick = now;
  }

  uiLoop();
  lv_timer_handler();
  delay(5);
}
