#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// AM3 telemetry/config JSON packets are larger than PubSubClient's default.
#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 2048
#endif
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

#include "Def.h"
#include "aws_cred.h"
#include "temp.h"
#include "MQTT.h"
#include "WifiThread.h"
#include "IO.h"
#include "DisplayHostLink.h"
#include "esp_task_wdt.h"

TaskHandle_t Task1 = NULL;

// Explicit declarations keep the sketch valid outside Arduino's automatic
// prototype generator and make function dependencies unambiguous.
void getTemperature();
void getPressures();
void scanAlarms();

constexpr unsigned long MQTT_RECONNECT_INTERVAL_MS = 10000UL;
unsigned long lastMqttReconnectAttemptMs = 0;

void InitPins() {
  Serial.println("\n\n booting up device....\n\n.");
  Serial.print("Device Name: ");
  Serial.println(devicename);
  Serial.println("");

  sensors.begin();
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();
  sensors.setWaitForConversion(true);
  // sensors.setResolution(12);

  DEVICE_INIT();
  // pinMode(COMP_PIN, INPUT);
  // pinMode(OIL_SW, INPUT);  // OIL SW
  // pinMode(HIGH_PRE, INPUT);
  // pinMode(LOW_PRE, INPUT);

  // pinMode(START, INPUT);
  // pinMode(STOP, INPUT);

  // attachInterrupt(digitalPinToInterrupt(STOP), handleInterrupt1, FALLING);
  // attachInterrupt(digitalPinToInterrupt(START), handleInterrupt2, FALLING);
}

void setup() {
  esp_task_wdt_init(10, true);  // 10s timeout
  Serial.begin(115200);
  pinMode(RUN, OUTPUT);
  digitalWrite(RUN, LOW);
  pinMode(ALARM, OUTPUT);
  digitalWrite(ALARM, LOW);

  initInputs();

  InitPins();

  // Start the local display network and TCP server first. Router and MQTT
  // connections are independent and must never delay the display link.
  WiFi.setHostname(hostname.c_str());
  ensureHardwareHostAccessPoint();
  initDisplayHostLink();

  if (loadCredentials(ssid, password)) {
    Serial.println("Loaded router Wi-Fi credentials from memory.");
    connectToWiFi(ssid, password);  // non-blocking
  } else {
    Serial.println("No router Wi-Fi credentials found; display AP remains active.");
    startAccessPoint();
  }

  server_setup();
  macaddress = WiFi.macAddress();
}

void loop() {
  serviceRouterWiFi();
  serviceDisplayHostLink();
  // inputs = readInputs(0x20);
  // for (int i = 0; i < 8; i++) {
  //   Serial.print(bitRead(inputs, i));
  //   Serial.print(" ");
  // }

  // Periodic sensor read
  if (millis() - pmillis >= 1000) {
    sensors.begin();
    const int detectedDevices = sensors.getDeviceCount();
    numberOfDevices = detectedDevices;
    if (numberOfDevices > MAX_ONEWIRE_DEVICES) {
      numberOfDevices = MAX_ONEWIRE_DEVICES;
      Serial.print("Warning: detected ");
      Serial.print(detectedDevices);
      Serial.print(" DS18B20 sensors; only the first ");
      Serial.print(MAX_ONEWIRE_DEVICES);
      Serial.println(" are processed.");
    }
    if (numberOfDevices < 5) {
      Serial.print("numberOfDevices = ");
      Serial.println(numberOfDevices);
    }
    for (int i = 0; i < numberOfDevices; i++) {
      memset(tempSensorAddresses[i], 0x00, sizeof(DeviceAddress));
      if (!sensors.getAddress(tempSensorAddresses[i], i)) {
        Serial.println("Unable to find address for Device " + String(i));
      }
    }
    getTemperature();
    getPressures();
    pmillis = millis();
  }

  scanAlarms();

  // ---------------- MONITORING + CONTROL MODE ----------------
  if (modesw == 1) {
    inputs = readInputs(0x20);
    if (bitRead(inputs, START) == 0) {
      startSW = 1;
      //     StartSW_App = 0;
    } else if (Machine_Shutdown == 0 && bitRead(inputs, STOP) == 0) {
      stopSW = 1;
      //     StartSW_App = 0;
    }

    // -------- START REQUEST LOGIC --------
    if (!startup_flag && !alarmFlag) {
      if (startSW == 1 || StartSW_App == 1) {
        Serial.println("Start Sw");
        startSW = stopSW = 0;
        StartSW_App = 1;
        startup_flag = true;
        COMP_START_TIME = millis();
      }
      // Auto Restart Mode
      else if (AutoAlarmReset == 1 && millis() - RESTART_TIME > Comp_restart_delay_ms) {
        startup_flag = true;
        StartSW_App = 1;
        AutoAlarmReset = 0;
        Serial.println("Auto Startup!");
      }
    }

    bool MahineStatus = (Machine_Shutdown == 0 && Switches_alarm == 0 && ReturnTempC != 888);
    // -------- STOP / RESET LOGIC --------
    if (MahineStatus) {
      if (stopSW == 1 || (StartSW_App == 0 && startup_flag && startSWEnable == 1) || (!startup_flag && resetSW == 1 && startSWEnable == 1)) {
        startSW = stopSW = alarmFlag = startup_flag = false;
        // machineStopped = true;
        Serial.println("Machine STOP");
        digitalWrite(ALARM, 0);
        digitalWrite(RUN, 0);
        resetSW = 0;
        comp_Status = StartSW_App = 0;
        // COMP_FB_STUCK_TIME = millis();
        delay(300);
      }

      // AUTO RESET
      if (autoReset == 1 && alarmFlag && (millis() - COMP_TRIP_HOLD_TIME > 20000)) {
        Serial.println("RESTARTING");
        digitalWrite(ALARM, 0);
        AutoAlarmReset = 1;
        alarmFlag = false;
        comp_Status = 4;
        RESTART_TIME = millis();
      }
    }

    if (startup_flag) {
      // MACHINE TRIP CONDITION
      if (Machine_Shutdown == 1 || (Switches_alarm == 1 && millis() - SWITCH_ALARM_HOLD_TIME > 250) || comp_alarm == 1 || ReturnTempC == 888) {
        Serial.println("Machine Tripped!");
        digitalWrite(RUN, 0);
        digitalWrite(ALARM, 1);
        startup_flag = false;
        alarmFlag = true;
        // machineStopped = true;
        comp_alarm = 0;
        StartSW_App = 0;
        // COMP_FB_STUCK_TIME = millis();
        COMP_TRIP_HOLD_TIME = millis();
        // if (dis_alarm) comp_Status = HPS_ALARM;
        // else if (suction_alarm) comp_Status = LPS_ALARM;
        // else if (oil_alarm) comp_Status = OPS_ALARM;
        return;
      }

      // MACHINE START
      if (comp_Status != 1 && (int)ReturnTempC > ReturnSp && !Switches_alarm && (millis() - COMP_START_TIME > static_cast<unsigned long>(Comp_Start_delay > 0 ? Comp_Start_delay : 0) * 1000UL)) {
        Serial.println("Machine Started!");
        digitalWrite(RUN, 1);
        comp_Status = 1;
        COMP_START_TIME = millis();
      }

      // MACHINE AUTO-STOPPED  - RETURN SP REACHED
      if ((int)ReturnTempC < ReturnSp && comp_Status != 2) {
        Serial.println("Machine_AUTO_STOPPED");
        digitalWrite(RUN, 0);
        comp_Status = 2;
      }
    }
  }
  // MONITORING MODE
  else if (modesw == 0) {
    // MACHINE TRIP CONDITION
    bool shutdown = (Machine_Shutdown == 1 || (Switches_alarm == 1 && millis() - SWITCH_ALARM_HOLD_TIME > 500) || ReturnTempC == 888);
    if (shutdown && !alarmFlag) {
      Serial.println("Machine TRIPPED");
      // COMP_FB_STUCK_TIME = millis();
      digitalWrite(ALARM, 1);
      comp_alarm = 0;
      alarmFlag = true;
    } else if (!shutdown && alarmFlag) {
      digitalWrite(ALARM, 0);
      alarmFlag = false;
    }

    if (!alarmFlag) {
      if (bitRead(inputs, COMP_PIN) == LOW) {
        comp_Status = 0;
      } else {
        comp_Status = 1;
      }
    }
  }

  const unsigned long mqttNow = millis();
  if (WiFi.status() == WL_CONNECTED && !client.connected() &&
      (lastMqttReconnectAttemptMs == 0 ||
       mqttNow - lastMqttReconnectAttemptMs >= MQTT_RECONNECT_INTERVAL_MS)) {
    lastMqttReconnectAttemptMs = mqttNow;
    reconnect();
    // MQTT/TLS connection attempts can take time. Service the local display
    // immediately afterwards so it does not wait for another loop cycle.
    serviceDisplayHostLink();
  }

  if (millis() - wait_time >= 2000) {
    if (client.connected() && !message_received && !message_received_config) {
      publishJson();
      temp_sensor_select_publish();
    } else {
      message_received = false;
      message_received_config = false;
    }

    wait_time = millis();
  }
  client.loop();
  serviceDisplayHostLink();
}

///////////////////////////////////////////////////////////////////////////////////////

int convertToFahrenheit(float temp) {
  return ((temp * 9) / 5) + 32;
}

bool isValidDS18B20Temperature(float temperatureC) {
  // DS18B20 operating range is -55 C to +125 C. The DallasTemperature
  // disconnected value (-127 C) and any impossible value are rejected.
  return isfinite(temperatureC) && temperatureC >= -55.0f && temperatureC <= 125.0f;
}

void getTemperature() {
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();
  sensors.setWaitForConversion(true);

  if (temp2Assigned) {
    SuctionTempC = sensors.getTempC(temp2Address);
    if (isValidDS18B20Temperature(SuctionTempC)) {
      SuctionTempC += suction_temp_offset;
      SuctionTemp = convertToFahrenheit(SuctionTempC);
    } else {
      SuctionTempC = SuctionTemp = SENSOR_DISCONNECTED;
    }
  } else {
    SuctionTempC = SuctionTemp = SENSOR_NOT_SELECTED;
  }

  if (temp4Assigned) {
    dischargeTempC = sensors.getTempC(temp4Address);
    if (isValidDS18B20Temperature(dischargeTempC)) {
      dischargeTempC += discharge_temp_offset;
      dischargeTemp = convertToFahrenheit(dischargeTempC);
    } else {
      dischargeTempC = dischargeTemp = SENSOR_DISCONNECTED;
    }
  } else {
    dischargeTempC = dischargeTemp = SENSOR_NOT_SELECTED;
  }

  if (temp1Assigned) {
    SupplyTempC = sensors.getTempC(temp1Address);
    if (isValidDS18B20Temperature(SupplyTempC)) {
      SupplyTempC += supply_temp_offset;
      SupplyTemp = convertToFahrenheit(SupplyTempC);
    } else {
      SupplyTempC = SupplyTemp = SENSOR_DISCONNECTED;
    }
  } else {
    SupplyTempC = SupplyTemp = SENSOR_NOT_SELECTED;
  }

  if (temp3Assigned) {
    ReturnTempC = sensors.getTempC(temp3Address);
    if (isValidDS18B20Temperature(ReturnTempC)) {
      ReturnTempC += return_air_temp_offset;
      ReturnTemp = convertToFahrenheit(ReturnTempC);
    } else {
      ReturnTempC = ReturnTemp = SENSOR_DISCONNECTED;
    }
  } else {
    ReturnTempC = ReturnTemp = SENSOR_NOT_SELECTED;
  }

  if (temp5Assigned) {
    OilTempC = sensors.getTempC(temp5Address);
    if (isValidDS18B20Temperature(OilTempC)) {
      OilTempC += oil_temp_offset;
      OilTemp = convertToFahrenheit(OilTempC);
    } else {
      OilTempC = OilTemp = SENSOR_DISCONNECTED;
    }
  } else {
    OilTempC = OilTemp = SENSOR_NOT_SELECTED;
  }
}

String getPressureString(int tempF, const float* pressureArray, size_t pressureCount) {
  if (pressureArray == nullptr || tempF < 0 || static_cast<size_t>(tempF) >= pressureCount) {
    return "888";
  }
  return String(pressureArray[tempF], 1);
}

void getPressures() {
  // Choose refrigerant pressure table. Each table covers 0 F to 150 F.
  const float* pressureTable = nullptr;
  size_t pressureCount = 0;

  switch (gas_selection) {
    case 0:
      pressureTable = r22_PSI;
      pressureCount = sizeof(r22_PSI) / sizeof(r22_PSI[0]);
      break;
    case 1:
      pressureTable = r32_PSI;
      pressureCount = sizeof(r32_PSI) / sizeof(r32_PSI[0]);
      break;
    case 2:
      pressureTable = r407_vapor_PSI;
      pressureCount = sizeof(r407_vapor_PSI) / sizeof(r407_vapor_PSI[0]);
      break;
    case 3:
      pressureTable = r410_vapor_PSI;
      pressureCount = sizeof(r410_vapor_PSI) / sizeof(r410_vapor_PSI[0]);
      break;
    default:
      dischargePressure = dischargePressureB = "888";
      suctionPressure = suctionPressureB = "888";
      return;
  }

  if (dischargeTemp == SENSOR_NOT_SELECTED) {
    dischargePressure = dischargePressureB = "999";
  } else if (dischargeTemp == SENSOR_DISCONNECTED) {
    dischargePressure = dischargePressureB = "888";
  } else {
    dischargePressure = getPressureString(dischargeTemp, pressureTable, pressureCount);
    if (dischargePressure == "888") {
      dischargePressureB = "888";
    } else {
      const float dischargeBar = dischargePressure.toFloat() / 14.5038f;
      dischargePressureB = String(dischargeBar, 1);
    }
  }

  if (SuctionTemp == SENSOR_NOT_SELECTED) {
    suctionPressure = suctionPressureB = "999";
  } else if (SuctionTemp == SENSOR_DISCONNECTED) {
    suctionPressure = suctionPressureB = "888";
  } else {
    suctionPressure = getPressureString(SuctionTemp, pressureTable, pressureCount);
    if (suctionPressure == "888") {
      suctionPressureB = "888";
    } else {
      const float suctionBar = suctionPressure.toFloat() / 14.5038f;
      suctionPressureB = String(suctionBar, 1);
    }
  }
}

void scanAlarms() {
  inputs = readInputs(0x20);
  if (bitRead(inputs, HIGH_PRE) == 1 && HPS_SW == 1) {
    dis_alarm = true;
    HPS = "HIGH";
    if (startup_flag == 1 || modesw == 0) comp_Status = HPS_ALARM;
  } else {
    dis_alarm = false;
    HPS = "LOW";
  }

  if (bitRead(inputs, LOW_PRE) == 1 && LPS_SW == 1) {
    suction_alarm = true;
    LPS = "LOW";
    if (startup_flag == 1 || modesw == 0) comp_Status = LPS_ALARM;
  } else {
    suction_alarm = false;
    LPS = "HIGH";
  }

  if (bitRead(inputs, OIL_SW) == 1 && OPS_SW == 1) {
    OPS = "LOW";
    oil_alarm = true;
    if (startup_flag == 1 || modesw == 0) comp_Status = OPS_ALARM;
  } else {
    oil_alarm = false;
    OPS = "HIGH";
  }

  if (modesw == 1 && comp_Status == 1 && bitRead(inputs, COMP_PIN) == 1 && millis() - COMP_START_TIME > 2500) {
    comp_alarm = true;
    Serial.println("Compressor Feedback Error!");
    if (startup_flag == 1) comp_Status = COMP_FAIL_TO_RUN;
  }

  if (suction_alarm == 1 || dis_alarm == 1 || oil_alarm == 1 || comp_alarm == 1) {
    if (!Switches_alarm) {
      SWITCH_ALARM_HOLD_TIME = millis();
      Switches_alarm = 1;
    }
    return;
  } else {
    Switches_alarm = 0;
  }
  Machine_Shutdown = 0;

  if (ReturnTempC >= 888) {
    Machine_Shutdown = 1;
    if (startup_flag == 1 || modesw == 0) comp_Status = TEMP_SENSOR_ERROR;
    return;
  }

  if ((SuctionTempC < suctionAlertSp || SuctionTempC > 100.0) && SuctionTempC < 888) {
    // Serial.println("HVAC ALERT: Suction Temperature Low");
    Machine_Shutdown = 1;
    if (startup_flag == 1 || modesw == 0) comp_Status = SUCTION_TEMP_ALERT;
    return;
  }
  if (SuctionTempC >= 888) {
    Machine_Shutdown = 1;
    if (startup_flag == 1 || modesw == 0) comp_Status = TEMP_SENSOR_ERROR;
    return;
  }

  if (dischargeTempC > dischargeAlertSp && dischargeTempC < 888) {
    // Serial.println("HVAC ALERT: Discharge Temperature HIGH");
    Machine_Shutdown = 1;
    if (startup_flag == 1 || modesw == 0) comp_Status = DISCHARGE_TEMP_ALERT;
    return;
  }
  if (dischargeTempC >= 888) {
    Machine_Shutdown = 1;
    if (startup_flag == 1 || modesw == 0) comp_Status = TEMP_SENSOR_ERROR;
    return;
  }
}
