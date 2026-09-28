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

// void load_temperature_sensor_addresses() {
//   for (int i = 0; i < 3; i++) {
//     switch (i) {
//       case 0:
//         // memcpy(temp1Address, tempAddress, sizeof(DeviceAddress));
//         // temp1Assigned = true;
//         if (preferences.getBytes("temp1Address", temp1Address, sizeof(temp1Address)) == 0) {
//           Serial.println("No data found, setting address to 00000000 00000000");
//           memset(temp1Address, 0x00, sizeof(temp1Address));  // Set all bytes to 0x00
//         }
//         break;
//       case 1:
//         // memcpy(temp2Address, tempAddress, sizeof(DeviceAddress));
//         // temp2Assigned = true;
//         if (preferences.getBytes("temp2Address", temp2Address, sizeof(temp2Address)) == 0) {
//           Serial.println("No data found, setting address to 00000000 00000000");
//           memset(temp2Address, 0x00, sizeof(temp2Address));  // Set all bytes to 0x00
//         }
//         break;
//       case 2:
//         // memcpy(temp3Address, tempAddress, sizeof(DeviceAddress));
//         temp3Assigned = true;
//         if (preferences.getBytes("temp3Address", temp3Address, sizeof(temp3Address)) == 0) {
//           Serial.println("No data found, setting address to 00000000 00000000");
//           memset(temp3Address, 0x00, sizeof(temp3Address));  // Set all bytes to 0x00
//         }

//         break;
//     }
//   }
// }

// void assignSensor(String tempVar, DeviceAddress sensorAddress) {
//   preferences.begin("values", false);
//   printAddress(sensorAddress);

//   if (tempVar == "temp1") {
//     memcpy(temp1Address, sensorAddress, sizeof(DeviceAddress));
//     temp1Assigned = true;
//     preferences.putBool("temp1_assign", temp1Assigned);
//     preferences.putBytes("temp1Address", sensorAddress, 8);
//   }
//   if (tempVar == "temp2") {
//     memcpy(temp2Address, sensorAddress, sizeof(DeviceAddress));
//     temp2Assigned = true;
//     preferences.putBool("temp2_assign", temp2Assigned);
//     preferences.putBytes("temp2Address", sensorAddress, 8);
//   }
//   if (tempVar == "temp3") {
//     memcpy(temp3Address, sensorAddress, sizeof(DeviceAddress));
//     temp3Assigned = true;
//     preferences.putBool("temp3_assign", temp3Assigned);
//     preferences.putBytes("temp3Address", sensorAddress, 8);
//   }
//   if (tempVar == "temp4") {
//     memcpy(temp4Address, sensorAddress, sizeof(DeviceAddress));
//     temp4Assigned = true;
//     preferences.putBool("temp4_assign", temp4Assigned);
//     preferences.putBytes("temp4Address", sensorAddress, 8);
//   }
//   preferences.end();
// }

void assignSensor(int sensor_sel, DeviceAddress sensorAddress) {
  preferences.begin("values", false);

  printAddress(sensorAddress);

  // // sdsr
  // if (sensor_sel == 3) {
  //   memcpy(temp1Address, sensorAddress, sizeof(DeviceAddress));
  //   supply_temp_offset = offset1;
  //   temp1Assigned = true;

  //   preferences.putBool("temp1_assign", temp1Assigned);
  //   preferences.putBytes("temp1Address", sensorAddress, 8);
  //   // preferences.putInt("temp1Address", sensorAddress, 8);
  // }
  // if (sensor_sel == 1) {
  //   memcpy(temp2Address, sensorAddress, sizeof(DeviceAddress));
  //   temp2Assigned = true;
  //   suction_temp_offset = offset2;
  //   preferences.putBool("temp2_assign", temp2Assigned);
  //   preferences.putBytes("temp2Address", sensorAddress, 8);
  // }
  // if (sensor_sel == 4) {
  //   memcpy(temp3Address, sensorAddress, sizeof(DeviceAddress));
  //   temp3Assigned = true;
  //   return_air_temp_offset = offset3;
  //   preferences.putBool("temp3_assign", temp3Assigned);
  //   preferences.putBytes("temp3Address", sensorAddress, 8);
  // }
  // if (sensor_sel == 2) {
  //   memcpy(temp4Address, sensorAddress, sizeof(DeviceAddress));
  //   temp4Assigned = true;
  //   discharge_temp_offset = offset4;
  //   preferences.putBool("temp4_assign", temp4Assigned);
  //   preferences.putBytes("temp4Address", sensorAddress, 8);
  // }
  // if (sensor_sel == 0) {

  //   if (compareAddresses(sensorAddress, temp1Address)) {
  //     temp1Assigned = false;
  //     memset(temp1Address, 0x00, sizeof(temp1Address));
  //     preferences.putBool("temp1_assign", temp1Assigned);
  //     preferences.putBytes("temp1Address", 0, 8);
  //   } else if (compareAddresses(sensorAddress, temp2Address)) {
  //     temp2Assigned = false;
  //     memset(temp2Address, 0x00, sizeof(temp2Address));
  //     preferences.putBool("temp2_assign", temp2Assigned);
  //     preferences.putBytes("temp2Address", 0, 8);
  //   } else if (compareAddresses(sensorAddress, temp3Address)) {
  //     temp3Assigned = false;
  //     memset(temp3Address, 0x00, sizeof(temp3Address));
  //     preferences.putBool("temp3_assign", temp3Assigned);
  //     preferences.putBytes("temp3Address", 0, 8);
  //   } else if (compareAddresses(sensorAddress, temp4Address)) {
  //     temp4Assigned = false;
  //     memset(temp4Address, 0x00, sizeof(temp4Address));
  //     preferences.putBool("temp4_assign", temp4Assigned);
  //     preferences.putBytes("temp4Address", 0, 8);
  //   }
  // }

  if (sensor_sel == 1) {
    memcpy(temp1Address, sensorAddress, sizeof(DeviceAddress));
    discharge_temp_offset = offset1;

    temp1Assigned = true;
    preferences.putBool("temp1_assign", temp1Assigned);
    preferences.putBytes("temp1Address", sensorAddress, 8);
  }
  else if (sensor_sel == 2) {
    memcpy(temp2Address, sensorAddress, sizeof(DeviceAddress));
    temp2Assigned = true;
    supply_temp_offset = offset2;
    preferences.putBool("temp2_assign", temp2Assigned);
    preferences.putBytes("temp2Address", sensorAddress, 8);
  }
  else if (sensor_sel == 3) {
    memcpy(temp3Address, sensorAddress, sizeof(DeviceAddress));
    temp3Assigned = true;
    return_air_temp_offset = offset3;
    preferences.putBool("temp3_assign", temp3Assigned);
    preferences.putBytes("temp3Address", sensorAddress, 8);
  }
  else if (sensor_sel == 0) {

    if (compareAddresses(sensorAddress, temp1Address)) {
      temp1Assigned = false;
      memset(temp1Address, 0x00, sizeof(temp1Address));
      preferences.putBool("temp1_assign", temp1Assigned);
      preferences.putBytes("temp1Address", temp1Address, 8);
    } else if (compareAddresses(sensorAddress, temp2Address)) {
      temp2Assigned = false;
      memset(temp2Address, 0x00, sizeof(temp2Address));
      preferences.putBool("temp2_assign", temp2Assigned);
      preferences.putBytes("temp2Address", temp2Address, 8);
    } else if (compareAddresses(sensorAddress, temp3Address)) {
      temp3Assigned = false;
      memset(temp3Address, 0x00, sizeof(temp3Address));
      preferences.putBool("temp3_assign", temp3Assigned);
      preferences.putBytes("temp3Address", temp3Address, 8);
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
    discharge_temp_offset = offset_received;
  } else if (compareAddresses(tempSensorAddresses, temp2Address)) {
    supply_temp_offset = offset_received;
  } else if (compareAddresses(tempSensorAddresses, temp3Address)) {
    return_air_temp_offset = offset_received;
  }
}

void read_temp(float& temp, DeviceAddress deviceaddress) {
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();
  sensors.setWaitForConversion(true);
  temp = sensors.getTempC(deviceaddress);
}
