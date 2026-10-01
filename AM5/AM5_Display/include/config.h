#pragma once

#include <Arduino.h>

// ============================================================
// AM5 DISPLAY - JC2432W328 / XH-32S
// TFT_eSPI + LVGL 8.3.11, landscape 320 x 240
// Wi-Fi/HTTP communication with AM5 Indoor main controller.
// ============================================================

// ---------- Display ----------
#define AM5_SCREEN_WIDTH          320
#define AM5_SCREEN_HEIGHT         240
#define AM5_DISPLAY_ROTATION      1
#define AM5_BACKLIGHT_PIN         27
#define AM5_BACKLIGHT_PWM_CH      0
#define AM5_BACKLIGHT_PWM_FREQ    5000
#define AM5_BACKLIGHT_PWM_BITS    8
#define AM5_BACKLIGHT_LEVEL       235

// Physical AM5 panel calibration from the real hardware photo.
// This JC2432W328 panel displays correct black/cyan/green colors with
// ST7789 inversion OFF. Inversion ON complements the whole UI (black->white,
// cyan->orange/red, green->purple).
#define AM5_TFT_INVERT            0

// LVGL partial render buffer: 320 x 20 x 2 bytes = 12.8 KB.
#define AM5_LVGL_BUFFER_LINES     20

// ---------- CST820 capacitive touch ----------
#define AM5_TOUCH_SDA             33
#define AM5_TOUCH_SCL             32
#define AM5_TOUCH_RST             25
#define AM5_TOUCH_I2C_HZ          100000UL

#define AM5_TOUCH_READ_RETRIES     3U
#define AM5_TOUCH_RELEASE_GRACE_MS 45UL

// ---------- Indoor Wi-Fi / HTTP ----------
#define AM5_INDOOR_AP_SSID        "AM5-AAA001"
#define AM5_INDOOR_AP_PASSWORD    "bitahomes"

#define AM5_INDOOR_IP_0           192
#define AM5_INDOOR_IP_1           168
#define AM5_INDOOR_IP_2           4
#define AM5_INDOOR_IP_3           1

// Fixed display address on the Indoor SoftAP network. A high host number is
// used to avoid collisions with phones receiving low DHCP addresses.
#define AM5_DISPLAY_IP_0          192
#define AM5_DISPLAY_IP_1          168
#define AM5_DISPLAY_IP_2          4
#define AM5_DISPLAY_IP_3          50

#define AM5_INDOOR_COMMAND_PATH   "/from_display"
#define AM5_DISPLAY_RECEIVE_PATH  "/receive"
#define AM5_WIFI_RETRY_MS         3000UL
#define AM5_WIFI_CONNECT_TIMEOUT_MS 10000UL
#define AM5_INDOOR_STATE_TIMEOUT_MS  5000UL
#define AM5_HTTP_CONNECT_TIMEOUT_MS   350UL
#define AM5_HTTP_RESPONSE_TIMEOUT_MS  700UL
#define AM5_HTTP_RETRY_MS         1000UL

// ---------- UI timing ----------
#define AM5_SPLASH_DURATION_MS    1400UL
#define AM5_UI_MODEL_REFRESH_MS   25UL

// ---------- AM5 states ----------
enum AM5FanSpeed : uint8_t {
    AM5_FAN_LOW    = 1,
    AM5_FAN_MEDIUM = 2,
    AM5_FAN_HIGH   = 3
};

enum AM5Mode : uint8_t {
    AM5_MODE_COOL = 0,
    AM5_MODE_HEAT = 1
};
