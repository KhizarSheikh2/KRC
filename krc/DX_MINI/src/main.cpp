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
#include "UI.h"
#include "WifiThread.h"
#include "MQTT.h"

// PlatformIO/C++ forward declarations (Arduino .ino generated these automatically).
void updateTimeNow();

// These setpoint limits and updateControlOutputs() originally lived in OLED.h.
// The OLED is removed, so the same original control definitions are kept here
// unchanged to preserve DX-Mini behavior.
static constexpr int MIN_RTEMP = 0;
static constexpr int MIN_DTEMP = 0;
static constexpr int MAX_RTEMP = 100;
static constexpr int MAX_DTEMP = 200;

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

void setup() {
  esp_task_wdt_init(10, true);  // 10s timeout  
  Serial.begin(115200);

  if (client.setBufferSize(1024)) {
    Serial.println("[MQTT] Packet buffer: 1024 bytes");
  } else {
    Serial.println("[MQTT] ERROR: unable to allocate 1024-byte packet buffer");
  }

  uiInit();
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
  uiSetBootProgress(30, "RESTORING SETTINGS");
  DEVICE_INIT();
  uiSetBootProgress(55, "SETTINGS READY");

  //=========================================//
  macaddress = WiFi.macAddress();
  server_setup();

  uiShowMain();
  updateDisplay();
}

void beep(int duration = BUZZER_DURATION, int freq = BUZZER_FREQ) {
  tone(BUZZER_PIN, freq, duration);
  delay(10);
}

void Poweroff() {
    EVAP_COOL_SYSTEM_SWITCH = 0;
    AUTO_WASH_SYSTEM_SWITCH = 0;
    UNIT_COMMAND_SWITCH = 0;

    // Reset status
    EVAP_COOL_SYSTEM_STATUS = false;
    AUTO_WASH_SYSTEM_STATUS = false;
    UNIT_COMMAND_STATUS = false;

    updateControlOutputs();

    showCenteredText("POWER OFF", 2);
}

void processTouchAction(UiAction action) {
  if (action == UI_ACTION_NONE) return;

  beep();

  bool stateChanged = false;

  switch (action) {
    case UI_ACTION_TOGGLE_POWER:
      PowerState = !PowerState;
      preferences.begin("values", false);
      preferences.putBool("PowerState", PowerState);
      preferences.end();

      if (PowerState) {
        showCenteredText("POWER ON", 2);
        updateControlOutputs();
      } else {
        Poweroff();
      }
      stateChanged = true;
      break;

    case UI_ACTION_TOGGLE_MODE: {
      const int previousMode = MODE_SWITCH;
      MODE_SWITCH = MODE_SWITCH == 0 ? 1 : 0;

      preferences.begin("values", false);
      preferences.putInt("modeSw", MODE_SWITCH);

      if (previousMode != MODE_SWITCH && MODE_SWITCH == 0) {
        EVAP_COOL_SYSTEM_SWITCH = 0;
        AUTO_WASH_SYSTEM_SWITCH = 0;
        UNIT_COMMAND_SWITCH = 0;

        EVAP_COOL_SYSTEM_STATUS = false;
        AUTO_WASH_SYSTEM_STATUS = false;
        UNIT_COMMAND_STATUS = false;

        EVAP_COOL_SYSTEM_PENDING = -1;
        AUTO_WASH_SYSTEM_PENDING = -1;
        UNIT_COMMAND_PENDING = -1;

        digitalWrite(EVAP_COOL_SYSTEM, LOW);
        digitalWrite(AUTO_WASH_SYSTEM, LOW);
        digitalWrite(UNIT_COMMAND, LOW);

        preferences.putInt("r1Sw", EVAP_COOL_SYSTEM_SWITCH);
        preferences.putInt("r2Sw", AUTO_WASH_SYSTEM_SWITCH);
        preferences.putInt("r3Sw", UNIT_COMMAND_SWITCH);
      }
      preferences.end();

      showCenteredText(MODE_SWITCH == 0 ? "MANUAL MODE" : "AUTO MODE", 2);
      stateChanged = true;
      break;
    }

    case UI_ACTION_DISCHARGE_MINUS:
      if (PowerState && dischargeSp > MIN_DTEMP) {
        dischargeSp--;
        preferences.begin("values", false);
        preferences.putInt("dischargeSp", dischargeSp);
        preferences.end();
        stateChanged = true;
      }
      break;

    case UI_ACTION_DISCHARGE_PLUS:
      if (PowerState && dischargeSp < MAX_DTEMP) {
        dischargeSp++;
        preferences.begin("values", false);
        preferences.putInt("dischargeSp", dischargeSp);
        preferences.end();
        stateChanged = true;
      }
      break;

    case UI_ACTION_RETURN_MINUS:
      if (PowerState && ReturnSp > MIN_RTEMP) {
        ReturnSp--;
        preferences.begin("values", false);
        preferences.putInt("returnSp", ReturnSp);
        preferences.end();
        stateChanged = true;
      }
      break;

    case UI_ACTION_RETURN_PLUS:
      if (PowerState && ReturnSp < MAX_RTEMP) {
        ReturnSp++;
        preferences.begin("values", false);
        preferences.putInt("returnSp", ReturnSp);
        preferences.end();
        stateChanged = true;
      }
      break;

    case UI_ACTION_TOGGLE_EVAP:
      if (PowerState && MODE_SWITCH == 0) {
        EVAP_COOL_SYSTEM_SWITCH = !EVAP_COOL_SYSTEM_SWITCH;
        preferences.begin("values", false);
        preferences.putInt("r1Sw", EVAP_COOL_SYSTEM_SWITCH);
        preferences.end();
        stateChanged = true;
      } else if (MODE_SWITCH != 0) {
        showCenteredText("AUTO CONTROL", 2);
      }
      break;

    case UI_ACTION_TOGGLE_WASH:
      if (PowerState && MODE_SWITCH == 0) {
        AUTO_WASH_SYSTEM_SWITCH = !AUTO_WASH_SYSTEM_SWITCH;
        preferences.begin("values", false);
        preferences.putInt("r2Sw", AUTO_WASH_SYSTEM_SWITCH);
        preferences.end();
        stateChanged = true;
      } else if (MODE_SWITCH != 0) {
        showCenteredText("AUTO CONTROL", 2);
      }
      break;

    case UI_ACTION_TOGGLE_UNIT:
      if (PowerState && MODE_SWITCH == 0) {
        if (AUTO_WASH_SYSTEM_SWITCH == 1 && UNIT_COMMAND_SWITCH == 0) {
          showCenteredText("WASH INTERLOCK", 2);
        } else {
          UNIT_COMMAND_SWITCH = !UNIT_COMMAND_SWITCH;
          preferences.begin("values", false);
          preferences.putInt("r3Sw", UNIT_COMMAND_SWITCH);
          preferences.end();
          stateChanged = true;
        }
      } else if (MODE_SWITCH != 0) {
        showCenteredText("AUTO CONTROL", 2);
      }
      break;

    default:
      break;
  }

  if (stateChanged) {
    updateDisplay();
    if (client.connected()) publishJson();
  }
}

void loop() {
  static bool wasPoweredOff = false;

  uiLoop();
  processTouchAction(uiTakeAction());

  if (!PowerState) {
    if (!wasPoweredOff) {
      Poweroff();
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
  wasPoweredOff = false;

  ///////////////////////////////////////////////////////////
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

  if (MODE_SWITCH == 0) {

    if (MODE_SWITCH == 0 && timeschen != 0) {
      timeschen = 0;
      tmatched = 0;
      preferences.begin("timeenable", false);
      preferences.putInt("timeschen", timeschen);
      preferences.end();
    }

    if (EVAP_COOL_SYSTEM_SWITCH != (int)EVAP_COOL_SYSTEM_STATUS) {
      if (EVAP_COOL_SYSTEM_PENDING != EVAP_COOL_SYSTEM_SWITCH) {
        EVAP_COOL_SYSTEM_PENDING = EVAP_COOL_SYSTEM_SWITCH;
        evapCoolPendingTime = millis();
      } else if (millis() - evapCoolPendingTime >= RELAY_SWITCH_DELAY) {
        EVAP_COOL_SYSTEM_STATUS = (EVAP_COOL_SYSTEM_SWITCH == 1);
        digitalWrite(EVAP_COOL_SYSTEM, EVAP_COOL_SYSTEM_STATUS ? HIGH : LOW);
        EVAP_COOL_SYSTEM_PENDING = -1;
      }
    } else {
      EVAP_COOL_SYSTEM_PENDING = -1;
    }

    if (AUTO_WASH_SYSTEM_SWITCH == 1 && !AUTO_WASH_SYSTEM_STATUS) {
      if (AUTO_WASH_SYSTEM_PENDING != 1) {
        AUTO_WASH_SYSTEM_PENDING = 1;
        autoWashPendingTime = millis();
      } else if (millis() - autoWashPendingTime >= RELAY_SWITCH_DELAY) {
        AUTO_WASH_SYSTEM_STATUS = true;
        digitalWrite(AUTO_WASH_SYSTEM, HIGH);
        AUTO_WASH_SYSTEM_PENDING = -1;

        UNIT_COMMAND_STATUS = false;
        UNIT_COMMAND_SWITCH = 0;
        UNIT_COMMAND_PENDING = -1;
        digitalWrite(UNIT_COMMAND, LOW);
      }
    } else if (AUTO_WASH_SYSTEM_SWITCH == 0 && AUTO_WASH_SYSTEM_STATUS) {
      if (AUTO_WASH_SYSTEM_PENDING != 0) {
        AUTO_WASH_SYSTEM_PENDING = 0;
        autoWashPendingTime = millis();
      } else if (millis() - autoWashPendingTime >= RELAY_SWITCH_DELAY) {
        AUTO_WASH_SYSTEM_STATUS = false;
        digitalWrite(AUTO_WASH_SYSTEM, LOW);
        AUTO_WASH_SYSTEM_PENDING = -1;
      }
    } else {
      AUTO_WASH_SYSTEM_PENDING = -1;
    }

    
    if (AUTO_WASH_SYSTEM_SWITCH == 1) {
      UNIT_COMMAND_SWITCH = 0;
    }

    if (UNIT_COMMAND_SWITCH == 1 && !UNIT_COMMAND_STATUS) {
      if (UNIT_COMMAND_PENDING != 1) {
        UNIT_COMMAND_PENDING = 1;
        unitCmdPendingTime = millis();
      } else if (millis() - unitCmdPendingTime >= RELAY_SWITCH_DELAY) {
        UNIT_COMMAND_STATUS = true;
        digitalWrite(UNIT_COMMAND, HIGH);
        UNIT_COMMAND_PENDING = -1;
      }
    } else if (UNIT_COMMAND_SWITCH == 0 && UNIT_COMMAND_STATUS) {
      if (UNIT_COMMAND_PENDING != 0) {
        UNIT_COMMAND_PENDING = 0;
        unitCmdPendingTime = millis();
      } else if (millis() - unitCmdPendingTime >= RELAY_SWITCH_DELAY) {
        UNIT_COMMAND_STATUS = false;
        digitalWrite(UNIT_COMMAND, LOW);
        UNIT_COMMAND_PENDING = -1;
      }
    } else {
      UNIT_COMMAND_PENDING = -1;
    }

  } else {  // AUTO MODE

    runScheduler();

    // ===== AUTO WASH SCHEDULE =====
    if (tmatched && timeschen) {

      // Start Auto Wash
      if (AUTO_WASH_SYSTEM_PENDING != 1) {
        AUTO_WASH_SYSTEM_PENDING = 1;
        autoWashPendingTime = millis();
      } else if (millis() - autoWashPendingTime >= RELAY_SWITCH_DELAY) {
        AUTO_WASH_SYSTEM_STATUS = true;
        digitalWrite(AUTO_WASH_SYSTEM, HIGH);
        AUTO_WASH_SYSTEM_PENDING = -1;

        // Cooling must stop during wash
        UNIT_COMMAND_STATUS = false;
        UNIT_COMMAND_SWITCH = 0;
        UNIT_COMMAND_PENDING = -1;
        digitalWrite(UNIT_COMMAND, LOW);
      }

    } else {

      AUTO_WASH_SYSTEM_PENDING = -1;

      // Stop Auto Wash
      if (AUTO_WASH_SYSTEM_STATUS) {
        if (AUTO_WASH_SYSTEM_PENDING != 0) {
          AUTO_WASH_SYSTEM_PENDING = 0;
          autoWashPendingTime = millis();
        } else if (millis() - autoWashPendingTime >= RELAY_SWITCH_DELAY) {
          AUTO_WASH_SYSTEM_STATUS = false;
          digitalWrite(AUTO_WASH_SYSTEM, LOW);
          AUTO_WASH_SYSTEM_PENDING = -1;
        }
      }

      // Cooling on!
      if (!UNIT_COMMAND_STATUS) {
        if (UNIT_COMMAND_PENDING != 1) {
          UNIT_COMMAND_PENDING = 1;
          unitCmdPendingTime = millis();
        } else if (millis() - unitCmdPendingTime >= RELAY_SWITCH_DELAY) {
          UNIT_COMMAND_STATUS = true;
          UNIT_COMMAND_SWITCH = 1;
          digitalWrite(UNIT_COMMAND, HIGH);
          UNIT_COMMAND_PENDING = -1;
        }
      } else {
        UNIT_COMMAND_PENDING = -1;
      }
    }

    if (dischargeTemp >= (float)dischargeSp + TEMP_HYSTERESIS) {
      if (EVAP_COOL_SYSTEM_PENDING != 1) {
        EVAP_COOL_SYSTEM_PENDING = 1;
        evapCoolPendingTime = millis();
      } else if (millis() - evapCoolPendingTime >= RELAY_SWITCH_DELAY) {
        EVAP_COOL_SYSTEM_STATUS = true;
        EVAP_COOL_SYSTEM_SWITCH = 1;
        digitalWrite(EVAP_COOL_SYSTEM, HIGH);
        SYSTEM_STATUS = 1;
        EVAP_COOL_SYSTEM_PENDING = -1;
      }
    } else if (dischargeTemp <= (float)dischargeSp - TEMP_HYSTERESIS) {
      if (EVAP_COOL_SYSTEM_PENDING != 0) {
        EVAP_COOL_SYSTEM_PENDING = 0;
        evapCoolPendingTime = millis();
      } else if (millis() - evapCoolPendingTime >= RELAY_SWITCH_DELAY) {
        EVAP_COOL_SYSTEM_STATUS = false;
        EVAP_COOL_SYSTEM_SWITCH = 0;
        digitalWrite(EVAP_COOL_SYSTEM, LOW);
        SYSTEM_STATUS = 0;
        EVAP_COOL_SYSTEM_PENDING = -1;
      }
    } else {
      // Inside deadband — cancel any pending change, hold current state
      EVAP_COOL_SYSTEM_PENDING = -1;
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
  
  char buffer[16];

  
  weekDay = timeinfo.tm_wday - 1;
  current_hour = timeinfo.tm_hour;

  // Format the string with leading zeroes if needed
  snprintf(buffer, sizeof(buffer), "%02d%02d%02d%02d%02d%02d",
           timeinfo.tm_mday,
           timeinfo.tm_mon + 1,
           timeinfo.tm_year % 100,
           timeinfo.tm_hour,
           timeinfo.tm_min,
           timeinfo.tm_sec);

  timenow = String(buffer);
}
