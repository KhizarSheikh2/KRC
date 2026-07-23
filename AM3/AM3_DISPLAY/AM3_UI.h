#pragma once

/*
  AM3 2.8-inch TFT UI integration

  This file contains only the display/touch presentation layer taken from
  the sample AM3_Touch_Pressure_Temperature_UI project.

  It does NOT create another OneWire bus, read separate sensors, connect to
  Wi-Fi, calculate pressures, control outputs, scan alarms, or change MQTT.
  It only displays the existing AM3 variables maintained by the base code.

  Display: ILI9341 240x320 SPI TFT, landscape 320x240
  Touch:   XPT2046-compatible controller

  This display has its own ESP32, so it uses the sample display wiring.
  GPIO21 is available here because the PCF8574 remains on the separate
  hardware-host ESP32.
*/

#include <SPI.h>
#include <time.h>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include "AM3_UI_Types.h"
#include "DisplayState.h"

// =====================================================
// TFT and touch pins
// =====================================================

#define AM3_TFT_MISO 19
#define AM3_TFT_MOSI 23
#define AM3_TFT_SCLK 18
#define AM3_TFT_CS   15
#define AM3_TFT_DC    2
#define AM3_TFT_RST   4
#define AM3_TOUCH_CS 21

constexpr uint32_t AM3_TFT_SPI_FREQUENCY = 27000000UL;
constexpr uint8_t AM3_DISPLAY_ROTATION = 1;
constexpr int16_t AM3_SCREEN_WIDTH = 320;
constexpr int16_t AM3_SCREEN_HEIGHT = 240;

Adafruit_ILI9341 am3Tft(AM3_TFT_CS, AM3_TFT_DC, AM3_TFT_RST);
XPT2046_Touchscreen am3Touch(AM3_TOUCH_CS);

// =====================================================
// Touch calibration
// =====================================================

#define AM3_TOUCH_DEBUG 0

constexpr int16_t AM3_TOUCH_RAW_X_MIN = 220;
constexpr int16_t AM3_TOUCH_RAW_X_MAX = 3900;
constexpr int16_t AM3_TOUCH_RAW_Y_MIN = 220;
constexpr int16_t AM3_TOUCH_RAW_Y_MAX = 3900;
constexpr bool AM3_TOUCH_INVERT_X = false;
constexpr bool AM3_TOUCH_INVERT_Y = false;
constexpr int16_t AM3_TOUCH_MIN_PRESSURE = 300;
constexpr int16_t AM3_SWIPE_MIN_DISTANCE = 55;
constexpr unsigned long AM3_TOUCH_DEBOUNCE_MS = 180;

// =====================================================
// 1x4 membrane keypad
// =====================================================

constexpr byte ROWS = 1;
constexpr byte COLS = 4;
byte rowPins[ROWS] = {27};
byte colPins[COLS] = {25, 26, 32, 33};
const char am3KeypadKeys[COLS] = {'1', '2', '3', '4'};

constexpr unsigned long AM3_KEYPAD_DEBOUNCE_MS = 35;
char am3KeypadLastRawKey = '\0';
char am3KeypadStableKey = '\0';
unsigned long am3KeypadLastChange = 0;
uint8_t am3SelectedTemperatureIndex = 0;

// =====================================================
// Colours
// =====================================================

constexpr uint16_t am3Rgb565(uint8_t red, uint8_t green, uint8_t blue) {
  return static_cast<uint16_t>(
    ((red & 0xF8) << 8) |
    ((green & 0xFC) << 3) |
    (blue >> 3)
  );
}

constexpr uint16_t AM3_COLOR_BACKGROUND = am3Rgb565(7, 22, 38);
constexpr uint16_t AM3_COLOR_HEADER     = am3Rgb565(13, 42, 70);
constexpr uint16_t AM3_COLOR_CARD       = am3Rgb565(246, 249, 252);
constexpr uint16_t AM3_COLOR_SHADOW     = am3Rgb565(2, 10, 18);
constexpr uint16_t AM3_COLOR_TEXT       = am3Rgb565(17, 35, 53);
constexpr uint16_t AM3_COLOR_MUTED      = am3Rgb565(91, 111, 128);
constexpr uint16_t AM3_COLOR_DIVIDER    = am3Rgb565(204, 216, 226);
constexpr uint16_t AM3_COLOR_BLUE       = am3Rgb565(36, 105, 221);
constexpr uint16_t AM3_COLOR_GREEN      = am3Rgb565(24, 154, 91);
constexpr uint16_t AM3_COLOR_ORANGE     = am3Rgb565(239, 132, 36);
constexpr uint16_t AM3_COLOR_RED        = am3Rgb565(218, 62, 68);
constexpr uint16_t AM3_COLOR_PURPLE     = am3Rgb565(130, 76, 190);
constexpr uint16_t AM3_COLOR_CYAN       = am3Rgb565(33, 145, 187);
constexpr uint16_t AM3_COLOR_YELLOW     = am3Rgb565(246, 190, 55);
constexpr uint16_t AM3_COLOR_SUCCESS    = am3Rgb565(27, 164, 98);
constexpr uint16_t AM3_COLOR_ERROR      = am3Rgb565(221, 64, 70);

// =====================================================
// UI state
// =====================================================

constexpr long AM3_GMT_OFFSET_SECONDS = 5L * 60L * 60L;
constexpr int AM3_DAYLIGHT_OFFSET_SECONDS = 0;
constexpr unsigned long AM3_UI_REFRESH_INTERVAL = 1000;

AM3ScreenPage am3CurrentPage = AM3_PAGE_MAIN;
unsigned long am3LastUiRefresh = 0;
bool am3LastWiFiConnected = false;
bool am3UiInitialized = false;

bool am3TouchInProgress = false;
int16_t am3TouchStartX = 0;
int16_t am3TouchStartY = 0;
int16_t am3TouchLastX = 0;
int16_t am3TouchLastY = 0;
unsigned long am3LastTouchAction = 0;

AM3DashboardCard am3MainCards[4] = {
  {8,   53, 148, 80, "TIME / DATE",         AM3_COLOR_BLUE,   AM3_ICON_CLOCK},
  {164, 53, 148, 80, "SYSTEM STATUS",       AM3_COLOR_GREEN,  AM3_ICON_SYSTEM_STATUS},
  {8,  141, 148, 84, "SUCTION PRESSURE",   AM3_COLOR_ORANGE, AM3_ICON_SUCTION_PRESSURE},
  {164,141, 148, 84, "DISCHARGE PRESSURE", AM3_COLOR_RED,    AM3_ICON_DISCHARGE_PRESSURE}
};

constexpr uint8_t AM3_TEMPERATURE_COUNT = 5;
const char* AM3_TEMPERATURE_NAMES[AM3_TEMPERATURE_COUNT] = {
  "RETURN TEMP",
  "SUCTION TEMP",
  "DSC TEMP",
  "SUPPLY TEMP",
  "OIL TEMP"
};

const uint16_t AM3_TEMPERATURE_COLORS[AM3_TEMPERATURE_COUNT] = {
  AM3_COLOR_BLUE,
  AM3_COLOR_GREEN,
  AM3_COLOR_RED,
  AM3_COLOR_CYAN,
  AM3_COLOR_PURPLE
};

// =====================================================
// Text helpers
// =====================================================

void am3DrawCenteredText(
  const String& text,
  int16_t centerX,
  int16_t y,
  uint8_t textSize,
  uint16_t textColor,
  uint16_t backgroundColor
) {
  int16_t x1;
  int16_t y1;
  uint16_t textWidth;
  uint16_t textHeight;

  am3Tft.setTextSize(textSize);
  am3Tft.setTextColor(textColor, backgroundColor);
  am3Tft.getTextBounds(text, 0, 0, &x1, &y1, &textWidth, &textHeight);
  am3Tft.setCursor(centerX - static_cast<int16_t>(textWidth / 2), y);
  am3Tft.print(text);
}

void am3DrawLeftText(
  const String& text,
  int16_t x,
  int16_t y,
  uint8_t textSize,
  uint16_t textColor,
  uint16_t backgroundColor
) {
  am3Tft.setTextSize(textSize);
  am3Tft.setTextColor(textColor, backgroundColor);
  am3Tft.setCursor(x, y);
  am3Tft.print(text);
}

// Draws a stronger label using the built-in Adafruit GFX font.
// The first pass clears the text area; the second transparent pass is shifted
// by one pixel to create a reliable bold appearance without another font file.
void am3DrawBoldLeftText(
  const String& text,
  int16_t x,
  int16_t y,
  uint8_t textSize,
  uint16_t textColor,
  uint16_t backgroundColor
) {
  am3Tft.setTextSize(textSize);
  am3Tft.setTextColor(textColor, backgroundColor);
  am3Tft.setCursor(x, y);
  am3Tft.print(text);

  am3Tft.setTextColor(textColor);
  am3Tft.setCursor(x + 1, y);
  am3Tft.print(text);
}

// =====================================================
// Icons
// =====================================================

void am3DrawClockIcon(int16_t cx, int16_t cy, uint16_t color) {
  am3Tft.drawCircle(cx, cy, 6, color);
  am3Tft.drawLine(cx, cy, cx, cy - 4, color);
  am3Tft.drawLine(cx, cy, cx + 3, cy + 2, color);
}

void am3DrawCalendarIcon(int16_t cx, int16_t cy, uint16_t color) {
  am3Tft.drawRect(cx - 6, cy - 5, 12, 11, color);
  am3Tft.drawFastHLine(cx - 5, cy - 1, 10, color);
  am3Tft.drawFastVLine(cx - 3, cy - 7, 4, color);
  am3Tft.drawFastVLine(cx + 3, cy - 7, 4, color);
}

void am3DrawPressureGaugeIcon(int16_t cx, int16_t cy, uint16_t color, bool highSide) {
  am3Tft.drawCircle(cx, cy + 1, 7, color);
  am3Tft.drawFastHLine(cx - 5, cy + 6, 10, color);
  am3Tft.drawLine(cx, cy + 1, highSide ? cx + 4 : cx - 3, highSide ? cy - 3 : cy - 2, color);
  am3Tft.fillCircle(cx, cy + 1, 1, color);
}

void am3DrawThermometerIcon(int16_t cx, int16_t cy, uint16_t color) {
  am3Tft.drawRoundRect(cx - 2, cy - 7, 5, 11, 2, color);
  am3Tft.drawFastVLine(cx, cy - 4, 8, color);
  am3Tft.fillCircle(cx, cy + 5, 4, color);
}

// =====================================================
// Header and footer
// =====================================================

void am3DrawHeader() {
  am3Tft.fillRect(0, 0, AM3_SCREEN_WIDTH, 50, AM3_COLOR_BACKGROUND);
  am3Tft.fillRoundRect(6, 5, 308, 39, 9, AM3_COLOR_HEADER);

  // am3Tft.fillCircle(25, 24, 13, AM3_COLOR_BLUE);
  // am3Tft.drawCircle(25, 24, 10, ILI9341_WHITE);
  // am3DrawCenteredText("A", 25, 16, 2, ILI9341_WHITE, AM3_COLOR_BLUE);
  am3DrawLeftText("AM3", 20, 15, 3, ILI9341_WHITE, AM3_COLOR_HEADER);

  const char* pageTitle =
    (am3CurrentPage == AM3_PAGE_MAIN) ? "DASHBOARD" : "TEMPERATURES";

  am3DrawCenteredText(pageTitle, 172, 18, 2, AM3_COLOR_YELLOW, AM3_COLOR_HEADER);

  am3Tft.fillRect(8, 46, 304, 2, AM3_COLOR_BLUE);
  am3Tft.fillRect(8, 48, 304, 1, AM3_COLOR_GREEN);
}

void am3DrawNavigationFooter() {
  am3Tft.fillRect(0, 228, AM3_SCREEN_WIDTH, 12, AM3_COLOR_BACKGROUND);

  const uint16_t page0Color =
    (am3CurrentPage == AM3_PAGE_MAIN) ? AM3_COLOR_BLUE : AM3_COLOR_MUTED;
  const uint16_t page1Color =
    (am3CurrentPage == AM3_PAGE_TEMPERATURES) ? AM3_COLOR_GREEN : AM3_COLOR_MUTED;

  am3Tft.fillCircle(153, 234, 3, page0Color);
  am3Tft.fillCircle(167, 234, 3, page1Color);

  // A right swipe always toggles between Dashboard and Temperatures.
  am3DrawLeftText("KEY 4 / SWIPE RIGHT", 190, 231, 1, AM3_COLOR_MUTED, AM3_COLOR_BACKGROUND);
  am3Tft.drawLine(306, 231, 311, 234, AM3_COLOR_GREEN);
  am3Tft.drawLine(311, 234, 306, 237, AM3_COLOR_GREEN);
}

// =====================================================
// Splash
// =====================================================

void am3DrawSplashScreen() {
  am3Tft.fillScreen(AM3_COLOR_BACKGROUND);
  am3Tft.fillRoundRect(32, 29, 256, 176, 16, AM3_COLOR_HEADER);
  am3Tft.drawRoundRect(32, 29, 256, 176, 16, AM3_COLOR_BLUE);
  am3Tft.drawRoundRect(35, 32, 250, 170, 14, AM3_COLOR_GREEN);

  am3Tft.fillCircle(160, 89, 36, AM3_COLOR_BLUE);
  am3Tft.drawCircle(160, 89, 30, ILI9341_WHITE);
  am3Tft.drawCircle(160, 89, 24, AM3_COLOR_HEADER);
  am3DrawCenteredText("A", 160, 70, 5, ILI9341_WHITE, AM3_COLOR_BLUE);
  am3DrawCenteredText("AM3", 160, 132, 4, ILI9341_WHITE, AM3_COLOR_HEADER);
  am3DrawCenteredText("INITIALIZING", 160, 166, 1, AM3_COLOR_YELLOW, AM3_COLOR_HEADER);

  constexpr int16_t barX = 65;
  constexpr int16_t barY = 188;
  constexpr int16_t barW = 190;
  constexpr int16_t barH = 8;
  am3Tft.drawRoundRect(barX, barY, barW, barH, 4, AM3_COLOR_MUTED);

  for (int16_t progress = 0; progress <= barW - 4; progress += 10) {
    am3Tft.fillRoundRect(barX + 2, barY + 2, progress, barH - 4, 2, AM3_COLOR_GREEN);
    delay(20);
  }
}

// =====================================================
// Main cards
// =====================================================

void am3DrawMainCardIcon(const AM3DashboardCard& card) {
  const int16_t cx = card.x + 15;
  const int16_t cy = card.y + 13;

  switch (card.icon) {
    case AM3_ICON_CLOCK:
      am3DrawClockIcon(cx, cy, ILI9341_WHITE);
      break;
    case AM3_ICON_CALENDAR:
      am3DrawCalendarIcon(cx, cy, ILI9341_WHITE);
      break;
    case AM3_ICON_SYSTEM_STATUS:
      am3Tft.drawCircle(cx, cy, 7, ILI9341_WHITE);
      am3Tft.drawFastVLine(cx, cy - 8, 7, ILI9341_WHITE);
      break;
    case AM3_ICON_SUCTION_PRESSURE:
      am3DrawPressureGaugeIcon(cx, cy, ILI9341_WHITE, false);
      break;
    case AM3_ICON_DISCHARGE_PRESSURE:
      am3DrawPressureGaugeIcon(cx, cy, ILI9341_WHITE, true);
      break;
  }
}

void am3DrawDashboardCard(const AM3DashboardCard& card) {
  am3Tft.fillRoundRect(card.x + 2, card.y + 3, card.width, card.height, 8, AM3_COLOR_SHADOW);
  am3Tft.fillRoundRect(card.x, card.y, card.width, card.height, 8, AM3_COLOR_CARD);
  am3Tft.fillRoundRect(card.x, card.y, card.width, 27, 8, card.accentColor);
  am3Tft.fillRect(card.x, card.y + 14, card.width, 13, card.accentColor);
  am3Tft.drawRoundRect(card.x, card.y, card.width, card.height, 8, card.accentColor);

  am3DrawMainCardIcon(card);
  am3DrawLeftText(card.title, card.x + 27, card.y + 9, 1, ILI9341_WHITE, card.accentColor);
  am3Tft.fillCircle(card.x + card.width - 13, card.y + 13, 3, ILI9341_WHITE);
}

void am3ClearMainCardValueArea(const AM3DashboardCard& card) {
  am3Tft.fillRect(card.x + 3, card.y + 30, card.width - 6, card.height - 34, AM3_COLOR_CARD);
}

void am3DrawWaitingValue(
  const AM3DashboardCard& card,
  const String& mainText,
  const String& smallText
) {
  am3ClearMainCardValueArea(card);
  am3DrawCenteredText(mainText, card.x + card.width / 2, card.y + 43, 2, card.accentColor, AM3_COLOR_CARD);
  am3DrawCenteredText(smallText, card.x + card.width / 2, card.y + card.height - 13, 1, AM3_COLOR_MUTED, AM3_COLOR_CARD);
}

// =====================================================
// Temperature rows
// =====================================================

// Five compact cards fit between the 50 px header and 228 px footer.
// The label and live-value zones are deliberately separated for readability.
constexpr int16_t AM3_TEMP_ROW_X = 8;
constexpr int16_t AM3_TEMP_ROW_W = 304;
constexpr int16_t AM3_TEMP_ROW_H = 31;
constexpr int16_t AM3_TEMP_ROW_FIRST_Y = 54;
constexpr int16_t AM3_TEMP_ROW_GAP = 3;
constexpr int16_t AM3_TEMP_ICON_W = 42;
constexpr int16_t AM3_TEMP_DIVIDER_X = AM3_TEMP_ROW_X + 205;
constexpr int16_t AM3_TEMP_VALUE_X = AM3_TEMP_DIVIDER_X + 5;
constexpr int16_t AM3_TEMP_VALUE_W = (AM3_TEMP_ROW_X + AM3_TEMP_ROW_W - 4) - AM3_TEMP_VALUE_X;
constexpr int16_t AM3_TEMP_VALUE_CENTER_X = AM3_TEMP_VALUE_X + (AM3_TEMP_VALUE_W / 2);

int16_t am3GetTemperatureRowY(uint8_t index) {
  return AM3_TEMP_ROW_FIRST_Y + index * (AM3_TEMP_ROW_H + AM3_TEMP_ROW_GAP);
}

void am3DrawTemperatureRowFrame(uint8_t index) {
  if (index >= AM3_TEMPERATURE_COUNT) {
    return;
  }

  const int16_t rowY = am3GetTemperatureRowY(index);
  const uint16_t accent = AM3_TEMPERATURE_COLORS[index];

  // Card and shadow
  am3Tft.fillRoundRect(AM3_TEMP_ROW_X + 2, rowY + 2,
                       AM3_TEMP_ROW_W, AM3_TEMP_ROW_H,
                       7, AM3_COLOR_SHADOW);
  am3Tft.fillRoundRect(AM3_TEMP_ROW_X, rowY,
                       AM3_TEMP_ROW_W, AM3_TEMP_ROW_H,
                       7, AM3_COLOR_CARD);
  am3Tft.drawRoundRect(AM3_TEMP_ROW_X, rowY,
                       AM3_TEMP_ROW_W, AM3_TEMP_ROW_H,
                       7, accent);

  // Coloured icon panel
  am3Tft.fillRoundRect(AM3_TEMP_ROW_X, rowY,
                       AM3_TEMP_ICON_W, AM3_TEMP_ROW_H,
                       7, accent);
  am3Tft.fillRect(AM3_TEMP_ROW_X + 21, rowY,
                  AM3_TEMP_ICON_W - 21, AM3_TEMP_ROW_H,
                  accent);
  am3DrawThermometerIcon(AM3_TEMP_ROW_X + 20, rowY + 15, ILI9341_WHITE);

  // Temperature name
  am3DrawLeftText(AM3_TEMPERATURE_NAMES[index], AM3_TEMP_ROW_X + 48, rowY + 10, 2, AM3_COLOR_TEXT, AM3_COLOR_CARD);

  // Clear visual division between the name and live reading
  am3Tft.drawFastVLine(AM3_TEMP_DIVIDER_X,rowY + 5,AM3_TEMP_ROW_H - 10,AM3_COLOR_DIVIDER);
  am3Tft.drawFastVLine(AM3_TEMP_DIVIDER_X + 1,rowY + 8,AM3_TEMP_ROW_H - 16,accent);
}

void am3ClearTemperatureRowValueArea(uint8_t index) {
  if (index >= AM3_TEMPERATURE_COUNT) {
    return;
  }

  const int16_t rowY = am3GetTemperatureRowY(index);
  am3Tft.fillRect(AM3_TEMP_VALUE_X,
                  rowY + 3,
                  AM3_TEMP_VALUE_W,
                  AM3_TEMP_ROW_H - 6,
                  AM3_COLOR_CARD);
}

float am3GetTemperatureC(uint8_t index) {
  switch (index) {
    case 0: return ReturnTempC;
    case 1: return SuctionTempC;
    case 2: return dischargeTempC;
    case 3: return SupplyTempC;
    case 4: return OilTempC;
    default: return NAN;
  }
}

bool am3IsValidTemperature(float value) {
  return isfinite(value) && value > -100.0f && value < SENSOR_DISCONNECTED;
}

void am3UpdateTemperatureRow(uint8_t index) {
  if (index >= AM3_TEMPERATURE_COUNT) {
    return;
  }

  am3ClearTemperatureRowValueArea(index);
  const int16_t rowY = am3GetTemperatureRowY(index);
  const float value = am3GetTemperatureC(index);

  if (!am3IsValidTemperature(value)) {
    const char* message = (value >= SENSOR_NOT_SELECTED) ? "NOT SET" : "NO SENSOR";
    am3DrawCenteredText(message,
                        AM3_TEMP_VALUE_CENTER_X,
                        rowY + 12,
                        1,
                        AM3_COLOR_ERROR,
                        AM3_COLOR_CARD);
    return;
  }

  const String valueText = String(value, 1) + " C";
  am3DrawCenteredText(valueText,
                      AM3_TEMP_VALUE_CENTER_X,
                      rowY + 8,
                      2,
                      AM3_TEMPERATURE_COLORS[index],
                      AM3_COLOR_CARD);
}

void am3DrawTemperatureSelection(uint8_t index, bool selected) {
  if (index >= AM3_TEMPERATURE_COUNT) {
    return;
  }

  const int16_t rowY = am3GetTemperatureRowY(index);

  if (selected) {
    // Two-pixel highlight is easier to see than the previous single border.
    am3Tft.drawRoundRect(AM3_TEMP_ROW_X - 2,
                         rowY - 2,
                         AM3_TEMP_ROW_W + 4,
                         AM3_TEMP_ROW_H + 4,
                         8,
                         AM3_COLOR_YELLOW);
    am3Tft.drawRoundRect(AM3_TEMP_ROW_X - 1,
                         rowY - 1,
                         AM3_TEMP_ROW_W + 2,
                         AM3_TEMP_ROW_H + 2,
                         8,
                         AM3_COLOR_ORANGE);
  } else {
    // Remove the old highlight and redraw the row so its shadow is restored.
    am3Tft.drawRoundRect(AM3_TEMP_ROW_X - 2,
                         rowY - 2,
                         AM3_TEMP_ROW_W + 4,
                         AM3_TEMP_ROW_H + 4,
                         8,
                         AM3_COLOR_BACKGROUND);
    am3Tft.drawRoundRect(AM3_TEMP_ROW_X - 1,
                         rowY - 1,
                         AM3_TEMP_ROW_W + 2,
                         AM3_TEMP_ROW_H + 2,
                         8,
                         AM3_COLOR_BACKGROUND);
    am3DrawTemperatureRowFrame(index);
    am3UpdateTemperatureRow(index);
  }
}

void am3SelectTemperatureRow(uint8_t newIndex) {
  if (newIndex >= AM3_TEMPERATURE_COUNT || newIndex == am3SelectedTemperatureIndex) {
    return;
  }

  am3DrawTemperatureSelection(am3SelectedTemperatureIndex, false);
  am3SelectedTemperatureIndex = newIndex;
  am3DrawTemperatureSelection(am3SelectedTemperatureIndex, true);
}

// =====================================================
// Page drawing
// =====================================================

void am3DrawMainPage() {
  am3CurrentPage = AM3_PAGE_MAIN;
  am3Tft.fillScreen(AM3_COLOR_BACKGROUND);
  am3DrawHeader();

  for (uint8_t i = 0; i < 4; i++) {
    am3DrawDashboardCard(am3MainCards[i]);
  }

  am3DrawWaitingValue(am3MainCards[0], "SYNCING", "TIME AND DATE");
  am3DrawWaitingValue(am3MainCards[1], "STOPPED", "COMPRESSOR STATUS");
  am3DrawWaitingValue(am3MainCards[2], "WAITING", "AM3 PRESSURE");
  am3DrawWaitingValue(am3MainCards[3], "WAITING", "AM3 PRESSURE");
  am3DrawNavigationFooter();
}

void am3DrawTemperaturesPage() {
  am3CurrentPage = AM3_PAGE_TEMPERATURES;
  am3Tft.fillScreen(AM3_COLOR_BACKGROUND);
  am3DrawHeader();

  for (uint8_t i = 0; i < AM3_TEMPERATURE_COUNT; i++) {
    am3DrawTemperatureRowFrame(i);
    am3UpdateTemperatureRow(i);
  }

  am3DrawTemperatureSelection(am3SelectedTemperatureIndex, true);
  am3DrawNavigationFooter();
}

// =====================================================
// Dynamic card values
// =====================================================

void am3UpdateTimeDateCard(const struct tm& timeInfo) {
  if (am3CurrentPage != AM3_PAGE_MAIN) {
    return;
  }

  am3ClearMainCardValueArea(am3MainCards[0]);

  char timeText[20];
  char dateText[16];
  char dayTextBuffer[16];
  strftime(timeText, sizeof(timeText), "%I:%M:%S %p", &timeInfo);
  strftime(dateText, sizeof(dateText), "%d-%m-%Y", &timeInfo);
  strftime(dayTextBuffer, sizeof(dayTextBuffer), "%a", &timeInfo);

  String dayText(dayTextBuffer);
  dayText.toUpperCase();

  am3DrawCenteredText(String(timeText), am3MainCards[0].x + am3MainCards[0].width / 2,
                      am3MainCards[0].y + 34, 2, AM3_COLOR_BLUE, AM3_COLOR_CARD);
  am3DrawCenteredText(String(dateText) + "  " + dayText,
                      am3MainCards[0].x + am3MainCards[0].width / 2,
                      am3MainCards[0].y + 59, 1, AM3_COLOR_TEXT, AM3_COLOR_CARD);
}

void am3UpdateSystemStatusCard() {
  if (am3CurrentPage != AM3_PAGE_MAIN) {
    return;
  }

  am3ClearMainCardValueArea(am3MainCards[1]);
  const bool running = (remoteCompStatus == 1);
  const char* statusText = running ? "RUNNING" : "STOPPED";
  const uint16_t statusColor = running ? AM3_COLOR_SUCCESS : AM3_COLOR_ERROR;

  am3DrawCenteredText(statusText,
                      am3MainCards[1].x + am3MainCards[1].width / 2,
                      am3MainCards[1].y + 40, 3, statusColor, AM3_COLOR_CARD);
  am3DrawCenteredText("COMPRESSOR",
                      am3MainCards[1].x + am3MainCards[1].width / 2,
                      am3MainCards[1].y + am3MainCards[1].height - 13,
                      1, AM3_COLOR_MUTED, AM3_COLOR_CARD);
}

void am3DisplayTimeError() {
  if (am3CurrentPage != AM3_PAGE_MAIN) {
    return;
  }

  am3ClearMainCardValueArea(am3MainCards[0]);
  am3DrawCenteredText("NO TIME", am3MainCards[0].x + am3MainCards[0].width / 2,
                      am3MainCards[0].y + 37, 2, AM3_COLOR_ERROR, AM3_COLOR_CARD);
  am3DrawCenteredText("WAITING FOR HARDWARE", am3MainCards[0].x + am3MainCards[0].width / 2,
                      am3MainCards[0].y + am3MainCards[0].height - 13,
                      1, AM3_COLOR_MUTED, AM3_COLOR_CARD);
}

float am3PressureToFloat(const String& pressureText) {
  if (pressureText.length() == 0 || pressureText == "888" || pressureText == "999") {
    return NAN;
  }

  char* endPointer = nullptr;
  const float value = strtof(pressureText.c_str(), &endPointer);
  if (endPointer == pressureText.c_str() || !isfinite(value) || value < 0.0f) {
    return NAN;
  }
  return value;
}

void am3UpdatePressureCard(
  const AM3DashboardCard& card,
  float pressurePsi,
  const char* statusText
) {
  if (am3CurrentPage != AM3_PAGE_MAIN) {
    return;
  }

  am3ClearMainCardValueArea(card);

  if (!isfinite(pressurePsi)) {
    am3DrawCenteredText("NO INPUT", card.x + card.width / 2, card.y + 46,
                        2, AM3_COLOR_ERROR, AM3_COLOR_CARD);
    am3DrawCenteredText("CHECK AM3 VALUE", card.x + card.width / 2,
                        card.y + card.height - 13, 1, AM3_COLOR_MUTED, AM3_COLOR_CARD);
    return;
  }

  const String valueText =
    (pressurePsi < 100.0f) ? String(pressurePsi, 1) + " PSI" : String(pressurePsi, 0) + " PSI";
  const uint8_t valueSize = (valueText.length() <= 8) ? 3 : 2;
  const int16_t valueY = (valueSize == 3) ? card.y + 42 : card.y + 47;

  am3DrawCenteredText(valueText, card.x + card.width / 2, valueY,
                      valueSize, card.accentColor, AM3_COLOR_CARD);
  am3DrawCenteredText(statusText, card.x + card.width / 2,
                      card.y + card.height - 13, 1, AM3_COLOR_MUTED, AM3_COLOR_CARD);
}

void am3RefreshCurrentPageValues() {
  if (am3CurrentPage == AM3_PAGE_MAIN) {
    struct tm timeInfo;
    if (am3GetRemotePakistanTime(timeInfo)) {
      am3UpdateTimeDateCard(timeInfo);
    } else {
      am3DisplayTimeError();
    }

    am3UpdateSystemStatusCard();
    am3UpdatePressureCard(am3MainCards[2], am3PressureToFloat(suctionPressure), "LOW SIDE | LIVE");
    am3UpdatePressureCard(am3MainCards[3], am3PressureToFloat(dischargePressure), "HIGH SIDE | LIVE");
  } else {
    for (uint8_t i = 0; i < AM3_TEMPERATURE_COUNT; i++) {
      am3UpdateTemperatureRow(i);
    }
  }
}

// =====================================================
// Touch navigation
// =====================================================

bool am3ReadTouchPoint(int16_t& screenX, int16_t& screenY) {
  if (!am3Touch.touched()) {
    return false;
  }

  const TS_Point point = am3Touch.getPoint();

#if AM3_TOUCH_DEBUG
  Serial.print("Touch raw X=");
  Serial.print(point.x);
  Serial.print(" Y=");
  Serial.print(point.y);
  Serial.print(" Z=");
  Serial.println(point.z);
#endif

  if (point.z < AM3_TOUCH_MIN_PRESSURE) {
    return false;
  }

  int32_t mappedX = map(point.x, AM3_TOUCH_RAW_X_MIN, AM3_TOUCH_RAW_X_MAX, 0, AM3_SCREEN_WIDTH - 1);
  int32_t mappedY = map(point.y, AM3_TOUCH_RAW_Y_MIN, AM3_TOUCH_RAW_Y_MAX, 0, AM3_SCREEN_HEIGHT - 1);
  mappedX = constrain(mappedX, static_cast<int32_t>(0), static_cast<int32_t>(AM3_SCREEN_WIDTH - 1));
  mappedY = constrain(mappedY, static_cast<int32_t>(0), static_cast<int32_t>(AM3_SCREEN_HEIGHT - 1));

  if (AM3_TOUCH_INVERT_X) {
    mappedX = (AM3_SCREEN_WIDTH - 1) - mappedX;
  }
  if (AM3_TOUCH_INVERT_Y) {
    mappedY = (AM3_SCREEN_HEIGHT - 1) - mappedY;
  }

  screenX = static_cast<int16_t>(mappedX);
  screenY = static_cast<int16_t>(mappedY);
  return true;
}

void am3SwitchToPage(AM3ScreenPage newPage) {
  if (newPage == am3CurrentPage) {
    return;
  }

  if (newPage == AM3_PAGE_MAIN) {
    am3DrawMainPage();
  } else {
    am3DrawTemperaturesPage();
  }
  am3RefreshCurrentPageValues();
}

void am3ProcessTouchNavigation() {
  const unsigned long now = millis();
  int16_t x = 0;
  int16_t y = 0;
  const bool pressed = am3ReadTouchPoint(x, y);

  if (pressed) {
    if (!am3TouchInProgress) {
      am3TouchInProgress = true;
      am3TouchStartX = x;
      am3TouchStartY = y;
      am3TouchLastX = x;
      am3TouchLastY = y;
    } else {
      am3TouchLastX = x;
      am3TouchLastY = y;
    }
    return;
  }

  if (!am3TouchInProgress) {
    return;
  }

  am3TouchInProgress = false;
  if (now - am3LastTouchAction < AM3_TOUCH_DEBOUNCE_MS) {
    return;
  }

  const int16_t deltaX = am3TouchLastX - am3TouchStartX;
  const int16_t deltaY = am3TouchLastY - am3TouchStartY;
  bool pageChanged = false;

  if (abs(deltaX) >= AM3_SWIPE_MIN_DISTANCE && abs(deltaX) > abs(deltaY)) {
    // A physical finger movement to the right toggles both pages:
    // Dashboard -> Temperatures -> Dashboard.
    if (deltaX > 0) {
      const AM3ScreenPage targetPage =
        (am3CurrentPage == AM3_PAGE_MAIN) ? AM3_PAGE_TEMPERATURES : AM3_PAGE_MAIN;
      am3SwitchToPage(targetPage);
      pageChanged = true;
    }
  }

  if (!pageChanged && am3TouchLastY >= 226 && am3TouchLastX >= 250) {
    const AM3ScreenPage targetPage =
      (am3CurrentPage == AM3_PAGE_MAIN) ? AM3_PAGE_TEMPERATURES : AM3_PAGE_MAIN;
    am3SwitchToPage(targetPage);
    pageChanged = true;
  }

  if (pageChanged) {
    am3LastTouchAction = now;
  }
}

// =====================================================
// Membrane keypad navigation
// =====================================================

void am3InitKeypad() {
  for (byte row = 0; row < ROWS; row++) {
    pinMode(rowPins[row], OUTPUT);
    digitalWrite(rowPins[row], HIGH);
  }

  for (byte col = 0; col < COLS; col++) {
    pinMode(colPins[col], INPUT_PULLUP);
  }
}

char am3ReadRawKeypadKey() {
  digitalWrite(rowPins[0], LOW);
  delayMicroseconds(3);

  char pressedKey = '\0';
  for (byte col = 0; col < COLS; col++) {
    if (digitalRead(colPins[col]) == LOW) {
      pressedKey = am3KeypadKeys[col];
      break;
    }
  }

  digitalWrite(rowPins[0], HIGH);
  return pressedKey;
}

char am3GetKeypadPress() {
  const char rawKey = am3ReadRawKeypadKey();
  const unsigned long now = millis();

  if (rawKey != am3KeypadLastRawKey) {
    am3KeypadLastRawKey = rawKey;
    am3KeypadLastChange = now;
  }

  if (now - am3KeypadLastChange < AM3_KEYPAD_DEBOUNCE_MS) {
    return '\0';
  }

  if (rawKey == am3KeypadStableKey) {
    return '\0';
  }

  am3KeypadStableKey = rawKey;
  return (am3KeypadStableKey != '\0') ? am3KeypadStableKey : '\0';
}

void am3NavigateLeft() {
  if (am3CurrentPage == AM3_PAGE_TEMPERATURES) {
    am3SwitchToPage(AM3_PAGE_MAIN);
  }
}

void am3NavigateRight() {
  const AM3ScreenPage targetPage =
    (am3CurrentPage == AM3_PAGE_MAIN) ? AM3_PAGE_TEMPERATURES : AM3_PAGE_MAIN;
  am3SwitchToPage(targetPage);
}

void am3NavigateUp() {
  if (am3CurrentPage != AM3_PAGE_TEMPERATURES) {
    return;
  }

  const uint8_t nextIndex =
    (am3SelectedTemperatureIndex == 0)
      ? static_cast<uint8_t>(AM3_TEMPERATURE_COUNT - 1)
      : static_cast<uint8_t>(am3SelectedTemperatureIndex - 1);
  am3SelectTemperatureRow(nextIndex);
}

void am3NavigateDown() {
  if (am3CurrentPage != AM3_PAGE_TEMPERATURES) {
    return;
  }

  const uint8_t nextIndex =
    static_cast<uint8_t>((am3SelectedTemperatureIndex + 1) % AM3_TEMPERATURE_COUNT);
  am3SelectTemperatureRow(nextIndex);
}

void am3ProcessKeypadNavigation() {
  const char key = am3GetKeypadPress();
  if (key == '\0') {
    return;
  }

  Serial.print("[KEYPAD] Key ");
  Serial.println(key);

  switch (key) {
    case '1':
      am3NavigateLeft();
      break;
    case '2':
      am3NavigateUp();
      break;
    case '3':
      am3NavigateDown();
      break;
    case '4':
      am3NavigateRight();
      break;
    default:
      break;
  }
}

// =====================================================
// Public integration functions
// =====================================================

void am3ConfigureNetworkTime() {
  // The display has no direct MQTT/internet role. Time is supplied by hardware.
}

void initAM3UI() {
  am3InitKeypad();

  pinMode(AM3_TFT_CS, OUTPUT);
  pinMode(AM3_TOUCH_CS, OUTPUT);
  digitalWrite(AM3_TFT_CS, HIGH);
  digitalWrite(AM3_TOUCH_CS, HIGH);

  SPI.begin(AM3_TFT_SCLK, AM3_TFT_MISO, AM3_TFT_MOSI);
  am3Tft.begin(AM3_TFT_SPI_FREQUENCY);
  am3Tft.setRotation(AM3_DISPLAY_ROTATION);
  am3Tft.setTextWrap(false);

  am3Touch.begin(SPI);
  am3Touch.setRotation(AM3_DISPLAY_ROTATION);

  am3ConfigureNetworkTime();

  am3DrawSplashScreen();
  am3DrawMainPage();
  am3LastWiFiConnected = (am3DisplayLinkConnected);
  am3LastUiRefresh = millis() - AM3_UI_REFRESH_INTERVAL;
  am3UiInitialized = true;
}

void updateAM3UI() {
  if (!am3UiInitialized) {
    return;
  }

  am3ProcessTouchNavigation();
  am3ProcessKeypadNavigation();

  const bool wifiConnected = (am3DisplayLinkConnected);
  if (wifiConnected != am3LastWiFiConnected) {
    am3LastWiFiConnected = wifiConnected;
    if (wifiConnected) {
      am3ConfigureNetworkTime();
    }
    am3DrawHeader();
  }

  const unsigned long now = millis();
  if (now - am3LastUiRefresh >= AM3_UI_REFRESH_INTERVAL) {
    am3LastUiRefresh = now;
    am3RefreshCurrentPageValues();
  }
}