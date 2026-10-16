#pragma once

// JC2432W328 2.8" display/touch hardware.
#define TFT_BACKLIGHT_PIN 27

#define TOUCH_SDA       33
#define TOUCH_SCL       32
#define TOUCH_RST       25
#define TOUCH_INT       21
#define TOUCH_I2C_ADDR  0x15

#define DX_UI_WIDTH   320
#define DX_UI_HEIGHT  240

// CST820 reports portrait-native coordinates: X=0..239, Y=0..319.
#define DX_TOUCH_RAW_WIDTH   240
#define DX_TOUCH_RAW_HEIGHT  320

// The TFT is rendered with tft.setRotation(1), giving a 320x240 landscape UI.
// Touch coordinates must be transformed from the controller's fixed portrait
// coordinate space INTO that rotated logical display space.  This is the
// inverse transform of the display rotation:
//
//   screenX = rawY
//   screenY = 239 - rawX
//
// Written as explicit calibration flags so orientation is unambiguous.
#define DX_TOUCH_SWAP_XY   1
#define DX_TOUCH_INVERT_X  0
#define DX_TOUCH_INVERT_Y  1

// Optional small edge compensation. Leave at zero for the JC2432W328 panel.
#define DX_TOUCH_X_OFFSET  0
#define DX_TOUCH_Y_OFFSET  0

// Set to 1 temporarily to print raw and mapped coordinates on Serial.
#define DX_TOUCH_DEBUG 0
