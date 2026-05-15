#define DEBUG

#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
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

#define BUZZER_PIN 16
#define BUZZER_FREQ 2000
#define BUZZER_DURATION 50

#define PUMP_CONTROL_PIN 17
#define WATER_LEVEL_PIN 18   // sensor 1 (medium level)
#define WATER_LEVEL_PIN_2 23 // sensor 2 (high level alarm)
#define ALARM_PIN 16

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const byte ROWS = 1;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'2', '1', '3', '4'}
};

byte rowPins[ROWS] = {27};
byte colPins[COLS] = {26, 25, 33, 32};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

unsigned long lastUpdate = 0;
unsigned long modePressStart = 0;
bool modePressed = false;
const unsigned long LONG_PRESS_TIME = 2000; // 2 seconds

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
  display.setCursor(18, 25);
  display.print("SEWERAGE");
  display.setCursor(28, 45);
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
  display.setFont(&Org_01);
  display.setTextSize(2);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(modeMsg, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  int y = (SCREEN_HEIGHT - h) / 2 ; // keep vertically centered

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
    display.print("L:");
    display.print(lowLevel ? "ON " : "OFF");
    display.setCursor(0, 45);
    display.print("M:");
    display.print(midLevel ? "ON " : "OFF");
    display.setCursor(0, 60);
    display.print("H:");
    display.print(highLevel ? "ON " : "OFF");

    display.drawFastVLine(62, 20, 60, SH110X_WHITE);

    display.setTextSize(2);
    display.setCursor(75, 35);
    display.print("P:");
    display.print(pumpstate ? "ON" : "OFF");
    display.setCursor(75, 50);
    display.print("A:");
    display.print(alarmstate ? "ON" : "OFF");
  }
  else if (current_mode == PUMP) {
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

void readSensorsOnly() {
  if (!main_control) {
    pumpstate = false;
    alarmstate = false;
    updateControlOutputs();
    return;
  }

  // Read sensor inputs for display purposes only
  int sensor1 = digitalRead(WATER_LEVEL_PIN) ? 0 : 1;
  int sensor2 = digitalRead(WATER_LEVEL_PIN_2) ? 0 : 1;

  #ifdef DEBUG
    Serial.println("=== Reading Sensors (Manual Mode) ===");
    Serial.print("sensor1 (mid): ");
    Serial.println(sensor1);
    Serial.print("sensor2 (high): ");
    Serial.println(sensor2);
  #endif

  // Determine water level
  if (sensor2 == 1) {
    waterlevel = 2;
    alarmstate = true;  // Always alarm on high water
    beep();
    
    #ifdef DEBUG
      Serial.println("HIGH WATER DETECTED IN MANUAL MODE - ALARM ON");
    #endif
  }
  else if (sensor1 == 1) {
    waterlevel = 1;
    alarmstate = false;
  }
  else {
    waterlevel = 0;
    alarmstate = false;
  }

  // Update display flags
  lowLevel = (waterlevel == 0);
  midLevel = (waterlevel == 1);
  highLevel = (waterlevel == 2);

  // In manual mode, pumpstate is controlled by pumpsw
  pumpstate = pumpsw;

  publishJson();
}

void seweragecontrol() {
  if (!main_control) {
    pumpstate = false;
    alarmstate = false;
    updateControlOutputs();  // Added this line
    return;
  }

  // Read sensor inputs (sensors return 1 when water is AT/ABOVE them)
  int sensor1 = digitalRead(WATER_LEVEL_PIN) ? 0 : 1;   // Medium level sensor
  int sensor2 = digitalRead(WATER_LEVEL_PIN_2) ? 0 : 1;  // High level sensor

  #ifdef DEBUG
    Serial.println("=== Sensor Reading ===");
    Serial.print("sensor1 (mid): ");
    Serial.println(sensor1);
    Serial.print("sensor2 (high): ");
    Serial.println(sensor2);
  #endif

  // Determine water level and control outputs
  if (sensor2 == 1) {
    // HIGH LEVEL: Both sensors detect water
    waterlevel = 2;
    pumpstate = true;    // Keep pump ON
    alarmstate = true;   // Alarm ON at high level
    beep();              // Sound alarm
    
    #ifdef DEBUG
      Serial.println("→ HIGH LEVEL: Pump ON, Alarm ON");
    #endif
  }
  else if (sensor1 == 1) {
    // MID LEVEL: Only sensor1 detects water
    waterlevel = 1;
    pumpstate = true;    // Pump ON at mid level
    alarmstate = false;  // No alarm yet
    
    #ifdef DEBUG
      Serial.println("→ MID LEVEL: Pump ON, Alarm OFF");
    #endif
  }
  else {
    // LOW LEVEL: Neither sensor detects water
    waterlevel = 0;
    pumpstate = false;   // Pump OFF
    alarmstate = false;  // No alarm
    
    #ifdef DEBUG
      Serial.println("→ LOW LEVEL: Pump OFF, Alarm OFF");
    #endif
  }

  // Update display flags
  lowLevel = (waterlevel == 0);
  midLevel = (waterlevel == 1);
  highLevel = (waterlevel == 2);

  publishJson();
}

void handleAdjust(char key) {
  if (current_mode == PUMP && pump_control == 0) { // manual mode
    pumpsw = !pumpsw;
    pumpstate = pumpsw;
    updateControlOutputs();  // Use this instead of direct digitalWrite for consistency

    // NEW: Save the change to preferences
    preferences.begin("parameters", false);
    preferences.putUInt("pumpsw", pumpsw);
    preferences.end();
  }
  updateDisplay();
  publishJson();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(OLED_ADDRESS, true)) {
    Serial.println("OLED init failed");
    while (1);
  }

  showStartupScreen();

  pinMode(WATER_LEVEL_PIN, INPUT_PULLUP);
  pinMode(WATER_LEVEL_PIN_2, INPUT_PULLUP);
  pinMode(PUMP_CONTROL_PIN, OUTPUT);
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(PUMP_CONTROL_PIN, LOW);
  digitalWrite(ALARM_PIN, LOW);

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

void loop() {
  static unsigned long lastRepeat = 0;
  unsigned long now = millis();

  char key = keypad.getKey();

  if (key) {
    beep();
    Serial.print("Key: ");
    Serial.println(key);

    switch (key) {
      case '1': // Toggle Power
        main_control = !main_control;

        preferences.begin("parameters", false);
        preferences.putBool("main_control", main_control);
        preferences.end();

        if (main_control) {
          showCenteredText("POWER ON", 2);
          delay(800);
          updateDisplay();
        } else {
          pumpstate = false;
          alarmstate = false;
          updateControlOutputs();
          // display.setFont(); 
          showCenteredText("POWER OFF", 2);
        }
        break;

      case '2':
        if (main_control) {
          if (!modePressed) {
            modePressed = true;
            modePressStart = millis();
          }
        }
        break;

      case '3':
      case '4':
        if (main_control) {
          handleAdjust(key);
          lastRepeat = millis();
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

  if (millis() - wifi_setting_time >= 300000 && wifi_ap_mode == true) {
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
    if (pump_control) {
      // AUTO MODE: Sensors control the pump
      seweragecontrol();
      
      #ifdef DEBUG
        Serial.println(">>> AUTO MODE: Sensors controlling pump");
      #endif
    } else {
      // MANUAL MODE: User controls pump, but still read sensors
      readSensorsOnly();
      
      #ifdef DEBUG
        Serial.println(">>> MANUAL MODE: User controlling pump");
        Serial.print("    pumpsw = ");
        Serial.println(pumpsw);
        Serial.print("    pumpstate = ");
        Serial.println(pumpstate);
      #endif
    }
    
    updateControlOutputs();
    updateDisplay();
    lastUpdate = now;
  }

  delay(15);
}
