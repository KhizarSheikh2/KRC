#include <Wire.h>
#include <Adafruit_SH110X.h>
#include <Fonts/Org_01.h>
#include <Keypad.h>

// Keypad Membrane Pins
#define ROW_PIN 27
#define COL_PIN_1 26
#define COL_PIN_2 25
#define COL_PIN_3 33
#define COL_PIN_4 32

const byte ROWS = 1;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  { '2', '1', '3', '4' }
};

byte rowPins[ROWS] = { ROW_PIN };
byte colPins[COLS] = { COL_PIN_1, COL_PIN_2, COL_PIN_3, COL_PIN_4 };

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

unsigned long lastRepeat = 0;
char heldKey = NO_KEY;
const int repeatRate = 250;

const int MIN_RTEMP = 0;
const int MIN_DTEMP = 0;
const int MAX_RTEMP = 100;
const int MAX_DTEMP = 200;

enum Mode {
  E_TEMPERATURE,  // DISCHARGE
  R_TEMPERATURE,  // RETURN
  S_TEMPERATURE,  // SUPPLY
  EVAP_COOl_STATUS,
  AUTO_WASH_STATUS,
  UNIT_COMM_STATUS,
  NUM_MODES
};

int8_t current_mode = E_TEMPERATURE;

#define I2C_SDA 21
#define I2C_SCL 22

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void drawBorder() {
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);
}

void updateDisplay() {
  display.clearDisplay();
  //drawBorder();

  // ===== HEADER =====
  display.setFont();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(2);

  String modeName;

  switch (current_mode) {
    case E_TEMPERATURE: modeName = "Discharge"; break;
    case R_TEMPERATURE: modeName = "Return Air"; break;
    case S_TEMPERATURE: modeName = "Supply Air"; break;
    case EVAP_COOl_STATUS: modeName = "Evap Cool"; break;
    case AUTO_WASH_STATUS: modeName = "Auto Wash"; break;
    case UNIT_COMM_STATUS: modeName = "Unit"; break;
  }
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(modeName.c_str(), 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, 4);
  display.print(modeName);
  display.drawFastHLine(2, 22, SCREEN_WIDTH - 4, SH110X_WHITE);

  // ===== MAIN CONTENT =====
  String line1 = "";
  String line2 = "";

  switch (current_mode) {
    case EVAP_COOl_STATUS:
      line1 = "STATUS:";
      line1 += EVAP_COOL_SYSTEM_STATUS ? "ON" : "OFF";
      break;

    case AUTO_WASH_STATUS:
      line1 = "STATUS:";
      line1 += AUTO_WASH_SYSTEM_STATUS ? "ON" : "OFF";
      break;

    case UNIT_COMM_STATUS:
      line1 = "STATUS:";
      line1 += UNIT_COMMAND_STATUS ? "ON" : "OFF";
      break;

    case E_TEMPERATURE:
      line1 = "Temp: " + String((int)dischargeTemp) + (char)247 + "C";
      line2 = "S.P: " + String(dischargeSp) + (char)247 + "C";
      break;

    case R_TEMPERATURE:
      line1 = "Temp: " + String((int)ReturnTemp) + (char)247 + "C";
      line2 = "S.P: " + String(ReturnSp) + (char)247 + "C";
      break;

    case S_TEMPERATURE:
      line1 = "Temp: " + String((int)SupplyTemp) + (char)247 + "C";
      break;
  }

  if (current_mode == EVAP_COOl_STATUS || current_mode == AUTO_WASH_STATUS || current_mode == UNIT_COMM_STATUS) {
    display.setTextSize(2);
    display.getTextBounds(line1.c_str(), 0, 0, &x1, &y1, &w, &h);
    int tempX = (SCREEN_WIDTH - w) / 2;
    // int tempY = (SCREEN_HEIGHT - h) / 2 + 12;  // adjust +6 to shift slightly down
    display.setCursor(tempX, 27);
    display.print(line1);
  } else {
    display.getTextBounds(line1.c_str(), 0, 0, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - w) / 2;
    display.setCursor(x, 27);
    display.print(line1);
    if (line2 != "") {
      display.getTextBounds(line2.c_str(), 0, 0, &x1, &y1, &w, &h);
      int x = (SCREEN_WIDTH - w) / 2;
      display.setCursor(x, 47);
      display.print(line2);
    }
  }

  // if (current_mode == E_TEMPERATURE) {
  //   // Actual temperature
  //   display.setTextSize(2);
  //   display.setCursor(7, 27);
  //   display.print("TEMP:");
  //   // display.setTextSize(2);
  //   display.setCursor(70, 27);
  //   display.print((int)(dischargeTemp));
  //   display.print((char)247);
  //   display.print("C");

  //   // Setpoint
  //   display.setTextSize(2);
  //   display.setCursor(9, 47);
  //   // display.setTextSize(2);
  //   display.print("S.P:- ");
  //   display.setCursor(78, 47);
  //   display.print((int)(dischargeSp));
  //   display.setCursor(108, 47);
  //   display.print("+");
  //   // display.print((char)247); // degree symbol
  //   // display.print("C");
  // } else if (current_mode == R_TEMPERATURE) {
  //   // Actual temperature
  //   display.setTextSize(2);
  //   display.setCursor(9, 27);
  //   display.print("TEMP:");
  //   // display.setTextSize(2);
  //   display.setCursor(70, 27);
  //   display.print((int)(ReturnTemp));
  //   display.print((char)247);
  //   display.print("C");

  //   // Setpoint
  //   display.setTextSize(2);
  //   display.setCursor(9, 47);
  //   // display.setTextSize(2);
  //   display.print("S.P : - ");
  //   display.setCursor(70, 47);
  //   display.print((int)(ReturnSp));
  //   display.print("   +");
  //   // display.print((char)247); // degree symbol
  //   // display.print("C");
  // } else if (current_mode == S_TEMPERATURE) {
  //   // Actual temperature
  //   display.setTextSize(2);
  //   display.setCursor(9, 27);
  //   display.print("TEMP:");
  //   display.setCursor(70, 27);
  //   display.print((int)(SupplyTemp));
  //   display.print((char)247);
  //   display.print("C");
  // } else if (current_mode == EVAP_COOl_STATUS) {
  //   display.setTextSize(2);
  //   display.setCursor(5, 35);
  //   display.print("STATUS");
  //   display.drawFastVLine(80, 25, 35, SH110X_WHITE);
  //   display.setTextSize(2);
  //   display.setCursor(90, 35);
  //   display.print(EVAP_COOL_SYSTEM_STATUS ? "ON" : "OFF");
  // } else if (current_mode == AUTO_WASH_STATUS) {
  //   display.setTextSize(2);
  //   display.setCursor(5, 35);
  //   display.print("STATUS");
  //   display.drawFastVLine(80, 25, 35, SH110X_WHITE);
  //   display.setTextSize(2);
  //   display.setCursor(90, 35);
  //   display.print(AUTO_WASH_SYSTEM_STATUS ? "ON" : "OFF");
  // } else if (current_mode == UNIT_COMM_STATUS) {
  //   display.setTextSize(2);
  //   display.setCursor(5, 35);
  //   display.print("STATUS");
  //   display.drawFastVLine(80, 25, 35, SH110X_WHITE);
  //   display.setTextSize(2);
  //   display.setCursor(90, 35);
  //   display.print(UNIT_COMMAND_STATUS ? "ON" : "OFF");
  // }
  display.display();
}


void updateControlOutputs() {
  if (PowerState) {
    digitalWrite(EVAP_COOL_SYSTEM, EVAP_COOL_SYSTEM_SWITCH ? HIGH : LOW);
    digitalWrite(AUTO_WASH_SYSTEM, AUTO_WASH_SYSTEM_SWITCH ? HIGH : LOW);
    digitalWrite(UNIT_COMMAND, UNIT_COMMAND_SWITCH ? HIGH : LOW);
  } else {
    digitalWrite(EVAP_COOL_SYSTEM, LOW);
    digitalWrite(AUTO_WASH_SYSTEM, LOW);
    digitalWrite(UNIT_COMMAND, LOW);
  }
}

void handleAdjust(char key) {
  if (current_mode == E_TEMPERATURE) {
    if (key == '3' && dischargeSp < MAX_DTEMP) dischargeSp++;
    else if (key == '4' && dischargeSp > MIN_DTEMP) dischargeSp--;
  } else if (current_mode == R_TEMPERATURE) {
    if (key == '3' && ReturnSp < MAX_RTEMP) ReturnSp++;
    else if (key == '4' && ReturnSp > MIN_RTEMP) ReturnSp--;
  }
  updateDisplay();
}

void showCenteredText(const char* text, uint8_t textSize = 2, int yOffset = 0) {
  display.clearDisplay();
  display.setTextSize(textSize);
  display.setFont();
  display.setTextColor(SH110X_WHITE);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  int y = (SCREEN_HEIGHT - h) / 2 + yOffset;

  //drawBorder();
  display.setCursor(x, y);
  display.print(text);
  display.display();
}

void showStartupScreen() {
  display.clearDisplay();
  drawBorder();
  display.setTextColor(SH110X_WHITE);
  display.setFont(&Org_01);
  display.setTextSize(2);

  const char* msg = "Starting";
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  int y = (SCREEN_HEIGHT - h) / 2 - 7;
  display.setCursor(x, y);
  display.print(msg);
  display.display();

  delay(300);
  for (int i = 0; i < 3; i++) {
    display.print(".");
    display.display();
    delay(300);
  }

  delay(400);
  showCenteredText("DX-Mini", 2);
  delay(1500);
  display.clearDisplay();
}

void showModeScreen() {
  const char* modeMsg;

  switch (current_mode) {
    case E_TEMPERATURE: modeMsg = "EVAP TEMP"; break;
    case R_TEMPERATURE: modeMsg = "RA TEMP"; break;
    case S_TEMPERATURE: modeMsg = "S TEMP"; break;
    case EVAP_COOl_STATUS: modeMsg = "EVAP COOL"; break;
    case AUTO_WASH_STATUS: modeMsg = "AUTO WASH"; break;
    case UNIT_COMM_STATUS: modeMsg = "UNIT"; break;
  }

  display.clearDisplay();
  //drawBorder();
  display.setTextColor(SH110X_WHITE);
  display.setFont();
  display.setTextSize(2);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(modeMsg, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  int y = (SCREEN_HEIGHT - h) / 2;  // keep vertically centered

  display.setCursor(x, y);
  display.print(modeMsg);
  display.display();
}
