#pragma once

/*
  AM4 2.8-inch TFT UI integration

  This file contains only the display/touch presentation layer taken from
  the sample AM4_Touch_Pressure_Temperature_UI project.

  It does NOT create another OneWire bus, read separate sensors, connect to
  Wi-Fi, calculate pressures, control outputs, scan alarms, or change MQTT.
  It only displays the existing AM4 variables maintained by the base code.

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
#include <Fonts/FreeSansBold9pt7b.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include "AM4_UI_Types.h"
#include "DisplayState.h"

// =====================================================
// TFT and touch pins
// =====================================================

#define AM4_TFT_MISO 19
#define AM4_TFT_MOSI 23
#define AM4_TFT_SCLK 18
#define AM4_TFT_CS   15
#define AM4_TFT_DC    2
#define AM4_TFT_RST   4
#define AM4_TOUCH_CS 21

constexpr uint32_t AM4_TFT_SPI_FREQUENCY = 27000000UL;
constexpr uint8_t AM4_DISPLAY_ROTATION = 1;
constexpr int16_t AM4_SCREEN_WIDTH = 320;
constexpr int16_t AM4_SCREEN_HEIGHT = 240;

Adafruit_ILI9341 am4Tft(AM4_TFT_CS, AM4_TFT_DC, AM4_TFT_RST);
XPT2046_Touchscreen am4Touch(AM4_TOUCH_CS);

// =====================================================
// Touch calibration
// =====================================================

#define AM4_TOUCH_DEBUG 0

constexpr int16_t AM4_TOUCH_RAW_X_MIN = 220;
constexpr int16_t AM4_TOUCH_RAW_X_MAX = 3900;
constexpr int16_t AM4_TOUCH_RAW_Y_MIN = 220;
constexpr int16_t AM4_TOUCH_RAW_Y_MAX = 3900;
constexpr bool AM4_TOUCH_INVERT_X = false;
constexpr bool AM4_TOUCH_INVERT_Y = false;
constexpr int16_t AM4_TOUCH_MIN_PRESSURE = 300;
constexpr int16_t AM4_SWIPE_MIN_DISTANCE = 55;
constexpr unsigned long AM4_TOUCH_DEBOUNCE_MS = 180;

// =====================================================
// 1x4 membrane keypad
// =====================================================

constexpr byte ROWS = 1;
constexpr byte COLS = 4;
byte rowPins[ROWS] = {27};
byte colPins[COLS] = {25, 26, 32, 33};
const char am4KeypadKeys[COLS] = {'1', '2', '3', '4'};

constexpr unsigned long AM4_KEYPAD_DEBOUNCE_MS = 35;
char am4KeypadLastRawKey = '\0';
char am4KeypadStableKey = '\0';
unsigned long am4KeypadLastChange = 0;
uint8_t am4SelectedTemperatureIndex = 0;

// =====================================================
// Colours
// =====================================================

constexpr uint16_t am4Rgb565(uint8_t red, uint8_t green, uint8_t blue) {
  return static_cast<uint16_t>(
    ((red & 0xF8) << 8) |
    ((green & 0xFC) << 3) |
    (blue >> 3)
  );
}

constexpr uint16_t AM4_COLOR_BACKGROUND = am4Rgb565(7, 22, 38);
constexpr uint16_t AM4_COLOR_HEADER     = am4Rgb565(13, 42, 70);
constexpr uint16_t AM4_COLOR_CARD       = am4Rgb565(246, 249, 252);
constexpr uint16_t AM4_COLOR_SHADOW     = am4Rgb565(2, 10, 18);
constexpr uint16_t AM4_COLOR_TEXT       = am4Rgb565(17, 35, 53);
constexpr uint16_t AM4_COLOR_MUTED      = am4Rgb565(91, 111, 128);
constexpr uint16_t AM4_COLOR_DIVIDER    = am4Rgb565(204, 216, 226);
constexpr uint16_t AM4_COLOR_BLUE       = am4Rgb565(36, 105, 221);
constexpr uint16_t AM4_COLOR_GREEN      = am4Rgb565(24, 154, 91);
constexpr uint16_t AM4_COLOR_ORANGE     = am4Rgb565(239, 132, 36);
constexpr uint16_t AM4_COLOR_RED        = am4Rgb565(218, 62, 68);
constexpr uint16_t AM4_COLOR_PURPLE     = am4Rgb565(130, 76, 190);
constexpr uint16_t AM4_COLOR_CYAN       = am4Rgb565(33, 145, 187);
constexpr uint16_t AM4_COLOR_YELLOW     = am4Rgb565(246, 190, 55);
constexpr uint16_t AM4_COLOR_SUCCESS    = am4Rgb565(27, 164, 98);
constexpr uint16_t AM4_COLOR_ERROR      = am4Rgb565(221, 64, 70);

// =====================================================
// UI state
// =====================================================

constexpr long AM4_GMT_OFFSET_SECONDS = 5L * 60L * 60L;
constexpr int AM4_DAYLIGHT_OFFSET_SECONDS = 0;
constexpr unsigned long AM4_UI_REFRESH_INTERVAL = 1000;

AM4ScreenPage am4CurrentPage = AM4_PAGE_MAIN;
unsigned long am4LastUiRefresh = 0;
bool am4LastWiFiConnected = false;
bool am4UiInitialized = false;

bool am4TouchInProgress = false;
int16_t am4TouchStartX = 0;
int16_t am4TouchStartY = 0;
int16_t am4TouchLastX = 0;
int16_t am4TouchLastY = 0;
unsigned long am4LastTouchAction = 0;

AM4DashboardCard am4MainCards[4] = {
  {8,   53, 148, 80, "TIME / DATE",         AM4_COLOR_BLUE,   AM4_ICON_CLOCK},
  {164, 53, 148, 80, "SYSTEM STATUS",       AM4_COLOR_GREEN,  AM4_ICON_SYSTEM_STATUS},
  {8,  141, 148, 84, "SUCTION PRESSURE",   AM4_COLOR_ORANGE, AM4_ICON_SUCTION_PRESSURE},
  {164,141, 148, 84, "DISCHARGE PRESSURE", AM4_COLOR_RED,    AM4_ICON_DISCHARGE_PRESSURE}
};

constexpr uint8_t AM4_TEMPERATURE_COUNT = 6;
const char* AM4_TEMPERATURE_NAMES[AM4_TEMPERATURE_COUNT] = {
  "RETURN TEMP",
  "SUCTION TEMP",
  "DSC TEMP",
  "SUPPLY TEMP",
  "OIL TEMP",
  "OTHER SENSOR"
};

const uint16_t AM4_TEMPERATURE_COLORS[AM4_TEMPERATURE_COUNT] = {
  AM4_COLOR_BLUE,
  AM4_COLOR_GREEN,
  AM4_COLOR_RED,
  AM4_COLOR_CYAN,
  AM4_COLOR_PURPLE,
  AM4_COLOR_YELLOW
};

// =====================================================
// Text helpers
// =====================================================

void am4DrawCenteredText(
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

  am4Tft.setTextSize(textSize);
  am4Tft.setTextColor(textColor, backgroundColor);
  am4Tft.getTextBounds(text, 0, 0, &x1, &y1, &textWidth, &textHeight);
  am4Tft.setCursor(centerX - static_cast<int16_t>(textWidth / 2), y);
  am4Tft.print(text);
}

void am4DrawLeftText(
  const String& text,
  int16_t x,
  int16_t y,
  uint8_t textSize,
  uint16_t textColor,
  uint16_t backgroundColor
) {
  am4Tft.setTextSize(textSize);
  am4Tft.setTextColor(textColor, backgroundColor);
  am4Tft.setCursor(x, y);
  am4Tft.print(text);
}

// Draw temperature names with a genuine bold GFX font.
// Custom GFX fonts render transparently, so the row frame is drawn first and
// already provides the clean card background. The default font is restored
// immediately so other dashboard text keeps its original size and alignment.
void am4DrawBoldTemperatureLabel(
  const String& text,
  int16_t x,
  int16_t baselineY,
  uint16_t textColor
) {
  am4Tft.setFont(&FreeSansBold9pt7b);
  am4Tft.setTextSize(1);
  am4Tft.setTextColor(textColor);
  am4Tft.setCursor(x, baselineY);
  am4Tft.print(text);
  am4Tft.setFont(nullptr);
  am4Tft.setTextSize(1);
}

// =====================================================
// Icons
// =====================================================

void am4DrawClockIcon(int16_t cx, int16_t cy, uint16_t color) {
  am4Tft.drawCircle(cx, cy, 6, color);
  am4Tft.drawLine(cx, cy, cx, cy - 4, color);
  am4Tft.drawLine(cx, cy, cx + 3, cy + 2, color);
}

void am4DrawCalendarIcon(int16_t cx, int16_t cy, uint16_t color) {
  am4Tft.drawRect(cx - 6, cy - 5, 12, 11, color);
  am4Tft.drawFastHLine(cx - 5, cy - 1, 10, color);
  am4Tft.drawFastVLine(cx - 3, cy - 7, 4, color);
  am4Tft.drawFastVLine(cx + 3, cy - 7, 4, color);
}

void am4DrawPressureGaugeIcon(int16_t cx, int16_t cy, uint16_t color, bool highSide) {
  am4Tft.drawCircle(cx, cy + 1, 7, color);
  am4Tft.drawFastHLine(cx - 5, cy + 6, 10, color);
  am4Tft.drawLine(cx, cy + 1, highSide ? cx + 4 : cx - 3, highSide ? cy - 3 : cy - 2, color);
  am4Tft.fillCircle(cx, cy + 1, 1, color);
}

void am4DrawThermometerIcon(int16_t cx, int16_t cy, uint16_t color) {
  am4Tft.drawRoundRect(cx - 2, cy - 7, 5, 11, 2, color);
  am4Tft.drawFastVLine(cx, cy - 4, 8, color);
  am4Tft.fillCircle(cx, cy + 5, 4, color);
}

// =====================================================
// Header and footer
// =====================================================

void am4DrawHeader() {
  am4Tft.fillRect(0, 0, AM4_SCREEN_WIDTH, 50, AM4_COLOR_BACKGROUND);
  am4Tft.fillRoundRect(6, 5, 308, 39, 9, AM4_COLOR_HEADER);

  // am4Tft.fillCircle(25, 24, 13, AM4_COLOR_BLUE);
  // am4Tft.drawCircle(25, 24, 10, ILI9341_WHITE);
  // am4DrawCenteredText("A", 25, 16, 2, ILI9341_WHITE, AM4_COLOR_BLUE);
  am4DrawLeftText("AM4", 20, 15, 3, ILI9341_WHITE, AM4_COLOR_HEADER);

  const char* pageTitle =
    (am4CurrentPage == AM4_PAGE_MAIN) ? "DASHBOARD" : "TEMPERATURES";

  am4DrawCenteredText(pageTitle, 172, 18, 2, AM4_COLOR_YELLOW, AM4_COLOR_HEADER);

  am4Tft.fillRect(8, 46, 304, 2, AM4_COLOR_BLUE);
  am4Tft.fillRect(8, 48, 304, 1, AM4_COLOR_GREEN);
}

void am4DrawNavigationFooter() {
  am4Tft.fillRect(0, 228, AM4_SCREEN_WIDTH, 12, AM4_COLOR_BACKGROUND);

  const uint16_t page0Color =
    (am4CurrentPage == AM4_PAGE_MAIN) ? AM4_COLOR_BLUE : AM4_COLOR_MUTED;
  const uint16_t page1Color =
    (am4CurrentPage == AM4_PAGE_TEMPERATURES) ? AM4_COLOR_GREEN : AM4_COLOR_MUTED;

  am4Tft.fillCircle(153, 234, 3, page0Color);
  am4Tft.fillCircle(167, 234, 3, page1Color);

  // A right swipe always toggles between Dashboard and Temperatures.
  am4DrawLeftText("KEY 4 / SWIPE RIGHT", 190, 231, 1, AM4_COLOR_MUTED, AM4_COLOR_BACKGROUND);
  am4Tft.drawLine(306, 231, 311, 234, AM4_COLOR_GREEN);
  am4Tft.drawLine(311, 234, 306, 237, AM4_COLOR_GREEN);
}

// =====================================================
// Splash
// =====================================================

void am4DrawSplashScreen() {
  am4Tft.fillScreen(AM4_COLOR_BACKGROUND);
  am4Tft.fillRoundRect(32, 29, 256, 176, 16, AM4_COLOR_HEADER);
  am4Tft.drawRoundRect(32, 29, 256, 176, 16, AM4_COLOR_BLUE);
  am4Tft.drawRoundRect(35, 32, 250, 170, 14, AM4_COLOR_GREEN);

  am4Tft.fillCircle(160, 89, 36, AM4_COLOR_BLUE);
  am4Tft.drawCircle(160, 89, 30, ILI9341_WHITE);
  am4Tft.drawCircle(160, 89, 24, AM4_COLOR_HEADER);
  am4DrawCenteredText("A", 160, 70, 5, ILI9341_WHITE, AM4_COLOR_BLUE);
  am4DrawCenteredText("AM4", 160, 132, 4, ILI9341_WHITE, AM4_COLOR_HEADER);
  am4DrawCenteredText("INITIALIZING", 160, 166, 1, AM4_COLOR_YELLOW, AM4_COLOR_HEADER);

  constexpr int16_t barX = 65;
  constexpr int16_t barY = 188;
  constexpr int16_t barW = 190;
  constexpr int16_t barH = 8;
  am4Tft.drawRoundRect(barX, barY, barW, barH, 4, AM4_COLOR_MUTED);

  for (int16_t progress = 0; progress <= barW - 4; progress += 10) {
    am4Tft.fillRoundRect(barX + 2, barY + 2, progress, barH - 4, 2, AM4_COLOR_GREEN);
    delay(20);
  }
}

// =====================================================
// Main cards
// =====================================================

void am4DrawMainCardIcon(const AM4DashboardCard& card) {
  const int16_t cx = card.x + 15;
  const int16_t cy = card.y + 13;

  switch (card.icon) {
    case AM4_ICON_CLOCK:
      am4DrawClockIcon(cx, cy, ILI9341_WHITE);
      break;
    case AM4_ICON_CALENDAR:
      am4DrawCalendarIcon(cx, cy, ILI9341_WHITE);
      break;
    case AM4_ICON_SYSTEM_STATUS:
      am4Tft.drawCircle(cx, cy, 7, ILI9341_WHITE);
      am4Tft.drawFastVLine(cx, cy - 8, 7, ILI9341_WHITE);
      break;
    case AM4_ICON_SUCTION_PRESSURE:
      am4DrawPressureGaugeIcon(cx, cy, ILI9341_WHITE, false);
      break;
    case AM4_ICON_DISCHARGE_PRESSURE:
      am4DrawPressureGaugeIcon(cx, cy, ILI9341_WHITE, true);
      break;
  }
}

void am4DrawDashboardCard(const AM4DashboardCard& card) {
  am4Tft.fillRoundRect(card.x + 2, card.y + 3, card.width, card.height, 8, AM4_COLOR_SHADOW);
  am4Tft.fillRoundRect(card.x, card.y, card.width, card.height, 8, AM4_COLOR_CARD);
  am4Tft.fillRoundRect(card.x, card.y, card.width, 27, 8, card.accentColor);
  am4Tft.fillRect(card.x, card.y + 14, card.width, 13, card.accentColor);
  am4Tft.drawRoundRect(card.x, card.y, card.width, card.height, 8, card.accentColor);

  am4DrawMainCardIcon(card);
  am4DrawLeftText(card.title, card.x + 27, card.y + 9, 1, ILI9341_WHITE, card.accentColor);
  am4Tft.fillCircle(card.x + card.width - 13, card.y + 13, 3, ILI9341_WHITE);
}

void am4ClearMainCardValueArea(const AM4DashboardCard& card) {
  am4Tft.fillRect(card.x + 3, card.y + 30, card.width - 6, card.height - 34, AM4_COLOR_CARD);
}

void am4DrawWaitingValue(
  const AM4DashboardCard& card,
  const String& mainText,
  const String& smallText
) {
  am4ClearMainCardValueArea(card);
  am4DrawCenteredText(mainText, card.x + card.width / 2, card.y + 43, 2, card.accentColor, AM4_COLOR_CARD);
  am4DrawCenteredText(smallText, card.x + card.width / 2, card.y + card.height - 13, 1, AM4_COLOR_MUTED, AM4_COLOR_CARD);
}

// =====================================================
// Temperature rows
// =====================================================

// Six compact cards fit between the 50 px header and 228 px footer.
// The label and live-value zones are deliberately separated for readability.
constexpr int16_t AM4_TEMP_ROW_X = 8;
constexpr int16_t AM4_TEMP_ROW_W = 304;
constexpr int16_t AM4_TEMP_ROW_H = 26;
constexpr int16_t AM4_TEMP_ROW_FIRST_Y = 52;
constexpr int16_t AM4_TEMP_ROW_GAP = 2;
constexpr int16_t AM4_TEMP_ICON_W = 42;
constexpr int16_t AM4_TEMP_DIVIDER_X = AM4_TEMP_ROW_X + 205;
constexpr int16_t AM4_TEMP_VALUE_X = AM4_TEMP_DIVIDER_X + 5;
constexpr int16_t AM4_TEMP_VALUE_W = (AM4_TEMP_ROW_X + AM4_TEMP_ROW_W - 4) - AM4_TEMP_VALUE_X;
constexpr int16_t AM4_TEMP_VALUE_CENTER_X = AM4_TEMP_VALUE_X + (AM4_TEMP_VALUE_W / 2);

int16_t am4GetTemperatureRowY(uint8_t index) {
  return AM4_TEMP_ROW_FIRST_Y + index * (AM4_TEMP_ROW_H + AM4_TEMP_ROW_GAP);
}

void am4DrawTemperatureRowFrame(uint8_t index) {
  if (index >= AM4_TEMPERATURE_COUNT) {
    return;
  }

  const int16_t rowY = am4GetTemperatureRowY(index);
  const uint16_t accent = AM4_TEMPERATURE_COLORS[index];

  // Card and shadow
  am4Tft.fillRoundRect(AM4_TEMP_ROW_X + 2, rowY + 2,
                       AM4_TEMP_ROW_W, AM4_TEMP_ROW_H,
                       7, AM4_COLOR_SHADOW);
  am4Tft.fillRoundRect(AM4_TEMP_ROW_X, rowY,
                       AM4_TEMP_ROW_W, AM4_TEMP_ROW_H,
                       7, AM4_COLOR_CARD);
  am4Tft.drawRoundRect(AM4_TEMP_ROW_X, rowY,
                       AM4_TEMP_ROW_W, AM4_TEMP_ROW_H,
                       7, accent);

  // Coloured icon panel
  am4Tft.fillRoundRect(AM4_TEMP_ROW_X, rowY,
                       AM4_TEMP_ICON_W, AM4_TEMP_ROW_H,
                       7, accent);
  am4Tft.fillRect(AM4_TEMP_ROW_X + 21, rowY,
                  AM4_TEMP_ICON_W - 21, AM4_TEMP_ROW_H,
                  accent);
  am4DrawThermometerIcon(AM4_TEMP_ROW_X + 20, rowY + 13, ILI9341_WHITE);

  // Temperature name
  am4DrawBoldTemperatureLabel(AM4_TEMPERATURE_NAMES[index], AM4_TEMP_ROW_X + 48, rowY + 20, AM4_COLOR_TEXT);

  // Clear visual division between the name and live reading
  am4Tft.drawFastVLine(AM4_TEMP_DIVIDER_X,rowY + 5,AM4_TEMP_ROW_H - 10,AM4_COLOR_DIVIDER);
  am4Tft.drawFastVLine(AM4_TEMP_DIVIDER_X + 1,rowY + 8,AM4_TEMP_ROW_H - 16,accent);
}

void am4ClearTemperatureRowValueArea(uint8_t index) {
  if (index >= AM4_TEMPERATURE_COUNT) {
    return;
  }

  const int16_t rowY = am4GetTemperatureRowY(index);
  am4Tft.fillRect(AM4_TEMP_VALUE_X,
                  rowY + 3,
                  AM4_TEMP_VALUE_W,
                  AM4_TEMP_ROW_H - 6,
                  AM4_COLOR_CARD);
}

float am4GetTemperatureC(uint8_t index) {
  switch (index) {
    case 0: return ReturnTempC;
    case 1: return SuctionTempC;
    case 2: return dischargeTempC;
    case 3: return SupplyTempC;
    case 4: return OilTempC;
    case 5: return OtherTempC;
    default: return NAN;
  }
}

bool am4IsValidTemperature(float value) {
  return isfinite(value) && value > -100.0f && value < SENSOR_DISCONNECTED;
}

void am4UpdateTemperatureRow(uint8_t index) {
  if (index >= AM4_TEMPERATURE_COUNT) {
    return;
  }

  am4ClearTemperatureRowValueArea(index);
  const int16_t rowY = am4GetTemperatureRowY(index);
  const float value = am4GetTemperatureC(index);

  if (!am4IsValidTemperature(value)) {
    const char* message = (value >= SENSOR_NOT_SELECTED) ? "NOT SET" : "NO SENSOR";
    am4DrawCenteredText(message,
                        AM4_TEMP_VALUE_CENTER_X,
                        rowY + 10,
                        1,
                        AM4_COLOR_ERROR,
                        AM4_COLOR_CARD);
    return;
  }

  const String valueText = String(value, 1) + " C";
  am4DrawCenteredText(valueText,
                      AM4_TEMP_VALUE_CENTER_X,
                      rowY + 5,
                      2,
                      AM4_TEMPERATURE_COLORS[index],
                      AM4_COLOR_CARD);
}

void am4DrawTemperatureSelection(uint8_t index, bool selected) {
  if (index >= AM4_TEMPERATURE_COUNT) {
    return;
  }

  const int16_t rowY = am4GetTemperatureRowY(index);

  if (selected) {
    // Two-pixel highlight is easier to see than the previous single border.
    am4Tft.drawRoundRect(AM4_TEMP_ROW_X - 2,
                         rowY - 2,
                         AM4_TEMP_ROW_W + 4,
                         AM4_TEMP_ROW_H + 4,
                         8,
                         AM4_COLOR_YELLOW);
    am4Tft.drawRoundRect(AM4_TEMP_ROW_X - 1,
                         rowY - 1,
                         AM4_TEMP_ROW_W + 2,
                         AM4_TEMP_ROW_H + 2,
                         8,
                         AM4_COLOR_ORANGE);
  } else {
    // Remove the old highlight and redraw the row so its shadow is restored.
    am4Tft.drawRoundRect(AM4_TEMP_ROW_X - 2,
                         rowY - 2,
                         AM4_TEMP_ROW_W + 4,
                         AM4_TEMP_ROW_H + 4,
                         8,
                         AM4_COLOR_BACKGROUND);
    am4Tft.drawRoundRect(AM4_TEMP_ROW_X - 1,
                         rowY - 1,
                         AM4_TEMP_ROW_W + 2,
                         AM4_TEMP_ROW_H + 2,
                         8,
                         AM4_COLOR_BACKGROUND);
    am4DrawTemperatureRowFrame(index);
    am4UpdateTemperatureRow(index);
  }
}

void am4SelectTemperatureRow(uint8_t newIndex) {
  if (newIndex >= AM4_TEMPERATURE_COUNT || newIndex == am4SelectedTemperatureIndex) {
    return;
  }

  am4DrawTemperatureSelection(am4SelectedTemperatureIndex, false);
  am4SelectedTemperatureIndex = newIndex;
  am4DrawTemperatureSelection(am4SelectedTemperatureIndex, true);
}

// =====================================================
// Page drawing
// =====================================================

void am4DrawMainPage() {
  am4CurrentPage = AM4_PAGE_MAIN;
  am4Tft.fillScreen(AM4_COLOR_BACKGROUND);
  am4DrawHeader();

  for (uint8_t i = 0; i < 4; i++) {
    am4DrawDashboardCard(am4MainCards[i]);
  }

  am4DrawWaitingValue(am4MainCards[0], "SYNCING", "TIME AND DATE");
  am4DrawWaitingValue(am4MainCards[1], "STOPPED", "COMPRESSOR STATUS");
  am4DrawWaitingValue(am4MainCards[2], "WAITING", "AM4 PRESSURE");
  am4DrawWaitingValue(am4MainCards[3], "WAITING", "AM4 PRESSURE");
  am4DrawNavigationFooter();
}

void am4DrawTemperaturesPage() {
  am4CurrentPage = AM4_PAGE_TEMPERATURES;
  am4Tft.fillScreen(AM4_COLOR_BACKGROUND);
  am4DrawHeader();

  for (uint8_t i = 0; i < AM4_TEMPERATURE_COUNT; i++) {
    am4DrawTemperatureRowFrame(i);
    am4UpdateTemperatureRow(i);
  }

  am4DrawTemperatureSelection(am4SelectedTemperatureIndex, true);
  am4DrawNavigationFooter();
}

// =====================================================
// Dynamic card values
// =====================================================

void am4UpdateTimeDateCard(const struct tm& timeInfo) {
  if (am4CurrentPage != AM4_PAGE_MAIN) {
    return;
  }

  am4ClearMainCardValueArea(am4MainCards[0]);

  char timeText[20];
  char dateText[16];
  char dayTextBuffer[16];
  strftime(timeText, sizeof(timeText), "%I:%M:%S %p", &timeInfo);
  strftime(dateText, sizeof(dateText), "%d-%m-%Y", &timeInfo);
  strftime(dayTextBuffer, sizeof(dayTextBuffer), "%a", &timeInfo);

  String dayText(dayTextBuffer);
  dayText.toUpperCase();

  am4DrawCenteredText(String(timeText), am4MainCards[0].x + am4MainCards[0].width / 2,
                      am4MainCards[0].y + 34, 2, AM4_COLOR_BLUE, AM4_COLOR_CARD);
  am4DrawCenteredText(String(dateText) + "  " + dayText,
                      am4MainCards[0].x + am4MainCards[0].width / 2,
                      am4MainCards[0].y + 59, 1, AM4_COLOR_TEXT, AM4_COLOR_CARD);
}

void am4UpdateSystemStatusCard() {
  if (am4CurrentPage != AM4_PAGE_MAIN) {
    return;
  }

  am4ClearMainCardValueArea(am4MainCards[1]);
  const bool running = (remoteCompStatus == 1);
  const char* statusText = running ? "RUNNING" : "STOPPED";
  const uint16_t statusColor = running ? AM4_COLOR_SUCCESS : AM4_COLOR_ERROR;

  am4DrawCenteredText(statusText,
                      am4MainCards[1].x + am4MainCards[1].width / 2,
                      am4MainCards[1].y + 40, 3, statusColor, AM4_COLOR_CARD);
  am4DrawCenteredText("COMPRESSOR",
                      am4MainCards[1].x + am4MainCards[1].width / 2,
                      am4MainCards[1].y + am4MainCards[1].height - 13,
                      1, AM4_COLOR_MUTED, AM4_COLOR_CARD);
}

void am4DisplayTimeError() {
  if (am4CurrentPage != AM4_PAGE_MAIN) {
    return;
  }

  am4ClearMainCardValueArea(am4MainCards[0]);
  am4DrawCenteredText("NO TIME", am4MainCards[0].x + am4MainCards[0].width / 2,
                      am4MainCards[0].y + 37, 2, AM4_COLOR_ERROR, AM4_COLOR_CARD);
  am4DrawCenteredText("WAITING FOR HARDWARE", am4MainCards[0].x + am4MainCards[0].width / 2,
                      am4MainCards[0].y + am4MainCards[0].height - 13,
                      1, AM4_COLOR_MUTED, AM4_COLOR_CARD);
}

float am4PressureToFloat(const String& pressureText) {
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

void am4UpdatePressureCard(
  const AM4DashboardCard& card,
  float pressurePsi,
  const char* statusText
) {
  if (am4CurrentPage != AM4_PAGE_MAIN) {
    return;
  }

  am4ClearMainCardValueArea(card);

  if (!isfinite(pressurePsi)) {
    am4DrawCenteredText("NO INPUT", card.x + card.width / 2, card.y + 46,
                        2, AM4_COLOR_ERROR, AM4_COLOR_CARD);
    am4DrawCenteredText("CHECK AM4 VALUE", card.x + card.width / 2,
                        card.y + card.height - 13, 1, AM4_COLOR_MUTED, AM4_COLOR_CARD);
    return;
  }

  const String valueText =
    (pressurePsi < 100.0f) ? String(pressurePsi, 1) + " PSI" : String(pressurePsi, 0) + " PSI";
  const uint8_t valueSize = (valueText.length() <= 8) ? 3 : 2;
  const int16_t valueY = (valueSize == 3) ? card.y + 42 : card.y + 47;

  am4DrawCenteredText(valueText, card.x + card.width / 2, valueY,
                      valueSize, card.accentColor, AM4_COLOR_CARD);
  am4DrawCenteredText(statusText, card.x + card.width / 2,
                      card.y + card.height - 13, 1, AM4_COLOR_MUTED, AM4_COLOR_CARD);
}

void am4RefreshCurrentPageValues() {
  if (am4CurrentPage == AM4_PAGE_MAIN) {
    struct tm timeInfo;
    if (am4GetRemotePakistanTime(timeInfo)) {
      am4UpdateTimeDateCard(timeInfo);
    } else {
      am4DisplayTimeError();
    }

    am4UpdateSystemStatusCard();
    am4UpdatePressureCard(am4MainCards[2], am4PressureToFloat(suctionPressure), "LOW SIDE | LIVE");
    am4UpdatePressureCard(am4MainCards[3], am4PressureToFloat(dischargePressure), "HIGH SIDE | LIVE");
  } else {
    for (uint8_t i = 0; i < AM4_TEMPERATURE_COUNT; i++) {
      am4UpdateTemperatureRow(i);
    }
  }
}

// =====================================================
// Touch navigation
// =====================================================

bool am4ReadTouchPoint(int16_t& screenX, int16_t& screenY) {
  if (!am4Touch.touched()) {
    return false;
  }

  const TS_Point point = am4Touch.getPoint();

#if AM4_TOUCH_DEBUG
  Serial.print("Touch raw X=");
  Serial.print(point.x);
  Serial.print(" Y=");
  Serial.print(point.y);
  Serial.print(" Z=");
  Serial.println(point.z);
#endif

  if (point.z < AM4_TOUCH_MIN_PRESSURE) {
    return false;
  }

  int32_t mappedX = map(point.x, AM4_TOUCH_RAW_X_MIN, AM4_TOUCH_RAW_X_MAX, 0, AM4_SCREEN_WIDTH - 1);
  int32_t mappedY = map(point.y, AM4_TOUCH_RAW_Y_MIN, AM4_TOUCH_RAW_Y_MAX, 0, AM4_SCREEN_HEIGHT - 1);
  mappedX = constrain(mappedX, static_cast<int32_t>(0), static_cast<int32_t>(AM4_SCREEN_WIDTH - 1));
  mappedY = constrain(mappedY, static_cast<int32_t>(0), static_cast<int32_t>(AM4_SCREEN_HEIGHT - 1));

  if (AM4_TOUCH_INVERT_X) {
    mappedX = (AM4_SCREEN_WIDTH - 1) - mappedX;
  }
  if (AM4_TOUCH_INVERT_Y) {
    mappedY = (AM4_SCREEN_HEIGHT - 1) - mappedY;
  }

  screenX = static_cast<int16_t>(mappedX);
  screenY = static_cast<int16_t>(mappedY);
  return true;
}

void am4SwitchToPage(AM4ScreenPage newPage) {
  if (newPage == am4CurrentPage) {
    return;
  }

  if (newPage == AM4_PAGE_MAIN) {
    am4DrawMainPage();
  } else {
    am4DrawTemperaturesPage();
  }
  am4RefreshCurrentPageValues();
}

void am4ProcessTouchNavigation() {
  const unsigned long now = millis();
  int16_t x = 0;
  int16_t y = 0;
  const bool pressed = am4ReadTouchPoint(x, y);

  if (pressed) {
    if (!am4TouchInProgress) {
      am4TouchInProgress = true;
      am4TouchStartX = x;
      am4TouchStartY = y;
      am4TouchLastX = x;
      am4TouchLastY = y;
    } else {
      am4TouchLastX = x;
      am4TouchLastY = y;
    }
    return;
  }

  if (!am4TouchInProgress) {
    return;
  }

  am4TouchInProgress = false;
  if (now - am4LastTouchAction < AM4_TOUCH_DEBOUNCE_MS) {
    return;
  }

  const int16_t deltaX = am4TouchLastX - am4TouchStartX;
  const int16_t deltaY = am4TouchLastY - am4TouchStartY;
  bool pageChanged = false;

  if (abs(deltaX) >= AM4_SWIPE_MIN_DISTANCE && abs(deltaX) > abs(deltaY)) {
    // A physical finger movement to the right toggles both pages:
    // Dashboard -> Temperatures -> Dashboard.
    if (deltaX > 0) {
      const AM4ScreenPage targetPage =
        (am4CurrentPage == AM4_PAGE_MAIN) ? AM4_PAGE_TEMPERATURES : AM4_PAGE_MAIN;
      am4SwitchToPage(targetPage);
      pageChanged = true;
    }
  }

  if (!pageChanged && am4TouchLastY >= 226 && am4TouchLastX >= 250) {
    const AM4ScreenPage targetPage =
      (am4CurrentPage == AM4_PAGE_MAIN) ? AM4_PAGE_TEMPERATURES : AM4_PAGE_MAIN;
    am4SwitchToPage(targetPage);
    pageChanged = true;
  }

  if (pageChanged) {
    am4LastTouchAction = now;
  }
}

// =====================================================
// Membrane keypad navigation
// =====================================================

void am4InitKeypad() {
  for (byte row = 0; row < ROWS; row++) {
    pinMode(rowPins[row], OUTPUT);
    digitalWrite(rowPins[row], HIGH);
  }

  for (byte col = 0; col < COLS; col++) {
    pinMode(colPins[col], INPUT_PULLUP);
  }
}

char am4ReadRawKeypadKey() {
  digitalWrite(rowPins[0], LOW);
  delayMicroseconds(3);

  char pressedKey = '\0';
  for (byte col = 0; col < COLS; col++) {
    if (digitalRead(colPins[col]) == LOW) {
      pressedKey = am4KeypadKeys[col];
      break;
    }
  }

  digitalWrite(rowPins[0], HIGH);
  return pressedKey;
}

char am4GetKeypadPress() {
  const char rawKey = am4ReadRawKeypadKey();
  const unsigned long now = millis();

  if (rawKey != am4KeypadLastRawKey) {
    am4KeypadLastRawKey = rawKey;
    am4KeypadLastChange = now;
  }

  if (now - am4KeypadLastChange < AM4_KEYPAD_DEBOUNCE_MS) {
    return '\0';
  }

  if (rawKey == am4KeypadStableKey) {
    return '\0';
  }

  am4KeypadStableKey = rawKey;
  return (am4KeypadStableKey != '\0') ? am4KeypadStableKey : '\0';
}

void am4NavigateLeft() {
  if (am4CurrentPage == AM4_PAGE_TEMPERATURES) {
    am4SwitchToPage(AM4_PAGE_MAIN);
  }
}

void am4NavigateRight() {
  const AM4ScreenPage targetPage =
    (am4CurrentPage == AM4_PAGE_MAIN) ? AM4_PAGE_TEMPERATURES : AM4_PAGE_MAIN;
  am4SwitchToPage(targetPage);
}

void am4NavigateUp() {
  if (am4CurrentPage != AM4_PAGE_TEMPERATURES) {
    return;
  }

  const uint8_t nextIndex =
    (am4SelectedTemperatureIndex == 0)
      ? static_cast<uint8_t>(AM4_TEMPERATURE_COUNT - 1)
      : static_cast<uint8_t>(am4SelectedTemperatureIndex - 1);
  am4SelectTemperatureRow(nextIndex);
}

void am4NavigateDown() {
  if (am4CurrentPage != AM4_PAGE_TEMPERATURES) {
    return;
  }

  const uint8_t nextIndex =
    static_cast<uint8_t>((am4SelectedTemperatureIndex + 1) % AM4_TEMPERATURE_COUNT);
  am4SelectTemperatureRow(nextIndex);
}

void am4ProcessKeypadNavigation() {
  const char key = am4GetKeypadPress();
  if (key == '\0') {
    return;
  }

  Serial.print("[KEYPAD] Key ");
  Serial.println(key);

  switch (key) {
    case '1':
      am4NavigateLeft();
      break;
    case '2':
      am4NavigateUp();
      break;
    case '3':
      am4NavigateDown();
      break;
    case '4':
      am4NavigateRight();
      break;
    default:
      break;
  }
}

// =====================================================
// Public integration functions
// =====================================================

void am4ConfigureNetworkTime() {
  // The display has no direct MQTT/internet role. Time is supplied by hardware.
}

void initAM4UI() {
  am4InitKeypad();

  pinMode(AM4_TFT_CS, OUTPUT);
  pinMode(AM4_TOUCH_CS, OUTPUT);
  digitalWrite(AM4_TFT_CS, HIGH);
  digitalWrite(AM4_TOUCH_CS, HIGH);

  SPI.begin(AM4_TFT_SCLK, AM4_TFT_MISO, AM4_TFT_MOSI);
  am4Tft.begin(AM4_TFT_SPI_FREQUENCY);
  am4Tft.setRotation(AM4_DISPLAY_ROTATION);
  am4Tft.setTextWrap(false);

  am4Touch.begin(SPI);
  am4Touch.setRotation(AM4_DISPLAY_ROTATION);

  am4ConfigureNetworkTime();

  am4DrawSplashScreen();
  am4DrawMainPage();
  am4LastWiFiConnected = (am4DisplayLinkConnected);
  am4LastUiRefresh = millis() - AM4_UI_REFRESH_INTERVAL;
  am4UiInitialized = true;
}

void updateAM4UI() {
  if (!am4UiInitialized) {
    return;
  }

  am4ProcessTouchNavigation();
  am4ProcessKeypadNavigation();

  const bool wifiConnected = (am4DisplayLinkConnected);
  if (wifiConnected != am4LastWiFiConnected) {
    am4LastWiFiConnected = wifiConnected;
    if (wifiConnected) {
      am4ConfigureNetworkTime();
    }
    am4DrawHeader();
  }

  const unsigned long now = millis();
  if (now - am4LastUiRefresh >= AM4_UI_REFRESH_INTERVAL) {
    am4LastUiRefresh = now;
    am4RefreshCurrentPageValues();
  }
}