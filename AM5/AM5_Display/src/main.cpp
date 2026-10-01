#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

#include "config.h"
#include "cst820_touch.h"
#include "display_state.h"
#include "ui.h"
#include "wireless_link.h"

TFT_eSPI tft;
CST820Touch touch;

namespace {
static lv_disp_draw_buf_t drawBuffer;
static lv_color_t drawBufferPixels[AM5_SCREEN_WIDTH * AM5_LVGL_BUFFER_LINES];

uint32_t lastLvglTickMs = 0;
uint32_t lastLvglHandlerMs = 0;
uint32_t backlightFadeStartedMs = 0;
uint8_t lastBacklightLevel = 0;
bool firstFrameReady = false;

void displayFlush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* colorMap) 
{
    const uint32_t width = static_cast<uint32_t>(area->x2 - area->x1 + 1);
    const uint32_t height = static_cast<uint32_t>(area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, width, height);
    tft.pushColors(reinterpret_cast<uint16_t*>(&colorMap->full), width * height, true);
    tft.endWrite();

    if (!firstFrameReady) {
        firstFrameReady = true;
        backlightFadeStartedMs = millis();
        Serial.println("[display] First LVGL frame ready; enabling backlight fade.");
    }

    lv_disp_flush_ready(disp);
}

void touchRead(lv_indev_drv_t* /*indev*/, lv_indev_data_t* data) {
    static bool pressLatched = false;
    static uint16_t lastX = 0;
    static uint16_t lastY = 0;
    static uint32_t lastValidTouchMs = 0;

    const uint32_t now = millis();
    uint16_t x = 0;
    uint16_t y = 0;

    if (touch.available() && touch.read(x, y)) {
        lastX = x;
        lastY = y;
        lastValidTouchMs = now;

        if (!pressLatched) {
            Serial.print("[TOUCH] DOWN x=");
            Serial.print(x);
            Serial.print(" y=");
            Serial.println(y);
        }

        pressLatched = true;
        data->state = LV_INDEV_STATE_PR;
        data->point.x = static_cast<lv_coord_t>(x);
        data->point.y = static_cast<lv_coord_t>(y);
        return;
    }

    // The CST820 can occasionally miss one I2C sample while a finger is
    // still down. Keep the previous press alive briefly so LVGL does not see
    // false release/re-press cycles that make buttons feel unreliable.
    if (pressLatched && (now - lastValidTouchMs) <= AM5_TOUCH_RELEASE_GRACE_MS) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = static_cast<lv_coord_t>(lastX);
        data->point.y = static_cast<lv_coord_t>(lastY);
        return;
    }

    if (pressLatched) {
        Serial.println("UP");
    }
    pressLatched = false;
    data->state = LV_INDEV_STATE_REL;
    data->point.x = static_cast<lv_coord_t>(lastX);
    data->point.y = static_cast<lv_coord_t>(lastY);
}

void initBacklight() {
    ledcSetup(AM5_BACKLIGHT_PWM_CH,
              AM5_BACKLIGHT_PWM_FREQ,
              AM5_BACKLIGHT_PWM_BITS);
    ledcAttachPin(AM5_BACKLIGHT_PIN, AM5_BACKLIGHT_PWM_CH);
    ledcWrite(AM5_BACKLIGHT_PWM_CH, 0);
    backlightFadeStartedMs = millis();
    lastBacklightLevel = 0;
}

void serviceBacklight() {
    if (!firstFrameReady) return;
    if (lastBacklightLevel >= AM5_BACKLIGHT_LEVEL) return;

    const uint32_t elapsed = millis() - backlightFadeStartedMs;
    uint32_t value = (elapsed * AM5_BACKLIGHT_LEVEL) / 320UL;
    if (value > AM5_BACKLIGHT_LEVEL) value = AM5_BACKLIGHT_LEVEL;

    const uint8_t level = static_cast<uint8_t>(value);
    if (level != lastBacklightLevel) {
        lastBacklightLevel = level;
        ledcWrite(AM5_BACKLIGHT_PWM_CH, level);
    }
}

void initLvgl() {
    lv_init();

    lv_disp_draw_buf_init(&drawBuffer,
                          drawBufferPixels,
                          nullptr,
                          AM5_SCREEN_WIDTH * AM5_LVGL_BUFFER_LINES);

    static lv_disp_drv_t displayDriver;
    lv_disp_drv_init(&displayDriver);
    displayDriver.hor_res = AM5_SCREEN_WIDTH;
    displayDriver.ver_res = AM5_SCREEN_HEIGHT;
    displayDriver.flush_cb = displayFlush;
    displayDriver.draw_buf = &drawBuffer;
    lv_disp_drv_register(&displayDriver);

    static lv_indev_drv_t inputDriver;
    lv_indev_drv_init(&inputDriver);
    inputDriver.type = LV_INDEV_TYPE_POINTER;
    inputDriver.read_cb = touchRead;
    lv_indev_drv_register(&inputDriver);

    lastLvglTickMs = millis();
    lastLvglHandlerMs = 0;
}

void serviceLvgl() {
    const uint32_t now = millis();
    const uint32_t elapsed = now - lastLvglTickMs;
    if (elapsed > 0) {
        lv_tick_inc(elapsed);
        lastLvglTickMs = now;
    }

    if (now - lastLvglHandlerMs >= 5UL) {
        lastLvglHandlerMs = now;
        lv_timer_handler();
    }
}
} // namespace

void setup() {
    Serial.begin(115200);
    delay(60);

    Serial.println();
    Serial.println("====================================");
    Serial.println(" AM5 DISPLAY ");
    Serial.println(" TFT_eSPI + LVGL ");
    Serial.println("====================================");

    initBacklight();

    tft.init();
    tft.setRotation(AM5_DISPLAY_ROTATION);
    tft.invertDisplay(AM5_TFT_INVERT != 0);
    delay(20);
    // Keep the physical framebuffer black while the backlight is off.
    // The panel inversion command is handled by the controller; RGB data is not software-complemented.
    tft.fillScreen(TFT_BLACK);

    const bool touchOk = touch.begin();
    Serial.println(touchOk ? "Ready" : "Not detected");

    displayStateBegin();
    wirelessLinkBegin();
    initLvgl();
    uiBegin();
}

void loop() {
    wirelessLinkLoop();
    serviceBacklight();
    serviceLvgl();
    uiLoop();
    delay(1);
}
