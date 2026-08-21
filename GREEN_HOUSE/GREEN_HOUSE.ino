#define DEBUG
#include <ArduinoJson.h>
#include <WiFi.h>
// #include <ISL1208_RTC.h>
// #include <PCA9554.h>
// #include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
// #include <HTTPClient.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
// #include "time.h"
#include <Wire.h>
#include <Adafruit_SH110X.h>
#include <Fonts/Org_01.h>
#include <Keypad.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <Preferences.h>
#include <Fonts/FreeMono12pt7b.h>

#include "variables.h"
#include "aws_certificates.h"
#include "wifithread.h"

#define I2C_SDA 21
#define I2C_SCL 22
#define OLED_ADDRESS 0x3C
#define OLED_RESET -1
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define ONE_WIRE_BUS 4 // temp sensor!

#define DHT_PIN 18
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// bool main_control = true;
// bool pumpstate = false;
// float temperatureC = 0.0;
// float humidity = 0.0;
// int userSetHumidity = 50;  // default set-point
// const int MIN_HUMIDITY = 10;
// const int MAX_HUMIDITY = 100;
// float tprev = -1000;
// unsigned long time1 = 0;
// unsigned long time3 = 0;
// bool tmp_req = true;
// String temp1 = "0";

#define BUZZER_PIN 16
#define BUZZER_FREQ 2000
#define BUZZER_DURATION 50

enum Mode {
  TEMPERATURE,
  HUMIDITY,
  PUMP,
  NUM_MODES
};
int current_mode = TEMPERATURE;

const byte ROWS = 1;
const byte COLS = 4;
char keys[ROWS][COLS] = { { '1', '2', '3', '4' } };

byte rowPins[ROWS] = { 27 };
byte colPins[COLS] = { 25, 26, 33, 32 };

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void drawBorder() {
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);
}

void beep(int duration = 150, int freq = 2000) {
  tone(BUZZER_PIN, freq, duration);
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
// ---------------------------------------------------------

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
  for (int i = 0; i < 5; i++) {
    display.print(".");
    display.display();
    delay(300);
  }

  display.clearDisplay();
  drawBorder();
  display.setFont(&Org_01);
  display.setTextSize(2);
  display.setCursor(30, 25);
  display.print("GREEN");
  display.setCursor(30, 45);
  display.print("HOUSE");
  display.display();
  delay(1500);
  display.clearDisplay();
}

// ---------------------------------------------------------
void updateDisplay() {
  display.clearDisplay();
  display.setFont(&Org_01);
  display.setTextColor(SH110X_WHITE);

  // ===== HEADER =====
  display.setTextSize(2);
  String modeName;
  switch (current_mode) {
    case TEMPERATURE: modeName = "TEMP"; break;
    case HUMIDITY: modeName = "HUMIDITY"; break;
    case PUMP: modeName = "PUMP"; break;
  }

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(modeName.c_str(), 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;


  display.setCursor(x, 8);
  display.setTextSize(2);
  display.print(modeName);
  display.drawFastHLine(2, 20, SCREEN_WIDTH - 4, SH110X_WHITE);


  if (current_mode == TEMPERATURE) {
    display.setTextSize(2);
    display.setCursor(5, 45);
    // display.setFont();
    display.print("TEMP:");
    display.setCursor(60, 45);
    display.print(temperatureC, 1);
    display.print((char)247);
    display.print("C");

  }

  else if (current_mode == HUMIDITY) {
    // The old layout printed "Humidity:" and the value on top of each
    // other at text size 2. Keep the reading on its own centered line.
    display.setTextSize(2);

    String humidityText;
    if (isnan(humidity)) {
      humidityText = "--%";
    } else {
      humidityText = String(humidity, 0) + "%";
    }

    int16_t hx1, hy1;
    uint16_t hw, hh;
    display.getTextBounds(humidityText.c_str(), 0, 0, &hx1, &hy1, &hw, &hh);
    display.setCursor((SCREEN_WIDTH - hw) / 2, 42);
    display.print(humidityText);

    // Smaller set-point line so it fits cleanly on the 128x64 OLED.
    display.setTextSize(1);
    String spText = "S.P: " + String(userSetHumidity) + "%";
    display.getTextBounds(spText.c_str(), 0, 0, &hx1, &hy1, &hw, &hh);
    display.setCursor((SCREEN_WIDTH - hw) / 2, 59);
    display.print(spText);
  }

  else if (current_mode == PUMP) {
    display.setTextSize(3);
    display.setCursor(45, 45);
    display.print(pumpstate ? "ON" : "OFF");
    Serial.print(pumpstate);
  }

  display.display();
}

// ---------------------------------------------------------

//logics begins
void loopFunction() {
  if (WiFi.status() == WL_CONNECTED && deviceConnected == false) {
    wifi_status = 1;
  }
  if (main_control) {  //check that power is on or not
    pump_auto_control();
  } else {
    digitalWrite(PUMP_CONTROL_PIN, LOW);  // For ESP32 Board
    pumpstate = 0;                         //pump state
  }
}

void pump_auto_control() {
  if (main_control) {
  if (humidity < userSetHumidity) {
    digitalWrite(PUMP_CONTROL_PIN, HIGH);  // Pump ON
    pumpstate = true;
  } else {
    digitalWrite(PUMP_CONTROL_PIN, LOW);  // Pump OFF
    pumpstate = false;
    }
  }
}

// ---------------------------------------------------------
void handleAdjust(char key) {
  // HUMIDITY SETPOINT MODE
  if (current_mode == HUMIDITY) {
    if (key == '3' && userSetHumidity < MAX_HUMIDITY) {
      userSetHumidity++;
    } else if (key == '4' && userSetHumidity > MIN_HUMIDITY) {
      userSetHumidity--;
    }
  }
}
// ================================================================
void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  dht.begin();
  Serial.print("DHT11 initialized on GPIO ");
  Serial.println(DHT_PIN);
  sensors.begin();

  if (!display.begin(OLED_ADDRESS, true)) {
    Serial.println("OLED failed!");
    while (1);
  }

  showStartupScreen();

  // PUMP Relay
  pinMode(PUMP_CONTROL_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(PUMP_CONTROL_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  showCenteredText("POWER OFF", 2);  // show POWER OFF on screen

  DEVICE_INIT();

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

// ================================================================
void loop() {
  // Read keypad
  char key = keypad.getKey();
  if (key) {
    beep();
    Serial.print("Key: ");
    Serial.println(key);
    
    switch (key) {
      case '1':

        main_control = !main_control;
        // Save to Preferences
        preferences.begin("parameters", false);
        preferences.putBool("main_control", main_control);
        preferences.end();

        if (main_control) {
          // power ON
          showCenteredText("POWER ON", 2);
          delay(800);
          updateDisplay();  // Show main control screen
        } else {
          // power OFF
          pumpstate = false;

          digitalWrite(PUMP_CONTROL_PIN, LOW);

          // main_control = false;
          showCenteredText("POWER OFF", 2);
        }

        break;
      case '2':
        current_mode = (current_mode + 1) % NUM_MODES;
        break;
      case '3':
      case '4':
      if (main_control){
        handleAdjust(key);
      }
        break;
    }
  }

  if (millis() - wifi_setting_time >= 300000 && wifi_ap_mode == true) {  // Ap mode will off after 5 Minutes 300000 = 5 * 60 * 1000 seconds
    WiFi.softAPdisconnect();
    wifi_ap_mode = false;
  }

  if (millis() - time1 >= 2000) {
    time1 = millis();

    // DS18B20
    sensors.requestTemperatures();
    temperatureC = sensors.getTempCByIndex(0);
  }

  // DHT11 is a slow sensor. Read it independently every 2500 ms so
  // DS18B20 conversion/MQTT work cannot disturb its sampling schedule.
  static unsigned long lastDhtRead = 0;
  if (millis() - lastDhtRead >= 2500) {
    lastDhtRead = millis();

    float newHumidity = dht.readHumidity();
    float dhtTemperature = dht.readTemperature();

    Serial.print("[DHT11 GPIO ");
    Serial.print(DHT_PIN);
    Serial.print("] ");

    if (isnan(newHumidity) || isnan(dhtTemperature)) {
      Serial.println("READ FAILED - check DATA pull-up/power/sensor type");
    } else {
      humidity = newHumidity;

      Serial.print("Humidity: ");
      Serial.print(humidity, 1);
      Serial.print(" % | Temperature: ");
      Serial.print(dhtTemperature, 1);
      Serial.println(" C");
    }
  }

  pump_auto_control();
  if (main_control) updateDisplay();

  if (millis() - time3 >= 3000) {

    if (!client.connected()) {
      reconnect();
    }

    if (!message_received) {
      publishJson();
    } else {
      message_received = false;
      time3 = millis() + 3000;
    }
    time3 = millis();
  }
  client.loop();
}
