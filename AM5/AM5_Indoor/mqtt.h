#ifndef MQTT_H
#define MQTT_H

#include <math.h>
#include <stdlib.h>

// Forward declarations used by the compatibility helpers below.
bool parseFloatValue(JsonVariantConst value, float& parsedValue);
bool parseRoleValue(JsonVariantConst value, uint8_t maxRole, uint8_t& parsedRole);

inline bool isValidTemperature(float value) {
  return value != SENSOR_DISCONNECTED && !isnan(value);
}

inline float mqttTemperatureValue(float value) {
  return isValidTemperature(value) ? value : SENSOR_DISCONNECTED;
}

inline int tempCToFahrenheitInt(float tempC) {
  if (!isValidTemperature(tempC)) return 888;
  return static_cast<int>(roundf((tempC * 9.0f / 5.0f) + 32.0f));
}

inline float getIndoorPhysicalTemperature(uint8_t index) {
  if (index >= INDOOR_SENSOR_COUNT || !tempSensorPresent[index]) return SENSOR_DISCONNECTED;
  if (!isValidTemperature(indoorSensorRawTempC[index])) return SENSOR_DISCONNECTED;
  return indoorSensorRawTempC[index] + tempSensorOffset[index];
}

inline float getOutdoorPhysicalTemperature(uint8_t index) {
  if (index >= OUTDOOR_SENSOR_COUNT || !outdoorSensorPresent[index]) return SENSOR_DISCONNECTED;
  if (isValidTemperature(outdoorSensorPhysicalTempC[index])) return outdoorSensorPhysicalTempC[index];
  if (!isValidTemperature(outdoorSensorRawTempC[index])) return SENSOR_DISCONNECTED;
  return outdoorSensorRawTempC[index] + outdoorSensorOffset[index];
}

inline float appTemperatureValue(float value) {
  if (!isValidTemperature(value)) return SENSOR_DISCONNECTED;
  return roundf(value * 10.0f) / 10.0f;
}

inline float appOffsetValue(float value) {
  if (!isfinite(value)) return 0.0f;
  return roundf(value * 100.0f) / 100.0f;
}

inline String sensorAliasKey(uint8_t index) {
  return String("sensor0") + String(index + 1);
}

inline String sensorAliasKeyShort(uint8_t index) {
  return String("sensor") + String(index + 1);
}

inline String roleAliasKey(uint8_t index) {
  return String("role") + String(index + 1);
}

inline bool readRoleForSensorAliases(const StaticJsonDocument<4096>& doc,
                                     uint8_t index,
                                     const String& addressString,
                                     uint8_t maxRole,
                                     uint8_t& roleOut) {
  String upperAddress = addressString;
  upperAddress.toUpperCase();

  if (doc.containsKey(addressString) &&
      parseRoleValue(doc[addressString], maxRole, roleOut)) return true;
  if (doc.containsKey(upperAddress) &&
      parseRoleValue(doc[upperAddress], maxRole, roleOut)) return true;

  const String aliasA = sensorAliasKey(index);
  const String aliasB = sensorAliasKeyShort(index);
  const String aliasC = roleAliasKey(index);
  if (doc.containsKey(aliasA) && parseRoleValue(doc[aliasA], maxRole, roleOut)) return true;
  if (doc.containsKey(aliasB) && parseRoleValue(doc[aliasB], maxRole, roleOut)) return true;
  if (doc.containsKey(aliasC) && parseRoleValue(doc[aliasC], maxRole, roleOut)) return true;
  return false;
}

inline bool readOffsetForSensorAliases(const StaticJsonDocument<4096>& doc,
                                       uint8_t index,
                                       const String& addressString,
                                       float& offsetOut) {
  const String canonicalOffsetKey = "offset" + addressString;
  String upperOffsetKey = canonicalOffsetKey;
  upperOffsetKey.toUpperCase();
  const String positionalOffsetKey = "offset" + String(index + 1);
  const String legacyOffsetKey = String("offsetsensor0") + String(index + 1);
  const String legacyOffsetKeyShort = String("offsetsensor") + String(index + 1);

  if (doc.containsKey(canonicalOffsetKey) && parseFloatValue(doc[canonicalOffsetKey], offsetOut)) return true;
  if (doc.containsKey(upperOffsetKey) && parseFloatValue(doc[upperOffsetKey], offsetOut)) return true;
  if (doc.containsKey(legacyOffsetKey) && parseFloatValue(doc[legacyOffsetKey], offsetOut)) return true;
  if (doc.containsKey(legacyOffsetKeyShort) && parseFloatValue(doc[legacyOffsetKeyShort], offsetOut)) return true;
  if (doc.containsKey(positionalOffsetKey) && parseFloatValue(doc[positionalOffsetKey], offsetOut)) return true;
  return false;
}

inline int fanSpeedToMqttFanSw() {
  switch (fan_speed) {
    case FAN_LOW: return 0;
    case FAN_MEDIUM: return 1;
    case FAN_HIGH: return 2;
    default: return 0;
  }
}

inline int effectiveOutdoorStatusForPublish() {
  if (system_power == 0 || outdoor_sw == 0) return OUT_STATUS_STOPPED;
  if (!rs485_outdoor_online) return OUT_STATUS_POWER_FAULT;
  if (outdoorStatusCode < OUT_STATUS_STOPPED ||
      outdoorStatusCode > OUT_STATUS_OVERLOAD_TRIPPED) {
    return OUT_STATUS_STOPPED;
  }
  return outdoorStatusCode;
}

inline void publishDocument(const String& topic,
                            const StaticJsonDocument<4096>& doc,
                            bool retained = true) {
  String payload;
  serializeJson(doc, payload);
  const bool published = client.publish(topic.c_str(), payload.c_str(), retained);

  Serial.println();
  Serial.println("[MQTT][PUBLISH] ========================================");
  Serial.print("[MQTT][PUBLISH] Topic   : ");
  Serial.println(topic);
  Serial.print("[MQTT][PUBLISH] Retained: ");
  Serial.println(retained ? "YES" : "NO");
  Serial.print("[MQTT][PUBLISH] Result  : ");
  Serial.println(published ? "SUCCESS" : "FAILED");
  Serial.print("[MQTT][PUBLISH] JSON    : ");
  Serial.println(payload);
  Serial.println("[MQTT][PUBLISH] ========================================");
}

// =====================================================
// PUBLISHED: /KRC/AM5-AAA001
// =====================================================
void publishMainStatus() {
  if (!client.connected()) return;

  StaticJsonDocument<4096> doc;
  doc["fanSw"] = fanSpeedToMqttFanSw();
  doc["indoorsw"] = indoor_sw;
  doc["outdoorsw"] = outdoor_sw;
  doc["statusout"] = effectiveOutdoorStatusForPublish();
  doc["supply"] = appTemperatureValue(SupplyTempC);
  doc["setPoint"] = appTemperatureValue(setPoint);
  doc["return"] = appTemperatureValue(ReturnTempC);
  doc["powersw"] = system_power;
  doc["mode"] = hvac_mode;

  publishDocument(device_topic_p_main, doc);
}

// =====================================================
// PUBLISHED: /KRC/AM5-AAA001/IndoorTemps
// =====================================================
void publishIndoorTemps() {
  if (!client.connected()) return;

  StaticJsonDocument<4096> doc;
  for (uint8_t i = 0; i < INDOOR_SENSOR_COUNT; i++) {
    const String key = "temp" + String(i + 1) + "in";
    doc[key] = appTemperatureValue(getIndoorPhysicalTemperature(i));
  }

  publishDocument(device_topic_p_indoor_temps, doc);
}

// =====================================================
// PUBLISHED: /KRC/AM5-AAA001/OutdoorTemps
// =====================================================
void publishOutdoorTemps() {
  if (!client.connected()) return;

  StaticJsonDocument<4096> doc;
  for (uint8_t i = 0; i < OUTDOOR_SENSOR_COUNT; i++) {
    const String key = "temp" + String(i + 1) + "out";
    doc[key] = appTemperatureValue(getOutdoorPhysicalTemperature(i));
  }

  publishDocument(device_topic_p_outdoor_temps, doc);
}

// =====================================================
// PUBLISHED: /KRC/AM5-AAA001/indoorSensor
// IMPORTANT: temperatures, offsets and roles are NUMBERS. The previous build
// published these as JSON strings, which breaks mobile apps that deserialize
// them into numeric fields. Compatibility aliases are included as extra keys.
// =====================================================
void publishIndoorSensorConfig() {
  if (!client.connected()) return;

  StaticJsonDocument<4096> doc;

  for (uint8_t i = 0; i < INDOOR_SENSOR_COUNT; i++) {
    const uint8_t position = static_cast<uint8_t>(i + 1);
    const String tempKey = "temp" + String(position);
    const String offsetKey = "offset" + String(position);
    const String addressKey = "address" + String(position);
    const String aliasRoleKey = sensorAliasKey(i);
    const String aliasOffsetKey = String("offsetsensor0") + String(position);

    if (!tempSensorPresent[i]) {
      doc[tempKey] = SENSOR_DISCONNECTED;
      doc[offsetKey] = 0.0f;
      doc[addressKey] = "";
      doc[aliasRoleKey] = 0;
      doc[aliasOffsetKey] = 0.0f;
      continue;
    }

    const String addressString = getAddressString(tempSensorAddresses[i]);
    const float tempValue = appTemperatureValue(getIndoorPhysicalTemperature(i));
    const float offsetValue = appOffsetValue(tempSensorOffset[i]);
    const int roleValue = static_cast<int>(tempSensorRole[i]);

    doc[tempKey] = tempValue;
    doc[offsetKey] = offsetValue;
    doc[addressKey] = addressString;
    doc[addressString] = roleValue;

    // App compatibility aliases. They do not replace the ROM-based model.
    doc[aliasRoleKey] = roleValue;
    doc[aliasOffsetKey] = offsetValue;
    doc[String("offset") + addressString] = offsetValue;
  }

  publishDocument(device_topic_p_indoor_sensor, doc);
}

// =====================================================
// PUBLISHED: /KRC/AM5-AAA001/outdoorSensor
// Numeric JSON types + compatibility aliases, same as Indoor.
// =====================================================
void publishOutdoorSensorConfig() {
  if (!client.connected()) return;

  // Do not send the app a known-stale role snapshot while an Outdoor
  // configuration transaction is waiting for its fresh authoritative B2.
  if (outdoorConfigAwaitingFreshSnapshot) {
    Serial.println("[MQTT][OUTDOOR CONFIG] Publish deferred until fresh RS485 snapshot");
    return;
  }

  StaticJsonDocument<4096> doc;

  for (uint8_t i = 0; i < OUTDOOR_SENSOR_COUNT; i++) {
    const uint8_t position = static_cast<uint8_t>(i + 1);
    const String tempKey = "temp" + String(position);
    const String offsetKey = "offset" + String(position);
    const String addressKey = "address" + String(position);
    const String aliasRoleKey = sensorAliasKey(i);
    const String aliasOffsetKey = String("offsetsensor0") + String(position);

    if (!outdoorConfigSnapshotValid || !outdoorSensorPresent[i]) {
      doc[tempKey] = SENSOR_DISCONNECTED;
      doc[offsetKey] = 0.0f;
      doc[addressKey] = "";
      doc[aliasRoleKey] = 0;
      doc[aliasOffsetKey] = 0.0f;
      continue;
    }

    const String addressString = getAddressString(outdoorSensorAddresses[i]);
    const float tempValue = appTemperatureValue(getOutdoorPhysicalTemperature(i));
    const float offsetValue = appOffsetValue(outdoorSensorOffset[i]);
    const int roleValue = static_cast<int>(outdoorSensorRole[i]);

    doc[tempKey] = tempValue;
    doc[offsetKey] = offsetValue;
    doc[addressKey] = addressString;
    doc[addressString] = roleValue;
    doc[aliasRoleKey] = roleValue;
    doc[aliasOffsetKey] = offsetValue;
    doc[String("offset") + addressString] = offsetValue;
  }

  publishDocument(device_topic_p_outdoor_sensor, doc);
}

// =====================================================
// PUBLISHED: /KRC/AM5-AAA001/AM-Input-Output
// Outdoor mapping:
// SW1 High PSI A, SW2 Low PSI A, SW3 Overload A, SW4 Power A
// SW5 High PSI B, SW6 Low PSI B, SW7 Overload B, SW8 Power B
// R1 Comp A, R2 Comp B, R3 Cond A, R4 Cond B
// =====================================================
void publishInputOutput() {
  if (!client.connected()) return;

  StaticJsonDocument<4096> doc;
  const bool outdoorValid = rs485_outdoor_online;

  doc["outhipsiA"] = outdoorValid && outdoorSwitchState[0] ? 1 : 0;
  doc["outhipsiB"] = outdoorValid && outdoorSwitchState[4] ? 1 : 0;
  doc["outlopsiA"] = outdoorValid && outdoorSwitchState[1] ? 1 : 0;
  doc["outlopsiB"] = outdoorValid && outdoorSwitchState[5] ? 1 : 0;
  doc["outoverloadA"] = outdoorValid && outdoorSwitchState[2] ? 1 : 0;
  doc["outoverloadB"] = outdoorValid && outdoorSwitchState[6] ? 1 : 0;
  doc["outpowerA"] = outdoorValid && outdoorSwitchState[3] ? 1 : 0;
  doc["outpowerB"] = outdoorValid && outdoorSwitchState[7] ? 1 : 0;

  doc["outcompA"] = outdoorValid && outdoorR1 ? 1 : 0;
  doc["outcompB"] = outdoorValid && outdoorR2 ? 1 : 0;
  doc["outcondA"] = outdoorValid && outdoorR3 ? 1 : 0;
  doc["outcondB"] = outdoorValid && outdoorR4 ? 1 : 0;

  doc["infanlo"] = indoorOutputsEnabled() && fan_speed == FAN_LOW ? 1 : 0;
  doc["infanhi"] = indoorOutputsEnabled() && fan_speed == FAN_HIGH ? 1 : 0;
  doc["infanmed"] = indoorOutputsEnabled() && fan_speed == FAN_MEDIUM ? 1 : 0;
  doc["inheater"] = indoorOutputsEnabled() && hvac_mode == MODE_HEAT ? 1 : 0;

  publishDocument(device_topic_p_io, doc);
}

void publishAllMqttState() {
  if (!client.connected()) return;
  publishMainStatus();
  publishIndoorTemps();
  publishOutdoorTemps();
  publishIndoorSensorConfig();
  publishOutdoorSensorConfig();
  publishInputOutput();
}

// Backward-compatible wrapper used by any older code path.
void publishJson() {
  publishMainStatus();
}

// =====================================================
// JSON VALUE PARSERS
// =====================================================
bool parseZeroOneValue(JsonVariantConst value, int& parsedValue) {
  if (value.is<bool>()) {
    parsedValue = value.as<bool>() ? 1 : 0;
    return true;
  }

  if (value.is<int>()) {
    const int v = value.as<int>();
    if (v == 0 || v == 1) {
      parsedValue = v;
      return true;
    }
    return false;
  }

  if (value.is<const char*>()) {
    String text = value.as<const char*>();
    text.trim();
    text.toLowerCase();
    if (text == "1" || text == "on" || text == "true") {
      parsedValue = 1;
      return true;
    }
    if (text == "0" || text == "off" || text == "false") {
      parsedValue = 0;
      return true;
    }
  }

  return false;
}

bool parseFanSwValue(JsonVariantConst value, int& parsedInternalSpeed) {
  if (value.is<int>()) {
    const int appFan = value.as<int>();
    if (appFan < 0 || appFan > 2) return false;
    parsedInternalSpeed = appFan + 1;
    return true;
  }

  if (value.is<const char*>()) {
    String text = value.as<const char*>();
    text.trim();
    text.toLowerCase();
    if (text == "0" || text == "low") parsedInternalSpeed = FAN_LOW;
    else if (text == "1" || text == "mid" || text == "medium") parsedInternalSpeed = FAN_MEDIUM;
    else if (text == "2" || text == "high") parsedInternalSpeed = FAN_HIGH;
    else return false;
    return true;
  }

  return false;
}

bool parseInternalFanSpeedValue(JsonVariantConst value, int& parsedSpeed) {
  if (value.is<int>()) {
    parsedSpeed = value.as<int>();
    return parsedSpeed >= FAN_LOW && parsedSpeed <= FAN_HIGH;
  }
  return false;
}

bool parseModeValue(JsonVariantConst value, int& parsedMode) {
  if (value.is<int>()) {
    parsedMode = value.as<int>();
    return parsedMode == MODE_COOL || parsedMode == MODE_HEAT;
  }

  if (value.is<const char*>()) {
    String text = value.as<const char*>();
    text.trim();
    text.toLowerCase();
    if (text == "0" || text == "cool") parsedMode = MODE_COOL;
    else if (text == "1" || text == "heat") parsedMode = MODE_HEAT;
    else return false;
    return true;
  }

  return false;
}

bool parseFloatValue(JsonVariantConst value, float& parsedValue) {
  if (value.is<int>() || value.is<float>() || value.is<double>()) {
    const float v = value.as<float>();
    if (!isfinite(v)) return false;
    parsedValue = v;
    return true;
  }

  if (value.is<const char*>()) {
    const char* text = value.as<const char*>();
    if (text == nullptr || *text == '\0') return false;
    char* endPtr = nullptr;
    const float v = strtof(text, &endPtr);
    if (endPtr == text || !isfinite(v)) return false;
    parsedValue = v;
    return true;
  }

  return false;
}

bool parseRoleValue(JsonVariantConst value, uint8_t maxRole, uint8_t& parsedRole) {
  int role = -1;

  if (value.is<int>()) {
    role = value.as<int>();
  }
  else if (value.is<const char*>()) {
    const char* text = value.as<const char*>();
    if (text == nullptr || *text == '\0') return false;
    char* endPtr = nullptr;
    const long parsed = strtol(text, &endPtr, 10);
    if (endPtr == text) return false;
    role = static_cast<int>(parsed);
  }
  else {
    return false;
  }

  if (role < 0 || role > maxRole) return false;
  parsedRole = static_cast<uint8_t>(role);
  return true;
}

// =====================================================
// RECEIVED: /test/AM5-AAA001/2
// {"indoorsw":1,"outdoorsw":1,"setPoint":22,"powersw":1,
//  "fanSw":1,"mode":0}
// =====================================================
void Extract_control_json(String incomingMessage) {
  DeserializationError error = deserializeJson(received_doc, incomingMessage);
  if (error) {
    Serial.print("[MQTT] Control JSON error: ");
    Serial.println(error.c_str());
    return;
  }

  const int oldPower = system_power;
  const int oldIndoorSw = indoor_sw;
  const int oldOutdoorSw = outdoor_sw;
  const int oldFan = fan_speed;
  const int oldMode = hvac_mode;
  const float oldSetPoint = setPoint;

  int requestedPower = system_power;
  int requestedIndoorSw = indoor_sw;
  int requestedOutdoorSw = outdoor_sw;
  int requestedFan = fan_speed;
  int requestedMode = hvac_mode;
  float requestedSetPoint = setPoint;

  bool hasPower = false;
  bool hasIndoorSw = false;
  bool hasOutdoorSw = false;
  bool hasFan = false;
  bool hasMode = false;
  bool hasSetPoint = false;

  if (received_doc.containsKey("powersw")) {
    hasPower = parseZeroOneValue(received_doc["powersw"], requestedPower);
  }
  else if (received_doc.containsKey("system_power")) {
    hasPower = parseZeroOneValue(received_doc["system_power"], requestedPower);
  }
  else if (received_doc.containsKey("power")) {
    hasPower = parseZeroOneValue(received_doc["power"], requestedPower);
  }

  if (received_doc.containsKey("indoorsw")) {
    hasIndoorSw = parseZeroOneValue(received_doc["indoorsw"], requestedIndoorSw);
  }

  if (received_doc.containsKey("outdoorsw")) {
    hasOutdoorSw = parseZeroOneValue(received_doc["outdoorsw"], requestedOutdoorSw);
  }

  if (received_doc.containsKey("fanSw")) {
    hasFan = parseFanSwValue(received_doc["fanSw"], requestedFan);
  }
  else if (received_doc.containsKey("fan_speed")) {
    hasFan = parseInternalFanSpeedValue(received_doc["fan_speed"], requestedFan);
  }

  if (received_doc.containsKey("mode")) {
    hasMode = parseModeValue(received_doc["mode"], requestedMode);
  }

  if (received_doc.containsKey("setPoint")) {
    hasSetPoint = parseFloatValue(received_doc["setPoint"], requestedSetPoint);
  }

  // If this packet turns the complete system OFF, do that first so changing
  // fan/mode in the same JSON cannot briefly energize an output.
  if (hasPower && requestedPower == 0) setSystemPower(0);

  if (hasIndoorSw) setIndoorSwitch(requestedIndoorSw);
  if (hasOutdoorSw) setOutdoorSwitch(requestedOutdoorSw);
  if (hasFan) setFanSpeed(requestedFan);
  if (hasMode) setHVACMode(requestedMode);
  if (hasSetPoint) setPoint = requestedSetPoint;

  // If this packet turns the complete system ON, do it last so the selected
  // section/fan/mode state is already in place.
  if (hasPower && requestedPower == 1) setSystemPower(1);

  preferences.begin("values", false);
  if (hasIndoorSw) preferences.putInt("indoor_sw", indoor_sw);
  if (hasOutdoorSw) preferences.putInt("outdoor_sw", outdoor_sw);
  if (hasFan) preferences.putInt("fan_speed", fan_speed);
  if (hasMode) preferences.putInt("hvac_mode", hvac_mode);
  if (hasSetPoint) preferences.putFloat("setPoint", setPoint);
  preferences.end();

  const bool changed = oldPower != system_power ||
                       oldIndoorSw != indoor_sw ||
                       oldOutdoorSw != outdoor_sw ||
                       oldFan != fan_speed ||
                       oldMode != hvac_mode ||
                       fabsf(oldSetPoint - setPoint) > 0.001f;

  if (changed) {
    mqtt_publish_requested = true;
    if (oldPower != system_power || oldOutdoorSw != outdoor_sw) {
      rs485_urgent_outdoor_sync = true;
    }
  }

  if (received_doc.containsKey("powersw") && !hasPower) {
    Serial.println("[MQTT][CONTROL] WARNING: invalid powersw; expected 0/1");
  }
  if (received_doc.containsKey("indoorsw") && !hasIndoorSw) {
    Serial.println("[MQTT][CONTROL] WARNING: invalid indoorsw; expected 0/1");
  }
  if (received_doc.containsKey("outdoorsw") && !hasOutdoorSw) {
    Serial.println("[MQTT][CONTROL] WARNING: invalid outdoorsw; expected 0/1");
  }
  if (received_doc.containsKey("fanSw") && !hasFan) {
    Serial.println("[MQTT][CONTROL] WARNING: invalid fanSw; expected 0/1/2");
  }
  if (received_doc.containsKey("mode") && !hasMode) {
    Serial.println("[MQTT][CONTROL] WARNING: invalid mode; expected 0/1");
  }
  if (received_doc.containsKey("setPoint") && !hasSetPoint) {
    Serial.println("[MQTT][CONTROL] WARNING: invalid setPoint");
  }

  Serial.print("[MQTT] Control accepted | power="); Serial.print(system_power);
  Serial.print(" indoor="); Serial.print(indoor_sw);
  Serial.print(" outdoor="); Serial.print(outdoor_sw);
  Serial.print(" fanSw="); Serial.print(fanSpeedToMqttFanSw());
  Serial.print(" mode="); Serial.print(hvac_mode);
  Serial.print(" setPoint="); Serial.println(setPoint, 1);

  message_received = true;
}

// A config packet for Outdoor can arrive before the first RS485 ROM snapshot.
// Keep it instead of silently discarding the user's selection.
static String pendingOutdoorSensorConfigJson;
static bool pendingOutdoorSensorConfig = false;

inline bool addressFieldMatches(const StaticJsonDocument<4096>& doc,
                                uint8_t index,
                                const String& addressString) {
  const String addressKey = "address" + String(index + 1);
  if (!doc.containsKey(addressKey)) return true; // slot-only legacy packet
  String supplied = doc[addressKey].as<String>();
  supplied.trim();
  supplied.toLowerCase();
  return supplied.isEmpty() || supplied == addressString;
}

// =====================================================
// RECEIVED: /test/AM5-AAA001/4  (Indoor sensor config)
// Accepted role formats:
//   "<16HEX-ROM>": 3
//   "sensor01": 3       (legacy/mobile-app slot alias)
//   "sensor1": 3
//   "role1": 3
// If address1..6 are included, slot aliases are matched to that ROM.
// Accepted offset formats:
//   offset<ROM>, offsetsensor01, offsetsensor1, offset1
// =====================================================
void Extract_indoor_sensor_config(String incomingMessage) {
  DeserializationError error = deserializeJson(received_doc, incomingMessage);
  if (error) {
    Serial.print("[MQTT] Indoor sensor JSON error: ");
    Serial.println(error.c_str());
    return;
  }

  bool anyRecognizedField = false;
  bool anyAcceptedChange = false;

  for (uint8_t i = 0; i < INDOOR_SENSOR_COUNT; i++) {
    if (!tempSensorPresent[i]) continue;

    const String addressString = getAddressString(tempSensorAddresses[i]);
    uint8_t role = tempSensorRole[i];
    float sensorOffset = tempSensorOffset[i];

    bool roleFound = false;
    if (addressFieldMatches(received_doc, i, addressString)) {
      uint8_t requestedRole = role;
      roleFound = readRoleForSensorAliases(
        received_doc, i, addressString, INDOOR_SENSOR_COUNT, requestedRole);
      if (roleFound) role = requestedRole;
    }

    float requestedOffset = sensorOffset;
    const bool offsetFound = readOffsetForSensorAliases(
      received_doc, i, addressString, requestedOffset);
    if (offsetFound) sensorOffset = requestedOffset;

    if (!roleFound && !offsetFound) continue;
    anyRecognizedField = true;

    Serial.print("[MQTT][INDOOR CONFIG] slot=");
    Serial.print(i + 1);
    Serial.print(" ROM=");
    Serial.print(addressString);
    Serial.print(" requestedRole=");
    Serial.print(role);
    Serial.print(" offset=");
    Serial.println(sensorOffset, 2);

    if (setIndoorSensorConfig(tempSensorAddresses[i], role, sensorOffset)) {
      anyAcceptedChange = true;
      Serial.println("[MQTT][INDOOR CONFIG] APPLIED + SAVED");
    }
    else {
      Serial.println("[MQTT][INDOOR CONFIG] REJECTED");
    }
  }

  if (!anyRecognizedField) {
    Serial.println("[MQTT][INDOOR CONFIG] WARNING: packet contained no recognized ROM/slot role or offset keys");
  }

  if (anyAcceptedChange) {
    mqtt_publish_requested = true;
    Serial.println("[MQTT][INDOOR CONFIG] Configuration will be republished to app");
  }
  message_received_config = true;
}

// =====================================================
// RECEIVED: /test/AM5-AAA001/3  (Outdoor sensor config)
// The command is cached if the Outdoor ROM snapshot has not arrived yet.
// Once the snapshot is available it is replayed automatically.
// =====================================================
void Extract_outdoor_sensor_config(String incomingMessage) {
  DeserializationError error = deserializeJson(received_doc, incomingMessage);
  if (error) {
    Serial.print("[MQTT] Outdoor sensor JSON error: ");
    Serial.println(error.c_str());
    return;
  }

  if (!outdoorConfigSnapshotValid) {
    pendingOutdoorSensorConfigJson = incomingMessage;
    pendingOutdoorSensorConfig = true;
    rs485_outdoor_config_snapshot_requested = true;
    Serial.println("[MQTT][OUTDOOR CONFIG] ROM snapshot not ready; command CACHED until snapshot arrives");
    message_received_config = true;
    return;
  }

  bool anyRecognizedField = false;
  bool anyQueuedChange = false;

  for (uint8_t i = 0; i < OUTDOOR_SENSOR_COUNT; i++) {
    if (!outdoorSensorPresent[i]) continue;

    const String addressString = getAddressString(outdoorSensorAddresses[i]);
    uint8_t role = outdoorSensorRole[i];
    float sensorOffset = outdoorSensorOffset[i];

    bool roleFound = false;
    if (addressFieldMatches(received_doc, i, addressString)) {
      uint8_t requestedRole = role;
      roleFound = readRoleForSensorAliases(
        received_doc, i, addressString, OUTDOOR_SENSOR_COUNT, requestedRole);
      if (roleFound) role = requestedRole;
    }

    float requestedOffset = sensorOffset;
    const bool offsetFound = readOffsetForSensorAliases(
      received_doc, i, addressString, requestedOffset);
    if (offsetFound) sensorOffset = requestedOffset;

    if (!roleFound && !offsetFound) continue;
    anyRecognizedField = true;

    Serial.print("[MQTT][OUTDOOR CONFIG] slot=");
    Serial.print(i + 1);
    Serial.print(" ROM=");
    Serial.print(addressString);
    Serial.print(" requestedRole=");
    Serial.print(role);
    Serial.print(" offset=");
    Serial.println(sensorOffset, 2);

    if (rs485QueueOutdoorSensorConfig(outdoorSensorAddresses[i], role, sensorOffset)) {
      anyQueuedChange = true;
      outdoorConfigAwaitingFreshSnapshot = true;
      Serial.println("[MQTT][OUTDOOR CONFIG] QUEUED FOR RS485");
    }
    else {
      Serial.println("[MQTT][OUTDOOR CONFIG] REJECTED / QUEUE FULL");
    }
  }

  if (!anyRecognizedField) {
    Serial.println("[MQTT][OUTDOOR CONFIG] WARNING: packet contained no recognized ROM/slot role or offset keys");
  }

  // IMPORTANT: do not republish the old Outdoor snapshot here. The app would
  // immediately receive role=0 again and visually undo the user's selection.
  // Republish only after Outdoor ACK + a fresh B2 snapshot is received.
  if (anyQueuedChange) {
    Serial.println("[MQTT][OUTDOOR CONFIG] Waiting for Outdoor ACK + fresh snapshot before app republish");
  }

  message_received_config = true;
}

inline void servicePendingOutdoorSensorConfig() {
  if (!pendingOutdoorSensorConfig || !outdoorConfigSnapshotValid) return;

  String replay = pendingOutdoorSensorConfigJson;
  pendingOutdoorSensorConfigJson = "";
  pendingOutdoorSensorConfig = false;

  Serial.println("[MQTT][OUTDOOR CONFIG] Replaying cached config after ROM snapshot");
  Extract_outdoor_sensor_config(replay);
}

void DEVICE_INIT() {
  Serial.println("Loading saved AM5 Indoor settings from Preferences");

  // Indoor is the authoritative master. Restore the user's last complete-system
  // power state here. Outdoor still starts fail-safe OFF and requires a fresh
  // RS485 B0 command before any Outdoor relay is allowed to energize.
  preferences.begin("values", false);
  fan_speed = preferences.getInt("fan_speed", FAN_LOW);
  hvac_mode = preferences.getInt("hvac_mode", MODE_COOL);
  indoor_sw = preferences.getInt("indoor_sw", 1);
  outdoor_sw = preferences.getInt("outdoor_sw", 1);
  setPoint = preferences.getFloat("setPoint", 22.0f);
  int savedPowerState = preferences.getInt("power_state", 0);
  preferences.end();

  if (fan_speed < FAN_LOW || fan_speed > FAN_HIGH) fan_speed = FAN_LOW;
  if (hvac_mode != MODE_COOL && hvac_mode != MODE_HEAT) hvac_mode = MODE_COOL;
  if (indoor_sw != 0 && indoor_sw != 1) indoor_sw = 1;
  if (outdoor_sw != 0 && outdoor_sw != 1) outdoor_sw = 1;
  if (!isfinite(setPoint)) setPoint = 22.0f;
  if (savedPowerState != 0 && savedPowerState != 1) savedPowerState = 0;

  loadIndoorStoredSensorConfig();

  // relayInit() deliberately starts physical outputs OFF. Apply the saved state
  // only after all saved fan/mode/section settings have been validated.
  setSystemPower(savedPowerState, false);

  rs485_urgent_outdoor_sync = true;
  rs485_outdoor_config_snapshot_requested = true;

  Serial.print("Saved fanSw: "); Serial.println(fanSpeedToMqttFanSw());
  Serial.print("Saved HVAC mode: "); Serial.println(hvac_mode);
  Serial.print("Saved setPoint: "); Serial.println(setPoint, 1);
  Serial.print("Saved indoorsw: "); Serial.println(indoor_sw);
  Serial.print("Saved outdoorsw: "); Serial.println(outdoor_sw);
  Serial.print("Saved power_state: "); Serial.println(savedPowerState);
  Serial.print("Authoritative system_power after boot: ");
  Serial.println(system_power);
}

void callback(char* topic, byte* message, unsigned int length) {
  String messageTemp;
  messageTemp.reserve(length + 1U);
  for (unsigned int i = 0; i < length; i++) {
    messageTemp += static_cast<char>(message[i]);
  }

  const String receivedTopic(topic ? topic : "");

  Serial.println();
  Serial.println("[MQTT][RX] =============================================");
  Serial.print("[MQTT][RX] Topic   : ");
  Serial.println(receivedTopic);
  Serial.print("[MQTT][RX] Length  : ");
  Serial.println(length);
  Serial.print("[MQTT][RX] JSON    : ");
  Serial.println(messageTemp);

  if (receivedTopic == device_topic_s_control) {
    Serial.println("[MQTT][RX] Route   : CONTROL (/2)");
    Extract_control_json(messageTemp);
  }
  else if (receivedTopic == device_topic_s_outdoor_sensor) {
    Serial.println("[MQTT][RX] Route   : OUTDOOR SENSOR CONFIG (/3)");
    Extract_outdoor_sensor_config(messageTemp);
  }
  else if (receivedTopic == device_topic_s_indoor_sensor) {
    Serial.println("[MQTT][RX] Route   : INDOOR SENSOR CONFIG (/4)");
    Extract_indoor_sensor_config(messageTemp);
  }
  else {
    Serial.println("[MQTT][RX] Route   : IGNORED (unknown topic)");
  }
  Serial.println("[MQTT][RX] =============================================");
}

#endif
