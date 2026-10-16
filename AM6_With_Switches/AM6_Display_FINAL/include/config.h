#pragma once

#include <Arduino.h>

// ============================================================
// AM6 DISPLAY - JC2432W328 / XH-32S
// TFT_eSPI + LVGL 8.3.11, landscape 320 x 240
// Wi-Fi/HTTP monitoring link with AM6 Indoor main controller.
// ============================================================

// ---------- Display ----------
#define AM6_SCREEN_WIDTH          320
#define AM6_SCREEN_HEIGHT         240
#define AM6_DISPLAY_ROTATION      1
#define AM6_BACKLIGHT_PIN         27
#define AM6_BACKLIGHT_PWM_CH      0
#define AM6_BACKLIGHT_PWM_FREQ    5000
#define AM6_BACKLIGHT_PWM_BITS    8
#define AM6_BACKLIGHT_LEVEL       235

// Physical AM6 panel calibration from the real hardware photo.
// This JC2432W328 panel displays correct black/cyan/green colors with
// ST7789 inversion OFF. Inversion ON complements the whole UI (black->white,
// cyan->orange/red, green->purple).
#define AM6_TFT_INVERT            0

// LVGL partial render buffer: 320 x 20 x 2 bytes = 12.8 KB.
#define AM6_LVGL_BUFFER_LINES     20

// ---------- CST820 capacitive touch ----------
#define AM6_TOUCH_SDA             33
#define AM6_TOUCH_SCL             32
#define AM6_TOUCH_RST             25
#define AM6_TOUCH_I2C_HZ          100000UL

#define AM6_TOUCH_READ_RETRIES     3U
#define AM6_TOUCH_RELEASE_GRACE_MS 45UL

// ---------- Indoor Wi-Fi / HTTP ----------
#define AM6_INDOOR_AP_SSID        "AM6-AAA001"
#define AM6_INDOOR_AP_PASSWORD    "bitahomes"

#define AM6_INDOOR_IP_0           192
#define AM6_INDOOR_IP_1           168
#define AM6_INDOOR_IP_2           4
#define AM6_INDOOR_IP_3           1

// Fixed display address on the Indoor SoftAP network. A high host number is
// used to avoid collisions with phones receiving low DHCP addresses.
#define AM6_DISPLAY_IP_0          192
#define AM6_DISPLAY_IP_1          168
#define AM6_DISPLAY_IP_2          4
#define AM6_DISPLAY_IP_3          50

#define AM6_INDOOR_COMMAND_PATH   "/from_display" // legacy; monitoring build does not POST commands
#define AM6_DISPLAY_RECEIVE_PATH  "/receive"
#define AM6_WIFI_RETRY_MS           2000UL
#define AM6_WIFI_CONNECT_TIMEOUT_MS  10000UL
#define AM6_INDOOR_STATE_TIMEOUT_MS  12000UL
#define AM6_WIFI_UI_GRACE_MS          8000UL
#define AM6_HTTP_CONNECT_TIMEOUT_MS   350UL
#define AM6_HTTP_RESPONSE_TIMEOUT_MS  700UL
#define AM6_HTTP_RETRY_MS         1000UL

// ---------- UI timing ----------
#define AM6_SPLASH_DURATION_MS    2200UL
#define AM6_UI_MODEL_REFRESH_MS   25UL

// ---------- AM6 states ----------
enum AM6FanSpeed : uint8_t {
    AM6_FAN_LOW    = 1,
    AM6_FAN_MEDIUM = 2,
    AM6_FAN_HIGH   = 3
};

enum AM6Mode : uint8_t {
    AM6_MODE_COOL = 0,
    AM6_MODE_HEAT = 1
};


enum AM6OutdoorStatus : uint8_t {
    AM6_OUT_STOPPED = 0,
    AM6_OUT_RUNNING = 1,
    AM6_OUT_HIGH_PSI = 2,
    AM6_OUT_LOW_PSI = 3,
    AM6_OUT_POWER_FAULT = 4,
    AM6_OUT_OVERLOAD = 5
};
