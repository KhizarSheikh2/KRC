#include <IRremote.hpp>

#define DEBUG
#define MQTT_MAX_PACKET_SIZE 1024

#include "variables.h"
#include <Preferences.h>
Preferences preferences;

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <Keypad.h>

#include <OneWire.h>
#include <DallasTemperature.h>

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

WiFiClientSecure espClient;
PubSubClient client(espClient);

#include "GET_from_API.h"
#include "Servo.h"
#include "ServerForWiFiCredentials.h"
#include "OLED_Display.h"
#include "aws_cred.h"


TaskHandle_t Task1;

bool wifi_check(const char* ssid, const char* password) {
  unsigned long check_time = millis();

  WiFi.begin(ssid, password);

  Serial.println("Checking Wifi Connection");
  Serial.println("SSID: " + String(ssid));
  Serial.println("Password: " + String(password));
  Serial.print("Connecting to WiFi..");

  while (WiFi.status() != WL_CONNECTED) {

    if (millis() - check_time >= 4000) {

      is_wifi_connected = 0;
      return false;
    }
    delay(10);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected to ");
  Serial.println(ssid);

  is_wifi_connected = 1;
  return true;
}

void readSensors() {
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);
  if (temperature == DEVICE_DISCONNECTED_C) {
    temperature = -999;
    Serial.println("DS18B20 Error");
  } else {
    temp = (int16_t)temperature;  // ADD THIS — sync to control variable
    Serial.println("Temperature: " + String(temperature) + "°C");
  }
}

void Pot_Calib(int16_t min, int16_t max) {
  if (min == 0 || max == 0) {
    if (min == 0) {
      // myservo.write(servo_close_pos);
      // delay(1000);
      int16_t potmin = analogRead(34);
      delay(1000);
      preferences.begin("Pot", false);
      preferences.putInt("Min", potmin);
      preferences.end();

      Serial.print("PotMin:");
      Serial.println(potmin);
    }
    if (max == 0) {
      // myservo.write(servo_open_pos);
      // delay(1000);
      int16_t potmax = analogRead(34);
      delay(1000);
      preferences.begin("Pot", false);
      preferences.putInt("Max", potmax);
      preferences.end();

      Serial.print("potmax:");
      Serial.println(potmax);
    }
  }
}

void Beep(uint16_t beepDelay, uint8_t numberOfBeeps) {
  for (int i = 0; i < numberOfBeeps; i++) {
    digitalWrite(Buzzer_Pin, HIGH);
    delay(beepDelay);
    digitalWrite(Buzzer_Pin, LOW);
    delay(beepDelay);
  }
}

int16_t ReadPot(int16_t potPin) {

  int minValue = minval;  // at servo_close_pos
  int maxValue = maxval;  // at servo_open_pos

  // int potValue = map(analogRead(potPin), 0, 2400, 0, 4095);
  // int mappedValue = map(potValue, minValue, maxValue, 0, 100);

  int mappedValue = map(analogRead(potPin), minValue, maxValue, 0, 100);

  if (mappedValue < minValue) {
    minValue = mappedValue;
  } else if (mappedValue > maxValue) {
    maxValue = mappedValue;
  }

  // Serial.println("Pot Value");
  // Serial.println(potValue);
  Serial.println("Mapped Value+");
  Serial.println(mappedValue);

  return mappedValue;
}

void Init() {
  Serial.println("IR Receiver ready");

  // ===== LOAD CFM FROM PREFERENCES =====
  preferences.begin("CFM", false);
  start_value = preferences.getInt("cfm_min", 0);
  end_value = preferences.getInt("cfm_max", 50);
  preferences.end();

  // Fix servo_close_pos using origin constants (prevents self-corruption)
  // servo_close_pos = map(start_value, 0, 100, SERVO_CLOSE_ORIGIN, SERVO_OPEN_ORIGIN);
  servo_close_pos = map(start_value, 0, 100, servo_close_pos, servo_open_pos);

  // Sync all CFM variables from loaded end_value
  syncCFM(end_value);

  Serial.print("Init CFM loaded: start=");
  Serial.print(start_value);
  Serial.print(" end=");
  Serial.print(end_value);
  Serial.print(" CFM_max=");
  Serial.print(CFM_max);
  Serial.print(" CFM step=");
  Serial.println(CFM);

  // ===== POWER AND DAMPER STATE =====
  if (power == 1) {
    dampertsw = 1;
    dampstate = 1;
  } else {
    dampertsw = 0;
    dampstate = 0;
  }

  // ===== SYNC DISPLAY VARIABLES =====
  dmptempsp = setpointt;
  dmptemp = temp;

  // ===== LOAD TIME SCHEDULE =====
  preferences.begin("timeenable", false);
  timesch = preferences.getString("timesch", "hoursch=24|daysch=7");
  timeschen = preferences.getBool("timeschen", 0);

  for (uint8_t i = 0; i < 7; i++) {
    String days_key = "days_key" + String(i);
    days_in_num[i] = preferences.getBool(days_key.c_str(), days_in_num[i]);
  }
  for (uint8_t i = 0; i < 24; i++) {
    String hours_key = "hours_key" + String(i);
    hours_num[i] = preferences.getBool(hours_key.c_str(), hours_num[i]);
  }
  preferences.end();

  Serial.print("timeschen: ");
  Serial.println(timeschen);
  Serial.print("timesch: ");
  Serial.println(timesch);

  // ===== MQTT CONNECT =====
  if (!client.connected()) {
    reconnect();
  }

  Beep(200, 1);
  bp = 1;
}

// void Init() {
//   // IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);  // Start receiver
//   Serial.println("IR Receiver ready");

//   beca_check();

//   if (beca_power == 1) {
//     dampertsw = 1;
//     dampstate = 1;
//   } else {
//     dampertsw = 0;
//     dampstate = 0;
//   }
//   dmptempsp = setpointt;
//   dmptemp = temp_by_beca;

//   preferences.begin("CFM", false);

//   start_value = preferences.getInt("cfm_min", 0);
//   end_value = preferences.getInt("cfm_max", 50);
//   preferences.end();
//   supcfm = String(start_value) + "-" + String(end_value);

//   servo_close_pos = map(start_value, 0, 100, servo_close_pos, servo_open_pos);
//   CFM_max = map(end_value, 0, 100, servo_close_pos, servo_open_pos);

//   preferences.begin("timeenable", false);
//   timesch = preferences.getString("timesch", "hoursch=24|daysch=7");
//   timeschen = preferences.getBool("timeschen", 0);

//   for (uint8_t i = 0; i < 7; i++) {
//     String days_key = "days_key" + String(i);
//     days_in_num[i] = preferences.getBool(days_key.c_str(), days_in_num[i]);
//   }
//   for (uint8_t i = 0; i < 24; i++) {
//     String hours_key = "hours_key" + String(i);
//     hours_num[i] = preferences.getBool(hours_key.c_str(), hours_num[i]);
//   }
//   preferences.end();

//   Serial.print("timeschen: ");
//   Serial.println(timeschen);
//   if (!client.connected()) {
//     reconnect();
//   }
//   Beep(200, 1);
//   bp = 1;
// }

void oled_display() {
  unsigned long currentMillis = millis();

  // Setpoint was changed — show it briefly then revert to temperature
  if (setpointt != last_setpoint) {
    setpointStartTime = currentMillis;
    setpoint_flag = true;
    last_setpoint = setpointt;
    previousMillis = currentMillis;
    updateDisplay();  // shows SETPOINT header + value immediately
  } else if (setpoint_flag && currentMillis - setpointStartTime >= setpointDuration) {
    setpoint_flag = false;
    updateDisplay();  // revert to showing temperature
  }
}

void ControlDamper_wrt_mode() {
  oled_display();

  if (seasonsw == 0) {  // Cool mode
    if (setpointt >= temp && power == true) {
      MoveServo(servo_close_pos, 1, servo_delay);
      dampstate = 0;
    } else if (setpointt < temp && power == true) {
      MoveServo(CFM_max, 1, servo_delay);
      dampstate = 1;
    }
  } else if (seasonsw == 1) {  // Heat mode
    if (setpointt <= temp && power == true) {
      MoveServo(servo_close_pos, 1, servo_delay);
      dampstate = 0;
    } else if (setpointt > temp && power == true) {
      MoveServo(CFM_max, 1, servo_delay);
      dampstate = 1;
    }
  }

  updateDisplay();
}

// void handleAdjust(char key) {
//   if (current_mode == Temp_sc) {
//     // Adjust setpoint up/down
//     if (key == '3' && setpointt < 36) setpointt++;
//     else if (key == '4' && setpointt > 0) setpointt--;

//     // Save to Preferences
//     preferences.begin("parameters", false);
//     preferences.putInt("setpointt", setpointt);
//     preferences.end();

//     // Trigger setpoint overlay on Temp_sc screen
//     setpointStartTime = millis();
//     setpoint_flag = true;
//     last_setpoint = setpointt;
//     updateDisplay();

//   } else if (current_mode == Season_sc) {
//     if (key == '3' || key == '4') {
//       // seasonsw = (seasonsw == 0) ? 1 : 0;
//       // seasonsw = seasonsw ? 0 : 1;
//       seasonsw = !seasonsw;
//       preferences.begin("parameters", false);
//       preferences.putInt("seasonsw", seasonsw);
//       preferences.end();
//     }
//     updateDisplay();
//     publishJson();
//   } else if (current_mode == CFM_sc) {
//     // Step end_value by 10% per keypress
//     if (key == '3' && end_value < 100) end_value += 10;
//     else if (key == '4' && end_value > 10) end_value -= 10;

//     syncCFM(end_value);  // syncs CFM, CFM_max, supcfm, saves pref

//     if (power) {
//       MoveServo(CFM_max, 1, servo_delay);
//     }
//     updateDisplay();
//     publishJson();  // tell app immediately
//   }
// }

void handleAdjust(char key) {

  if (current_mode == Temp_sc) {

    if (key == '3' && setpointt < 36) setpointt++;
    else if (key == '4' && setpointt > 0) setpointt--;

    preferences.begin("parameters", false);
    preferences.putInt("setpointt", setpointt);
    preferences.end();

    setpointStartTime = millis();
    setpoint_flag = true;
    last_setpoint = setpointt;

    updateDisplay();
    publishJson();
  }

  else if (current_mode == Season_sc) {

    seasonsw = !seasonsw;

    preferences.begin("parameters", false);
    preferences.putInt("seasonsw", seasonsw);
    preferences.end();

    updateDisplay();
    publishJson();
  }

  else if (current_mode == CFM_sc) {

    if (key == '3' && end_value < 100) end_value += 10;
    else if (key == '4' && end_value > 10) end_value -= 10;

    syncCFM(end_value);

    if (power) {
      MoveServo(CFM_max, 1, servo_delay);
    }

    updateDisplay();
    publishJson();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.print("Device Name:");
  Serial.println(devicename);

  Wire.begin(I2C_SDA, I2C_SCL);
  display.begin(OLED_Address, true);
  sensors.begin();

  // ===== STARTUP SCREEN =====
  display.clearDisplay();
  showCenteredText("BITA", 2, 10);
  display.fillRect(0, 0, display.width(), display.height() / 16, SH110X_WHITE);
  display.fillRect(0, 15 * (display.height() / 16), display.width(), display.height() / 16, SH110X_WHITE);
  display.drawBitmap(1, 19, BITA_LOGO, 32, 32, SH110X_WHITE);
  display.display();

  // ===== SERVO INIT =====
  Int_Servo();
  last_pos_servo = servo_close_pos;  // safe default before pot is read

  // ===== POT CALIBRATION =====
  preferences.begin("Pot", false);
  minval = preferences.getInt("Min", 0);
  maxval = preferences.getInt("Max", 0);
  preferences.end();

  Pot_Calib(minval, maxval);

  // Only use pot reading if calibration values are valid
  if (minval != 0 && maxval != 0 && maxval > minval) {
    int pot_value = ReadPot(34);
    pot_value = constrain(pot_value, 0, 100);
    last_pos_servo = map(pot_value, 0, 100, servo_close_pos, servo_open_pos);
  }
  Serial.print("last_pos_servo initialized to: ");
  Serial.println(last_pos_servo);

  // ===== LOAD SAVED PARAMETERS =====
  preferences.begin("parameters", false);
  power = preferences.getBool("power", false);
  setpointt = preferences.getInt("setpointt", 24);
  seasonsw = preferences.getInt("seasonsw", 0);
  preferences.end();

  Serial.print("Loaded power: ");
  Serial.println(power);
  Serial.print("Loaded setpointt: ");
  Serial.println(setpointt);
  Serial.print("Loaded season: ");
  Serial.println(seasonsw);

  // ===== PIN SETUP =====
  pinMode(Buzzer_Pin, OUTPUT);
  digitalWrite(Buzzer_Pin, LOW);
  pinMode(5, OUTPUT);
  digitalWrite(5, LOW);

  Serial.print("servo_open_pos: ");
  Serial.println(servo_open_pos);
  Serial.print("servo_close_pos: ");
  Serial.println(servo_close_pos);

  // ===== KEYPAD =====
  keypad.setHoldTime(200);

  // ===== SERVO BOOT SWEEP — only if power was ON =====
  if (power) {
    MoveServo(servo_open_pos, 1, servo_delay);
    delay(1000);
    MoveServo(servo_close_pos, 1, servo_delay);
  } else {
    // Power is off — just ensure damper is physically closed
    MoveServo(servo_close_pos, 1, servo_delay);
  }

  // ===== TIME =====
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  updateTimeNow();

  // ===== WIFI =====
  setup_wifi_credentials();
  macaddress = WiFi.macAddress();

  // ===== WEB SERVER ROUTES =====
  server.on("/configure", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(200, "text/html", wifimanager);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });

  server.on("/wifi_param_by_app", HTTP_OPTIONS, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });

  server.on(
    "/wifi_param_by_app", HTTP_POST,
    [](AsyncWebServerRequest* request) {},
    NULL,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (error) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
      }
      const char* ssid = doc["ssid"];
      const char* password = doc["password"];
      if (!ssid || !password || strlen(ssid) == 0 || strlen(password) == 0) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"SSID or password missing\"}");
        return;
      }
      Serial.println("Received Wifi Credentials:");
      Serial.println("SSID: " + String(ssid));
      Serial.println("Password: " + String(password));
      wifi_check(ssid, password);
      StaticJsonDocument<128> responseDoc;
      responseDoc["status"] = "success";
      responseDoc["message"] = "WiFi parameters saved. Restarting.";
      switch (is_wifi_connected) {
        case 0: responseDoc["wifi_status"] = 0; break;
        case 1: responseDoc["wifi_status"] = 1; break;
      }
      String responseBody;
      serializeJson(responseDoc, responseBody);
      request->send(200, "application/json", responseBody);
      preferences.begin("wifi_params", false);
      preferences.putString("ssid", ssid);
      preferences.putString("password", password);
      preferences.end();
      delay(300);
      Beep(100, 2);
      if (is_wifi_connected) {
        WiFi.softAPdisconnect();
      } else {
        wifi_ap_mode = true;
        wifi_setting_time = millis();
      }
    });

  server.on("/wifi_param", HTTP_POST, [](AsyncWebServerRequest* request) {
    int params = request->params();
    Serial.println("Received POST request to /wifi_param");
    Serial.print("Number of parameters: ");
    Serial.println(params);
    String body = "";
    for (int i = 0; i < params; i++) {
      AsyncWebParameter* p = request->getParam(i);
      if (p->isPost()) {
        body += p->name() + "=" + p->value() + "&";
      }
    }
    if (body.length() > 0) {
      body.remove(body.length() - 1);
    }
    Serial.println("POST Body: " + body);
    for (int i = 0; i < params; i++) {
      AsyncWebParameter* p = request->getParam(i);
      if (p->isPost()) {
        if (p->name() == PARAM_INPUT_1) {
          ssid = p->value().c_str();
          Serial.print("SSID set to: ");
          Serial.println(ssid);
        }
        if (p->name() == PARAM_INPUT_2) {
          password = p->value().c_str();
          Serial.print("Password set to: ");
          Serial.println(password);
        }
      }
    }
    if (ssid.length() > 0 && password.length() > 0) {
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router.");
      preferences.begin("wifi_params", false);
      preferences.putString("ssid", ssid);
      preferences.putString("password", password);
      preferences.end();
      delay(1000);
      ESP.restart();
    } else {
      request->send(200, "text/plain", "Try Again. SSID or Password Invalid.");
    }
  });

  server.begin();
  Serial.println("Server Begin");

  // ===== INIT (loads CFM, timesch, connects MQTT) =====
  Init();

  // ===== IR RECEIVER — always last, after Init() =====
  // IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("Setup Complete");
}

void loop() {

  // ===== KEYPAD HANDLING =====
  char key = keypad.getKey();  // fires ONCE on initial press only

  // Track which key is being held for continuous repeat
  if (key) {
    lastHeldKey = key;
  }
  if (keypad.getState() == RELEASED || keypad.getState() == IDLE) {
    lastHeldKey = 0;
  }

  // Handle initial key press
  if (key) {
    Beep(200, 1);
    Serial.print("Key: ");
    Serial.println(key);

    switch (key) {
      case '1':  // Toggle Power
        power = !power;
        preferences.begin("parameters", false);
        preferences.putBool("power", power);
        preferences.end();
        dampertsw = power ? 1 : 0;
        dampstate = power ? 1 : 0;
        if (power) {
          showCenteredText("POWER ON", 2);
          delay(800);
          MoveServo(CFM_max, 1, servo_delay);
          updateDisplay();
        } else {
          MoveServo(servo_close_pos, 1, servo_delay);
          showCenteredText("OFF", 2);
        }
        publishJson();
        break;

      case '2':  // Cycle display mode
        if (power) {
          current_mode = (Mode)((current_mode + 1) % NUM_MODES);
          Serial.print("Mode switched to: ");
          Serial.println(current_mode);
          updateDisplay();
        }
        break;

      case '3':  // Up — handle first press immediately
      case '4':  // Down — handle first press immediately
        if (power) {
          handleAdjust(key);
          lastRepeat = millis();  // start hold repeat timer
        }
        break;
    }
  }

  // ===== CONTINUOUS HOLD REPEAT FOR KEYS 3 AND 4 =====
  // if (power && lastHeldKey != 0) {
  //   if (keypad.getState() == HOLD) {
  //     if (lastHeldKey == '3' || lastHeldKey == '4') {
  //       if (millis() - lastRepeat >= repeatDelay) {
  //         handleAdjust(lastHeldKey);
  //         lastRepeat = millis();
  //         Serial.print("Hold repeat: ");
  //         Serial.println(lastHeldKey);
  //       }
  //     }
  //   }
  // }
  if (power && lastHeldKey != 0) {
    if (keypad.getState() == HOLD) {

      // ❗ BLOCK repeat for Season mode
      if (current_mode == Season_sc) return;

      if (lastHeldKey == '3' || lastHeldKey == '4') {
        if (millis() - lastRepeat >= repeatDelay) {
          handleAdjust(lastHeldKey);
          lastRepeat = millis();
        }
      }
    }
  }

  // ===== IR HANDLING =====
  // if (IrReceiver.decode()) {
  //   uint8_t cmd = IrReceiver.decodedIRData.command;
  //   Serial.print("IR Command: ");
  //   Serial.println(cmd);

  //   // Toggle remote enable/disable
  //   if (cmd == 77) {
  //     remoteEnabled = !remoteEnabled;
  //     showCenteredText(remoteEnabled ? "RON" : "ROFF", 2);
  //     Beep(200, 1);
  //     delay(remoteEnabled ? 500 : 100);
  //     updateDisplay();
  //   }

  //   if (remoteEnabled) {
  //     if (cmd == 11) {
  //       // Temp setpoint up (staging)
  //       if (dmptempsp < 36) dmptempsp++;
  //       showCenteredText(String(dmptempsp), 2);
  //       Beep(200, 1);

  //     } else if (cmd == 14) {
  //       // Temp setpoint down (staging)
  //       if (dmptempsp > 5) dmptempsp--;
  //       showCenteredText(String(dmptempsp), 2);
  //       Beep(200, 1);

  //     } else if (cmd == 16) {
  //       // CFM down (staging)
  //       if (end_value > 0) end_value--;
  //       showCenteredText(String(end_value) + "%", 2);
  //       Beep(200, 1);
  //       new_cfm = map(end_value, 0, 100, servo_close_pos, servo_open_pos);

  //     } else if (cmd == 17) {
  //       // CFM up (staging)
  //       if (end_value < 100) end_value++;
  //       showCenteredText(String(end_value) + "%", 2);
  //       Beep(200, 1);
  //       new_cfm = map(end_value, 0, 100, servo_close_pos, servo_open_pos);

  //     } else if (cmd == 26) {
  //     seasonsw = 0;
  //     preferences.begin("parameters", false);
  //     preferences.putInt("seasonsw", seasonsw);
  //     preferences.end();
  //     showCenteredText("COOL", 2);
  //     Beep(200, 1);
  //     delay(500);
  //     updateDisplay();

  //     } else if (cmd == 66) {
  //       seasonsw = 1;
  //       preferences.begin("parameters", false);
  //       preferences.putInt("seasonsw", seasonsw);
  //       preferences.end();
  //       showCenteredText("HEAT", 2);
  //       Beep(200, 1);
  //       delay(500);
  //       updateDisplay();
  //     } else if (cmd == 13) {
  //       // OK — commit staged values
  //       bool changed = false;

  //       // Commit setpoint if changed and valid
  //       if (setpointt != dmptempsp && dmptempsp >= 5 && dmptempsp <= 36) {
  //         setpointt = dmptempsp;
  //         preferences.begin("parameters", false);
  //         preferences.putInt("setpointt", setpointt);
  //         preferences.end();
  //         changed = true;
  //       }

  //       // Commit CFM if changed
  //       if (new_cfm != CFM_max) {
  //         syncCFM(end_value);     // end_value already staged by left/right IR
  //         cfm_duration_app = millis();
  //         cfm_settings_app = true;
  //         if (power) {
  //           MoveServo(CFM_max, 1, servo_delay);
  //         }
  //         publishJson();
  //       }

  //       if (changed) {
  //         publishJson();
  //       }
  //       showCenteredText("OK", 2);
  //       Beep(100, 1);
  //       delay(500);
  //       updateDisplay();
  //     }
  //   }
  //   IrReceiver.resume();
  // }

  // ===== WIFI AP TIMEOUT =====
  if (wifi_ap_mode && millis() - wifi_setting_time >= 300000) {
    WiFi.softAPdisconnect();
    wifi_ap_mode = false;
    Serial.println("AP mode timed out and disabled");
  }

  // ===== TIME UPDATE every 5 seconds =====
  unsigned long now = millis();
  if (now - previousMillis_2 >= 5000) {
    previousMillis_2 = now;
    updateTimeNow();
    Serial.println("------------ Time Updated ------------");
  }

  // ===== SENSOR READ AND MQTT PUBLISH every 3.5 seconds =====
  now = millis();
  if (now - previousMillis_1 >= 3500) {
    previousMillis_1 = now;
    readSensors();
    Serial.println("Sensors read");

    if (!client.connected()) {
      reconnect();
    }
    if (!message_received) {
      publishJson();
    }
    message_received = false;
  }
  client.loop();

  // ===== MAIN DAMPER LOGIC =====
  if (power == false) {
    // Power OFF — ensure damper is closed
    if (dampertsw != 0) {
      dampertsw = 0;
      dampstate = 0;
      MoveServo(servo_close_pos, 1, servo_delay);
    }
    showCenteredText("OFF", 2);

  } else {
    // Power ON
    dampertsw = 1;

    // Check schedule
    bool schedule_active = false;
    if (timeschen == 1) {
      schedule_active = days_in_num[weekDay] && hours_num[current_hour];
      if (schedule_active) {
        // Only control damper if sensor is working
        if (temperature > -100) {
          ControlDamper_wrt_mode();
        } else {
          // Sensor error — fail safe close
          if (dampstate != 0) {
            dampstate = 0;
            MoveServo(servo_close_pos, 1, servo_delay);
            Serial.println("Sensor error — damper closed for safety");
            updateDisplay();
          }
        }
      } else {
        // Outside scheduled hours — close damper
        // if (dampstate == 0) {
        //   // dampstate = 0;
        MoveServo(servo_close_pos, 1, servo_delay);
        //   Serial.println("Outside schedule — damper closed");
        //   return;
        // }
        // oled_display();
      }
    } else {
      ControlDamper_wrt_mode();
    }
  }

  // ===== CFM APP OVERRIDE TIMEOUT =====
  if (cfm_settings_app && millis() - cfm_duration_app >= 10000) {
    cfm_settings_app = false;
  }
}
