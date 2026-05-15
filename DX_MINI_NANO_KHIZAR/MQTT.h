#include "Scheduler.h"

//For MQTT
void publishJson() {
  StaticJsonDocument<512> doc;
  doc["powerState"] = String(PowerState);
  doc["returnTemp"] = String(ReturnTemp);
  doc["supplyTemp"] = String(SupplyTemp);
  doc["dischargeTemp"] = String(dischargeTemp);

  doc["returnSp"] = String(ReturnSp);
  doc["dischargeSp"] = String(dischargeSp);
  doc["returnTempAlarm"] = returnTempAlarm;
  doc["dischargeTempAlarm"] = dischargeTempAlarm;

  doc["r1Status"] = String(EVAP_COOL_SYSTEM_STATUS);
  doc["r2Status"] = String(AUTO_WASH_SYSTEM_STATUS);
  doc["r3Status"] = String(UNIT_COMMAND_STATUS);

  doc["r1Sw"] = String(EVAP_COOL_SYSTEM_SWITCH);
  doc["r2Sw"] = String(AUTO_WASH_SYSTEM_SWITCH);
  doc["r3Sw"] = String(UNIT_COMMAND_SWITCH);

  doc["modeSw"] = String(MODE_SWITCH);

  doc["faultSw"] = String(FAULT_SWITCH);
  doc["systemStatus"] = String(PowerState);
  doc["timesch"] = timesch;
  doc["timeschen"] = String(timeschen);
  doc["tmatched"] = tmatched;
  doc["mac_address"] = macaddress;
  doc["ip_address"] = myIP;
  // doc["ssid"] = ssid;
  // doc["password"] = password;

  char jsonBuffer[1024];
  size_t n = serializeJson(doc, jsonBuffer);
  client.publish(device_topic_p.c_str(), jsonBuffer, true);

  Serial.println("");
  Serial.println(jsonBuffer);
  Serial.println("JSON message published");
}

void temp_sensor_select_publish() {
  DynamicJsonDocument json_doc(512);

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
  }
  // json_doc["offset1"] = discharge_temp_offset;
  // json_doc["offset2"] = supply_temp_offset;
  // json_doc["offset3"] = return_air_temp_offset;

  json_doc["offset1"] = offset1;
  json_doc["offset2"] = offset2;
  json_doc["offset3"] = offset3;

  char doc[512];
  serializeJson(json_doc, doc);
  String device_topic_temp_config = device_topic_p + "/temperature_config_DXM";
  client.publish(device_topic_temp_config.c_str(), doc, true);
}

void Extract_temp_config(String incoming_message) {
  DeserializationError error = deserializeJson(received_doc, incoming_message);
  if (error) {
    Serial.print("JSON deserialization failed: ");
    Serial.println(error.c_str());
    return;
  }

  String addressString;
  int8_t sensor_selected;
  for (int i = 0; i < numberOfDevices; i++) {
    addressString = getAddressString(tempSensorAddresses[i]);
    if (received_doc.containsKey(addressString)) {
      sensor_selected = received_doc[addressString].as<int>();
      Serial.print("addressString: ");
      Serial.println(addressString);
      Serial.print("sensor_selected: ");
      Serial.println(sensor_selected);
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
        default:
          break;
      }
      setoffset(tempSensorAddresses[i], offset);
    }
  }
  preferences.end();
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
  Serial.println(incomingMessage);

  preferences.begin("values", false);
  update_values_from_json("dischargeSp", dischargeSp);
  update_values_from_json("returnSp", ReturnSp);

  // ▼ FIX: Read modeSw FIRST so all subsequent checks use the correct/current mode
  int prevModeSwitch = MODE_SWITCH;
  update_values_from_json("modeSw", MODE_SWITCH);

  // In AUTO mode, EVAP relay is owned by loop() — ignore r1Sw from cloud
  if (MODE_SWITCH == 0) {
    update_values_from_json("r1Sw", EVAP_COOL_SYSTEM_SWITCH);
  }
  update_values_from_json("r2Sw", AUTO_WASH_SYSTEM_SWITCH);
  update_values_from_json("r3Sw", UNIT_COMMAND_SWITCH);

  // Sync relay outputs immediately
  if (UNIT_COMMAND_SWITCH == 0) {
    UNIT_COMMAND_STATUS = false;
    digitalWrite(UNIT_COMMAND, LOW);
  }
  if (AUTO_WASH_SYSTEM_SWITCH == 0) {
    AUTO_WASH_SYSTEM_STATUS = false;
    digitalWrite(AUTO_WASH_SYSTEM, LOW);
  }
  if (MODE_SWITCH == 0 && EVAP_COOL_SYSTEM_SWITCH == 0) {
    EVAP_COOL_SYSTEM_STATUS = false;
    digitalWrite(EVAP_COOL_SYSTEM, LOW);
  }

  // When switching TO manual mode, turn all relays OFF
  if (prevModeSwitch != MODE_SWITCH && MODE_SWITCH == 0) {
    Serial.println("Switched to MANUAL mode — turning all relays OFF.");
    EVAP_COOL_SYSTEM_SWITCH = 0;
    AUTO_WASH_SYSTEM_SWITCH = 0;
    UNIT_COMMAND_SWITCH     = 0;

    EVAP_COOL_SYSTEM_STATUS = false;
    AUTO_WASH_SYSTEM_STATUS = false;
    UNIT_COMMAND_STATUS     = false;

    digitalWrite(EVAP_COOL_SYSTEM, LOW);
    digitalWrite(AUTO_WASH_SYSTEM, LOW);
    digitalWrite(UNIT_COMMAND,     LOW);

    preferences.putInt("r1Sw", EVAP_COOL_SYSTEM_SWITCH);
    preferences.putInt("r2Sw", AUTO_WASH_SYSTEM_SWITCH);
    preferences.putInt("r3Sw", UNIT_COMMAND_SWITCH);
  }

  update_values_from_json("faultSw", FAULT_SWITCH);

    bool PowerStateChanged = false; 
    if (received_doc.containsKey("powerState")) {
      int newPowerState = received_doc["powerState"].as<int>();
      if (newPowerState != PowerState) {
        PowerState = newPowerState;
        preferences.putBool("PowerState", PowerState);
        PowerStateChanged = true;  // Mark as changed
      }
    }
  preferences.end();

  

  preferences.begin("timeenable", false);
  if (received_doc.containsKey("timeschen") && received_doc["timeschen"].as<int>() != timeschen) {
    timeschen = received_doc["timeschen"].as<int>();
    preferences.putInt("timeschen", timeschen);
  }

  if (received_doc.containsKey("timesch") && received_doc["timesch"].as<String>() != timesch) {
    timesch = received_doc["timesch"].as<String>();
    preferences.putString("timesch", timesch);
    Serial.println("============ FROM MQTT SIDE =======================");
    Serial.println(timesch);
    Serial.println("==================================================");

    parse_schedule(timesch);
    // String _hoursch = timesch.substring(timesch.indexOf("hoursch=") + 8, timesch.indexOf("|"));
    // String _daysch = timesch.substring(timesch.indexOf("|daysch=") + 8, timesch.length());
    // Serial.println("============ FROM MQTT SIDE =======================");
    // Serial.print("hoursch:: ");
    // Serial.println(_hoursch);
    // Serial.print("daysch:: ");
    // Serial.println(_daysch);
    // Serial.print("timesch:: ");
    // Serial.println(timesch);
    // Serial.println("==================================================");
    // hour_day(_hoursch, _daysch);
  }
  preferences.end();
}

void DEVICE_INIT() {
  Serial.println("From Prefrences");

  preferences.begin("values", false);
  PowerState = preferences.getBool("PowerState", true);
  dischargeSp = preferences.getInt("dischargeSp", 0);
  ReturnSp = preferences.getInt("returnSp", 0);

  EVAP_COOL_SYSTEM_SWITCH = preferences.getInt("r1Sw", false);
  AUTO_WASH_SYSTEM_SWITCH = preferences.getInt("r2Sw", false);
  UNIT_COMMAND_SWITCH = preferences.getInt("r3Sw", false);

  MODE_SWITCH = preferences.getInt("modeSw", false);
  FAULT_SWITCH = preferences.getInt("faultSw", false);

  temp1Assigned = preferences.getBool("temp1_assign", false);
  size_t len_1 = preferences.getBytes("temp1Address", temp1Address, sizeof(DeviceAddress));

  temp2Assigned = preferences.getBool("temp2_assign", false);
  size_t len_2 = preferences.getBytes("temp2Address", temp2Address, sizeof(DeviceAddress));

  temp3Assigned = preferences.getBool("temp3_assign", false);
  size_t len_3 = preferences.getBytes("temp3Address", temp3Address, sizeof(DeviceAddress));

  offset1 = preferences.getInt("offset1", 0);
  offset2 = preferences.getInt("offset2", 0);
  offset3 = preferences.getInt("offset3", 0);

  discharge_temp_offset = offset1;
  supply_temp_offset = offset2;
  return_air_temp_offset = offset3;

  preferences.end();

  preferences.begin("timeenable", false);
  timesch = preferences.getString("timesch", "hoursch=24|daysch=7");

  Serial.print("timesch: ");
  Serial.println(timesch);
  parse_schedule(timesch);

  timeschen = preferences.getInt("timeschen", 0);
  Serial.print("timeschen: ");
  Serial.println(timeschen);

  for (uint8_t i = 0; i < 7; i++) {
    String days_key = "days_key" + String(i);
    days_in_num[i] = preferences.getBool(days_key.c_str(), days_in_num[i]);
  }
  for (uint8_t i = 0; i < 24; i++) {
    String hours_key = "hours_key" + String(i);
    hours_num[i] = preferences.getBool(hours_key.c_str(), hours_num[i]);
  }
  preferences.end();
}

void callback(char* topic, byte* message, unsigned int length) {
  String messageTemp;

  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }

  // Serial.println("Message Received on: ");
  // Serial.println(String(topic));

  if (String(topic) == device_topic_s_1) {  // /test/DXM-AAA001/1
    message_received = true;
    Extract_by_json(messageTemp);
  } else if (String(topic) == device_topic_s_2) {  // /test/DXM-AAA001/2
    message_received_config = true;
    Extract_temp_config(messageTemp);
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
