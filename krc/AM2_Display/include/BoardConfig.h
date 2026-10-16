#pragma once
#include <Arduino.h>

// Proven JC2432W328 / XH-32S variant used in the AM3/AM4 display conversions.
#define AM2_SCREEN_W 320
#define AM2_SCREEN_H 240
#define AM2_TFT_BL_PIN 27

// CST820 capacitive touch (native panel is 240x320 portrait).
#define AM2_TOUCH_SDA 33
#define AM2_TOUCH_SCL 32
#define AM2_TOUCH_RST 25
#define AM2_TOUCH_ADDR 0x15

// RS485 on the display board. These pins are available on the JC2432W328 CON1.
// GPIO16/17 are also connected to two RGB LED channels; this does not prevent UART use.
#define AM2_RS485_TX_PIN 16
#define AM2_RS485_RX_PIN 17
#define AM2_RS485_DE_RE_PIN 4
#define AM2_RS485_BAUD 9600
