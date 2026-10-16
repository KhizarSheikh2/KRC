#include <IRremote.hpp>

#define DEBUG
#define MQTT_MAX_PACKET_SIZE 1024

#include <Preferences.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>

WiFiClientSecure espClient;
PubSubClient client(espClient);
AsyncWebServer server(80);
Preferences preferences;

#include "variables.h"
// #include "RES_MODBUS_V1_3.h"
// #include "GET_from_API.h"
#include "Servo.h"
// #include "ServerForWiFiCredentials.h"
#include "OLED_Display.h"
#include "WifiThread.h"
#include "aws_cred.h"
#include "esp_task_wdt.h"


void Init() {
  Wire.begin(21, 22);
  if (!display.begin(OLED_Address, true)) {
    Serial.println("OLED init failed");
  }

  display.clearDisplay();
  display_on_OLED(&FreeMonoBold9pt7b, 2, 37, 43, "BITA");

  display.fillRect(0, 0, display.width(), display.height() / 16, SH110X_WHITE);
  display.fillRect(0, 15 * (display.height() / 16), display.width(), display.height() / 16, SH110X_WHITE);
  display.drawBitmap(1, 19, BITA_LOGO, 32, 32, SH110X_WHITE);
  display.display();

  preferences.begin("Pot", false);
  minval = preferences.getInt("Min", 0);
  maxval = preferences.getInt("Max", 0);
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

  // RS485Serial.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

  preferences.begin("CFM", false);
  start_value = preferences.getInt("cfm_min", 0);
  end_value = preferences.getInt("cfm_max", 50);
  preferences.end();
  supcfm = String(start_value) + "-" + String(end_value);

  servo_close_pos = map(start_value, 0, 100, servo_close_pos, servo_open_pos);
  CFM_max = map(end_value, 0, 100, servo_close_pos, servo_open_pos);

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
  esp_task_wdt_init(10, true);  // 10s timeout

  Serial.begin(115200);
  Serial.print("Device Name:");
  Serial.println(devicename);

  Int_Servo();
  Init();

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
        display.clearDisplay();
        display_on_OLED(&Org_01, 6, 5, 43, "RON");
        Beep(200, 1);
        delay(500);
      } else {
        display.clearDisplay();
        display_on_OLED(&Org_01, 6, 5, 43, "ROFF");
        Beep(200, 1);
        delay(100);
      }
    }

    if (remoteEnabled) {
      if (IrReceiver.decodedIRData.command == 11) {
        Serial.print("Up");
        if (dmptempsp < 35) dmptempsp++;
        display.clearDisplay();
        display_on_OLED(&Org_01, 6, 5, 43, String(dmptempsp));
        Beep(200, 1);
      } else if (IrReceiver.decodedIRData.command == 14) {
        Serial.print("Down");
        if (dmptempsp > 0) dmptempsp--;
        display.clearDisplay();
        display_on_OLED(&Org_01, 6, 5, 43, String(dmptempsp));
        Beep(200, 1);
      } else if (IrReceiver.decodedIRData.command == 16) {
        Serial.print("Left");
        if (end_value > 0) end_value -= 10;
        display.clearDisplay();
        display_on_OLED(&Org_01, 6, 5, 43, String(end_value));
        Beep(200, 1);
        new_cfm = map(end_value, 0, 100, servo_close_pos, servo_open_pos);
      } else if (IrReceiver.decodedIRData.command == 17) {
        Serial.print("Right");
        if (end_value < 100) end_value += 10;
        display.clearDisplay();
        display_on_OLED(&Org_01, 6, 5, 43, String(end_value));
        Beep(200, 1);
        new_cfm = map(end_value, 0, 100, servo_close_pos, servo_open_pos);
      } else if (IrReceiver.decodedIRData.command == 26) {
        Beep(200, 1);
        Serial.print("Summer");
        seasonsw = 0;
        beca_mode = 0;
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
        if (setpointt != dmptempsp) {
          if (dmptempsp >= 5 || dmptempsp < 36) {
            setpointt = dmptempsp;
            // writeSingleRegister(1, 0x03, setpointt * 10);
          }
        }
        if (new_cfm != CFM_max) {
          cfm_duration_app = millis();
          cfm_settings_app = true;
          CFM_max = new_cfm;
          MoveServo(CFM_max, 1, servo_delay);
        }
        display.clearDisplay();
        display_on_OLED(&Org_01, 6, 5, 43, "OK");
        Beep(100, 1);
        Send_Data_to_BECA();
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
    beca_status = false;  // It has been more than 10 seconds since the last POST
  } else {
    beca_status = true;  // Wi-Fi link is active
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
      end_value = preferences.getInt("cfm_max", cfm);
      preferences.end();

      setpointt = (end_value / 10) + 20;
      Serial.println(setpointt);
      if (setpointt > 20) {
        if (setpointt == 30) {
          display.clearDisplay();
          display_on_OLED(&Org_01, 5, 1, 40, "100%");
        } else if (setpointt > 30) {
          display.clearDisplay();
          display_on_OLED(&Org_01, 5, 1, 40, "100%");
          // writeSingleRegister(MODBUS_SLAVE_ID, 0x03, 30 * 10);
        } else {
          int8_t cfm_perc = setpointt % 10;
          display.clearDisplay();
          display_on_OLED(&Org_01, 5, 10, 40, String(cfm_perc) + "0 %");
        }
      }
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
    end_value = preferences.getInt("cfm_max", 50);
    preferences.end();

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
      CFM_max = map(cfm, 0, 100, servo_close_pos, servo_open_pos);
      MoveServo(CFM_max, 1, servo_delay);
    }

    unsigned long currentMillis = millis();
    // Check if the value of register[3] has changed
    if (setpointt != last_setpoint) {
      cfm_duration = millis();
      if (setpointt > 20) {
        if (setpointt == 30) {
          display.clearDisplay();
          display_on_OLED(&Org_01, 5, 1, 40, "100%");

        } else if (setpointt > 30) {
          int8_t cfm_perc = setpointt % 10;
          display.clearDisplay();
          display_on_OLED(&Org_01, 5, 1, 40, "100%");
          // writeSingleRegister(MODBUS_SLAVE_ID, 0x03, 30 * 10);
        } else {

          int8_t cfm_perc = setpointt % 10;
          // Serial.println(cfm_perc);
          display.clearDisplay();
          display_on_OLED(&Org_01, 5, 10, 40, String(cfm_perc) + "0 %");
        }
      } else {  // Write 21 on BECA if get lesser than 21

        display.clearDisplay();
        display_on_OLED(&Org_01, 5, 10, 40, "10 %");

        // writeSingleRegister(MODBUS_SLAVE_ID, 0x03, 21 * 10);
      }
      last_setpoint = setpointt;
      previousMillis = currentMillis;
    }
  } 
  else {
    /*-----------------Main Logic---------------------------------*/
    if (!cfm_settings_app) {
      if (beca_power == 0) {  // BECA OFF
        dampertsw = 0;
        dampstate = 0;
        MoveServo(servo_close_pos, 1, servo_delay);
        if (show_time == false) {
          display.clearDisplay();
          display_on_OLED(&Org_01, 6, 10, 43, "OFF");
        }


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
                uint16_t currentMillis = millis();
                ControlDamper_wrt_mode();
              }       //
              else {  // Hour Doesnot Matched
                dampstate = 0;
                oled_display();
                MoveServo(servo_close_pos, 1, servo_delay);
              }
            }       //
            else {  // Day Doesnot Matched

              dampstate = 0;
              oled_display();
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
}

// void beca_check() {
//   beca_status = readHoldingRegisters(MODBUS_SLAVE_ID, MODBUS_REGISTER_ADDRESS, MODBUS_REGISTER_COUNT, registers);
//   if (prev_beca_status != beca_status) {
//     if (prev_beca_status == 0) {
//       int16_t x2[3] = { 10, 80, 95 };
//       int16_t y2[3] = { 43, 18, 43 };
//       Beep(600, 1);
//       bp = 1;

//       Serial.println("Beca Status Become True");
//       previousMillis_3 = millis();
//       if (registers[0] != beca_power_saved) {
//         writeSingleRegister(MODBUS_SLAVE_ID, 0x00, beca_power_saved);
//       }
//       if (registers[2] != beca_mode) {
//         writeSingleRegister(MODBUS_SLAVE_ID, 0x02, beca_mode);
//       }
//       if (registers[3] / 10 != setpointt_saved) {
//         writeSingleRegister(MODBUS_SLAVE_ID, 0x03, setpointt_saved * 10);
//       }
//       cfmbutton = 0;
//       display.clearDisplay();
//       display_temp_on_OLED(&Org_01, 6, 1, x2, y2, String(registers[8] / 10), "o", "C");
//     }
//     prev_beca_status = beca_status;
//   }

//   if (beca_status) {
//     cfmbutton = registers[1];
//     beca_power = registers[0];
//     beca_mode = registers[2];
//     setpointt = registers[3] / 10;
//     temp_by_beca = registers[8] / 10;
//   } else {
//     Beep(150, 5);
//     bp = 1;
//     setpointt_saved = setpointt;
//     beca_power_saved = beca_power;

//     cfmbutton = 0;
//     beca_power = 0;
//     // beca_mode = 0;
//     setpointt = 0;
//     temp_by_beca = 0;


//     beca_status = readHoldingRegisters(MODBUS_SLAVE_ID, MODBUS_REGISTER_ADDRESS, MODBUS_REGISTER_COUNT, registers);

//     while (beca_status == false) {
//       beca_status = readHoldingRegisters(MODBUS_SLAVE_ID, MODBUS_REGISTER_ADDRESS, MODBUS_REGISTER_COUNT, registers);

//       client.loop();

//       display.clearDisplay();
//       display_on_OLED(&Org_01, 5, 2, 43, "Error");


//       /*--------------- This code is for Get data from Mobile APP -------------*/
//       wait_time = millis();
//       if (wait_time - previousMillis_1 >= 5000) {
//         previousMillis_1 = wait_time;
//         if (!client.connected()) {
//           reconnect();
//         }

//         if (!message_received) {
//           publishJson();
//         }
//         /* ----------------------------------------------------------------------------------------------- */
//       }

//       if (dampertsw == 1) {
//         beca_power = 1;
//         dampertsw = 1;
//         dampstate = 1;
//         setpointt = 0;
//         temp_by_beca = 0;
//         MoveServo(CFM_max, 1, servo_delay);
//       } else {
//         beca_power = 0;
//         dampertsw = 0;
//         dampstate = 0;
//         setpointt = 0;
//         temp_by_beca = 0;
//         MoveServo(servo_close_pos, 1, servo_delay);
//       }

//       /*------------------------Update Time---------------------------------------*/

//       wait_update_time = millis();
//       if (wait_update_time - previousMillis_2 >= 60000) {
//         updateTimeNow();
//         previousMillis_2 = wait_update_time;
//         Serial.println("------------------------ Time Updated ----------------------");
//       }

//       if (millis() - previousMillis_3 >= 10000 && cfm_flag == false) {
//         bool show_flag = true;
//         if (show_flag == true && show_time == false) {
//           previousMillis_4 = millis();
//           show_flag = false;
//         }
//       }
//     }
//   }

// }

void oled_display() {
  unsigned long currentMillis = millis();
  int16_t x[3] = { 10, 80, 95 };
  int16_t y[3] = { 43, 18, 43 };

  if (setpointt != last_setpoint) {
    previousMillis_3 = millis();

    display.clearDisplay();
    if (setpointt < 10) {
      display_temp_on_OLED(&Org_01, 9, 1, x, y, "0" + String(setpointt), "", "");
    } else {
      display_temp_on_OLED(&Org_01, 9, 1, x, y, String(setpointt), "", "");
    }

    setpoint_flag = true;

    last_setpoint = setpointt;
    previousMillis = currentMillis;
    // show_time = false;

  } else if ((currentMillis - previousMillis >= interval)) {
    display.clearDisplay();
    display_temp_on_OLED(&Org_01, 6, 1, x, y, String(temp_by_beca), "o", "C");
    setpoint_flag = false;
  }
}

void ControlDamper_wrt_mode() {
  oled_display();

  /*---------------------- Cool Mode ---------------------*/
  if (beca_mode == 0 || beca_mode == 2) {
    if (last_setpoint >= temp_by_beca && beca_power == true) {
      MoveServo(servo_close_pos, 1, servo_delay);
      dampstate = 0;
    } else if (last_setpoint < temp_by_beca && beca_power == true) {
      MoveServo(CFM_max, 1, servo_delay);
      dampstate = 1;
    }
  }
  /*---------------------- Heat Mode ---------------------*/
  else if (beca_mode == 1) {
    if (last_setpoint <= temp_by_beca && beca_power == true) {
      MoveServo(servo_close_pos, 1, servo_delay);
      dampstate = 0;
    } else if (last_setpoint > temp_by_beca && beca_power == true) {
      MoveServo(CFM_max, 1, servo_delay);
      dampstate = 1;
    }
  }
}

void Pot_Calib(int16_t min, int16_t max) {
  preferences.begin("Pot", false);
  if (min == 0) {
    int16_t potmin = analogRead(POT_PIN);
    preferences.putInt("Min", potmin);
    Serial.print("PotMin:");
    Serial.println(potmin);
  }

  if (max == 0) {
    int16_t potmax = analogRead(POT_PIN);
    preferences.putInt("Max", potmax);
    Serial.print("potmax:");
    Serial.println(potmax);
  }
  preferences.end();
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
  long potValue = 0;
  for (int i = 0; i < 10; i++) {
    potValue += analogRead(potPin);
  }
  potValue /= 10;
  Serial.println("Pot Value");
  Serial.println(potValue);

  potValue = constrain(potValue, minValue, maxValue);
  int mappedValue = map(potValue, minValue, maxValue, 0, 100);

  // if (mappedValue < minValue) {
  //   minValue = mappedValue;
  // } else if (mappedValue > maxValue) {
  //   maxValue = mappedValue;
  // }

  Serial.println("Mapped Value+");
  Serial.println(mappedValue);
  return mappedValue;
}
