#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "esp_task_wdt.h"

#include "Def.h"
#include "aws_cred.h"
#include "temp.h"
#include "OLED.h"
#include "WifiThread.h"
#include "MQTT.h"

void setup() {
  esp_task_wdt_init(10, true);  // 10s timeout  
  Serial.begin(115200);

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(OLED_ADDRESS, true)) {
    Serial.println("OLED init failed");
    while (1)
      ;
  }
  keypad.setHoldTime(200);
  showStartupScreen();
  sensors.begin();

  pinMode(EVAP_COOL_SYSTEM, OUTPUT);
  pinMode(AUTO_WASH_SYSTEM, OUTPUT);
  pinMode(UNIT_COMMAND, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(EVAP_COOL_SYSTEM, LOW);
  digitalWrite(AUTO_WASH_SYSTEM, LOW);
  digitalWrite(UNIT_COMMAND, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("\n\nbooting up device...\n\n");
  Serial.print("devicename: ");
  Serial.println(devicename);
  DEVICE_INIT();

  //=========================================//
  macaddress = WiFi.macAddress();
  if (loadCredentials(ssid, password)) {
    Serial.println("Loaded Wi-Fi credentials from memory.");
    connectToWiFi(ssid.c_str(), password.c_str());
  } else {
    Serial.println("No Wi-Fi credentials found. Starting in AP mode.");
    startAccessPoint();
  }
  server_setup();

  current_mode = E_TEMPERATURE;
  updateDisplay();
}

void beep(int duration = BUZZER_DURATION, int freq = BUZZER_FREQ) {
  tone(BUZZER_PIN, freq, duration);  // Non-blocking tone
  delay(10);                         // Small delay to ensure it triggers
}

void Poweroff() {
    EVAP_COOL_SYSTEM_SWITCH = 0;
    AUTO_WASH_SYSTEM_SWITCH = 0;
    UNIT_COMMAND_SWITCH = 0;

    // Reset status
    EVAP_COOL_SYSTEM_STATUS = false;
    AUTO_WASH_SYSTEM_STATUS = false;
    UNIT_COMMAND_STATUS = false;

    updateControlOutputs();   // turn all relays OFF

    showCenteredText("POWER OFF", 2);      // Small delay to ensure it triggers
}

void loop() {
  static bool wasPoweredOff = false;
  char key = keypad.getKey();

  if (key) {
    beep();
    Serial.print("Key: ");
    Serial.println(key);

    switch (key) {
      case '1':  // Toggle Power
        PowerState = !PowerState;

        preferences.begin("values", false);
        preferences.putBool("PowerState", PowerState);
        preferences.end();

        if (PowerState) {

          showCenteredText("POWER ON", 2);

          current_mode = E_TEMPERATURE;

          updateControlOutputs();  // restore relay states

          updateDisplay();

        } else {

          Poweroff();
        }

        //Immediately send MQTT update
        if (client.connected()) {
          publishJson();
        }

        break;

      case '2':  // Mode button
        if (PowerState) {
          current_mode = (current_mode + 1) % NUM_MODES;
          showModeScreen();
          delay(500);
          updateDisplay();
        }
        break;

      case '3':
      case '4':  // Adjustments
        if (PowerState) {
          handleAdjust(key);
          heldKey = key;
          lastRepeat = millis();
        }
        break;
    }
  }

  if (!PowerState) {
    if (!wasPoweredOff) {
      Poweroff();               // runs ONCE only
      wasPoweredOff = true;
    }

    // Publish at interval, not every loop
    if (millis() - wait_time >= MQTT_INTERVAL) {
      if (!client.connected()) reconnect();
      if (client.connected()) publishJson();
      wait_time = millis();
    }

    client.loop();
    return;
  }
  wasPoweredOff = false;  // resets when power is back ON

  ///////////////////////////////////////////////////////////
  // Periodic sensor read
  if (millis() - pmillis >= SENSOR_READ_INTERVAL) {
    sensors.begin();
    sensors.setWaitForConversion(false);
    sensors.requestTemperatures();
    sensors.setWaitForConversion(true);

    numberOfDevices = sensors.getDeviceCount();

    // Default all sensors to disconnected
    dischargeTemp = SENSOR_DISCONNECTED;
    SupplyTemp = SENSOR_DISCONNECTED;
    ReturnTemp = SENSOR_DISCONNECTED;

    for (int i = 0; i < numberOfDevices; i++) {
      if (!sensors.getAddress(tempSensorAddresses[i], i)) {
        Serial.println("Unable to find address for Device " + String(i));
        continue;
      }

      float temperature = sensors.getTempC(tempSensorAddresses[i]);
      if (temperature < 0) continue;

      if (temp1Assigned && compareAddresses(tempSensorAddresses[i], temp1Address)) {
        dischargeTemp = temperature + discharge_temp_offset;
      }

      else if (temp2Assigned && compareAddresses(tempSensorAddresses[i], temp2Address)) {
        SupplyTemp = temperature + supply_temp_offset;
      }

      else if (temp3Assigned && compareAddresses(tempSensorAddresses[i], temp3Address)) {
        ReturnTemp = temperature + return_air_temp_offset;
      }
    }

    if (numberOfDevices == 0) {
      Serial.println("No Sensor Found!");
    }

    updateDisplay();
    pmillis = millis();

    // ===== Temperature Alerts =====
    if (dischargeTemp >= dischargeSp) {
      dischargeTempAlarm = true;
    } else {
      dischargeTempAlarm = false;
    }

    if (ReturnTemp > ReturnSp) {
      returnTempAlarm = true;
    } else {
      returnTempAlarm = false;
    }
  }

  ///////////////////////////////////////////////////////////

  if (MODE_SWITCH == 0) {  // MANUAL MODE

    // Ensure scheduler disabled in manual mode
    if (MODE_SWITCH == 0 && timeschen != 0) {
      timeschen = 0;
      tmatched = 0;
      preferences.begin("timeenable", false);
      preferences.putInt("timeschen", timeschen);
      preferences.end();
    }

    // ===== EVAP COOL SYSTEM =====
    if (EVAP_COOL_SYSTEM_SWITCH == 1 && !EVAP_COOL_SYSTEM_STATUS) {
      EVAP_COOL_SYSTEM_STATUS = true;
      digitalWrite(EVAP_COOL_SYSTEM, HIGH);
    } else if (EVAP_COOL_SYSTEM_SWITCH == 0 && EVAP_COOL_SYSTEM_STATUS) {
      EVAP_COOL_SYSTEM_STATUS = false;
      digitalWrite(EVAP_COOL_SYSTEM, LOW);
    }

    // ===== AUTO WASH SYSTEM (R2) =====
    if (AUTO_WASH_SYSTEM_SWITCH == 1 && !AUTO_WASH_SYSTEM_STATUS) {

      AUTO_WASH_SYSTEM_STATUS = true;
      digitalWrite(AUTO_WASH_SYSTEM, HIGH);

      // INTERLOCK
      UNIT_COMMAND_STATUS = false;
      UNIT_COMMAND_SWITCH = 0;
      digitalWrite(UNIT_COMMAND, LOW);
    }

    else if (AUTO_WASH_SYSTEM_SWITCH == 0 && AUTO_WASH_SYSTEM_STATUS) {

      AUTO_WASH_SYSTEM_STATUS = false;
      digitalWrite(AUTO_WASH_SYSTEM, LOW);
    }

    // ===== UNIT COMMAND (R3) =====
    // Interlock: if auto wash is ON, force unit command OFF
    if (AUTO_WASH_SYSTEM_SWITCH == 1) {
      UNIT_COMMAND_SWITCH = 0;
    }

    if (UNIT_COMMAND_SWITCH == 1 && !UNIT_COMMAND_STATUS) {
      UNIT_COMMAND_STATUS = true;
      digitalWrite(UNIT_COMMAND, HIGH);
    } else if (UNIT_COMMAND_SWITCH == 0 && UNIT_COMMAND_STATUS) {
      UNIT_COMMAND_STATUS = false;
      digitalWrite(UNIT_COMMAND, LOW);
    }

  } else {  // AUTO MODE

    runScheduler();

    // ===== AUTO WASH SCHEDULE =====
    if (tmatched && timeschen) {

      // Start Auto Wash
      AUTO_WASH_SYSTEM_STATUS = true;
      digitalWrite(AUTO_WASH_SYSTEM, HIGH);  // R2 ON

      // Cooling must stop during wash
      UNIT_COMMAND_STATUS = false;
      UNIT_COMMAND_SWITCH = 0;
      digitalWrite(UNIT_COMMAND, LOW);

    } else {

      // Cooling on!
      if (!UNIT_COMMAND_STATUS) {
        UNIT_COMMAND_STATUS = true;
        UNIT_COMMAND_SWITCH = 1;
        digitalWrite(UNIT_COMMAND, HIGH);
      }

      // Stop Auto Wash
      AUTO_WASH_SYSTEM_STATUS = false;
      digitalWrite(AUTO_WASH_SYSTEM, LOW);
    }
    // ===== COOLING CONTROL =====
    if (dischargeTemp >= dischargeSp) {
      EVAP_COOL_SYSTEM_STATUS = true;
      EVAP_COOL_SYSTEM_SWITCH = 1;
      digitalWrite(EVAP_COOL_SYSTEM, HIGH);
      SYSTEM_STATUS = 1;
    } else {
      EVAP_COOL_SYSTEM_STATUS = false;
      EVAP_COOL_SYSTEM_SWITCH = 0;
      digitalWrite(EVAP_COOL_SYSTEM, LOW);
      SYSTEM_STATUS = 0;
    }

    // ===== RETURN AIR ALARM =====
    if (ReturnTemp > ReturnSp) {
      R_A_TEMP_Alarm = true;
    } else {
      R_A_TEMP_Alarm = false;
    }
  }

  ///////////////////////////////////////////////////////////
  if (millis() - wait_time >= MQTT_INTERVAL) {
    if (!client.connected()) {
      reconnect();
    }

    if (message_received || message_received_config) {
      message_received = false;
      message_received_config = false;
      if (client.connected()) {
        publishJson();
      }
    } else if (WiFi.status() == WL_CONNECTED) {
      publishJson();
      temp_sensor_select_publish();
    }

    wait_time = millis();
  }
  client.loop();

  /* This code is for time update after every 5 seconds*/
  if (millis() - wait_update_time >= 5000) {
    updateTimeNow();
    wait_update_time = millis();
  }
}
////////////////////Scheduler///////////////////

void runScheduler()
{
    if(!timeschen)
        return;

    if(days_in_num[weekDay] && hours_num[current_hour])
    {
        tmatched = true;
    }
    else
    {
        tmatched = false;
    }
}

void parse_schedule(String timesch)
{
    memset(days_in_num,0,sizeof(days_in_num));
    memset(hours_num,0,sizeof(hours_num));

    int pipeIndex = timesch.indexOf('|');
    if(pipeIndex==-1) return;

    String hoursPart = timesch.substring(0,pipeIndex);
    String daysPart  = timesch.substring(pipeIndex+1);

    // HOURS
    if(hoursPart.startsWith("hoursch="))
    {
        String hstr = hoursPart.substring(8);

        if(hstr=="24")
        {
            for(int i=0;i<24;i++) hours_num[i]=1;
        }
        else
        {
            int start=0;
            int comma = hstr.indexOf(',',start);

            while(comma!=-1)
            {
                int h = hstr.substring(start,comma).toInt();
                if(h>=0 && h<24) hours_num[h]=1;

                start = comma+1;
                comma = hstr.indexOf(',',start);
            }

            int h = hstr.substring(start).toInt();
            if(h>=0 && h<24) hours_num[h]=1;
        }
    }

    // DAYS
    if(daysPart.startsWith("daysch="))
    {
        String dstr = daysPart.substring(7);

        if(dstr=="7")
        {
            for(int i=0;i<7;i++) days_in_num[i]=1;
        }
        else
        {
            int start=0;
            int comma=dstr.indexOf(',',start);

            while(comma!=-1)
            {
                int d = dstr.substring(start,comma).toInt();
                if(d>=0 && d<7) days_in_num[d]=1;

                start = comma+1;
                comma = dstr.indexOf(',',start);
            }

            int d = dstr.substring(start).toInt();
            if(d>=0 && d<7) days_in_num[d]=1;
        }
    }
}



void updateTimeNow() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    if (WiFi.status() == WL_CONNECTED) {
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    }
    return;
  }
  // Format the time into WMMDDYYHHMMSS
  char buffer[16];

  // Convert weekDayday to numeric value (1=Monday, 2=Tuesday, ...)
  weekDay = timeinfo.tm_wday - 1;  // tm_wday is 0=Sunday, 1=Monday, ..., 6=Saturday
  current_hour = timeinfo.tm_hour;

  // Format the string with leading zeroes if needed
  snprintf(buffer, sizeof(buffer), "%02d%02d%02d%02d%02d%02d",
           timeinfo.tm_mday,
           timeinfo.tm_mon + 1,     // Month is 0-based, so add 1
           timeinfo.tm_year % 100,  // Year is years since 1900, so take the last two digits
           timeinfo.tm_hour,
           timeinfo.tm_min,
           timeinfo.tm_sec);

  timenow = String(buffer);
  // Serial.print("Sending timenow: ");
  // Serial.println(timenow);
}

// void hour_day(String hours_ch, String days_ch) {
//   for (uint8_t i = 0; i < 24; i++) {
//     hours_num[i] = false;
//   }
//   for (uint8_t i = 0; i < 7; i++) {
//     days_in_num[i] = false;
//   }

//   if (hours_ch.length() > 0 && days_ch.length() > 0) {
//     int8_t length_of_hours = 1;

//     for (uint8_t i = 0; i < hours_ch.length(); i++) {
//       if (hours_ch.charAt(i) == ',') {
//         length_of_hours++;  // 2
//       }
//     }
//     int8_t hours_start = 0;
//     uint8_t hours_end = hours_ch.indexOf(',');  // 1

//     for (uint8_t i = 0; i < length_of_hours; i++) {  // i-> 0, 1, end
//       if (hours_end == -1) {
//         hours_end = hours_ch.length();
//       }
//       int8_t hoursnum = hours_ch.substring(hours_start, hours_end).toInt();  // 0 , 1
//       hours_start = hours_end + 1;                                           // 2
//       hours_end = hours_ch.indexOf(',', hours_start);                        // 2
//       hours_num[hoursnum] = true;
//     }

//     int8_t length_of_days_ch = 1;
//     // days_in_num[] = { false, false, false, false, false, false, false };
//     //  { 0 = Monday, 1 = Tuesday, 2 = Wednesday, 3 = Thursday, 4 = Friday, 5 = Saturday, 6 = Sunday }

//     // length_of_days_ch = 1;
//     for (uint8_t i = 0; i < days_ch.length(); i++) {
//       // Serial.println("days_ch");
//       // Serial.print(days_ch.length());
//       if (days_ch.charAt(i) == ',') {
//         length_of_days_ch++;
//       }
//     }

//     int8_t day_start = 0;
//     uint8_t day_end = days_ch.indexOf(',');

//     for (int8_t i = 0; i < length_of_days_ch; i++) {
//       if (day_end == -1) {
//         // Handle the last element in the string
//         day_end = days_ch.length();
//       }
//       int8_t daynum = days_ch.substring(day_start, day_end).toInt();
//       day_start = day_end + 1;
//       day_end = days_ch.indexOf(',', day_start);

//       days_in_num[daynum] = true;
//     }

//     for (int8_t i = 0; i < 24; i++) {
//       String hours_key = "hours_key" + String(i);
//       preferences.putBool(hours_key.c_str(), hours_num[i]);
//     }
//     for (int8_t i = 0; i < 7; i++) {
//       String days_key = "days_key" + String(i);
//       preferences.putBool(days_key.c_str(), days_in_num[i]);
//     }
//   } else {
//     return;
//   }
// }
