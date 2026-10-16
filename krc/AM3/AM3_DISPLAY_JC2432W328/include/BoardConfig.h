#pragma once

#include <Arduino.h>

// ============================================================
// Guition / XH-32S JC2432W328 2.8-inch hardware configuration
// Native panel: 240 x 320 ST7789
// Logical UI:   320 x 240 landscape
// ============================================================

constexpr uint16_t AM3_SCREEN_WIDTH  = 320;
constexpr uint16_t AM3_SCREEN_HEIGHT = 240;
constexpr uint8_t  AM3_TFT_ROTATION  = 1;

// TFT_eSPI SPI pins are supplied from platformio.ini.
constexpr int8_t AM3_BACKLIGHT_PIN = 27;
constexpr bool   AM3_BACKLIGHT_ACTIVE_HIGH = true;

// CST820 capacitive touch (I2C).
constexpr int8_t AM3_TOUCH_SDA = 33;
constexpr int8_t AM3_TOUCH_SCL = 32;
constexpr int8_t AM3_TOUCH_RST = 25;
constexpr int8_t AM3_TOUCH_INT = -1;  // Polling is intentionally used.

// Native touch coordinates are portrait 240 x 320. For TFT rotation 1,
// working transformation is swap XY and invert the resulting Y axis.
constexpr bool AM3_TOUCH_SWAP_XY  = true;
constexpr bool AM3_TOUCH_INVERT_X = false;
constexpr bool AM3_TOUCH_INVERT_Y = true;

constexpr uint32_t AM3_TOUCH_I2C_HZ = 400000UL;
constexpr uint32_t AM3_UI_REFRESH_MS = 250UL;
constexpr uint32_t AM3_SPLASH_MS = 900UL;
