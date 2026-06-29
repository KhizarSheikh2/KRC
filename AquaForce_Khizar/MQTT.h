void temp_sensor_select_publish() {
  DynamicJsonDocument json_doc(1024);

  float temperature_read;
  String addressString;

  for (int i = 0; i < numberOfDevices; i++) {
    read_temp(temperature_read, tempSensorAddresses[i]);
    addressString = getAddressString(tempSensorAddresses[i]);
    switch (i) {
      case 0:
        json_doc["address1"] = addressString;
        json_doc["temp1"] = temperature_read;
        break;
      case 1:
        json_doc["address2"] = addressString;
        json_doc["temp2"] = temperature_read;
        break;
      case 2:
        json_doc["address3"] = addressString;
        json_doc["temp3"] = temperature_read;
        break;
      case 3:
        json_doc["address4"] = addressString;
        json_doc["temp4"] = temperature_read;
        break;
      case 4:
        json_doc["address5"] = addressString;
        json_doc["temp5"] = temperature_read;
        break;
      case 5:
        json_doc["address6"] = addressString;
        json_doc["temp6"] = temperature_read;
        break;
      default:
        break;
    }

    json_doc[addressString] = 0;
    if (temp1Assigned && compareAddresses(tempSensorAddresses[i], temp1Address)) {
      json_doc[addressString] = 1;
    }
    if (temp2Assigned && compareAddresses(tempSensorAddresses[i], temp2Address)) {
      json_doc[addressString] = 2;
    }
    if (temp3Assigned && compareAddresses(tempSensorAddresses[i], temp3Address)) {
      json_doc[addressString] = 3;
    }
    if (temp4Assigned && compareAddresses(tempSensorAddresses[i], temp4Address)) {
      json_doc[addressString] = 4;
    }
    if (temp5Assigned && compareAddresses(tempSensorAddresses[i], temp5Address)) {
      json_doc[addressString] = 5;
    }
    if (temp6Assigned && compareAddresses(tempSensorAddresses[i], temp6Address)) {
      json_doc[addressString] = 6;
    }
  }

  json_doc["offset1"] = offset1;
  json_doc["offset2"] = offset2;
  json_doc["offset3"] = offset3;
  json_doc["offset4"] = offset4;
  json_doc["offset5"] = offset5;
  json_doc["offset6"] = offset6;

  char doc[1024];
  serializeJson(json_doc, doc);
  String device_topic_temp_config = device_topic_p + "/SensorA";
  client.publish(device_topic_temp_config.c_str(), doc, true);
}

void input_output_publish() {
  DynamicJsonDocument json_doc(1024);

  json_doc["oilswA"] = (int)CompA.oil_level_low_alarm;
  json_doc["oilpsiA"] = (int)CompA.oil_pres_low_alarm_sw;
  json_doc["suctionA"] = (int)CompA.suc_pres_low_alarm_sw;
  json_doc["dischargeA"] = (int)CompA.dis_pres_high_alarm_sw;

  json_doc["starA"] = (int)CompA.star_status;
  json_doc["deltaA"] = (int)CompA.delta_status;
  json_doc["fan1A"] = (int)CompA.fan1_status;
  json_doc["fan3A"] = (int)CompA.fan3_status;

  char doc[1024];
  serializeJson(json_doc, doc);
  String device_topic_temp_config = device_topic_p + "/Input-Output";
  client.publish(device_topic_temp_config.c_str(), doc, true);
}


// /KRC/DM-AAA001/Input-Output
// { 
//   "oilswA": 1,
//   "oilpsiA": 1,
//   "suctionA": 0,
//   "dischargeA": 0,
//   "oilswB": 1,
//   "oilpsiB": 1,
//   "suctionB": 0,
//   "dischargeB": 0,

//   "starA": 0,
//   "deltaA": 0,
//   "fan1A": 0,
//   "fan3A": 0,
//   "starB": 0,
//   "deltaB": 0,
//   "fan1B": 0,
//   "fan3B": 0,
// }

void Extract_temp_config(String incoming_message) {
  DeserializationError error = deserializeJson(received_doc, incoming_message);
  if (error) {
    Serial.print("JSON deserialization failed: ");
    Serial.println(error.c_str());
    return;
  }

  Serial.println(incoming_message);

  String addressString;
  int8_t sensor_selected;
  for (int i = 0; i < numberOfDevices; i++) {
    addressString = getAddressString(tempSensorAddresses[i]);
    if (received_doc.containsKey(addressString)) {
      sensor_selected = received_doc[addressString].as<int>();
      // Serial.print("addressString: ");
      // Serial.println(addressString);
      // Serial.print("sensor_selected: ");
      // Serial.println(sensor_selected);
      assignSensor(sensor_selected, tempSensorAddresses[i]);
    }
  }

  String key_offset;
  preferences.begin("values", false);
  for (int i = 0; i < numberOfDevices; i++) {
    addressString = getAddressString(tempSensorAddresses[i]);
    key_offset = "offset" + addressString;
    if (received_doc.containsKey(key_offset)) {
      offset = received_doc[key_offset].as<int>();
      switch (i) {
        case 0:
          offset1 = offset;
          preferences.putInt("offset1", offset1);
          break;
        case 1:
          offset2 = offset;
          preferences.putInt("offset2", offset2);
          break;
        case 2:
          offset3 = offset;
          preferences.putInt("offset3", offset3);
          break;
        case 3:
          offset4 = offset;
          preferences.putInt("offset4", offset4);
          break;
        case 4:
          offset5 = offset;
          preferences.putInt("offset5", offset5);
          break;
        case 5:
          offset6 = offset;
          preferences.putInt("offset6", offset6);
          break;
        default:
          break;
      }
      setoffset(tempSensorAddresses[i], offset);
    }
  }
  preferences.end();
}

void temp_sensorB_select_publish() {
  DynamicJsonDocument json_doc(1024);

  float temperature_read;
  String addressString;

  for (int i = 0; i < numberOfDevicesB; i++) {
    read_tempB(temperature_read, tempSensorAddressesB[i]);
    addressString = getAddressString(tempSensorAddressesB[i]);
    switch (i) {
      case 0:
        json_doc["address1"] = addressString;
        json_doc["temp1"] = temperature_read;
        break;
      case 1:
        json_doc["address2"] = addressString;
        json_doc["temp2"] = temperature_read;
        break;
      case 2:
        json_doc["address3"] = addressString;
        json_doc["temp3"] = temperature_read;
        break;
      case 3:
        json_doc["address4"] = addressString;
        json_doc["temp4"] = temperature_read;
        break;
      default:
        break;
    }

    json_doc[addressString] = 0;
    if (temp7Assigned && compareAddresses(tempSensorAddressesB[i], temp7Address)) {
      json_doc[addressString] = 1;
    }
    if (temp8Assigned && compareAddresses(tempSensorAddressesB[i], temp8Address)) {
      json_doc[addressString] = 2;
    }
    if (temp9Assigned && compareAddresses(tempSensorAddressesB[i], temp9Address)) {
      json_doc[addressString] = 3;
    }
    if (temp10Assigned && compareAddresses(tempSensorAddressesB[i], temp10Address)) {
      json_doc[addressString] = 4;
    }
  }

  json_doc["offset1"] = offset7;
  json_doc["offset2"] = offset8;
  json_doc["offset3"] = offset9;
  json_doc["offset4"] = offset10;

  char doc[1024];
  serializeJson(json_doc, doc);
  String device_topic_temp_config = device_topic_p + "/SensorB";
  client.publish(device_topic_temp_config.c_str(), doc, true);
}

void Extract_temp_configB(String incoming_message) {
  DeserializationError error = deserializeJson(received_doc, incoming_message);
  if (error) {
    Serial.print("JSON deserialization failed: ");
    Serial.println(error.c_str());
    return;
  }

  Serial.println(incoming_message);

  String addressString;
  int8_t sensor_selected;
  for (int i = 0; i < numberOfDevicesB; i++) {
    addressString = getAddressString(tempSensorAddressesB[i]);
    if (received_doc.containsKey(addressString)) {
      sensor_selected = received_doc[addressString].as<int>();
      // Serial.print("addressString: ");
      // Serial.println(addressString);
      // Serial.print("sensor_selected: ");
      // Serial.println(sensor_selected);
      assignSensorB(sensor_selected, tempSensorAddressesB[i]);
    }
  }

  String key_offset;
  preferences.begin("valuesB", false);
  for (int i = 0; i < numberOfDevicesB; i++) {
    addressString = getAddressString(tempSensorAddressesB[i]);
    key_offset = "offset" + addressString;
    if (received_doc.containsKey(key_offset)) {
      offset = received_doc[key_offset].as<int>();
      switch (i) {
        case 0:
          offset7 = offset;
          preferences.putInt("offset1", offset7);
          break;
        case 1:
          offset8 = offset;
          preferences.putInt("offset2", offset8);
          break;
        case 2:
          offset9 = offset;
          preferences.putInt("offset3", offset9);
          break;
        case 3:
          offset10 = offset;
          preferences.putInt("offset4", offset10);
          break;
        default:
          break;
      }
      setoffset(tempSensorAddressesB[i], offset);
    }
  }
  preferences.end();
}

void publishJson() {
  StaticJsonDocument<512> doc;
  doc["statusA"] = String(CompA.state);
  doc["statusB"] = String(CompB.state);

  doc["tempSelect"] = thermostat_selection;
  doc["supplyTemp"] = String(SupplyTemp, 1);
  doc["return"] = String(ReturnTemp, 1);

  doc["setPoint"] = ReturnSp;
  doc["powerSwitch"] = startSw;
  doc["resetValues"] = ResetAlarm;
  // doc["celciusToFahrenheit"] = ftoC;
  doc["mac_address"] = macaddress;
  doc["ip_address"] = myIP;

  char jsonBuffer[512];
  size_t n = serializeJson(doc, jsonBuffer);
  client.publish(device_topic_p.c_str(), jsonBuffer, true);
}

void update_values_from_json(String key, int& value_variable) {
  if (received_doc.containsKey(key)) {
    if (received_doc[key].as<int>() != value_variable) {
      value_variable = received_doc[key].as<int>();
      preferences.putInt(key.c_str(), value_variable);
    }
  }
}

void update_values_from_json(String key, float& value_variable) {
  if (received_doc.containsKey(key)) {
    if (received_doc[key].as<float>() != value_variable) {
      value_variable = received_doc[key].as<float>();
      preferences.putFloat(key.c_str(), value_variable);
    }
  }
}

// {"setPoint":0,"powerSwitch":0,"resetValues":0,"celciusToFahrenheit":0,"tempSelect":0}
void Extract_by_json(String incomingMessage) {
  DeserializationError error = deserializeJson(received_doc, incomingMessage);
  if (error) {
    Serial.print("JSON deserialization failed: ");
    Serial.println(error.c_str());
    return;
  }
  // Serial.println(incomingMessage);

  if (received_doc.containsKey("resetValues")) {
    ResetAlarm = received_doc["resetValues"].as<int>();
  }
  if (received_doc.containsKey("powerSwitch")) {
    startSw = received_doc["powerSwitch"].as<int>();
  }

  preferences.begin("values", false);
  update_values_from_json("setPoint", ReturnSp);
  update_values_from_json("tempSelect", thermostat_selection);
  preferences.end();

  // --- Sync Setpoint ---
  if (ReturnSp != Setpoint && beca_Status) {
    Serial.println("App Changed!");
    // App changed it → update thermostat
    writeSingleRegister(BECA_SLAVE_ID, 0x03, ReturnSp * 10);
    Setpoint = ReturnSp;  // sync back
  }
  message_received = true;
}

void Extract_circuitA_config(String incoming_message) {
  DeserializationError error = deserializeJson(received_doc, incoming_message);
  if (error) {
    Serial.print("JSON deserialization failed: ");
    Serial.println(error.c_str());
    return;
  }

  preferences.begin("CompA", false);
  update_values_from_json("ExvselA", CompA.EXV_ENABLE);
  update_values_from_json("minA", EXV1.LowerLimit);            // Lower Limit
  update_values_from_json("maxA", EXV1.UpperLimit);            // Upper Limit
  update_values_from_json("modeA", EXV1.Mode);                 // EXV Mode
  update_values_from_json("exvMstepA", EXV1.exv_total_steps);  // Exv Max Steps
  update_values_from_json("exvStepDA", EXV1.exv_step_delay);   // Exv Step delay
  update_values_from_json("PidIA", EXV1.Ki);
  update_values_from_json("PidDA", EXV1.Kd);
  update_values_from_json("PidPA", EXV1.Kp);
  update_values_from_json("superheatspA", CompA.SuperHeatSV);

  update_values_from_json("ampSpA", CompA.amps_high_Sp);

  update_values_from_json("enableA", CompA.COMP_ENABLE);
  update_values_from_json("leadlagA", LeadSW);
  update_values_from_json("startdelayA", CompA.Starting_delay);
  update_values_from_json("gasA", CompA.gas_index);
  update_values_from_json("driveA", CompA.driveSelection);
  update_values_from_json("vfdMinFreA", CompA.min_vfd_freq);
  update_values_from_json("vfdMaxFreA", CompA.max_vfd_freq);
  update_values_from_json("vfdDelayA", CompA.VFD_Step_Delay);
  update_values_from_json("stepsizeA", CompA.step_size);

  update_values_from_json("SucPsiSpA", CompA.suc_pres_low_SV);
  update_values_from_json("DisPsiSpA", CompA.dis_pres_high_SV);
  update_values_from_json("oilPsiSpA", CompA.oil_pres_low_SV);
  update_values_from_json("SucTempSpA", CompA.suc_temp_high_SV);
  update_values_from_json("DisTempSpA", CompA.dis_temp_high_SV);
  update_values_from_json("SprayTempSpA", CompA.spray_temp_low_SV);

  update_values_from_json("sucPreTypeA", CompA.suction_pressure_type);
  update_values_from_json("sucPreRangeA", CompA.suction_pressure_range);
  update_values_from_json("sucPreUnitA", CompA.suction_pressure_unit);
  update_values_from_json("sucoffsetA", CompA.suction_offset);

  update_values_from_json("disPreTypeA", CompA.discharge_pressure_type);  // 0-5v = 1 , 4-20mA = 2
  update_values_from_json("disPreRangeA", CompA.discharge_pressure_range);
  update_values_from_json("disPreUnitA", CompA.discharge_pressure_unit);  // bar = 0 , PSI = 1
  update_values_from_json("disoffsetA", CompA.discharge_offset);

  update_values_from_json("oilPreTypeA", CompA.oil_pressure_type);
  update_values_from_json("oilPreRangeA", CompA.oil_pressure_range);
  update_values_from_json("oilPreUnitA", CompA.oil_pressure_unit);
  update_values_from_json("oiloffsetA", CompA.oil_offset);

  update_values_from_json("fan1EA", CompA.FAN1_ENABLE);
  update_values_from_json("fan3EA", CompA.FAN3_ENABLE);
  update_values_from_json("fan5EA", CompA.FAN5_ENABLE);
  update_values_from_json("fan1HA", CompA.Fan1_ON_PSI);
  update_values_from_json("fan3HA", CompA.Fan3_ON_PSI);
  update_values_from_json("fan5HA", CompA.Fan5_ON_PSI);
  update_values_from_json("fan1LA", CompA.Fan1_OFF_PSI);
  update_values_from_json("fan3LA", CompA.Fan3_OFF_PSI);
  update_values_from_json("fan5LA", CompA.Fan5_OFF_PSI);
  preferences.end();
  Serial.println("Data Saved!");

  if (CompA.suction_pressure_unit) {                    // psi
    CompA.sensorMax = CompA.suction_pressure_range;     // max 0-725psi
  } else {                                              // bar
    float range = CompA.suction_pressure_range * 14.7;  // max 0-50 bar
    CompA.sensorMax = (int)range;
  }
  if (CompA.discharge_pressure_unit) {                    // psi
    CompA.sensorMax2 = CompA.discharge_pressure_range;    // max 0-725psi
  } else {                                                // bar
    float range = CompA.discharge_pressure_range * 14.7;  // max 0-50 bar
    CompA.sensorMax2 = (int)range;
  }
}

void publish_circuitA_config() {
  DynamicJsonDocument json_doc(2048);

  json_doc["leadlagA"] = LeadSW;
  json_doc["gasA"] = CompA.gas_index;
  json_doc["enableA"] = CompA.COMP_ENABLE;
  json_doc["ExvselA"] = CompA.EXV_ENABLE;
  json_doc["driveA"] = CompA.driveSelection;

  json_doc["minA"] = EXV1.LowerLimit;
  json_doc["maxA"] = EXV1.UpperLimit;
  json_doc["exvCstepA"] = EXV1.currentStep;
  json_doc["exvMstepA"] = EXV1.exv_total_steps;
  json_doc["exvStepDA"] = EXV1.exv_step_delay;
  if (EXV1.exv_total_steps > 0) 
  {
  json_doc["exvA"] = ((EXV1.currentStep * 100) / EXV1.exv_total_steps);  // 0 - 100 %
  }
  json_doc["modeA"] = EXV1.Mode;
  json_doc["PidPA"] = EXV1.Kp;
  json_doc["PidIA"] = EXV1.Ki;
  json_doc["PidDA"] = EXV1.Kd;

  json_doc["SucPsiSpA"] = CompA.suc_pres_low_SV;
  json_doc["DisPsiSpA"] = CompA.dis_pres_high_SV;
  json_doc["oilPsiSpA"] = CompA.oil_pres_low_SV;

  json_doc["SucTempSpA"] = CompA.suc_temp_high_SV;
  json_doc["DisTempSpA"] = CompA.dis_temp_high_SV;
  json_doc["SprayTempSpA"] = CompA.spray_temp_low_SV;


  json_doc["ampA"] = AMP1;
  json_doc["ampSpA"] = CompA.amps_high_Sp;

  json_doc["superheatspA"] = CompA.SuperHeatSV;
  json_doc["stepsizeA"] = CompA.step_size;
  json_doc["startdelayA"] = CompA.Starting_delay;
  json_doc["vfdDelayA"] = CompA.VFD_Step_Delay;

  json_doc["SucTempA"] = String(CompA.SuctionTemp, 1);
  json_doc["SucPsiA"] = CompA.suctionPressure;
  json_doc["DisTempA"] = String(CompA.dischargeTemp, 1);
  json_doc["DisPsiA"] = CompA.dischargePressure;
  json_doc["oilPsiA"] = CompA.oilPressure;

  json_doc["subCoolingA"] = String(CompA.SubCoolTemp, 1);
  json_doc["sprayA"] = String(CompA.SprayTemp, 1);
  json_doc["shtA"] = CompA.SuperHeatPV;

  json_doc["runHoursA"] = CompA.Run_hrs;
  json_doc["fan1EA"] = CompA.FAN1_ENABLE;
  json_doc["fan3EA"] = CompA.FAN3_ENABLE;
  json_doc["fan5EA"] = CompA.FAN5_ENABLE;
  json_doc["fan1HA"] = CompA.Fan1_ON_PSI;
  json_doc["fan3HA"] = CompA.Fan3_ON_PSI;
  json_doc["fan5HA"] = CompA.Fan5_ON_PSI;
  json_doc["fan1LA"] = CompA.Fan1_OFF_PSI;
  json_doc["fan3LA"] = CompA.Fan3_OFF_PSI;
  json_doc["fan5LA"] = CompA.Fan5_OFF_PSI;

  json_doc["vfdMinFreA"] = CompA.min_vfd_freq;
  json_doc["vfdMaxFreA"] = CompA.max_vfd_freq;

  json_doc["sucPreTypeA"] = CompA.suction_pressure_type;
  json_doc["sucPreRangeA"] = CompA.suction_pressure_range;
  json_doc["sucPreUnitA"] = CompA.suction_pressure_unit;
  json_doc["sucoffsetA"] = CompA.suction_offset;

  json_doc["disPreTypeA"] = CompA.discharge_pressure_type;
  json_doc["disPreRangeA"] = CompA.discharge_pressure_range;
  json_doc["disPreUnitA"] = CompA.discharge_pressure_unit;
  json_doc["disoffsetA"] = CompA.discharge_offset;

  json_doc["oilPreTypeA"] = CompA.oil_pressure_type;
  json_doc["oilPreRangeA"] = CompA.oil_pressure_range;
  json_doc["oilPreUnitA"] = CompA.oil_pressure_unit;
  json_doc["oiloffsetA"] = CompA.oil_offset;

  char doc[2048];
  serializeJson(json_doc, doc);

  String device_topic_pressure_config = device_topic_p + "/CircuitA";
  client.publish(device_topic_pressure_config.c_str(), doc, true);
}

void Extract_circuitB_config(String incoming_message) {
  DeserializationError error = deserializeJson(received_doc, incoming_message);
  if (error) {
    Serial.print("JSON deserialization failed: ");
    Serial.println(error.c_str());
    return;
  }

  preferences.begin("CompB", false);
  update_values_from_json("ExvselB", CompB.EXV_ENABLE);
  update_values_from_json("minB", EXV2.LowerLimit);            // Lower Limit
  update_values_from_json("maxB", EXV2.UpperLimit);            // Upper Limit
  update_values_from_json("modeB", EXV2.Mode);                 // EXV Mode
  update_values_from_json("exvMstepB", EXV2.exv_total_steps);  // Exv Max Steps
  update_values_from_json("exvStepDB", EXV2.exv_step_delay);   // Exv Step delay
  update_values_from_json("PidIB", EXV2.Ki);
  update_values_from_json("PidDB", EXV2.Kd);
  update_values_from_json("PidPB", EXV2.Kp);
  update_values_from_json("superheatspB", CompB.SuperHeatSV);

  update_values_from_json("enableB", CompB.COMP_ENABLE);
  update_values_from_json("leadlagB", LeadSW);
  update_values_from_json("startdelayB", CompB.Starting_delay);
  update_values_from_json("gasB", CompB.gas_index);
  update_values_from_json("driveB", CompB.driveSelection);
  update_values_from_json("vfdMinFreB", CompB.min_vfd_freq);
  update_values_from_json("vfdMaxFreB", CompB.max_vfd_freq);
  update_values_from_json("vfdDelayB", CompB.VFD_Step_Delay);
  update_values_from_json("stepsizeB", CompB.step_size);

  update_values_from_json("SucPsiSpB", CompB.suc_pres_low_SV);
  update_values_from_json("DisPsiSpB", CompB.dis_pres_high_SV);
  update_values_from_json("SucTempSpB", CompB.suc_temp_high_SV);
  update_values_from_json("DisTempSpB", CompB.dis_temp_high_SV);
  update_values_from_json("SprayTempSpB", CompB.spray_temp_low_SV);

  update_values_from_json("sucPreTypeB", CompB.suction_pressure_type);
  update_values_from_json("sucPreRangeB", CompB.suction_pressure_range);
  update_values_from_json("sucPreUnitB", CompB.suction_pressure_unit);
  update_values_from_json("sucoffsetB", CompB.suction_offset);

  update_values_from_json("disPreTypeB", CompB.discharge_pressure_type);
  update_values_from_json("disPreRangeB", CompB.discharge_pressure_range);
  update_values_from_json("disPreUnitB", CompB.discharge_pressure_unit);
  update_values_from_json("disoffsetB", CompB.discharge_offset);

  update_values_from_json("fan1EB", CompB.FAN1_ENABLE);
  update_values_from_json("fan3EB", CompB.FAN3_ENABLE);
  update_values_from_json("fan5EB", CompB.FAN5_ENABLE);
  update_values_from_json("fan1HB", CompB.Fan1_ON_PSI);
  update_values_from_json("fan3HB", CompB.Fan3_ON_PSI);
  update_values_from_json("fan5HB", CompB.Fan5_ON_PSI);
  update_values_from_json("fan1LB", CompB.Fan1_OFF_PSI);
  update_values_from_json("fan3LB", CompB.Fan3_OFF_PSI);
  update_values_from_json("fan5LB", CompB.Fan5_OFF_PSI);
  preferences.end();
  Serial.println("Data Saved!");

  if (CompB.suction_pressure_unit) {  // psi
    CompB.sensorMax = CompB.suction_pressure_range;
  } else {  // bar
    float range = CompB.suction_pressure_range * 14.7;
    CompB.sensorMax = (int)range;
  }
  if (CompB.discharge_pressure_unit) {  // psi
    CompB.sensorMax2 = CompB.discharge_pressure_range;
  } else {  // bar
    float range = CompB.discharge_pressure_range * 14.7;
    CompB.sensorMax2 = (int)range;
  }
}

void publish_circuitB_config() {
  DynamicJsonDocument json_doc(2048);

  json_doc["leadlagB"] = LeadSW;
  json_doc["gasB"] = CompB.gas_index;
  json_doc["enableB"] = CompB.COMP_ENABLE;
  json_doc["ExvselB"] = CompB.EXV_ENABLE;
  json_doc["driveB"] = CompB.driveSelection;

  json_doc["minB"] = EXV2.LowerLimit;
  json_doc["maxB"] = EXV2.UpperLimit;
  json_doc["exvMstepB"] = EXV2.exv_total_steps;
  json_doc["exvStepDB"] = EXV2.exv_step_delay;

  if (EXV2.exv_total_steps > 0) json_doc["exvB"] = ((EXV2.currentStep * 100) / EXV2.exv_total_steps);
  json_doc["modeB"] = EXV2.Mode;
  json_doc["PidPB"] = EXV2.Kp;
  json_doc["PidIB"] = EXV2.Ki;
  json_doc["PidDB"] = EXV2.Kd;

  json_doc["SucPsiSpB"] = CompB.suc_pres_low_SV;
  json_doc["DisPsiSpB"] = CompB.dis_pres_high_SV;
  json_doc["SucTempSpB"] = CompB.suc_temp_high_SV;
  json_doc["DisTempSpB"] = CompB.dis_temp_high_SV;
  json_doc["SprayTempSpB"] = CompB.spray_temp_low_SV;

  json_doc["superheatspB"] = CompB.SuperHeatSV;
  json_doc["stepsizeB"] = CompB.step_size;
  json_doc["startdelayB"] = CompB.Starting_delay;
  json_doc["vfdDelayB"] = CompB.VFD_Step_Delay;

  json_doc["SucTempB"] = String(CompB.SuctionTemp, 1);
  json_doc["SucPsiB"] = CompB.suctionPressure;
  json_doc["DisTempB"] = String(CompB.dischargeTemp, 1);
  json_doc["DisPsiB"] = CompB.dischargePressure;
  json_doc["subCoolingB"] = String(CompB.SubCoolTemp, 1);
  json_doc["sprayB"] = String(CompB.SprayTemp, 1);
  json_doc["shtB"] = CompB.SuperHeatPV;

  json_doc["runHoursB"] = CompB.Run_hrs;
  json_doc["fan1EB"] = CompB.FAN1_ENABLE;
  json_doc["fan3EB"] = CompB.FAN3_ENABLE;
  json_doc["fan5EB"] = CompB.FAN5_ENABLE;
  json_doc["fan1HB"] = CompB.Fan1_ON_PSI;
  json_doc["fan3HB"] = CompB.Fan3_ON_PSI;
  json_doc["fan5HB"] = CompB.Fan5_ON_PSI;
  json_doc["fan1LB"] = CompB.Fan1_OFF_PSI;
  json_doc["fan3LB"] = CompB.Fan3_OFF_PSI;
  json_doc["fan5LB"] = CompB.Fan5_OFF_PSI;

  json_doc["vfdMinFreB"] = CompB.min_vfd_freq;
  json_doc["vfdMaxFreB"] = CompB.max_vfd_freq;

  json_doc["sucPreTypeB"] = CompB.suction_pressure_type;
  json_doc["sucPreRangeB"] = CompB.suction_pressure_range;
  json_doc["sucPreUnitB"] = CompB.suction_pressure_unit;
  json_doc["sucoffsetB"] = CompB.suction_offset;

  json_doc["disPreTypeB"] = CompB.discharge_pressure_type;
  json_doc["disPreRangeB"] = CompB.discharge_pressure_range;
  json_doc["disPreUnitB"] = CompB.discharge_pressure_unit;
  json_doc["disoffsetB"] = CompB.discharge_offset;

  char doc[2048];
  serializeJson(json_doc, doc);

  String device_topic_pressure_config = device_topic_p + "/CircuitB";
  client.publish(device_topic_pressure_config.c_str(), doc, true);
}

void DEVICE_INIT() {
  Serial.println("From Prefrences");

  preferences.begin("values", false);
  ReturnSp = preferences.getInt("setPoint", 0);
  // PowerSwitch = preferences.getInt("powerSwitch", 0);
  thermostat_selection = preferences.getInt("tempSelect", 0);

  temp1Assigned = preferences.getBool("temp1_assign", false);
  size_t len_1 = preferences.getBytes("temp1Address", temp1Address, sizeof(DeviceAddress));
  temp2Assigned = preferences.getBool("temp2_assign", false);
  size_t len_2 = preferences.getBytes("temp2Address", temp2Address, sizeof(DeviceAddress));
  temp3Assigned = preferences.getBool("temp3_assign", false);
  size_t len_3 = preferences.getBytes("temp3Address", temp3Address, sizeof(DeviceAddress));
  temp4Assigned = preferences.getBool("temp4_assign", false);
  size_t len_4 = preferences.getBytes("temp4Address", temp4Address, sizeof(DeviceAddress));
  temp5Assigned = preferences.getBool("temp5_assign", false);
  size_t len_5 = preferences.getBytes("temp5Address", temp5Address, sizeof(DeviceAddress));
  temp6Assigned = preferences.getBool("temp6_assign", false);
  size_t len_6 = preferences.getBytes("temp6Address", temp6Address, sizeof(DeviceAddress));

  offset1 = preferences.getInt("offset1", 0);
  offset2 = preferences.getInt("offset2", 0);
  offset3 = preferences.getInt("offset3", 0);
  offset4 = preferences.getInt("offset4", 0);
  offset5 = preferences.getInt("offset5", 0);
  offset6 = preferences.getInt("offset6", 0);

  suction_temp_offset = offset1;
  discharge_temp_offset = offset2;
  subcool_temp_offset = offset3;
  spray_temp_offset = offset4;
  supply_temp_offset = offset5;
  return_air_temp_offset = offset6;

  preferences.end();


  preferences.begin("CompA", false);
  CompA.COMP_ENABLE = preferences.getInt("enableA", 0);

  if (CompA.COMP_ENABLE == 1) {

    CompA.Run_hrs = preferences.getInt("runHrs", 0);
    EXV1.Kp = preferences.getFloat("PidPA", 0.0);
    EXV1.Ki = preferences.getFloat("PidIA", 0.0);
    EXV1.Kd = preferences.getFloat("PidDA", 0.0);
    CompA.step_size = preferences.getFloat("stepsizeA", 0.0);

    CompA.amps_high_Sp = preferences.getInt("ampSpA", 0);

    CompA.EXV_ENABLE = preferences.getInt("ExvselA", 0);
    EXV1.LowerLimit = preferences.getInt("minA", 0);
    EXV1.UpperLimit = preferences.getInt("maxA", 0);
    EXV1.Mode = preferences.getInt("modeA", 0);  // Manual / Auto
    EXV1.exv_total_steps = preferences.getInt("exvMstepA", 0);
    EXV1.exv_step_delay = preferences.getInt("exvStepDA", 0);

    CompA.SuperHeatSV = preferences.getInt("superheatspA", 0);
    LeadSW = preferences.getInt("leadlagA", 0);
    CompA.Starting_delay = preferences.getInt("startdelayA", 0);
    CompA.gas_index = preferences.getInt("gasA", 0);
    CompA.driveSelection = preferences.getInt("driveA", 0);  // SD

    CompA.min_vfd_freq = preferences.getInt("vfdMinFreA", 0);
    CompA.max_vfd_freq = preferences.getInt("vfdMaxFreA", 0);
    CompA.VFD_Step_Delay = preferences.getInt("vfdDelayA", 0);

    CompA.suc_pres_low_SV = preferences.getInt("SucPsiSpA", 0);
    CompA.dis_pres_high_SV = preferences.getInt("DisPsiSpA", 0);
    CompA.oil_pres_low_SV = preferences.getInt("oilPsiSpA", 0);

    CompA.suc_temp_high_SV = preferences.getInt("SucTempSpA", 0);
    CompA.dis_temp_high_SV = preferences.getInt("DisTempSpA", 0);
    CompA.spray_temp_low_SV = preferences.getInt("SprayTempSpA", 0);

    CompA.suction_pressure_type = preferences.getInt("sucPreTypeA", 0);
    CompA.suction_pressure_range = preferences.getInt("sucPreRangeA", 0);
    CompA.suction_pressure_unit = preferences.getInt("sucPreUnitA", 0);
    CompA.suction_offset = preferences.getInt("sucoffsetA", 0);

    CompA.discharge_pressure_type = preferences.getInt("disPreTypeA", 0);
    CompA.discharge_pressure_range = preferences.getInt("disPreRangeA", 0);
    CompA.discharge_pressure_unit = preferences.getInt("disPreUnitA", 0);
    CompA.discharge_offset = preferences.getInt("disoffsetA", 0);

    CompA.FAN1_ENABLE = preferences.getInt("fan1EA", 0);
    CompA.FAN3_ENABLE = preferences.getInt("fan3EA", 0);
    CompA.FAN5_ENABLE = preferences.getInt("fan5EA", 0);
    CompA.Fan1_ON_PSI = preferences.getInt("fan1HA", 80);
    CompA.Fan3_ON_PSI = preferences.getInt("fan3HA", 80);
    CompA.Fan5_ON_PSI = preferences.getInt("fan5HA", 80);
    CompA.Fan1_OFF_PSI = preferences.getInt("fan1LA", 60);
    CompA.Fan3_OFF_PSI = preferences.getInt("fan3LA", 60);
    CompA.Fan5_OFF_PSI = preferences.getInt("fan5LA", 60);



    if (CompA.suction_pressure_unit) {                    // psi
      CompA.sensorMax = CompA.suction_pressure_range;     // max 0-725psi
    } else {                                              // bar
      float range = CompA.suction_pressure_range * 14.7;  // max 0-50 bar
      CompA.sensorMax = (int)range;
    }
    if (CompA.discharge_pressure_unit) {                    // psi
      CompA.sensorMax2 = CompA.discharge_pressure_range;    // max 0-725psi
    } else {                                                // bar
      float range = CompA.discharge_pressure_range * 14.7;  // max 0-50 bar
      CompA.sensorMax2 = (int)range;
    }
  }
  preferences.end();

  // ------------------- CompB / EXV2 -------------------
  preferences.begin("CompB", false);
  CompB.COMP_ENABLE = preferences.getInt("enableB", 0);

  if (CompB.COMP_ENABLE == 1) {
    CompB.Run_hrs = preferences.getInt("runHrs", 0);

    EXV2.Kp = preferences.getFloat("PidPB", 0.0);
    EXV2.Ki = preferences.getFloat("PidIB", 0.0);
    EXV2.Kd = preferences.getFloat("PidDB", 0.0);
    CompB.step_size = preferences.getFloat("stepsizeB", 0.0);

    CompB.EXV_ENABLE = preferences.getInt("ExvselB", 0);
    EXV2.LowerLimit = preferences.getInt("minB", 0);
    EXV2.UpperLimit = preferences.getInt("maxB", 0);
    EXV2.Mode = preferences.getInt("modeB", 0);  // Manual / Auto
    EXV2.exv_total_steps = preferences.getInt("exvMstepB", 0);
    EXV2.exv_step_delay = preferences.getInt("exvStepDB", 0);

    CompB.SuperHeatSV = preferences.getInt("superheatspB", 0);
    CompB.Starting_delay = preferences.getInt("startdelayB", 0);
    CompB.gas_index = preferences.getInt("gasB", 0);
    CompB.driveSelection = preferences.getInt("driveB", 0);  // SD

    CompB.min_vfd_freq = preferences.getInt("vfdMinFreB", 0);
    CompB.max_vfd_freq = preferences.getInt("vfdMaxFreB", 0);
    CompB.VFD_Step_Delay = preferences.getInt("vfdDelayB", 0);

    CompB.suc_pres_low_SV = preferences.getInt("SucPsiSpB", 0);
    CompB.dis_pres_high_SV = preferences.getInt("DisPsiSpB", 0);
    CompB.suc_temp_high_SV = preferences.getInt("SucTempSpB", 0);
    CompB.dis_temp_high_SV = preferences.getInt("DisTempSpB", 0);
    CompB.spray_temp_low_SV = preferences.getInt("SprayTempSpB", 0);

    CompB.suction_pressure_type = preferences.getInt("sucPreTypeB", 0);
    CompB.suction_pressure_range = preferences.getInt("sucPreRangeB", 0);
    CompB.suction_pressure_unit = preferences.getInt("sucPreUnitB", 0);
    CompB.suction_offset = preferences.getInt("sucoffsetB", 0);
    CompB.discharge_pressure_type = preferences.getInt("disPreTypeB", 0);
    CompB.discharge_pressure_range = preferences.getInt("disPreRangeB", 0);
    CompB.discharge_pressure_unit = preferences.getInt("disPreUnitB", 0);
    CompB.discharge_offset = preferences.getInt("disoffsetB", 0);

    CompB.FAN1_ENABLE = preferences.getInt("fan1EB", 0);
    CompB.FAN3_ENABLE = preferences.getInt("fan3EB", 0);
    CompB.FAN5_ENABLE = preferences.getInt("fan5EB", 0);
    CompB.Fan1_ON_PSI = preferences.getInt("fan1HB", 80);
    CompB.Fan3_ON_PSI = preferences.getInt("fan3HB", 80);
    CompB.Fan5_ON_PSI = preferences.getInt("fan5HB", 80);
    CompB.Fan1_OFF_PSI = preferences.getInt("fan1LB", 60);
    CompB.Fan3_OFF_PSI = preferences.getInt("fan3LB", 60);
    CompB.Fan5_OFF_PSI = preferences.getInt("fan5LB", 60);

    // ------------------- Sensor Max Values -------------------
    if (CompB.suction_pressure_unit) {  // psi
      CompB.sensorMax = CompB.suction_pressure_range;
    } else {  // bar
      float range = CompB.suction_pressure_range * 14.7;
      CompB.sensorMax = (int)range;
    }

    if (CompB.discharge_pressure_unit) {  // psi
      CompB.sensorMax2 = CompB.discharge_pressure_range;
    } else {  // bar
      float range = CompB.discharge_pressure_range * 14.7;
      CompB.sensorMax2 = (int)range;
    }
  }
  preferences.end();
}

void callback(char* topic, byte* message, unsigned int length) {
  String messageTemp;

  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }

  Serial.println("Message Received on: ");
  Serial.println(String(topic));

  if (String(topic) == device_topic_s_1) {  // /test/DM-AAA001/1
    message_received = true;
    Extract_by_json(messageTemp);
  }

  else if (String(topic) == device_topic_s_2) {  // /test/DM-AAA001/2
    message_received_configA = true;
    Extract_circuitA_config(messageTemp);
  }

  else if (String(topic) == device_topic_s_3) {  // /test/DM-AAA001/3
    message_received_configB = true;
    Extract_circuitB_config(messageTemp);
  }

  else if (String(topic) == device_topic_s_4) {  // /test/DM-AAA001/4
    message_received_temp_config = true;
    Extract_temp_config(messageTemp);
  }

  else if (String(topic) == device_topic_s_5) {  // /test/DM-AAA001/4
    message_received_temp_configB = true;
    Extract_temp_configB(messageTemp);
  }

}

void reconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi is not connected. Attempting to reconnect...");
    return;
  }
  if (!client.connected()) {
    espClient.setCACert(root_ca);
    espClient.setCertificate(client_cert);
    espClient.setPrivateKey(client_key);
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    Serial.print("Attempting MQTT connection...");

    if (client.connect(devicename.c_str())) {
      Serial.println("connected");
      client.subscribe(device_topic_s_m.c_str());

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 10 milliseconds");
    }
  }
}