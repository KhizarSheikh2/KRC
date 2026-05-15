#define DEBUG

#include <WiFi.h>
#include <ISL1208_RTC.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>  // for MQTT
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_SH110X.h>
#include <Fonts/Org_01.h>
#include <Keypad.h>
#include "variables.h"
#include "aws_certificates.h"
#include "wifithread.h"

#define I2C_SDA 21
#define I2C_SCL 22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define BUZZER_PIN 16       // Choose an available GPIO pin
#define BUZZER_FREQ 2000    // Frequency in Hz (2 kHz typical)
#define BUZZER_DURATION 50  // Duration in ms

#define PUMP_CONTROL_PIN 17
#define LOW_LEVEL_PIN 18
#define MID_LEVEL_PIN 19
#define HIGH_LEVEL_PIN 23
#define ALARM_PIN 16

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const byte ROWS = 1;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  { '2', '1', '3', '4' }
};

byte rowPins[ROWS] = {27};
byte colPins[COLS] = {26, 25, 33, 32};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

unsigned long lastUpdate = 0;
unsigned long modePressStart = 0;
bool modePressed = false;
const unsigned long LONG_PRESS_TIME = 2000;  // 2 seconds

enum Mode {
  INFO,
  PUMP,
  NUM_MODES
};

int current_mode = INFO;

void beep(int duration = 150, int freq = 2000) {
  tone(BUZZER_PIN, freq, duration);
}

void showCenteredText(const char* msg, uint8_t textSize = 2, int yOffset = 0) {
  display.clearDisplay();
  display.setTextSize(textSize);
  display.setTextColor(SH110X_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
  display.print(msg);
  display.display();
}

void showStartupScreen() {
  display.clearDisplay();
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
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setFont(&Org_01);
  display.setTextSize(2);
  display.setCursor(30, 25);
  display.print("DRAIN");
  display.setCursor(25, 45);
  display.print("MASTER");
  display.display();
  delay(1500);
  display.clearDisplay();
}

void showModeScreen() {
  const char* modeMsg;
  switch (current_mode) {
    case INFO: modeMsg = "STATUS"; break;
    case PUMP: modeMsg = "PUMP"; break;
  }

  display.clearDisplay();
  //drawBorder();
  display.setTextColor(SH110X_WHITE);
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

void updateDisplay() {
  display.clearDisplay();
  display.setFont(&Org_01);
  display.setTextColor(SH110X_WHITE);

  // ===== HEADER =====
  display.setTextSize(2);
  String modeName;
  switch (current_mode) {
    case INFO: modeName = "STATUS"; break;
    case PUMP: modeName = "PUMP"; break;
  }
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(modeName.c_str(), 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, 9);
  display.print(modeName);
  display.drawFastHLine(2, 15, SCREEN_WIDTH - 4, SH110X_WHITE);

  if (current_mode == INFO) {
    display.setTextSize(2);
    display.setCursor(0, 30);
    display.print("E:");
    display.print(low ? "ON " : "OFF");
    display.setCursor(0, 45);
    display.print("F:");
    display.print(mid ? "ON " : "OFF");
    display.setCursor(0, 60);
    display.print("O.F:");
    display.print(high ? "ON " : "OFF");

    display.drawFastVLine(68, 20, 60, SH110X_WHITE);

    display.setTextSize(2);
    display.setCursor(75, 35);
    display.print("P:");
    display.print(pumpstate ? "ON" : "OFF");
    display.setCursor(75, 50);
    display.print("A:");
    display.print(alarmstate ? "ON" : "OFF");
  } else if (current_mode == PUMP) {
    display.setTextSize(2);
    display.setCursor(5, 38);
    display.print(pump_control ? " Auto:" : "Manual:");
    display.setTextSize(2);
    display.setCursor(85, 38);
    display.print(pumpstate ? "ON" : "OFF");
  }
  display.display();
}

void updateControlOutputs() {
  if (main_control) {
    digitalWrite(PUMP_CONTROL_PIN, pumpstate ? HIGH : LOW);
    digitalWrite(ALARM_PIN, alarmstate ? HIGH : LOW);
  } else {
    digitalWrite(PUMP_CONTROL_PIN, LOW);
    digitalWrite(ALARM_PIN, LOW);  // Ensure alarm off when power off
  }
}

void updateWaterLevel() {
  if (!main_control) {
    pumpstate = false;
    alarmstate = false;
    updateControlOutputs();
    return;
  }

  bool low_level = (digitalRead(LOW_LEVEL_PIN) == LOW) ? 1 : 0;
  bool mid_level = (digitalRead(MID_LEVEL_PIN) == LOW) ? 1 : 0;
  bool high_level = (digitalRead(HIGH_LEVEL_PIN) == LOW) ? 1 : 0;

  // var for display
  low = low_level;
  mid = mid_level;
  high = high_level;

  waterlevel = 0;  // Default
  if (high_level) {                                        
    waterlevel = 2;  // Overflow
  } else if (mid_level) {
    waterlevel = 1;  // Full
  } else if (low_level) {
    waterlevel = 0;  // Low/Empty
  }

  if (pump_control == 1) {  //Auto Mode
    alarmstate = false;
    pumpstate = false;
    if (high_level) {
      pumpstate = true;
      alarmstate = true;
    } else if (mid_level) {
      pumpstate = true;
      alarmstate = false;
    } else {
      pumpstate = false;
      alarmstate = false;
    }
  } else {               //Manual Mode
    pumpstate = pumpsw;
    if(high_level){
    alarmstate = true;
    }else {
    alarmstate = false;
    }
  }

  Serial.print("Water Level: ");
  Serial.print(waterlevel);
  Serial.print(" | Pump: ");
  Serial.print(pumpstate ? "ON" : "OFF");
  Serial.print(" | Alarm: ");
  Serial.print(alarmstate ? "ON" : "OFF");
  Serial.print(" | Sensors - Low: ");
  Serial.print(low_level ? "ON" : "OFF");
  Serial.print(" Mid: ");
  Serial.print(mid_level ? "ON" : "OFF");
  Serial.print(" High: ");
  Serial.println(high_level ? "ON" : "OFF");

  updateControlOutputs();
  updateDisplay();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(OLED_ADDRESS, true)) {
    Serial.println("OLED init failed");
    while (1)
      ;
  }

  showStartupScreen();

  pinMode(LOW_LEVEL_PIN, INPUT_PULLUP);
  pinMode(MID_LEVEL_PIN, INPUT_PULLUP);
  pinMode(HIGH_LEVEL_PIN, INPUT_PULLUP);

  pinMode(PUMP_CONTROL_PIN, OUTPUT);
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(PUMP_CONTROL_PIN, LOW);  // Pump off
  digitalWrite(ALARM_PIN, LOW);         // Alarm off

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(15, 25);
  display.print("Booting...");
  display.display();

  keypad.setHoldTime(200);

  // In setup()
  DEVICE_INIT();

  // Always start in Power Off visual and logic state
  // main_control = false;
  // preferences.begin("parameters", false);
  // preferences.putBool("main_control", false);
  // preferences.end();

  // showCenteredText("POWER OFF", 2);

  #ifdef DEBUG
    // Serial.begin(115200);
    Serial.println("\n\nbooting up device...\n\n");
    Serial.print("devicename: ");
    Serial.println(devicename);
  #endif

    //=========================================//
    macaddress = WiFi.macAddress();

    if (loadCredentials(ssid, password)) {
  #ifdef DEBUG
      Serial.println("Loaded Wi-Fi credentials from memory.");
  #endif
      connectToWiFi(ssid.c_str(), password.c_str());
    } else {
  #ifdef DEBUG
      Serial.println("No Wi-Fi credentials found. Starting in AP mode.");
  #endif
      startAccessPoint();
  }

  server_setup();
  reconnect();
}

void handleAdjust(char key) {
  if (current_mode == PUMP && pump_control == 0) {  // manual mode
    pumpsw = !pumpsw;
    pumpstate = pumpsw;
    updateControlOutputs();  // Use this instead of direct digitalWrite for consistency

    // Save the change to preferences
    preferences.begin("parameters", false);
    preferences.putUInt("pumpsw", pumpsw);
    preferences.end();
  }
  updateDisplay();
  publishJson();
}

void loop() {
  static unsigned long lastRepeat = 0;
  const unsigned long repeatRate = 150;
  static unsigned long lastInputTime = 0;
  unsigned long now = millis();

  char key = keypad.getKey();

  if (key) {
    beep();  // short feedback beep for every key press
    Serial.print("Key: ");
    Serial.println(key);

    switch (key) {
      case '1':  // Toggle Power
        main_control = !main_control;

        // Save the change to preferences
        preferences.begin("parameters", false);
        preferences.putBool("main_control", main_control);
        preferences.end();

        if (main_control) {
          // Turning power ON
          showCenteredText("POWER ON", 2);
          delay(800);
          updateDisplay();  // Show main control screen
        } else {
          // Turning power OFF (fully reset states)
          pumpstate = false;
          alarmstate = false;
          updateControlOutputs();  // Ensure outputs go LOW
          // display.setFont();
          showCenteredText("POWER OFF", 2);
        }
        break;

      case '2':  // Mode button
        if (main_control) {
          // Detect long press
          if (!modePressed) {
            modePressed = true;
            modePressStart = millis();
          }
        }
        break;

      case '3':
      case '4':  // Adjustments
        if (main_control) {
          handleAdjust(key);
          //  lastRepeat = millis();
        }
        break;
    }
  }
  // Detect long press WHILE button is held 
  if (modePressed) {
    unsigned long pressDuration = millis() - modePressStart;

    if (pressDuration >= LONG_PRESS_TIME && keypad.getState() == HOLD) {
      modePressed = false;
      if (current_mode == PUMP) {
        pump_control = !pump_control;
        preferences.begin("parameters", false);
        preferences.putBool("pump_control", pump_control);
        preferences.end();
        beep(100, 2000);
        // showCenteredText(pump_control ? "PUMP AUTO" : "PUMP MANUAL", 2);
        delay(800);
      }

      updateDisplay();
      publishJson();

    } else if (keypad.getState() == RELEASED) {
      if (pressDuration < LONG_PRESS_TIME) {
        if (main_control) {
          current_mode = (current_mode + 1) % NUM_MODES;
          beep(50, 2000);
          delay(600);
          updateDisplay();
        }
      }
      modePressed = false;
    }
  }

  if (millis() - wifi_setting_time >= 300000 && wifi_ap_mode == true) {  // Ap mode will off after 5 Minutes 300000 = 5 * 60 * 1000 seconds
    WiFi.softAPdisconnect();
    wifi_ap_mode = false;
  }

  if ((millis() - time3) > 2000) {
    if (!client.connected()) {
      reconnect();
    }
    publishJson();
    time3 = millis();
  }
  
  client.loop();

  if (main_control && now - lastUpdate >= 2000) {
    // Only update water level when power is ON
    updateWaterLevel();
    lastUpdate = now;
  }
  delay(15);
}
