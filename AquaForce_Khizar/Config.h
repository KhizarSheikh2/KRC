int convertToFahrenheit(float temp) {
  return ((temp * 9) / 5) + 32;
}

bool isValidTemperature(uint16_t tempRaw) {
  return (tempRaw > 0 && tempRaw < 65535);
}

void configurePIDController() {
  CompA.Setpoint = (float)CompA.SuperHeatSV;
  Serial.printf("SetpointA : %.2f\n", CompA.Setpoint);

  //apply PID gains
  PID_A.SetTunings(EXV1.Kp, EXV1.Ki, EXV1.Kd);              // PID gains
  PID_A.SetOutputLimits(EXV1.LowerLimit, EXV1.UpperLimit);  // EXV steps limits
  PID_A.SetSampleTimeUs(20000);                             // 20 ms
  PID_A.SetMode(PID_A.Control::automatic);                  // turn PID on

  CompB.Setpoint = (float)CompB.SuperHeatSV;
  Serial.printf("SetpointB : %.2f\n", CompB.Setpoint);

  //apply PID gains
  PID_B.SetTunings(EXV2.Kp, EXV2.Ki, EXV2.Kd);              // PID gains
  PID_B.SetOutputLimits(EXV2.LowerLimit, EXV2.UpperLimit);  // EXV steps limits
  PID_B.SetSampleTimeUs(20000);                             // 20 ms
  PID_B.SetMode(PID_B.Control::automatic);                  // turn PID on
}

void Init_Compressor(Compressor &comp) {
  Serial.print("Initialize the Compressor ");
  Serial.println(&comp == &CompA ? "A." : "B.");

  comp.startup_flag = true;
  comp.state = COMP_STARTING;
  comp.COMP_START_TIME = millis();
  comp.Starting_delay_ms = comp.Starting_delay * 1000;
}

void updateRunHours(Compressor &comp) {
  comp.counterMin++;

  if (comp.counterMin >= 10) {  // use 60 for 1hr
    comp.Run_hrs++;

    const char *prefName = (&comp == &CompA) ? "CompA" : "CompB";
    preferences.begin(prefName, false);
    preferences.putInt("runHrs", comp.Run_hrs);
    preferences.end();
    comp.counterMin = 0;
  }
}

void scanInputs(Compressor &comp) {
  uint8_t id = (&comp == &CompA) ? 0 : 1;
  uint8_t inputs = readInputs(INPUT_PortB);

  uint8_t Oil_level_Sw = (id == 0 ? Oil_level_Sw_C1 : Oil_level_Sw_C2);
  uint8_t Oil_Pres_Low_Sw = (id == 0 ? Oil_Pres_Low_Sw_C1 : Oil_Pres_Low_Sw_C2);
  uint8_t Suc_Pres_Low_Sw = (id == 0 ? Suc_Pres_Low_Sw_C1 : Suc_Pres_Low_Sw_C2);
  uint8_t Dis_Pres_High_Sw = (id == 0 ? Dis_Pres_High_Sw_C1 : Dis_Pres_High_Sw_C2);

  comp.oil_level_low_alarm = !bitRead(inputs, Oil_level_Sw);
  comp.oil_pres_low_alarm_sw = !bitRead(inputs, Oil_Pres_Low_Sw);
  comp.suc_pres_low_alarm_sw = !bitRead(inputs, Suc_Pres_Low_Sw);
  comp.dis_pres_high_alarm_sw = !bitRead(inputs, Dis_Pres_High_Sw);

  if (comp.oil_level_low_alarm || comp.oil_pres_low_alarm_sw || comp.suc_pres_low_alarm_sw || comp.dis_pres_high_alarm_sw) {
    if (!comp.Switches_Alarm) {
      comp.SWITCH_ALARM_TIME = millis();
      comp.Switches_Alarm = true;
      comp.state = COMP_TRIPPED;
    }
  } else {
    comp.Switches_Alarm = false;
  }
}

void scanAlarms(Compressor &comp) {
  if (comp.Switches_Alarm == 1) return;
  comp.Machine_Shutdown = false;

  // Suction Gas Temperature HIGH (spec alarm list). Guard against a disconnected
  // sensor (reads SENSOR_DISCONNECTED) and an un-set setpoint so neither raises a
  // false trip.
  comp.suc_temp_high_alarm = (comp.SuctionTemp < SENSOR_DISCONNECTED) && (comp.suc_temp_high_SV > 0)
                             && (comp.SuctionTemp >= comp.suc_temp_high_SV);
  if (comp.suc_temp_high_alarm)
    comp.state = SUCTION_TEMP_ALERT;

  comp.dis_temp_high_alarm = comp.dischargeTemp >= comp.dis_temp_high_SV;
  if (comp.dis_temp_high_alarm)
    comp.state = DISCHARGE_TEMP_ALERT;

  comp.spray_temp_low_alarm = comp.SprayTemp <= comp.spray_temp_low_SV;
  if (comp.spray_temp_low_alarm)
    comp.state = SPRAY_TEMP_ALERT;

  comp.suc_pres_low_alarm = comp.suctionPressure <= comp.suc_pres_low_SV;
  if (comp.suc_pres_low_alarm)
    comp.state = SUCTION_PSI_ALERT;

  comp.dis_pres_high_alarm = comp.dischargePressure >= comp.dis_pres_high_SV;
  if (comp.dis_pres_high_alarm)
    comp.state = DISCHARGE_PSI_ALERT;

  // Oil Pressure LOW (spec safety). Compare the measured oil pressure against the
  // LOW setpoint. Only evaluate when the analog oil-pressure sensor returned a
  // valid reading (not SENSOR_DISCONNECTED) and a setpoint has been configured,
  // so an unfitted/disconnected sensor cannot nuisance-trip the machine.
  comp.oil_pres_low_alarm = (comp.oilPressure < SENSOR_DISCONNECTED) && (comp.oil_pres_low_SV > 0)
                            && (comp.oilPressure <= comp.oil_pres_low_SV);
  if (comp.oil_pres_low_alarm)
    comp.state = OIL_PSI_ALERT;

  comp.amp_high_alarm = AMP1 >= comp.amps_high_Sp;
  if (comp.amp_high_alarm)
    comp.state = COMP_AMP_HIGH;

  if (comp.suc_temp_high_alarm || comp.dis_temp_high_alarm || comp.spray_temp_low_alarm || comp.suc_pres_low_alarm || comp.dis_pres_high_alarm || comp.oil_pres_low_alarm || comp.amp_high_alarm) {
    comp.Machine_Shutdown = true;
  }
}

void runVFD(Compressor &comp, uint16_t freq, uint16_t command) {
  uint8_t id = (&comp == &CompA) ? VFD_1_ID : VFD_2_ID;
  switch (command) {
    case 0:  // stop operation
      writeSingleRegister(id, Control_Address, VFD_Stop);
      break;
    case 1:  // run operation
      freq = freq * 200;
      writeSingleRegister(id, Freq_Address, freq);  // Set frequency to 50.00 Hz (50.00 * 200 = 10000)
      writeSingleRegister(id, Control_Address, VFD_Run);
      break;
    default:
      Serial.println("Invalid VFD Command !");
      break;
  }
}

void readVFDParameters(Compressor &comp) {
  uint8_t id = (&comp == &CompA) ? VFD_1_ID : VFD_2_ID;
  uint16_t vfdRegisters[5];
  comp.out_hz = comp.out_voltage = comp.out_amps = 888;
  if (readHoldingRegisters(id, 0x3000, 5, vfdRegisters)) {
    // for (int i = 0; i < 5; i++) Serial.printf("%d  ", vfdRegisters[i]);
    // Serial.println();

    comp.out_hz = vfdRegisters[0];
    comp.out_voltage = vfdRegisters[3];
    comp.out_amps = vfdRegisters[4];
  } else {
    Serial.printf("VFD # %d communication error\n", id);
  }
}

// bool readTemperatureSensors(Compressor &comp) {
//   uint8_t id = (&comp == &CompA) ? 0 : 1;
//   uint8_t slaveID = (id == 0) ? ADAM_1_ID : ADAM_2_ID;

//   comp.SuctionTemp = comp.SuctionTempF = 888;
//   comp.dischargeTemp = comp.dischargeTempF = 888;
//   comp.SprayTemp = comp.SprayTempF = 888;
//   comp.SubCoolTemp = comp.SubCoolTempF = 888;

//   uint16_t tempRegisters[6];
//   if (readHoldingRegisters(slaveID, 0, 6, tempRegisters)) {
//     // Suction temperature
//     uint16_t raw = (id == 0) ? tempRegisters[SUCTION_C1] : tempRegisters[SUCTION_C2];
//     if (isValidTemperature(raw)) {
//       comp.SuctionTemp = raw / 1000.0;
//       // comp.SuctionTempF = convertToFahrenheit(comp.SuctionTemp);
//     }

//     // Discharge temperature
//     raw = (id == 0) ? tempRegisters[DISCHARGE_C1] : tempRegisters[DISCHARGE_C2];
//     if (isValidTemperature(raw)) {
//       comp.dischargeTemp = raw / 1000.0;
//       // comp.dischargeTempF = convertToFahrenheit(comp.dischargeTemp);
//     }

//     // Sub-cooling temperature
//     raw = (id == 0) ? tempRegisters[SUB_COOLING_C1] : tempRegisters[SUB_COOLING_C2];
//     if (isValidTemperature(raw)) {
//       comp.SubCoolTemp = raw / 1000.0;
//       // comp.SubCoolTempF = convertToFahrenheit(comp.SubCoolTemp);
//     }

//     // Spray temperature
//     raw = (id == 0) ? tempRegisters[SPRAY_C1] : tempRegisters[SPRAY_C2];
//     if (isValidTemperature(raw)) {
//       comp.SprayTemp = raw / 1000.0;
//       // comp.SprayTempF = convertToFahrenheit(comp.SprayTemp);
//     }
//     return true;

//   } else {
//     Serial.printf("ADAM4015 # %d Communication Error!\n", id + 1);
//     return false;
//   }
// }

// Returns the saturation (dew-point) temperature in degF for a given gauge
// pressure, by binary-searching a PT chart. The chart is indexed by temperature:
// element i is the saturation pressure (PSIG) at (baseTempF + i) degF, so the
// matched index maps straight back to a real temperature. Returns SENSOR_DISCONNECTED
// when the input is unusable, and clamps to the chart's end-points out of range.
int16_t Temp_from_PT_chart(const float *pressureArray, int arraySize, float pressure, int baseTempF) {
  if (arraySize <= 0 || pressure >= SENSOR_DISCONNECTED) {
    return SENSOR_DISCONNECTED;  // invalid input or disconnected sensor
  }
  if (pressure <= pressureArray[0]) {
    return baseTempF;  // at or below the chart floor
  }
  if (pressure >= pressureArray[arraySize - 1]) {
    return baseTempF + arraySize - 1;  // at or above the chart ceiling
  }

  int low = 0, high = arraySize - 1;
  while (low < high) {
    int mid = (low + high + 1) / 2;
    if (pressureArray[mid] <= pressure) {
      low = mid;
    } else {
      high = mid - 1;
    }
  }

  return (int16_t)(baseTempF + low);
}

// Maps the operator-selected refrigerant (gas_index, spec order) to its PT chart
// and returns the saturated-vapour temperature for the given suction pressure.
int16_t SatTempForGas(int gas_index, float suctionPressure) {
  switch (gas_index) {
    case GAS_R134A: return Temp_from_PT_chart(R134a_PSIG, size_R134a, suctionPressure, PT_BASE_TEMP_F);
    case GAS_R22:   return Temp_from_PT_chart(R22_PSIG,   size_R22,   suctionPressure, PT_BASE_TEMP_F);
    case GAS_R410:  return Temp_from_PT_chart(R410A_PSIG, size_R410,  suctionPressure, PT_BASE_TEMP_F);
    case GAS_R407:  return Temp_from_PT_chart(R407C_PSIG, size_R407,  suctionPressure, PT_BASE_TEMP_F);
    default:        return SENSOR_DISCONNECTED;
  }
}

bool readTemperatureSensorsA() {
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();
  sensors.setWaitForConversion(true);
  numberOfDevices = sensors.getDeviceCount();

  Serial.print("\n numberOfDevices= ");
  Serial.println(numberOfDevices);

  // Default all sensors to disconnected
  CompA.SuctionTemp = SENSOR_DISCONNECTED;
  CompA.dischargeTemp = SENSOR_DISCONNECTED;
  CompA.SprayTemp = SENSOR_DISCONNECTED;
  CompA.SubCoolTemp = SENSOR_DISCONNECTED;
  ReturnTemp = SENSOR_DISCONNECTED;
  SupplyTemp = SENSOR_DISCONNECTED;

  if (numberOfDevices == 0) {
    Serial.println("No Sensor Found!");
    sensors.begin();
    return false;
  }

  for (int i = 0; i < numberOfDevices && i < MAX_SENSORS; i++) {
    if (!sensors.getAddress(tempSensorAddresses[i], i)) {
      Serial.println("Unable to find address for Device " + String(i));
      continue;  // Skip the rest of the current loop iteration and move to the next iteration immediately.
    }

    float temperature = sensors.getTempC(tempSensorAddresses[i]);
    if (temperature < 0) continue;

    if (temp5Assigned && compareAddresses(tempSensorAddresses[i], temp5Address)) {
      SupplyTemp = temperature + supply_temp_offset;
    } else if (temp6Assigned && compareAddresses(tempSensorAddresses[i], temp6Address)) {
      ReturnTemp = temperature + return_air_temp_offset;
    } else if (temp1Assigned && compareAddresses(tempSensorAddresses[i], temp1Address)) {
      CompA.SuctionTemp = temperature + suction_temp_offset;
    } else if (temp2Assigned && compareAddresses(tempSensorAddresses[i], temp2Address)) {
      CompA.dischargeTemp = temperature + discharge_temp_offset;
    } else if (temp3Assigned && compareAddresses(tempSensorAddresses[i], temp3Address)) {
      CompA.SubCoolTemp = temperature + subcool_temp_offset;
    } else if (temp4Assigned && compareAddresses(tempSensorAddresses[i], temp4Address)) {
      CompA.SprayTemp = temperature + spray_temp_offset;
    }
  }
  return true;
}

void scanSensorsA() {
  if (CompA.COMP_ENABLE == 1) {
    // uint8_t id = (&comp == &CompA) ? 0 : 1;
    scanADC(CompA);
    // Read VFD parameters if enabled
    if (CompA.driveSelection == 3) readVFDParameters(CompA);

    if (!readTemperatureSensorsA()) {
      CompA.SuperHeatPV = 888;
      return;
    }

    if (CompA.suctionPressure >= SENSOR_DISCONNECTED) {
      // Suction pressure sensor disconnected -> superheat is meaningless
      CompA.SuperHeatPV = SENSOR_DISCONNECTED;
    } else {
      int16_t Sat_Temp = SatTempForGas(CompA.gas_index, CompA.suctionPressure);
      CompA.SuctionTempF = convertToFahrenheit(CompA.SuctionTemp);
      CompA.SuperHeatPV = CompA.SuctionTempF - Sat_Temp;
    }

    if (CompA.FAN1_ENABLE == 1) {
      if (CompA.dischargePressure >= CompA.Fan1_ON_PSI && CompA.dischargePressure < 888) {
        Output_Write(CompA_Condenser_Fan1, 1);
        CompA.fan1_status = true;
      } else if (CompA.dischargePressure < CompA.Fan1_OFF_PSI || CompA.dischargePressure >= 888) {
        Output_Write(CompA_Condenser_Fan1, 0);
        CompA.fan1_status = false;
      }
    } else {
      Output_Write(CompA_Condenser_Fan1, 0);
    }
  }
}

bool readTemperatureSensorsB() {
  sensorsB.setWaitForConversion(false);
  sensorsB.requestTemperatures();
  sensorsB.setWaitForConversion(true);
  numberOfDevicesB = sensorsB.getDeviceCount();

  Serial.print("\n numberOfDevicesB= ");
  Serial.println(numberOfDevicesB);

  // Default all sensors to disconnected
  CompB.SuctionTemp = SENSOR_DISCONNECTED;
  CompB.dischargeTemp = SENSOR_DISCONNECTED;
  CompB.SprayTemp = SENSOR_DISCONNECTED;
  CompB.SubCoolTemp = SENSOR_DISCONNECTED;

  if (numberOfDevicesB == 0) {
    Serial.println("No Sensor Found!");
    sensorsB.begin();
    return false;
  }

  for (int i = 0; i < numberOfDevicesB && i < MAX_SENSORS; i++) {
    if (!sensorsB.getAddress(tempSensorAddressesB[i], i)) {
      Serial.println("Unable to find address for Device " + String(i));
      continue;  // Skip the rest of the current loop iteration and move to the next iteration immediately.
    }

    float temperature = sensorsB.getTempC(tempSensorAddressesB[i]);
    if (temperature < 0) continue;

    if (temp7Assigned && compareAddresses(tempSensorAddressesB[i], temp7Address)) {
      CompB.SuctionTemp = temperature + suction_temp_offset2;
    } else if (temp8Assigned && compareAddresses(tempSensorAddressesB[i], temp8Address)) {
      CompB.dischargeTemp = temperature + discharge_temp_offset2;
    } else if (temp9Assigned && compareAddresses(tempSensorAddressesB[i], temp9Address)) {
      CompB.SubCoolTemp = temperature + subcool_temp_offset2;
    } else if (temp10Assigned && compareAddresses(tempSensorAddressesB[i], temp10Address)) {
      CompB.SprayTemp = temperature + spray_temp_offset2;
    }
  }
  return true;
}

void scanSensorsB() {
  if (CompB.COMP_ENABLE == 1) {
    // uint8_t id = (&comp == &CompA) ? 0 : 1;
    scanADC(CompB);
    // Read VFD parameters if enabled
    if (CompB.driveSelection == 3) readVFDParameters(CompB);

    if (!readTemperatureSensorsB()) {
      CompB.SuperHeatPV = 888;
      return;
    }

    if (CompB.suctionPressure >= SENSOR_DISCONNECTED) {
      // Suction pressure sensor disconnected -> superheat is meaningless
      CompB.SuperHeatPV = SENSOR_DISCONNECTED;
    } else {
      int16_t Sat_Temp = SatTempForGas(CompB.gas_index, CompB.suctionPressure);
      CompB.SuctionTempF = convertToFahrenheit(CompB.SuctionTemp);
      CompB.SuperHeatPV = CompB.SuctionTempF - Sat_Temp;
    }

    if (CompB.FAN1_ENABLE == 1) {
      if (CompB.dischargePressure >= CompB.Fan1_ON_PSI && CompB.dischargePressure < 888) {
        Output_Write(CompB_Condenser_Fan1, 1);
      } else if (CompB.dischargePressure < CompB.Fan1_OFF_PSI || CompB.dischargePressure >= 888) {
        Output_Write(CompB_Condenser_Fan1, 0);
      }
    } else {
      Output_Write(CompB_Condenser_Fan1, 0);
    }
  }
}

void StopCompressor(Compressor &comp) {
  Serial.println("Stop the Compressor.");
  uint8_t id = (&comp == &CompA) ? 0 : 1;
  if (comp.driveSelection == 3) {
    Serial.printf("Compressor %d : VFD OFF\n", id + 1);
    runVFD(comp, comp.min_vfd_freq, 0);  // Stop VFD
  }
  //
  else if (comp.driveSelection == 0 || comp.driveSelection == 1) {
    Output_Write(STAR_PIN[id], 0);
    Output_Write(DELTA_PIN[id], 0);
    comp.STAR_TO_DELTA = false;
    comp.star_status = false;
    comp.delta_status = false;
  }

  comp.exvState = EXV_RESETTING;
  comp.state = COMP_STOPPING;
  ExpansionValve &exv = (&comp == &CompA) ? EXV1 : EXV2;
  if (comp.EXV_ENABLE == 1 || exv.Mode == 0)  // TXV SELECTED
    comp.state = COMP_STOPPED;

  comp.STOP_TIME = millis();  // start the 600s (10 min) restart lockout, per spec
}

void StartCompressor(Compressor &comp) {
  uint8_t id = (&comp == &CompA) ? 0 : 1;
  if (comp.driveSelection == 3) {
    Serial.printf("Compressor %d : VFD ON\n", id + 1);
    runVFD(comp, comp.min_vfd_freq, 1);
  }
  //
  else if (comp.driveSelection == 0) {
    Serial.printf("Compressor %d : SD ON\n", id + 1);
    Output_Write(STAR_PIN[id], 1);
    comp.star_status = true;
    comp.STAR_DELTA_TIME = millis();
  }
  //
  else if (comp.driveSelection == 1) {
    Serial.printf("Compressor %d : DOL ON\n", id + 1);
    Output_Write(DELTA_PIN[id], 1);
    comp.delta_status = true;
  }
}

void scanBeca() {
  ReturnTemp = 888;

  uint16_t registers[10];  // BECA
  beca_Status = readHoldingRegisters(BECA_SLAVE_ID, 0x0000, 10, registers);
  if (prev_status != beca_Status) {
    if (prev_status == 0) {
      // --- Sync Setpoint ---
      if (ReturnSp != registers[3] / 10) {
        Serial.println("Thermostat Changed!");
        // App changed it → update thermostat
        writeSingleRegister(BECA_SLAVE_ID, 0x03, ReturnSp * 10);
      }
    }
    prev_status = beca_Status;
  }
  if (beca_Status) {
    Power_state = registers[0];    // BECA Power On/Off State
    Setpoint = registers[3] / 10;  // Setpoint from BECA
    if (Power_state == 1) ReturnTemp = registers[8] / 10;

    if (Setpoint != ReturnSp) {
      ReturnSp = Setpoint;  // sync back
      preferences.begin("values", false);
      preferences.putInt("setPoint", ReturnSp);
      preferences.end();
    }
  }
}