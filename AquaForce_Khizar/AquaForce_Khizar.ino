#include <Arduino.h>
#include <Preferences.h>
#include <QuickPID.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "esp_task_wdt.h"

#include "Def.h"

QuickPID PID_A(&CompA.Input, &CompA.Output, &CompA.Setpoint, EXV1.Kp, EXV1.Ki, EXV1.Kd, PID_A.Action::reverse);
QuickPID PID_B(&CompB.Input, &CompB.Output, &CompB.Setpoint, EXV2.Kp, EXV2.Ki, EXV2.Kd, PID_B.Action::reverse);

#include "ModbusRTU.h"
#include "IO.h"
#include "ADC.h"
#include "temp.h"
#include "WifiThread.h"
#include "MQTT.h"
#include "Config.h"
#include "MasterSlave.h"
#include "LoaderUnloader.h"

///////////////////////////////////////////////////////////////////////////////

void setup() {
  esp_task_wdt_init(10, true);  // 10s timeout
  Serial.begin(115200);

  initGPIOs();
  adcInit();
  RS485Serial.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW);

  Serial.println("\n\n booting up device....\n\n.");
  Serial.print("Device Name: ");
  Serial.println(devicename);
  Serial.println("");

  sensors.begin();
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();
  sensors.setWaitForConversion(true);

  DEVICE_INIT();
  configurePIDController();
  ResetAlarm = 0;

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
  if (!client.connected()) {
    reconnect();
  }
}

void loop() {
  // Periodic sensor read
  if (millis() - pmillis >= 1000) {
    scanSensorsA();
    scanSensorsB();
    pmillis = millis();
  }
  read_AMP();
  if (thermostat_selection == 1) scanBeca();

  // UPDATE RUNNING HOURS
  if (millis() - previousMillis > 60000) {  // 1min
    if (CompA.state == COMP_RUNNING) updateRunHours(CompA);
    if (CompB.state == COMP_RUNNING) updateRunHours(CompB);
    previousMillis = millis();
  }

  // ---- MASTER / SLAVE (LEAD-LAG) ROLE & ASSIST MANAGEMENT ----
  manageLeadLag();

  // ---- LOADER / UNLOADER CAPACITY CONTROL (fixed-speed compressors only) ----
  updateCapacityControl(CompA);
  updateCapacityControl(CompB);

  ///////////////////////////////////////////////////////
  // ---------------- CIRCUIT # 1 ----------------
  if (CompA.COMP_ENABLE == 1) {
    bool MahineStatusA = (CompA.Machine_Shutdown == 0 && CompA.Switches_Alarm == 0 && ReturnTemp != 888);
    if (!CompA.startup_flag && CompA.state == COMP_STOPPED) {
      if (startSw == 1 && CompA.isMaster && restartAllowed(CompA))
        Init_Compressor(CompA);
    }

    if (MahineStatusA) {
      // -------- START REQUEST LOGIC --------
      // MACHINE STOP
      if (startSw == 0 && CompA.state == COMP_RUNNING && !CompA.alarmFlag) {
        CompA.startup_flag = false;
        StopCompressor(CompA);
      }
      if (ResetAlarm == 1 && CompA.state == COMP_TRIPPED) {
        ResetAlarm = 0;
        CompA.alarmFlag = false;
        CompA.state = COMP_STOPPED;
      }
    }
    scanInputs(CompA);
    scanAlarms(CompA);
  }

  if (CompA.startup_flag) {
    // MACHINE TRIP CONDITION
    if (CompA.Machine_Shutdown || (CompA.Switches_Alarm && millis() - CompA.SWITCH_ALARM_TIME > 500) || ReturnTemp == 888) {
      Serial.println("Compressor A Tripped.");
      StopCompressor(CompA);
      CompA.startup_flag = false;
      CompA.alarmFlag = true;
      if (CompB.COMP_ENABLE == 0) startSw = 0;
      return;
    }
    if (CompA.state != COMP_RUNNING && ReturnTemp > ReturnSp && millis() - CompA.COMP_START_TIME > CompA.Starting_delay_ms && restartAllowed(CompA)) {
      Serial.println("Compressor A Started!");
      StartCompressor(CompA);
      CompA.exvState = EXV_READY;
      CompA.state = COMP_RUNNING;
    }
    // MACHINE STOP - (Chiller Air Outlet Sp) REACHED
    else if (CompA.state != COMP_AUTO_STOPPED && ReturnTemp < ReturnSp) {
      Serial.println("AUTO_STOPPED");
      StopCompressor(CompA);
      CompA.state = COMP_AUTO_STOPPED;
    }

    if (!CompA.STAR_TO_DELTA && CompA.driveSelection == 0 && millis() - CompA.STAR_DELTA_TIME > 3000 && CompA.state == COMP_RUNNING) {
      Serial.println("Comp A : Star to Delta Converted");
      Output_Write(STAR_PIN[0], 0);
      CompA.star_status = false;
      delay(200);
      Output_Write(DELTA_PIN[0], 1);
      CompA.delta_status = true;
      CompA.STAR_TO_DELTA = true;
    }
  }

  // ---------- EXV Control # 1 ----------
  if (CompA.COMP_ENABLE == 1 && CompA.EXV_ENABLE == 0) {
    // AUTO MODE
    if (EXV1.Mode == 1) {
      currentTime = millis();
      // -------------------- EXV / PID IMPLEMENTATION ----------------------------
      if (CompA.exvState == EXV_READY && currentTime - EXV1.PIDmillis > 150) {
        CompA.Input = (float)CompA.SuperHeatPV;
        PID_A.Compute();
        int newOutput = map(CompA.Output, EXV1.LowerLimit, EXV1.UpperLimit, 0, EXV1.exv_total_steps);
        // Serial.print("Output : ");
        // Serial.println(CompA.Output);
        // Serial.print("NewOutput : ");
        // Serial.println(newOutput);
        // Serial.println("");
        write_on_EXV(EXV1, newOutput, currentTime, EXV1.exv_step_delay);
        EXV1.PIDmillis = millis();
      }
      // Handles EXV close sequencing : moving EXV to 0 position
      else if (CompA.exvState == EXV_RESETTING) {
        // write_on_EXV(EXV1, 0, currentTime, 10);  // STEP DELAY : 50ms
        if (EXV1.currentStep > 0) {
          Close(1, 10);
          EXV1.currentStep--;
        } else if (EXV1.currentStep <= 0) {
          if (CompA.state == COMP_STOPPING) CompA.state = COMP_STOPPED;
          CompA.exvState = EXV_IDLE;
          Serial.println("EXV FULLY CLOSED!");
        }
      }
    }
  }

  ///////////////////////////////////////////////////////
  // ---------------- CIRCUIT # 2 ----------------
  if (CompB.COMP_ENABLE == 1) {
    bool MahineStatusB = (CompB.Machine_Shutdown == 0 && CompB.Switches_Alarm == 0 && ReturnTemp != 888);
    if (!CompB.startup_flag && CompB.state == COMP_STOPPED) {
      if (startSw == 1 && CompB.isMaster && restartAllowed(CompB))
        Init_Compressor(CompB);
    }

    if (MahineStatusB) {
      // -------- START REQUEST LOGIC --------
      // MACHINE STOP
      if (startSw == 0 && CompB.state == COMP_RUNNING && !CompB.alarmFlag) {
        CompB.startup_flag = false;
        StopCompressor(CompB);
      }
      if (ResetAlarm == 1 && CompB.state == COMP_TRIPPED) {
        ResetAlarm = 0;
        CompB.alarmFlag = false;
        CompB.state = COMP_STOPPED;
      }
    }
    scanInputs(CompB);
    scanAlarms(CompB);
  }

  if (CompB.startup_flag) {
    // MACHINE TRIP CONDITION
    if (CompB.Machine_Shutdown || (CompB.Switches_Alarm && millis() - CompB.SWITCH_ALARM_TIME > 500) || ReturnTemp == 888) {
      Serial.println("Compressor B Tripped.");
      StopCompressor(CompB);
      CompB.startup_flag = false;
      CompB.alarmFlag = true;
      if (CompA.COMP_ENABLE == 0) startSw = 0;
      return;
    }
    if (CompB.state != COMP_RUNNING && ReturnTemp > ReturnSp && millis() - CompB.COMP_START_TIME > CompB.Starting_delay_ms && restartAllowed(CompB)) {
      Serial.println("Compressor B Started!");
      StartCompressor(CompB);
      CompB.exvState = EXV_READY;
      CompB.state = COMP_RUNNING;
    }
    // MACHINE STOP - (Chiller Air Outlet Sp) REACHED
    else if (CompB.state != COMP_AUTO_STOPPED && ReturnTemp < ReturnSp) {
      Serial.println("AUTO_STOPPED");
      StopCompressor(CompB);
      CompB.state = COMP_AUTO_STOPPED;
    }

    if (!CompB.STAR_TO_DELTA && CompB.driveSelection == 0 && millis() - CompB.STAR_DELTA_TIME > 3000 && CompB.state == COMP_RUNNING) {
      Serial.println("Comp B : Star to Delta Converted");
      Output_Write(STAR_PIN[1], 0);
      CompB.star_status = false;
      delay(200);
      Output_Write(DELTA_PIN[1], 1);
      CompB.delta_status = true;
      CompB.STAR_TO_DELTA = true;
    }
  }

  // ---------- EXV Control # 2----------
  if (CompB.COMP_ENABLE == 1 && CompB.EXV_ENABLE == 0) {
    // AUTO MODE
    if (EXV2.Mode == 1) {
      currentTime = millis();
      // -------------------- EXV / PID IMPLEMENTATION ----------------------------
      if (CompB.exvState == EXV_READY && currentTime - EXV2.PIDmillis > 150) {
        CompB.Input = (float)CompB.SuperHeatPV;
        PID_B.Compute();
        int newOutput = map(CompB.Output, EXV2.LowerLimit, EXV2.UpperLimit, 0, EXV2.exv_total_steps);
        write_on_EXV(EXV2, newOutput, currentTime, EXV2.exv_step_delay);
        EXV2.PIDmillis = millis();
      }
      // Handles EXV close sequencing : moving EXV to 0 position
      else if (CompB.exvState == EXV_RESETTING) {
        if (EXV2.currentStep > 0) {
          Close(2, 10);
          EXV2.currentStep--;
        } else if (EXV2.currentStep <= 0) {
          if (CompB.state == COMP_STOPPING) CompB.state = COMP_STOPPED;
          CompB.exvState = EXV_IDLE;
          Serial.println("EXV FULLY CLOSED!");
        }
      }
    }
  }

  ///////////////////////////////////////////////////////
  // if (CompA.state == COMP_TRIPPED && CompB.state == COMP_TRIPPED) {
  //   startSw = 0;
  // }

  if (millis() - wait_time >= MQTT_INTERVAL && WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      reconnect();
    }

    if (!message_received && !message_received_configA && !message_received_configB && !message_received_temp_config && !message_received_temp_configB) {
      Serial.println("MQTT Published!");
      publishJson();
      input_output_publish();
      temp_sensor_select_publish();
      temp_sensorB_select_publish();
      if (CompA.COMP_ENABLE == 1) publish_circuitA_config();
      if (CompB.COMP_ENABLE == 1) publish_circuitB_config();
    } else {
      message_received = false;
      message_received_temp_config = false;
      message_received_temp_configB = false;
      message_received_configA = false;
      message_received_configB = false;
    }
    wait_time = millis();
  }
  client.loop();
}

///////////////////////////////////////////////////////////////////////////////////////////////
void read_AMP() {
  uint16_t registers[10];
  if (readHoldingRegisters(MODBUS_SLAVE_ID, CT_REGISTER_ADDRESS, 3, registers)) {
    AMP1 = registers[0] / 100;
    AMP2 = registers[1] / 100;
    AMP3 = registers[2] / 100;
    // if (AMP1 > 100) AMP1 = 888;
    // if (AMP2 > 100) AMP2 = 888;
    // if (AMP3 > 100) AMP3 = 888;
  } else {
    Serial.println("Failed to read CT Module");
    AMP1 = 888;
    AMP2 = 888;
    AMP3 = 888;
  }
}
