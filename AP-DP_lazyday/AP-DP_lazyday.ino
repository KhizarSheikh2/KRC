#include <IRremote.hpp>

#define DEBUG
#define MQTT_MAX_PACKET_SIZE 1024

#include <Preferences.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>

WiFiClientSecure espClient;
PubSubClient client(espClient);
AsyncWebServer server(80);
Preferences preferences;

#include "variables.h"
#include "Servo.h"
#include "TFT_Display.h"
#include "WifiThread.h"
#include "aws_cred.h"


void Init() {
  preferences.begin("Pot", false);
  minval = preferences.getInt("Min", -1);
  maxval = preferences.getInt("Max", -1);
  preferences.end();

  Serial.print("\nminval: ");
  Serial.println(minval);
  Serial.print("maxval: ");
  Serial.println(maxval);

  Pot_Calib(minval, maxval);

  int last_pot_value = ReadPot(POT_PIN);
  last_pos_servo = map(last_pot_value, 0, 100, servo_close_pos, servo_open_pos);

  pinMode(Buzzer_Pin, OUTPUT);
  digitalWrite(Buzzer_Pin, LOW);

  preferences.begin("CFM", false);
  start_value = constrain(preferences.getInt("cfm_min", 0), 0, 100);
  end_value = constrain(preferences.getInt("cfm_max", 50), 0, 100);
  preferences.end();

  if (end_value < start_value) end_value = start_value;
  supcfm = String(start_value) + "-" + String(end_value);

  const int16_t mechanicalClose = servo_close_pos;
  servo_close_pos = map(start_value, 0, 100, mechanicalClose, servo_open_pos);
  CFM_max = map(end_value, 0, 100, servo_close_pos, servo_open_pos);
  CFM_min = servo_close_pos;
  new_cfm = CFM_max;
  cfm = end_value;
  last_setpoint = setpointt;

  // preferences.begin("timeenable", false);
  // timesch = preferences.getString("timesch", "hoursch=24|daysch=7");
  // timeschen = preferences.getBool("timeschen", 0);

  // for (uint8_t i = 0; i < 7; i++) {
  //   String days_key = "days_key" + String(i);
  //   days_in_num[i] = preferences.getBool(days_key.c_str(), days_in_num[i]);
  // }
  // for (uint8_t i = 0; i < 24; i++) {
  //   String hours_key = "hours_key" + String(i);
  //   hours_num[i] = preferences.getBool(hours_key.c_str(), hours_num[i]);
  // }
  // preferences.end();
  // Serial.print("timeschen: ");
  // Serial.println(timeschen);

  Beep(200, 1);
  bp = 1;
}

void setup() {
  Serial.begin(115200);
  Serial.print("Device Name:");
  Serial.println(devicename);

  initTFTDisplay();

  Int_Servo();
  Init();

  drawDashboard();
  refreshDashboard(true);

  // Serial.print("servo_open_pos: ");
  // Serial.println(servo_open_pos);
  // Serial.print("servo_close_pos: ");
  // Serial.println(servo_close_pos);
  // Serial.print("last_pos_servo: ");
  // Serial.println(last_pos_servo);

  MoveServo(servo_open_pos, 1, servo_delay);
  delay(1000);
  MoveServo(servo_close_pos, 1, servo_delay);

  if (loadCredentials(ssid, password)) {
    Serial.println("Loaded Wi-Fi credentials from memory.");
    connectToWiFi(ssid.c_str(), password.c_str());
  } else {
    Serial.println("No Wi-Fi credentials found. Starting in AP mode.");
    startAccessPoint();
  }
  server_setup();
  macaddress = WiFi.macAddress();

  if (!client.connected()) {
    reconnect();
  }


  IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);  // Start receiver
  irReceiverStarted = true;
  Serial.println("IR Receiver ready");
}

void loop() {
  if (IrReceiver.decode()) {
    // Serial.print("IR protocol: ");
    // Serial.println(IrReceiver.decodedIRData.protocol);

    Serial.print("Command: ");
    Serial.println(IrReceiver.decodedIRData.command);

    // Serial.print("Received IR code: ");
    // Serial.println(IrReceiver.decodedIRData.decodedRawData);

    if (IrReceiver.decodedIRData.command == 77) {
      Serial.print("Power On/Off");
      remoteEnabled = !remoteEnabled;
      if (remoteEnabled) {
        Serial.print("Remote Enabled");
        Beep(200, 1);
        delay(500);
      } else {
        Beep(200, 1);
        delay(100);
      }
    }

    if (remoteEnabled) {
      if (IrReceiver.decodedIRData.command == 11) {
        Serial.print("Up");
        if (setpointt < 35) setpointt++;
        Beep(200, 1);
        Send_Data_to_BECA();
      } else if (IrReceiver.decodedIRData.command == 14) {
        Serial.print("Down");
        if (setpointt > 5) setpointt--;
        Beep(200, 1);
        Send_Data_to_BECA();
      } else if (IrReceiver.decodedIRData.command == 16) {
        Serial.print("Left");
        end_value = (end_value >= 10) ? end_value - 10 : 0;
        Beep(200, 1);
        new_cfm = map(end_value, 0, 100, servo_close_pos, servo_open_pos);
      } else if (IrReceiver.decodedIRData.command == 17) {
        Serial.print("Right");
        end_value = min(100, static_cast<int>(end_value) + 10);
        Beep(200, 1);
        new_cfm = map(end_value, 0, 100, servo_close_pos, servo_open_pos);
      } else if (IrReceiver.decodedIRData.command == 26) {
        Beep(200, 1);
        Serial.print("Summer");
        seasonsw = 0;
        beca_mode = 0;
        Send_Data_to_BECA();
        // if (beca_mode != seasonsw) {
        //   if (seasonsw == 1) {
        //     beca_mode = 1;
        //     // writeSingleRegister(1, 0x02, 1);
        //   } else {
        //     beca_mode = 0;
        //     // writeSingleRegister(1, 0x02, 0);
        //   }
        // }
      } else if (IrReceiver.decodedIRData.command == 66) {
        Beep(200, 1);
        Serial.print("Winter");
        seasonsw = 1;
        beca_mode = 1;
        Send_Data_to_BECA();
        // if (beca_mode != seasonsw) {
        //   if (seasonsw == 1) {
        //     beca_mode = 1;
        //     // writeSingleRegister(1, 0x02, 1);
        //   } else {
        //     beca_mode = 0;
        //     // writeSingleRegister(1, 0x02, 0);
        //   }
        // }
      } else if (IrReceiver.decodedIRData.command == 13) {
        Serial.print("OK");
        // if (setpointt != dmptempsp) {
        //   if (dmptempsp >= 5 && dmptempsp <= 35) {
        //     setpointt = dmptempsp;
        //     // writeSingleRegister(1, 0x03, setpointt * 10);
        //   }
        // }
        if (new_cfm != CFM_max) {
          cfm_duration_app = millis();
          cfm_settings_app = true;
          CFM_max = new_cfm;
          MoveServo(CFM_max, 1, servo_delay);
        }
        Beep(100, 1);
      }
    }

    IrReceiver.resume();
  }
  // delay(100);

  // if (millis() - wifi_setting_time >= 300000 && wifi_ap_mode == true) {  // Ap mode will off after 5 Minutes 300000 = 5 * 60 * 1000 seconds
  //   WiFi.softAPdisconnect();
  //   wifi_ap_mode = false;
  // }

  /* This code is for time update after every 5 seconds*/
  unsigned long now = millis();
  // if (now - previousMillis_2 >= 5000) {
  //   updateTimeNow();
  //   previousMillis_2 = now;
  //   // Serial.println("------------------------ Time Updated ----------------------");
  // }

  // 1. DYNAMICALLY CHECK IF DEVICE IS STILL CONNECTED OVER WIFI
  if (now - lastBecaWifiTime > WIFI_TIMEOUT) {
    beca_status = false;
    if (lastTemperatureUpdateMs != 0 && now - lastTemperatureUpdateMs > WIFI_TIMEOUT) {
      invalidateTemperatureDisplay();
    }
  } else {
    beca_status = true;
  }

  /*--------------- This code is for Get data from Mobile APP -------------*/
  if (now - previousMillis_1 >= 3000) {
    if (!client.connected()) {
      reconnect();
    }

    if (!message_received) {
      //
      publishJson();
    }

    previousMillis_1 = now;
    message_received = false;
  }
  client.loop();

  // beca_check();

  if (cfmbutton != lst_cfmbutton) {
    if (bp < 11) {
      bp++;
      lst_cfmbutton = cfmbutton;
    } else {
      bp = 1;
      lst_cfmbutton = cfmbutton;
    }

    if (bp == 11 && cfm_flag == false) {  // Enter CFM Settings Mode
      Serial.println("\n\n Enter CFM Settings Mode");
      cfm_flag = true;
      save_setpointt = setpointt * 10;
      Serial.println(save_setpointt);
      cfm_duration = millis();

      preferences.begin("CFM", false);
      end_value = constrain(preferences.getInt("cfm_max", 50), 0, 100);
      preferences.end();

      setpointt = (end_value / 10) + 20;
      Serial.println(setpointt);
      refreshDashboard();
      Beep(50, 2);
      // writeSingleRegister(MODBUS_SLAVE_ID, 0x03, setpointt * 10);
      bp = 1;

    }                                        //
    else if (bp == 6 && cfm_flag == true) {  // Save CFM
      Serial.println("\n\n Save CFM");

      last_setpoint = save_setpointt / 10;
      setpointt = last_setpoint;
      Serial.println(setpointt);
      Beep(1000, 1);
      Serial.println(end_value);
      preferences.begin("CFM", false);
      preferences.putInt("cfm_max", end_value);
      preferences.end();
      supcfm = String(start_value) + "-" + String(end_value);
      bp = 1;
      // CFM_s = cfm_max;
      previousMillis_3 = millis();
      // writeSingleRegister(MODBUS_SLAVE_ID, 0x03, save_setpointt);
      cfm_flag = false;
    }
  }

  if (cfm_flag == true && millis() - cfm_duration >= 10000) {  // Failure to save CFM
    previousMillis_3 = millis();

    cfm_flag = false;
    last_setpoint = save_setpointt / 10;
    setpointt = last_setpoint;
    preferences.begin("CFM", false);
    end_value = constrain(preferences.getInt("cfm_max", 50), 0, 100);
    preferences.end();
    CFM_max = map(end_value, 0, 100, servo_close_pos, servo_open_pos);
    new_cfm = CFM_max;
    MoveServo(CFM_max, 1, servo_delay);

    Beep(50, 4);

    // writeSingleRegister(MODBUS_SLAVE_ID, 0x03, save_setpointt);
    cfm_duration = millis();
    bp = 1;
  }
  //
  else if ((cfm_flag == false && millis() - cfm_button_time >= 20000) && bp != 1) {  // Fail to enter CFM Mode
    cfm_flag = false;
    if (bp > 2) {
      Beep(50, 3);
    }
    cfm_button_time = millis();
    bp = 1;
  }

  if (cfm_flag == true && beca_power == true) {
    if (setpointt > 20 && setpointt < 31) {
      end_value = (setpointt - 20) * 10;
      CFM_max = map(end_value, 0, 100, servo_close_pos, servo_open_pos);
      MoveServo(CFM_max, 1, servo_delay);
    }  //
    else if (setpointt < 21) {
      end_value = 10;
      CFM_max = map(end_value, 0, 100, servo_close_pos, servo_open_pos);
      MoveServo(CFM_max, 1, servo_delay);
    }

    // Track CFM-setting changes without replacing the persistent dashboard.
    if (setpointt != last_setpoint) {
      cfm_duration = millis();
      last_setpoint = setpointt;
      refreshDashboard();
    }
  } else {
    /*-----------------Main Logic---------------------------------*/
    if (!cfm_settings_app) {
      if (beca_power == 0) {  // BECA OFF
        dampertsw = 0;
        dampstate = 0;
        MoveServo(servo_close_pos, 1, servo_delay);

      } else if (beca_power == 1) {
        if (dampertsw != 1) {  // No Auto Mode
          dampertsw = 1;
          // dampstate = 1;
          ControlDamper_wrt_mode();
        }                           //
        else if (dampertsw == 1) {  // Auto Mode
          if (timeschen == 1) {     // Schedule Enable

            if (days_in_num[weekDay] == 1) {       // Matched Day Number
              if (hours_num[current_hour] == 1) {  // Matched Hour Number
                // dampstate = 1;
                ControlDamper_wrt_mode();
              }       //
              else {  // Hour Doesnot Matched
                dampstate = 0;
                refreshDashboard();
                MoveServo(servo_close_pos, 1, servo_delay);
              }
            }       //
            else {  // Day Doesnot Matched

              dampstate = 0;
              refreshDashboard();
              MoveServo(servo_close_pos, 1, servo_delay);
            }
          }       //
          else {  // Schedule Disable
            // dampstate = 1;
            ControlDamper_wrt_mode();
          }
        }
      }
    }  //
    else {
      if (millis() - cfm_duration_app >= 10000) {
        cfm_settings_app = false;
      }
    }
  }

  // Keep the five dashboard values synchronized with live control data.
  refreshDashboard();
}


void ControlDamper_wrt_mode() {
  refreshDashboard();

  /*---------------------- Cool Mode ---------------------*/
  if (beca_mode == 0 || beca_mode == 2) {
    if (setpointt >= temp_by_beca && beca_power == true) {
      MoveServo(servo_close_pos, 1, servo_delay);
      dampstate = 0;
    } else if (setpointt < temp_by_beca && beca_power == true) {
      MoveServo(CFM_max, 1, servo_delay);
      dampstate = 1;
    }
  }
  /*---------------------- Heat Mode ---------------------*/
  else if (beca_mode == 1) {
    if (setpointt <= temp_by_beca && beca_power == true) {
      MoveServo(servo_close_pos, 1, servo_delay);
      dampstate = 0;
    } else if (setpointt > temp_by_beca && beca_power == true) {
      MoveServo(CFM_max, 1, servo_delay);
      dampstate = 1;
    }
  }
}

void Pot_Calib(int16_t storedMin, int16_t storedMax) {
  if (storedMin >= 0 && storedMax >= 0 && abs(storedMax - storedMin) >= 20) {
    minval = storedMin;
    maxval = storedMax;
    return;
  }

  Serial.println("Pot calibration missing/invalid - calibrating...");

  if (!myservo.attached()) {
    myservo.setPeriodHertz(50);
    myservo.attach(ServoPin);
  }

  myservo.write(servo_close_pos);
  delay(700);
  long closeSum = 0;
  for (uint8_t i = 0; i < 10; i++) {
    closeSum += analogRead(POT_PIN);
    delay(5);
  }
  minval = closeSum / 10;

  myservo.write(servo_open_pos);
  delay(900);
  long openSum = 0;
  for (uint8_t i = 0; i < 10; i++) {
    openSum += analogRead(POT_PIN);
    delay(5);
  }
  maxval = openSum / 10;

  // Leave the mechanism at the known closed endpoint after calibration.
  myservo.write(servo_close_pos);
  delay(700);

  if (abs(maxval - minval) < 20) {
    Serial.println("WARNING: Pot feedback range is too small. Using safe closed-position fallback.");
    minval = 0;
    maxval = 0;
    return;
  }

  preferences.begin("Pot", false);
  preferences.putInt("Min", minval);
  preferences.putInt("Max", maxval);
  preferences.end();

  Serial.print("Pot close ADC: ");
  Serial.println(minval);
  Serial.print("Pot open ADC: ");
  Serial.println(maxval);
}

void Beep(uint16_t beepDelay, uint8_t numberOfBeeps) {
  for (uint8_t i = 0; i < numberOfBeeps; i++) {
    digitalWrite(Buzzer_Pin, HIGH);
    delay(beepDelay);
    digitalWrite(Buzzer_Pin, LOW);
    delay(beepDelay);
  }
}

int16_t ReadPot(int16_t potPin) {
  long potValue = 0;
  for (uint8_t i = 0; i < 10; i++) {
    potValue += analogRead(potPin);
  }
  potValue /= 10;

  Serial.print("Pot Value: ");
  Serial.println(potValue);

  // Safe fallback if no usable potentiometer calibration exists.
  if (minval == maxval || abs(maxval - minval) < 20) {
    Serial.println("Pot calibration invalid; assuming closed position.");
    return 0;
  }

  // constrain() requires low <= high, but map() supports either ADC polarity.
  const int16_t adcLow = min(minval, maxval);
  const int16_t adcHigh = max(minval, maxval);
  potValue = constrain(potValue, adcLow, adcHigh);

  int mappedValue = map(potValue, minval, maxval, 0, 100);
  mappedValue = constrain(mappedValue, 0, 100);

  Serial.print("Mapped Pot %: ");
  Serial.println(mappedValue);
  return mappedValue;
}
