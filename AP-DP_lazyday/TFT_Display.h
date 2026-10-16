#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <math.h>

// =====================================================
// 1.8" ST7735 SPI TFT (128x160), landscape 160x128
// Keep these pins exactly as used by the latest working project.
// LCD labels: LED, SCK, SDA(MOSI), A0(DC), RST, CS, GND, VCC
// =====================================================
#define TFT_SCK    14
#define TFT_SDA    23   // LCD SDA = SPI MOSI (not I2C SDA)
#define TFT_A0     16   // LCD A0 = DC / Data-Command
#define TFT_RST    17
#define TFT_CS     5
#define TFT_MISO   -1   // Not connected / not required

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_A0, TFT_SDA, TFT_SCK, TFT_RST);

// =====================================================
// Industrial LIGHT theme (RGB565)
// Clean white cards, cool grey background, navy typography,
// blue technical accents and restrained state colours.
// =====================================================
#define TFT_BG             0xF7BF  // #F3F6FA
#define TFT_CARD           0xFFFF  // white
#define TFT_HEADER         0x09AA  // deep navy
#define TFT_BORDER         0xD6FD  // cool grey
#define TFT_DIVIDER        0xE77E  // very light grey
#define TFT_ACCENT         0x1B75  // technical blue
#define TFT_TEXT           0x09AA  // deep navy
#define TFT_MUTED          0x6BD1  // muted blue-grey
#define TFT_GREEN          0x1CEE
#define TFT_RED            0xD1E7
#define TFT_ORANGE         0xDC24
#define TFT_BLUE           0x33D8
#define TFT_TRACK          0xDF1D
#define TFT_PALE_GREEN     0xEFBD
#define TFT_PALE_RED       0xFF7D
#define TFT_PALE_BLUE      0xEF9F
#define TFT_PALE_ORANGE    0xFF9C

// Display-only temperatures received from the thermostat/AP-TH.
float displaySupplyTemp = 0.0f;
float displayReturnTemp = 0.0f;
bool displaySupplyTempValid = false;
bool displayReturnTempValid = false;
unsigned long lastTemperatureUpdateMs = 0;

// Cached values prevent unnecessary redraw and visible SPI flicker.
int8_t lastDisplayPower = -1;
int8_t lastDisplaySeason = -1;
int16_t lastDisplayCfm = -1;
int16_t lastDisplaySetpoint = -32768;
float lastDisplaySupply = -1000.0f;
float lastDisplayReturn = -1000.0f;
bool lastDisplaySupplyValid = false;
bool lastDisplayReturnValid = false;

inline void tftCenteredText(const char *text, int16_t x, int16_t y,
                            int16_t w, uint8_t textSize, uint16_t color,
                            uint16_t background = TFT_CARD) {
  int16_t x1, y1;
  uint16_t tw, th;
  tft.setTextSize(textSize);
  tft.setTextColor(color, background);
  tft.getTextBounds(text, 0, 0, &x1, &y1, &tw, &th);

  int16_t tx = x + (w - static_cast<int16_t>(tw)) / 2;
  if (tx < x + 1) tx = x + 1;
  tft.setCursor(tx, y);
  tft.print(text);
}

inline void drawCard(int16_t x, int16_t y, int16_t w, int16_t h,
                     const __FlashStringHelper *label) {
  // Small lower shadow gives separation without making the UI heavy.
  tft.fillRoundRect(x + 1, y + 2, w, h, 4, TFT_DIVIDER);
  tft.fillRoundRect(x, y, w, h, 4, TFT_CARD);
  tft.drawRoundRect(x, y, w, h, 4, TFT_BORDER);

  // Thin blue technical marker.
  tft.fillRoundRect(x + 5, y + 4, 3, 8, 1, TFT_ACCENT);

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(TFT_MUTED, TFT_CARD);
  tft.setCursor(x + 12, y + 5);
  tft.print(label);
}

inline void drawStatusCard(int16_t x, int16_t y, int16_t w,
                           const __FlashStringHelper *label) {
  tft.fillRoundRect(x + 1, y + 2, w, 22, 4, TFT_DIVIDER);
  tft.fillRoundRect(x, y, w, 22, 4, TFT_CARD);
  tft.drawRoundRect(x, y, w, 22, 4, TFT_BORDER);
  tft.fillRoundRect(x + 5, y + 4, 3, 7, 1, TFT_ACCENT);

  tft.setTextSize(1);
  tft.setTextColor(TFT_MUTED, TFT_CARD);
  tft.setCursor(x + 12, y + 3);
  tft.print(label);
}

inline void initTFTDisplay() {
  // This constructor uses explicit MOSI/SCK pins. Keep the SPI setup from the
  // latest project so the display wiring remains unchanged.
  SPI.begin(TFT_SCK, TFT_MISO, TFT_SDA, TFT_CS);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);  // 160 x 128 landscape
  tft.setTextWrap(false);
  tft.fillScreen(TFT_BG);
}

inline void drawDashboard() {
  tft.fillScreen(TFT_BG);

  // -----------------------------------------------------
  // Header: compact industrial identity bar
  // -----------------------------------------------------
  tft.fillRect(0, 0, 160, 18, TFT_HEADER);
  tft.fillRect(0, 17, 160, 1, TFT_ACCENT);

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, TFT_HEADER);
  tft.setCursor(6, 2);
  tft.print(F("AP-DP"));

  tft.setTextSize(1);
  tft.setTextColor(0xC6DD, TFT_HEADER);
  tft.setCursor(103, 6);
  tft.print(F("DAMPER"));

  // -----------------------------------------------------
  // Status row: Power + Season
  // -----------------------------------------------------
  drawStatusCard(3, 21, 75, F("POWER"));
  drawStatusCard(82, 21, 75, F("SEASON"));

  // -----------------------------------------------------
  // Main readings: Supply + Return temperature
  // -----------------------------------------------------
  drawCard(3, 46, 75, 40, F("SUPPLY"));
  drawCard(82, 46, 75, 40, F("RETURN"));

  // -----------------------------------------------------
  // Bottom row: Setpoint + CFM
  // -----------------------------------------------------
  drawCard(3, 89, 75, 36, F("SETPOINT"));
  drawCard(82, 89, 75, 36, F("CFM"));

  // CFM progress track. Fill amount is updated dynamically.
  tft.fillRoundRect(88, 118, 63, 3, 1, TFT_TRACK);

  // Force every dynamic field to repaint over the static layout.
  lastDisplayPower = -1;
  lastDisplaySeason = -1;
  lastDisplayCfm = -1;
  lastDisplaySetpoint = -32768;
  lastDisplaySupply = -1000.0f;
  lastDisplayReturn = -1000.0f;
  lastDisplaySupplyValid = !displaySupplyTempValid;
  lastDisplayReturnValid = !displayReturnTempValid;
}

inline void refreshDashboard(bool force = false) {
  const int powerNow = beca_power ? 1 : 0;
  const int seasonNow = (beca_mode == 1) ? 1 : 0;  // 0=Summer, 1=Winter
  const int cfmNow = constrain(static_cast<int>(end_value), 0, 100);
  const int setpointNow = constrain(static_cast<int>(setpointt), 5, 35);

  // -----------------------------------------------------
  // POWER status pill
  // -----------------------------------------------------
  if (force || powerNow != lastDisplayPower) {
    const uint16_t stateColor = powerNow ? TFT_GREEN : TFT_RED;
    const uint16_t stateBg = powerNow ? TFT_PALE_GREEN : TFT_PALE_RED;

    tft.fillRect(6, 31, 69, 10, TFT_CARD);
    tft.fillRoundRect(27, 31, 27, 10, 5, stateBg);
    tftCenteredText(powerNow ? "ON" : "OFF", 27, 32, 27, 1, stateColor, stateBg);
    lastDisplayPower = powerNow;
  }

  // -----------------------------------------------------
  // SEASON status pill
  // -----------------------------------------------------
  if (force || seasonNow != lastDisplaySeason) {
    const uint16_t stateColor = seasonNow ? TFT_BLUE : TFT_ORANGE;
    const uint16_t stateBg = seasonNow ? TFT_PALE_BLUE : TFT_PALE_ORANGE;

    tft.fillRect(85, 31, 69, 10, TFT_CARD);
    // Wider pill because SUMMER/WINTER are six characters.
    tft.fillRoundRect(96, 31, 48, 10, 5, stateBg);
    tftCenteredText(seasonNow ? "WINTER" : "SUMMER", 96, 32, 48, 1, stateColor, stateBg);
    lastDisplaySeason = seasonNow;
  }

  // -----------------------------------------------------
  // SUPPLY temperature
  // -----------------------------------------------------
  if (force || displaySupplyTempValid != lastDisplaySupplyValid ||
      (displaySupplyTempValid && fabsf(displaySupplyTemp - lastDisplaySupply) >= 0.05f)) {
    tft.fillRect(7, 63, 67, 18, TFT_CARD);

    char value[12];
    if (displaySupplyTempValid) {
      snprintf(value, sizeof(value), "%.1fC", displaySupplyTemp);
    } else {
      snprintf(value, sizeof(value), "--.-C");
    }

    tftCenteredText(value, 5, 65, 71, 2,
                    displaySupplyTempValid ? TFT_TEXT : TFT_MUTED);
    lastDisplaySupply = displaySupplyTemp;
    lastDisplaySupplyValid = displaySupplyTempValid;
  }

  // -----------------------------------------------------
  // RETURN temperature
  // -----------------------------------------------------
  if (force || displayReturnTempValid != lastDisplayReturnValid ||
      (displayReturnTempValid && fabsf(displayReturnTemp - lastDisplayReturn) >= 0.05f)) {
    tft.fillRect(86, 63, 67, 18, TFT_CARD);

    char value[12];
    if (displayReturnTempValid) {
      snprintf(value, sizeof(value), "%.1fC", displayReturnTemp);
    } else {
      snprintf(value, sizeof(value), "--.-C");
    }

    tftCenteredText(value, 84, 65, 71, 2,
                    displayReturnTempValid ? TFT_TEXT : TFT_MUTED);
    lastDisplayReturn = displayReturnTemp;
    lastDisplayReturnValid = displayReturnTempValid;
  }

  // -----------------------------------------------------
  // SETPOINT - now included as a live dashboard value
  // -----------------------------------------------------
  if (force || setpointNow != lastDisplaySetpoint) {
    tft.fillRect(7, 105, 67, 16, TFT_CARD);

    char value[12];
    snprintf(value, sizeof(value), "%.1fC", static_cast<float>(setpointNow));
    tftCenteredText(value, 5, 106, 71, 2, TFT_ACCENT);

    lastDisplaySetpoint = setpointNow;
  }

  // -----------------------------------------------------
  // CFM percentage + miniature airflow bar
  // -----------------------------------------------------
  if (force || cfmNow != lastDisplayCfm) {
    tft.fillRect(86, 104, 67, 13, TFT_CARD);

    char value[7];
    snprintf(value, sizeof(value), "%d%%", cfmNow);
    tftCenteredText(value, 84, 104, 71, 2, TFT_GREEN);

    tft.fillRoundRect(88, 118, 63, 3, 1, TFT_TRACK);
    const int16_t fillWidth = map(cfmNow, 0, 100, 0, 63);
    if (fillWidth > 0) {
      tft.fillRoundRect(88, 118, fillWidth, 3, 1, TFT_GREEN);
    }

    lastDisplayCfm = cfmNow;
  }
}

inline void invalidateTemperatureDisplay() {
  if (displaySupplyTempValid || displayReturnTempValid) {
    displaySupplyTempValid = false;
    displayReturnTempValid = false;
    refreshDashboard();
  }
}

#endif
