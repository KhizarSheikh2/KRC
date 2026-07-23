bool compareAddresses(DeviceAddress a, DeviceAddress b) {
  for (uint8_t i = 0; i < 8; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
  Serial.println();
}

void load_temperature_sensor_addresses() {
  struct SensorStorage {
    const char* addressKey;
    DeviceAddress* address;
    bool* assigned;
  };

  SensorStorage storedSensors[6] = {
    { "temp1Address", &temp1Address, &temp1Assigned },
    { "temp2Address", &temp2Address, &temp2Assigned },
    { "temp3Address", &temp3Address, &temp3Assigned },
    { "temp4Address", &temp4Address, &temp4Assigned },
    { "temp5Address", &temp5Address, &temp5Assigned },
    { "temp6Address", &temp6Address, &temp6Assigned }
  };

  for (int i = 0; i < 6; i++) {
    DeviceAddress& address = *storedSensors[i].address;

    if (!*storedSensors[i].assigned ||
        preferences.getBytes(storedSensors[i].addressKey, address, sizeof(DeviceAddress)) != sizeof(DeviceAddress)) {
      *storedSensors[i].assigned = false;
      memset(address, 0x00, sizeof(DeviceAddress));
    }
  }
}


void assignSensor(String tempVar, DeviceAddress sensorAddress) {
  preferences.begin("values", false);
#ifdef DEBUG
  printAddress(sensorAddress);
#endif
  if (tempVar == "temp1") {
    memcpy(temp1Address, sensorAddress, sizeof(DeviceAddress));
    temp1Assigned = true;
    preferences.putBool("temp1_assign", temp1Assigned);
    preferences.putBytes("temp1Address", sensorAddress, 8);
  }
  if (tempVar == "temp2") {
    memcpy(temp2Address, sensorAddress, sizeof(DeviceAddress));
    temp2Assigned = true;
    preferences.putBool("temp2_assign", temp2Assigned);
    preferences.putBytes("temp2Address", sensorAddress, 8);
  }
  if (tempVar == "temp3") {
    memcpy(temp3Address, sensorAddress, sizeof(DeviceAddress));
    temp3Assigned = true;
    preferences.putBool("temp3_assign", temp3Assigned);
    preferences.putBytes("temp3Address", sensorAddress, 8);
  }
  if (tempVar == "temp4") {
    memcpy(temp4Address, sensorAddress, sizeof(DeviceAddress));
    temp4Assigned = true;
    preferences.putBool("temp4_assign", temp4Assigned);
    preferences.putBytes("temp4Address", sensorAddress, 8);
  }
  if (tempVar == "temp5") {
    memcpy(temp5Address, sensorAddress, sizeof(DeviceAddress));
    temp5Assigned = true;
    preferences.putBool("temp5_assign", temp5Assigned);
    preferences.putBytes("temp5Address", sensorAddress, 8);
  }
  if (tempVar == "temp6") {
    memcpy(temp6Address, sensorAddress, sizeof(DeviceAddress));
    temp6Assigned = true;
    preferences.putBool("temp6_assign", temp6Assigned);
    preferences.putBytes("temp6Address", sensorAddress, 8);
  }
  preferences.end();
}


void assignSensor(int sensor_sel, DeviceAddress sensorAddress) {
  preferences.begin("values", false);
#ifdef DEBUG
  printAddress(sensorAddress);
#endif

  // sdsr
  if (sensor_sel == 3) {
    memcpy(temp1Address, sensorAddress, sizeof(DeviceAddress));
    supply_temp_offset = offset1;

    temp1Assigned = true;
    preferences.putBool("temp1_assign", temp1Assigned);
    preferences.putBytes("temp1Address", sensorAddress, 8);
    // preferences.putInt("temp1Address", sensorAddress, 8);
  }
  if (sensor_sel == 1) {
    memcpy(temp2Address, sensorAddress, sizeof(DeviceAddress));
    temp2Assigned = true;
    suction_temp_offset = offset2;
    preferences.putBool("temp2_assign", temp2Assigned);
    preferences.putBytes("temp2Address", sensorAddress, 8);
  }
  if (sensor_sel == 4) {
    memcpy(temp3Address, sensorAddress, sizeof(DeviceAddress));
    temp3Assigned = true;
    return_air_temp_offset = offset3;
    preferences.putBool("temp3_assign", temp3Assigned);
    preferences.putBytes("temp3Address", sensorAddress, 8);
  }
  if (sensor_sel == 2) {
    memcpy(temp4Address, sensorAddress, sizeof(DeviceAddress));
    temp4Assigned = true;
    discharge_temp_offset = offset4;
    preferences.putBool("temp4_assign", temp4Assigned);
    preferences.putBytes("temp4Address", sensorAddress, 8);
  }
  if (sensor_sel == 5) {
    memcpy(temp5Address, sensorAddress, sizeof(DeviceAddress));
    temp5Assigned = true;
    oil_temp_offset = offset5;
    preferences.putBool("temp5_assign", temp5Assigned);
    preferences.putBytes("temp5Address", sensorAddress, 8);
  }
  if (sensor_sel == 6) {
    memcpy(temp6Address, sensorAddress, sizeof(DeviceAddress));
    temp6Assigned = true;
    other_temp_offset = offset6;
    preferences.putBool("temp6_assign", temp6Assigned);
    preferences.putBytes("temp6Address", sensorAddress, 8);
  }
  if (sensor_sel == 0) {

    if (compareAddresses(sensorAddress, temp1Address)) {
      temp1Assigned = false;
      memset(temp1Address, 0x00, sizeof(temp1Address));
      preferences.putBool("temp1_assign", temp1Assigned);
      preferences.putBytes("temp1Address", temp1Address, sizeof(temp1Address));
    } else if (compareAddresses(sensorAddress, temp2Address)) {
      temp2Assigned = false;
      memset(temp2Address, 0x00, sizeof(temp2Address));
      preferences.putBool("temp2_assign", temp2Assigned);
      preferences.putBytes("temp2Address", temp2Address, sizeof(temp2Address));
    } else if (compareAddresses(sensorAddress, temp3Address)) {
      temp3Assigned = false;
      memset(temp3Address, 0x00, sizeof(temp3Address));
      preferences.putBool("temp3_assign", temp3Assigned);
      preferences.putBytes("temp3Address", temp3Address, sizeof(temp3Address));
    } else if (compareAddresses(sensorAddress, temp4Address)) {
      temp4Assigned = false;
      memset(temp4Address, 0x00, sizeof(temp4Address));
      preferences.putBool("temp4_assign", temp4Assigned);
      preferences.putBytes("temp4Address", temp4Address, sizeof(temp4Address));
    } else if (compareAddresses(sensorAddress, temp5Address)) {
      temp5Assigned = false;
      memset(temp5Address, 0x00, sizeof(temp5Address));
      preferences.putBool("temp5_assign", temp5Assigned);
      preferences.putBytes("temp5Address", temp5Address, sizeof(temp5Address));
    } else if (compareAddresses(sensorAddress, temp6Address)) {
      temp6Assigned = false;
      memset(temp6Address, 0x00, sizeof(temp6Address));
      preferences.putBool("temp6_assign", temp6Assigned);
      preferences.putBytes("temp6Address", temp6Address, sizeof(temp6Address));
    }
  }
  preferences.end();
}

String getAddressString(DeviceAddress deviceAddress) {
  String address = "";
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) address += "0";
    address += String(deviceAddress[i], HEX);
  }
  return address;
}


void setoffset(DeviceAddress tempSensorAddresses, int16_t offset_received) {
  if (compareAddresses(tempSensorAddresses, temp1Address)) {
    supply_temp_offset = offset_received;
  } else if (compareAddresses(tempSensorAddresses, temp2Address)) {
    suction_temp_offset = offset_received;
  } else if (compareAddresses(tempSensorAddresses, temp3Address)) {
    return_air_temp_offset = offset_received;
  } else if (compareAddresses(tempSensorAddresses, temp4Address)) {
    discharge_temp_offset = offset_received;
  } else if (compareAddresses(tempSensorAddresses, temp5Address)) {
    oil_temp_offset = offset_received;
  } else if (compareAddresses(tempSensorAddresses, temp6Address)) {
    other_temp_offset = offset_received;
  }
}

void read_temp(float& temp, DeviceAddress deviceaddress) {
  // The main one-second sampling cycle already starts DS18B20 conversions.
  // Read the latest completed scratchpad value here without starting another
  // conversion for every address in the MQTT configuration packet.
  temp = sensors.getTempC(deviceaddress);
}
