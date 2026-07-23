
void publishJson() {
  StaticJsonDocument<1536> doc;

  // Live temperatures and configured temperature setpoints.
  doc["returnlinetemp"] = String(ReturnTempC, 1);
  doc["returnlinetempF"] = String(ReturnTemp);
  doc["supplylinetemp"] = String(SupplyTempC, 1);
  doc["supplylinetempF"] = String(SupplyTemp);
  doc["suctionlinetemp"] = String(SuctionTempC, 1);
  doc["suctionlinetempF"] = String(SuctionTemp);
  doc["dischargelinetemp"] = String(dischargeTempC, 1);
  doc["dischargelinetempF"] = String(dischargeTemp);
  doc["oillinetemp"] = String(OilTempC, 1);
  doc["oillinetempF"] = String(OilTemp);

  doc["tempsphigh"] = String(ReturnAlertSp);
  doc["tempsplow"] = String(ReturnSp);
  doc["tempsp2"] = String(suctionAlertSp);
  doc["tempsp1"] = String(dischargeAlertSp);
  doc["oilsp"] = String(oilSp);

  // Pressure values: the legacy *F keys are retained because the mobile app
  // uses them for bar values.
  doc["suctionpressure"] = suctionPressure;
  doc["suctionpressureF"] = suctionPressureB;
  doc["dischargepressure"] = dischargePressure;
  doc["dischargepressureF"] = dischargePressureB;
  doc["pressuresp1"] = String(suctionPsiSp);
  doc["pressuresp2"] = String(dischargePsiSp);
  doc["gastype"] = gas_selection;

  doc["comprsw"] = String(comp_Status);
  doc["highpresw"] = HPS;
  doc["lowpresw"] = LPS;
  doc["oilpressure"] = OPS;

  doc["mac_address"] = macaddress;
  doc["ip_address"] = myIP;
  doc["ssid"] = ssid;
  doc["password"] = password;

  if (doc.overflowed()) {
    Serial.println("MQTT telemetry JSON document overflowed.");
    return;
  }

  char jsonBuffer[2048];
  const size_t jsonLength = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
  if (jsonLength == 0 || jsonLength >= sizeof(jsonBuffer)) {
    Serial.println("MQTT telemetry JSON buffer is too small.");
    return;
  }

  if (!client.publish(device_topic_p.c_str(), jsonBuffer, true)) {
    Serial.println("AM3 telemetry MQTT publish failed.");
  }
}

void update_values_from_json(String key, int& value_variable) {
  if (received_doc.containsKey(key)) {
    if (received_doc[key].as<int>() != value_variable) {
      value_variable = received_doc[key].as<int>();
      preferences.putInt(key.c_str(), value_variable);
    }
  }
}

void Extract_by_json(String incomingMessage) {
  DeserializationError error = deserializeJson(received_doc, incomingMessage);
  if (error) {
    Serial.print("JSON deserialization failed: ");
    Serial.println(error.c_str());
    return;
  }
  // Serial.println(incomingMessage);

  preferences.begin("values", false);
  update_values_from_json("tempsp1", dischargeAlertSp);  // discharge temp sp
  update_values_from_json("tempsp2", suctionAlertSp);    // suction temp sp
  update_values_from_json("tempsplow", ReturnSp);        // target temp
  update_values_from_json("tempsphigh", ReturnAlertSp);  // return temp sp
  update_values_from_json("oilsp", oilSp);                 // oil temp sp

  update_values_from_json("pressuresp1", suctionPsiSp);    // suction press sp
  update_values_from_json("pressuresp2", dischargePsiSp);  // discharge press sp
  update_values_from_json("gastype", gas_selection);       // GAS SELECTION

  preferences.end();

  message_received = true;
}

void temp_sensor_select_publish() {
  DynamicJsonDocument json_doc(2048);

  // Publish discovered sensors and their current role selections.
  float temperature_read = SENSOR_DISCONNECTED;
  String addressString;

  const int publishDeviceCount = (numberOfDevices > 5) ? 5 : numberOfDevices;
  for (int i = 0; i < publishDeviceCount; i++) {
    read_temp(temperature_read, tempSensorAddresses[i]);
    addressString = getAddressString(tempSensorAddresses[i]);

    const String addressKey = "address" + String(i + 1);
    const String temperatureKey = "temp" + String(i + 1);
    json_doc[addressKey] = addressString;
    json_doc[temperatureKey] = temperature_read;

    int sensorRole = 0;
    if (temp1Assigned && compareAddresses(tempSensorAddresses[i], temp1Address)) {
      sensorRole = 3;  // supply
    } else if (temp2Assigned && compareAddresses(tempSensorAddresses[i], temp2Address)) {
      sensorRole = 1;  // suction
    } else if (temp3Assigned && compareAddresses(tempSensorAddresses[i], temp3Address)) {
      sensorRole = 4;  // return
    } else if (temp4Assigned && compareAddresses(tempSensorAddresses[i], temp4Address)) {
      sensorRole = 2;  // discharge
    } else if (temp5Assigned && compareAddresses(tempSensorAddresses[i], temp5Address)) {
      sensorRole = 5;  // oil
    }
    json_doc[addressString] = sensorRole;
  }

  json_doc["offset1"] = offset1;
  json_doc["offset2"] = offset2;
  json_doc["offset3"] = offset3;
  json_doc["offset4"] = offset4;
  json_doc["offset5"] = offset5;

  json_doc["ftoC"] = ftoc;
  json_doc["psiTobar"] = psitobar;
  json_doc["returnTempSelection"] = returnTempSelection;
  json_doc["restartdelay"] = Comp_restart_delay;
  json_doc["startdelay"] = Comp_Start_delay;
  json_doc["autoSwitch"] = autoReset;
  json_doc["oilsw"] = OPS_SW;
  json_doc["suctionsw"] = LPS_SW;
  json_doc["dischargesw"] = HPS_SW;
  json_doc["mode"] = modesw;
  json_doc["switchEn"] = startSWEnable;
  json_doc["switch"] = StartSW_App;

  if (json_doc.overflowed()) {
    Serial.println("Temperature configuration JSON document overflowed.");
    return;
  }

  char outputBuffer[2048];
  const size_t jsonLength = serializeJson(json_doc, outputBuffer, sizeof(outputBuffer));
  if (jsonLength == 0 || jsonLength >= sizeof(outputBuffer)) {
    Serial.println("Temperature configuration JSON buffer is too small.");
    return;
  }

  const String device_topic_temp_config = device_topic_p + "/temperature_config_AM3";
  if (!client.publish(device_topic_temp_config.c_str(), outputBuffer, true)) {
    Serial.println("Temperature configuration MQTT publish failed.");
  }
}

void Extract_temp_config(String incoming_message) {
  DeserializationError error = deserializeJson(received_doc, incoming_message);
  if (error) {
    return;
  }

  // Save general configuration values in one Preferences session.
  preferences.begin("values", false);
  update_values_from_json("ftoC", ftoc);
  update_values_from_json("psiTobar", psitobar);
  update_values_from_json("switch", StartSW_App);
  update_values_from_json("switchEn", startSWEnable);
  update_values_from_json("mode", modesw);
  update_values_from_json("autoSwitch", autoReset);
  update_values_from_json("startdelay", Comp_Start_delay);
  update_values_from_json("restartdelay", Comp_restart_delay);
  update_values_from_json("oilsw", OPS_SW);
  update_values_from_json("suctionsw", LPS_SW);
  update_values_from_json("dischargesw", HPS_SW);
  update_values_from_json("returnTempSelection", returnTempSelection);

  preferences.end();
  Comp_restart_delay_ms = (Comp_restart_delay > 0)
                            ? static_cast<unsigned long>(Comp_restart_delay) * 1000UL
                            : 0UL;

  // assignSensor() manages its own Preferences session.
  String addressString;
  int8_t sensor_selected;
  for (int i = 0; i < numberOfDevices; i++) {
    addressString = getAddressString(tempSensorAddresses[i]);
    if (received_doc.containsKey(addressString)) {
      sensor_selected = received_doc[addressString].as<int>();
      assignSensor(sensor_selected, tempSensorAddresses[i]);
    }
  }

  // Save offsets by assigned sensor role, not by OneWire discovery index.
  preferences.begin("values", false);
  String key_offset;
  for (int i = 0; i < numberOfDevices; i++) {
    addressString = getAddressString(tempSensorAddresses[i]);
    key_offset = "offset" + addressString;

    if (!received_doc.containsKey(key_offset)) {
      continue;
    }

    offset = received_doc[key_offset].as<int>();

    if (temp1Assigned && compareAddresses(tempSensorAddresses[i], temp1Address)) {
      offset1 = offset;
      preferences.putInt("offset1", offset1);
    } else if (temp2Assigned && compareAddresses(tempSensorAddresses[i], temp2Address)) {
      offset2 = offset;
      preferences.putInt("offset2", offset2);
    } else if (temp3Assigned && compareAddresses(tempSensorAddresses[i], temp3Address)) {
      offset3 = offset;
      preferences.putInt("offset3", offset3);
    } else if (temp4Assigned && compareAddresses(tempSensorAddresses[i], temp4Address)) {
      offset4 = offset;
      preferences.putInt("offset4", offset4);
    } else if (temp5Assigned && compareAddresses(tempSensorAddresses[i], temp5Address)) {
      offset5 = offset;
      preferences.putInt("offset5", offset5);
    }

    setoffset(tempSensorAddresses[i], offset);
  }
  preferences.end();
}

void DEVICE_INIT() {
  Serial.println("From Prefrences");

  preferences.begin("values", false);
  dischargeAlertSp = preferences.getInt("tempsp1", 0);
  suctionAlertSp = preferences.getInt("tempsp2", 0);
  ReturnSp = preferences.getInt("tempsplow", 0);
  ReturnAlertSp = preferences.getInt("tempsphigh", 0);
  oilSp = preferences.getInt("oilsp", 0);

  suctionPsiSp = preferences.getInt("pressuresp1", 0);
  dischargePsiSp = preferences.getInt("pressuresp2", 0);
  gas_selection = preferences.getInt("gastype", 1);
  startSWEnable = preferences.getInt("switchEn", 0);
  returnTempSelection = preferences.getInt("returnTempSelection", 0);

  temp1Assigned = preferences.getBool("temp1_assign", false);
  temp2Assigned = preferences.getBool("temp2_assign", false);
  temp3Assigned = preferences.getBool("temp3_assign", false);
  temp4Assigned = preferences.getBool("temp4_assign", false);
  temp5Assigned = preferences.getBool("temp5_assign", false);

  offset1 = preferences.getInt("offset1", 0);
  offset2 = preferences.getInt("offset2", 0);
  offset3 = preferences.getInt("offset3", 0);
  offset4 = preferences.getInt("offset4", 0);
  offset5 = preferences.getInt("offset5", 0);

  supply_temp_offset = offset1;
  suction_temp_offset = offset2;
  return_air_temp_offset = offset3;
  discharge_temp_offset = offset4;
  oil_temp_offset = offset5;

  Comp_Start_delay = preferences.getInt("startdelay", 0);
  Comp_restart_delay = preferences.getInt("restartdelay", 0);
  Comp_restart_delay_ms = (Comp_restart_delay > 0)
                            ? static_cast<unsigned long>(Comp_restart_delay) * 1000UL
                            : 0UL;

  OPS_SW = preferences.getInt("oilsw", 0);
  LPS_SW = preferences.getInt("suctionsw", 0);
  HPS_SW = preferences.getInt("dischargesw", 0);

  autoReset = preferences.getInt("autoSwitch", 0);
  modesw = preferences.getInt("mode", 0);

  load_temperature_sensor_addresses();
  preferences.end();
}

void callback(char* topic, byte* message, unsigned int length) {
  String messageTemp;

  for (unsigned int i = 0; i < length; i++) {
    messageTemp += static_cast<char>(message[i]);
  }

  // Serial.println("Message Received on: ");
  // Serial.println(String(topic));

  if (String(topic) == device_topic_s_1) {  // /test/DX400-AAA001/1
    Extract_by_json(messageTemp);
  }

  else if (String(topic) == device_topic_s_2) {  // /test/DX400-AAA001/2
    message_received_config = true;
    Extract_temp_config(messageTemp);
  }

  // else if (String(topic) == device_topic_s_3) {  // /test/DX400-AAA001/3
  //   message_received_pressure_config = true;
  //   Extract_pressure_selection(messageTemp);
  // }
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
    // Keep a failed AWS/TLS attempt bounded so the local display connection
    // remains responsive even when internet or broker access is unavailable.
    espClient.setHandshakeTimeout(3);
    client.setSocketTimeout(3);
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    Serial.print("Attempting MQTT connection...");

    if (client.connect(devicename.c_str())) {
      Serial.println("connected");
      client.subscribe(device_topic_s_m.c_str());

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("; next attempt after 10 seconds");
    }
  }
}
