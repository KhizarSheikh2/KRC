#define DEBUG

// #include <PCA9554.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <PubSubClient.h>  // for MQTT
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "time.h"
#include <WiFi.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_SH110X.h>
#include <Fonts/Org_01.h>
#include <Keypad.h>

#include "aws_certificates.h"
#include "variables.h"
#include "wifithread.h"

#define FAN_CONTROL_PIN 17
#define PUMP_CONTROL_PIN 14
#define WATER_LEVEL_PIN 12
#define ONE_WIRE_BUS 19 // Temp sensor pin
#define I2C_SDA 21
#define I2C_SCL 22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
#define BUZZER_PIN 15      // Choose an available GPIO pin
#define BUZZER_FREQ 2000   // Frequency in Hz (2 kHz typical)
#define BUZZER_DURATION 50 // Duration in ms

// PCA9554 ioCon1(0x27);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

enum Mode {
  TEMPERATURE,
  FAN,
  PUMP,
  SCHEDULER,
  NUM_MODES
};

int current_mode = TEMPERATURE;

void beep(int duration = BUZZER_DURATION, int freq = BUZZER_FREQ) {
  tone(BUZZER_PIN, freq, duration);   // Non-blocking tone
  delay(10); // Small delay to ensure it triggers
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

  display.setCursor(x, y);
  display.print(text);
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

  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setFont(&Org_01);
  display.setTextSize(2);
  display.setCursor(8, 25);
  display.print("WATER TANK");
  display.setCursor(28, 45);
  display.print("COOLING");
  display.display();
  delay(1500);
  display.clearDisplay();
}

void updateDisplay() {
  display.clearDisplay();

  // display.setFont();
  display.setTextColor(SH110X_WHITE);

  // ===== HEADER =====
  display.setTextSize(2);
  String modeName;
  switch (current_mode) {
    case TEMPERATURE: modeName = "WATER TANK"; break;
    case FAN: modeName = "FAN"; break;
    case PUMP: modeName = "PUMP"; break;
    case SCHEDULER: modeName = "SCHEDULER"; break;
  }
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(modeName.c_str(), 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, 4);
  display.print(modeName);
  display.drawFastHLine(2, 22, SCREEN_WIDTH - 4, SH110X_WHITE);

  // ===== MAIN CONTENT =====
  if (current_mode == TEMPERATURE) {
    // Actual temperature
    display.setTextSize(2);
    display.setCursor(9, 27);
    display.print("TEMP:");
    display.setCursor(70, 27);
    display.print(temper);
    display.print((char)247);
    display.print("C");

    // Setpoint
    display.setTextSize(2);
    display.setCursor(9, 47);
    display.print("S.P :");
    display.setCursor(70, 47);
    display.print(sp);
    display.print((char)247); // degree symbol
    display.print("C");
  } 
  else if (current_mode == FAN) {
    display.setTextSize(2);
    display.setCursor(5, 38);
    display.print(fan_control ? " Auto:" : "Manual:");
    // display.drawFastVLine(80, 25, 35, SH110X_WHITE);
    display.setTextSize(2);
    display.setCursor(85, 38);
    display.print(fanstate ? "ON" : "OFF");
  } 
  else if (current_mode == PUMP) {
    display.setTextSize(2);
    display.setCursor(5, 38);
    display.print(pump_control ? " Auto:" : "Manual:");
    // display.drawFastVLine(80, 25, 35, SH110X_WHITE);
    display.setTextSize(2);
    display.setCursor(85, 38);
    display.print(pumpstate ? "ON" : "OFF");
  }
  else if (current_mode == SCHEDULER) {

    display.setTextSize(2);
    display.setCursor(25, 27);
    display.print(timeschen ? "Enable" : "Disable");

    if (timeschen) {
        display.setCursor(5, 47);
        display.print(tmatched ? "Active" : "Not-Active");
    }
  }
  display.display();
}

void updateControlOutputs() {
  if (main_control) {
    digitalWrite(FAN_CONTROL_PIN, fanstate ? HIGH : LOW);
    digitalWrite(PUMP_CONTROL_PIN, pumpstate ? HIGH : LOW);
  } else {
    digitalWrite(FAN_CONTROL_PIN, LOW);
    digitalWrite(PUMP_CONTROL_PIN, LOW);
  }
}

void loopFunction() {
  if (datainterrupt) {
    // esp_task_wdt_reset();
    datainterrupt = false;
    if (WiFi.status() == WL_CONNECTED && deviceConnected == false) {
      wifi_status = 1;

      NTP_TIME();
      inputs();
    }
  }
  if (main_control) {
    if (timeschen == 1) {
      fanScheduler();
    } else {
      if (fan_control) {
        fan_auto_control();
      } else {
        fan_manual_control();
      }
    }
    if (pump_control) {
      pump_auto_control();
    } else {
      pump_manual_control();
    }
  } else {
    //ioCon1.digitalWrite(4, LOW);
    digitalWrite(FAN_CONTROL_PIN, LOW); // For ESP32 Board
    fanstate = 0;

    //ioCon1.digitalWrite(5, LOW);
    digitalWrite(PUMP_CONTROL_PIN, LOW); // For ESP32 Board
    pumpstate = 0;
  }
  updateControlOutputs();
}

void getTemperature() {
  if (tmp_req) {
    sensors.setWaitForConversion(false);
    sensors.requestTemperatures();
    sensors.setWaitForConversion(true);
    tmp_req = 0;
    time3 = millis();
    temper = sensors.getTempCByIndex(0);
    if (temper > 0) {
      temp1 = String(temper);
    } else {
      temp1 = "0";
    }
  }
  if (temper != tprev) {
    op = 1;
    tprev = temper;
    time1 = millis();
  }
}

void inputs() {

  if (digitalRead(WATER_LEVEL_PIN) == LOW) {
  #ifdef DEBUG
      Serial.println("==========digitalRead(WATER_LEVEL_PIN) == LOW======");
      Serial.println("Full");
  #endif
      waterlevel = 1;
      waterlevelsp = 1;
    } else {
  #ifdef DEBUG
      Serial.println("==========digitalRead(WATER_LEVEL_PIN) == HIGH======");
      Serial.println("Low");
  #endif
      waterlevel = 0;
      waterlevelsp = 0;
    }
}

void fan_auto_control() {
  if (temper > sp && op == 0) {
    //ioCon1.digitalWrite(4, HIGH);
    digitalWrite(FAN_CONTROL_PIN, HIGH); // For ESP32 Board
    fanstate = 1;
  } else if (temper <= sp && op == 0) {
    //ioCon1.digitalWrite(4, LOW);
    digitalWrite(FAN_CONTROL_PIN, LOW); // For ESP32 Board
    fanstate = 0;
  }
}
void pump_auto_control() {
  if (waterlevel == 0) {
    //ioCon1.digitalWrite(5, HIGH);
    digitalWrite(PUMP_CONTROL_PIN, HIGH); // For ESP32 Board
    pumpstate = 1;
  } else if (waterlevel == 1) {
    //ioCon1.digitalWrite(5, LOW);
    digitalWrite(PUMP_CONTROL_PIN, LOW); // For ESP32 Board
    pumpstate = 0;
  }
}
void fan_manual_control() {
  if (fansw == 1) {
    // ioCon1.digitalWrite(4, HIGH);
    digitalWrite(FAN_CONTROL_PIN, HIGH); // For ESP32 Board
    fanstate = 1;
  } else {
    // ioCon1.digitalWrite(4, LOW);
    digitalWrite(FAN_CONTROL_PIN, LOW); // For ESP32 Board
    fanstate = 0;
  }
}
void pump_manual_control() {
  if (pumpsw == 1) {
    // ioCon1.digitalWrite(5, HIGH);
    digitalWrite(PUMP_CONTROL_PIN, HIGH); // For ESP32 Board
    pumpstate = 1;
  } else {
    // ioCon1.digitalWrite(5, LOW);
    digitalWrite(PUMP_CONTROL_PIN, LOW); // For ESP32 Board
    pumpstate = 0;
  }
}

void NTP_TIME() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
  // #ifdef DEBUG
      Serial.println("Failed to obtain time");
  // #endif
      return;
    }
  // #ifdef DEBUG
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    Serial.print("   ");
    Serial.print(timeinfo.tm_hour);
    Serial.print(" ");
  // #endif
    deviceTime = "";
  // #ifdef DEBUG
    Serial.print("day=");
    Serial.println(timeinfo.tm_wday);
  // #endif
    tmp = day_app[timeinfo.tm_wday];
    SchScan[0] = tmp;
    SchScan[1] = timeinfo.tm_mday;
    SchScan[2] = timeinfo.tm_mon + 1;
    SchScan[3] = timeinfo.tm_year - 100;
    SchScan[4] = timeinfo.tm_hour;
    SchScan[5] = timeinfo.tm_min;
    SchScan[6] = timeinfo.tm_sec;
  // #ifdef DEBUG
    Serial.println("NTP_TIME called");
  // #endif
}

void IRAM_ATTR onTimer() {
  if (milscount > 1000) {
    milscount = 0;
    datainterrupt = true;
  }
  if (wifi_status == 0 && deviceConnected == false) {
    if (ledcounter > 50) {
      ledcounter = 0;
      if (blink == 0) {
        // digitalWrite(23, HIGH);
        blink = 1;
      } else {
        // digitalWrite(23, LOW);
        blink = 0;
      }
    }
  } else if (wifi_status == 0 && deviceConnected == true) {
    if (ledcounter > 500) {
      ledretrycount += 1;
      if (ledretrycount > 5) {
        ledcounter = 0;
        ledretrycount = 0;
      } else {
        ledcounter = 400;
      }
      if (blink == 0) {
        // digitalWrite(23, HIGH);
        blink = 1;
      } else {
        // digitalWrite(23, LOW);
        blink = 0;
      }
    }
  } else if (wifi_status == 1 && deviceConnected == false) {
    if (ledcounter > 1001) {
      ledcounter = 0;
      if (blink == 0) {
        // digitalWrite(23, HIGH);
        blink = 1;
      } else {
        // digitalWrite(23, LOW);
        blink = 0;
      }
    }
  }
  ledcounter++;
  milscount++;
}

void parse_schedule() {
  // Clear arrays
  memset(days_in_num, 0, sizeof(days_in_num));
  memset(hours_num, 0, sizeof(hours_num));

  // Find the pipe separator
  int pipeIndex = timesch.indexOf('|');
  if (pipeIndex == -1) return;  // Invalid format, do nothing

  String hoursPart = timesch.substring(0, pipeIndex);
  String daysPart = timesch.substring(pipeIndex + 1);

  // Parse hours
  if (hoursPart.startsWith("hoursch=")) {
    String hstr = hoursPart.substring(8);  // After "hoursch="
    if (hstr == "24") {
      // All hours
      for (int i = 0; i < 24; i++) hours_num[i] = 1;
    } else {
      // Comma-separated list, e.g., "0,1,2"
      int start = 0;
      int comma = hstr.indexOf(',', start);
      while (comma != -1) {
        int h = hstr.substring(start, comma).toInt();
        if (h >= 0 && h < 24) hours_num[h] = 1;
        start = comma + 1;
        comma = hstr.indexOf(',', start);
      }
      // Last value
      int h = hstr.substring(start).toInt();
      if (h >= 0 && h < 24) hours_num[h] = 1;
    }
  }

  // Parse days
  if (daysPart.startsWith("daysch=")) {
    String dstr = daysPart.substring(7);  // After "daysch="
    if (dstr == "7") {
      // All days
      for (int i = 0; i < 7; i++) days_in_num[i] = 1;
    } else {
      // Comma-separated list, e.g., "0,1,2"
      int start = 0;
      int comma = dstr.indexOf(',', start);
      while (comma != -1) {
        int d = dstr.substring(start, comma).toInt();
        if (d >= 0 && d < 7) days_in_num[d] = 1;
        start = comma + 1;
        comma = dstr.indexOf(',', start);
      }
      // Last value
      int d = dstr.substring(start).toInt();
      if (d >= 0 && d < 7) days_in_num[d] = 1;
    }
  }

  // #ifdef DEBUG
    Serial.println("Parsed schedule:");
    Serial.print("Hours: ");
    for (int i = 0; i < 24; i++) if (hours_num[i]) Serial.print(String(i) + " ");
    Serial.println();
    Serial.print("Days: ");
    for (int i = 0; i < 7; i++) if (days_in_num[i]) Serial.print(String(i) + " ");
    Serial.println();
  // #endif
}
void fanScheduler() {
  if (timeschen == 0) {
    // Serial.println("Scheduler disabled (timeschen = 0)");
    return; // Scheduler disabled
  }

  // Fetch current NTP time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // #ifdef DEBUG
      Serial.println("NTP time unavailable - skipping scheduler");
    // #endif
    return; // Skip if NTP fails (e.g., no WiFi or sync issue)
  }

  int currentHour = timeinfo.tm_hour;  // Current hour from NTP
  int currentDay = timeinfo.tm_wday;   // Current day from NTP

  currentDay = (currentDay == 0) ? 6 : currentDay - 1;

  parse_schedule();

  Serial.printf("Scheduler: Day=%d Hour=%d\n", currentDay, currentHour);

  if (days_in_num[currentDay] == 1 && hours_num[currentHour] == 1) {
        tmatched = 1;
        fanstate = 1;
        // Serial.println("SCHEDULE MATCH → FAN ON");
    } else {
        tmatched = 0;
        fanstate = 0;
        // Serial.println("NO MATCH → FAN OFF");
  }
  
  // Save to Preferences (only if changed to avoid wear)
  static int prevTmatched = -1;
  if (tmatched != prevTmatched) {
    preferences.begin("parameters", false);
    preferences.putUInt("tmatched", tmatched);
    preferences.end();
    prevTmatched = tmatched;
  }
}

void handleAdjust(char key) {
  if (current_mode == TEMPERATURE) {
    if (key == '3' && sp < MAX_TEMP) sp++;
    else if (key == '4' && sp > MIN_TEMP) sp--;
    
    // Save to Preferences
    preferences.begin("parameters", false);
    preferences.putUInt("sp", sp);
    preferences.end();
  }
  else if (current_mode == FAN && !fan_control) {  // only if manual
    if (key == '4' || key == '3') {
      fansw = !fansw;
      fanstate = !fanstate;
      
      // Save to Preferences
      preferences.begin("parameters", false);
      preferences.putUInt("fansw", fansw);
      preferences.end();
    }
  } 
  else if (current_mode == PUMP && !pump_control) { // only if manual
    if (key == '4' || key == '3') {
      pumpsw = !pumpsw;
      pumpstate = !pumpstate;
      
      // Save to Preferences
      preferences.begin("parameters", false);
      preferences.putUInt("pumpsw", pumpsw);
      preferences.end();
    }
  }
  updateDisplay();
  updateControlOutputs();
  publishJson();
}

void setup() {

  // ioCon1.begin();
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(OLED_ADDRESS, true)) {
    Serial.println("OLED init failed");
    while (1);
  }

  showStartupScreen();

  // ioCon1.digitalWrite(0, LOW);
  // ioCon1.digitalWrite(1, LOW);
  // ioCon1.digitalWrite(2, LOW);
  // ioCon1.digitalWrite(3, LOW);
  // ioCon1.digitalWrite(4, LOW);
  // ioCon1.digitalWrite(5, LOW);
  // ioCon1.digitalWrite(6, LOW);
  // ioCon1.digitalWrite(7, LOW);

  // ioCon1.portMode(ALLOUTPUT);

  pinMode(WATER_LEVEL_PIN, INPUT_PULLUP);
  pinMode(FAN_CONTROL_PIN, OUTPUT);
  pinMode(PUMP_CONTROL_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(FAN_CONTROL_PIN, LOW);
  digitalWrite(PUMP_CONTROL_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  keypad.setHoldTime(200);

  // Start with power OFF screen
  showCenteredText("POWER OFF", 2);

  #ifdef DEBUG
    Serial.println("\n\nbooting up device...\n\n");
    Serial.print("devicename: ");
    Serial.println(devicename);
  #endif

    DEVICE_INIT();

    sensors.begin();
    sensors.setWaitForConversion(false);
    sensors.requestTemperatures();
    sensors.setWaitForConversion(true);

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

  /* For TIO444 Board*/

  // iocon  pin set to output
  // pinMode(2, OUTPUT);
  // pinMode(13, OUTPUT);

  // digitalWrite(2, HIGH); // For ESP32 Board
  // digitalWrite(13, HIGH); // For ESP32 Board
  /*-----------------*/

  /* For ESP32 Board*/
  // pinMode(2, OUTPUT);
  // pinMode(4, OUTPUT);

  // digitalWrite(2, HIGH);
  // digitalWrite(4, HIGH);
  /*-----------------*/


  // timer = timerBegin(0, 80, true);                    //Begin timer with 1 MHz frequency (80MHz/80)
  // timerAttachInterrupt(timer, &onTimer, true);        //Attach the interrupt to Timer1
  // unsigned int timerFactor = 1000000 / SamplingRate;  //Calculate the time interval between two readings, or more accurately, the number of cycles between two readings
  // timerAlarmWrite(timer, timerFactor, true);          //Initialize the timer
  // timerAlarmEnable(timer);

  // esp_task_wdt_init(WDT_TIMEOUT, true);  //enable panic so ESP32 restarts
  // esp_task_wdt_add(NULL);                //add current thread to WDT watch

  reconnect();
}

void loop() {
  static char heldKey = NO_KEY;

  if (millis() - lastInputTime > 2000) {  // Every 2 seconds
    inputs();
    lastInputTime = millis();
  }

  char key = keypad.getKey();

  if (key) {
    beep(); // short feedback beep for every key press
    Serial.print("Key: ");
    Serial.println(key);

    switch (key) {
      case '1': // Toggle Power
        main_control = !main_control;

        // Save to Preferences
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
          fanstate = 0;
          pumpstate = 0;
          updateControlOutputs(); // Ensure outputs go LOW
          showCenteredText("POWER OFF", 2);
        }
        break;

      case '2': // Mode button
        if (main_control) {
          // Detect long press
          if (!modePressed) {
            modePressed = true;
            modePressStart = millis();
          }
        }
          break;

      case '3':
      case '4': // Adjustments
        if (main_control) {
          if (current_mode == TEMPERATURE) {
            handleAdjust(key);
            heldKey = key;
            lastRepeat = millis();
          } else if (current_mode == FAN || current_mode == PUMP || current_mode == SCHEDULER) {
            handleAdjust(key);
            lastRepeat = millis();
          }
        }
        break;
    }
  }

 // Detect long press release for mode button
  if (modePressed && keypad.getState() == RELEASED) {
    unsigned long pressDuration = millis() - modePressStart;
    modePressed = false;
    if (pressDuration > LONG_PRESS_TIME) {
      // === LONG PRESS ===
      if (current_mode == SCHEDULER) {
        timeschen = !timeschen;  // Toggle scheduler on/off
        beep(200, 1500);

        // Save to Preferences
        preferences.begin("parameters", false);
        preferences.putUInt("timeschen", timeschen);
        preferences.end();

        updateDisplay();
        publishJson();
      }
      else if (current_mode == FAN) {
        if (timeschen == 0) {
          fan_control = !fan_control;  // toggle auto/manual

            // Save to Preferences
            preferences.begin("parameters", false);
            preferences.putBool("fan_control", fan_control);
            preferences.end();

          if (!fan_control) {
            timeschen = 0;  // Disable scheduler when switching to manual mode
            updateDisplay();
          }
        }else if (timeschen == 1) {
          showCenteredText("Scheduler is Active");
          beep(200, 1500);
          delay(1000);
        }
      } else if (current_mode == PUMP) {
        pump_control = !pump_control;

        // Save to Preferences
        preferences.begin("parameters", false);
        preferences.putBool("pump_control", pump_control);
        preferences.end();

        // showCenteredText(pump_control ? "PUMP AUTO" : "PUMP MANUAL", 2);
      } else {
        showCenteredText("AUTO/MANUAL", 2);
      }
      delay(800);
      updateDisplay();
      publishJson();
    } else {
      // === SHORT PRESS ===
      if (main_control) {
        current_mode = (current_mode + 1) % NUM_MODES;
        delay(600);
        updateDisplay();
      }
    }
  }
  fanScheduler();    // run fan scheduler

  // Handle continuous key hold
  if (heldKey != NO_KEY && main_control) {
    if (keypad.getState() == HOLD && millis() - lastRepeat > repeatRate) {
      handleAdjust(heldKey);
      lastRepeat = millis();
    }
    if (keypad.getState() == RELEASED) {
      heldKey = NO_KEY;
    }
  }

  if (millis() - wifi_setting_time >= 300000 && wifi_ap_mode == true) {  // Ap mode will off after 5 Minutes 300000 = 5 * 60 * 1000 seconds
    WiFi.softAPdisconnect();
    wifi_ap_mode = false;
  }

  if ((millis() - time1) > 8000) {
    op = 0;
  }
  if ((millis() - time3) > 2000) {
    tmp_req = true;
  #ifdef DEBUG
      sensors.begin();
      int numberOfDevices = sensors.getDeviceCount();
      Serial.print("numberOfDevices = ");
      Serial.println(numberOfDevices);
      Serial.println("request temperatures-----------------------------------------");
      getTemperature();
      if (current_mode == TEMPERATURE && main_control) {
        updateDisplay();
      }
  #endif
      if (!client.connected()) {
        reconnect();
      }
      publishJson();
      time3 = millis();
    }
  loopFunction();
  client.loop();

  if (pumpstate == 1 || fanstate == 1) {
    watertankcoolsw = 2;
  }
  if (pumpstate == 0 && fanstate == 0) {
    watertankcoolsw = 0;
  }
  delay(15);
}
