#ifndef AM5_DISPLAY_LINK_H
#define AM5_DISPLAY_LINK_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <math.h>

// ============================================================
// AM5 INDOOR <-> DISPLAY WI-FI / HTTP LINK
// Indoor remains the authoritative controller.
//
// Indoor SoftAP : 192.168.4.1
// Display STA   : 192.168.4.50 (static)
//
// Display -> Indoor : POST /from_display
// Indoor  -> Display: POST http://192.168.4.50/receive
// ============================================================

static const char* AM5_DISPLAY_IP = "192.168.4.50";
static const char* AM5_DISPLAY_RECEIVE_PATH = "/receive";
static const char* AM5_INDOOR_DISPLAY_RX_PATH = "/from_display";
static const uint32_t AM5_DISPLAY_PUSH_INTERVAL_MS = 5000UL;
static const uint32_t AM5_DISPLAY_OFFLINE_RETRY_MS = 5000UL;
static const uint32_t AM5_DISPLAY_HTTP_CONNECT_TIMEOUT_MS = 300UL;
static const uint32_t AM5_DISPLAY_HTTP_TIMEOUT_MS = 600UL;
static const size_t AM5_DISPLAY_MAX_BODY_BYTES = 512U;

struct AM5DisplayPendingCommand {
  bool hasPower = false;
  int power = 0;
  bool hasFan = false;
  int fanSpeed = FAN_LOW;  // internal 1..3
  bool hasMode = false;
  int mode = MODE_COOL;
  bool hasSetPoint = false;
  float setPointValue = 22.0f;
};

static portMUX_TYPE am5DisplayCommandMux = portMUX_INITIALIZER_UNLOCKED;
static AM5DisplayPendingCommand am5DisplayPendingCommand;
static bool am5DisplayCommandPending = false;
static unsigned long am5DisplayLastPushMs = 0;
static unsigned long am5DisplayLastSuccessMs = 0;
static unsigned long am5DisplayNextAttemptMs = 0;
static bool am5DisplayOnline = false;
static bool am5DisplayForcePush = true;
static int am5DisplayLastPower = -1;
static int am5DisplayLastFanSw = -1;
static int am5DisplayLastMode = -1;
static int am5DisplayLastIndoorSw = -1;
static int am5DisplayLastOutdoorSw = -1;
static float am5DisplayLastSetPoint = NAN;
static int am5DisplayLastStatusOut = -1;

inline void displayLinkAddCorsHeaders(AsyncWebServerResponse* response) {
  if (!response) return;
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

inline void displayLinkQueueCommand(const AM5DisplayPendingCommand& incoming) {
  portENTER_CRITICAL(&am5DisplayCommandMux);

  if (incoming.hasPower) {
    am5DisplayPendingCommand.hasPower = true;
    am5DisplayPendingCommand.power = incoming.power;
  }
  if (incoming.hasFan) {
    am5DisplayPendingCommand.hasFan = true;
    am5DisplayPendingCommand.fanSpeed = incoming.fanSpeed;
  }
  if (incoming.hasMode) {
    am5DisplayPendingCommand.hasMode = true;
    am5DisplayPendingCommand.mode = incoming.mode;
  }
  if (incoming.hasSetPoint) {
    am5DisplayPendingCommand.hasSetPoint = true;
    am5DisplayPendingCommand.setPointValue = incoming.setPointValue;
  }

  am5DisplayCommandPending = true;
  portEXIT_CRITICAL(&am5DisplayCommandMux);
}

inline bool displayLinkParseIncomingJson(const String& body,
                                         AM5DisplayPendingCommand& command) {
  StaticJsonDocument<512> doc;
  const DeserializationError error = deserializeJson(doc, body);
  if (error) {
    Serial.print("[DISPLAY][HTTP RX] JSON error: ");
    Serial.println(error.c_str());
    return false;
  }

  bool anyField = false;

  int parsed = 0;
  if (doc.containsKey("powersw")) {
    if (!parseZeroOneValue(doc["powersw"], parsed)) return false;
    command.hasPower = true;
    command.power = parsed;
    anyField = true;
  }

  if (doc.containsKey("fanSw")) {
    int internalSpeed = fan_speed;
    if (!parseFanSwValue(doc["fanSw"], internalSpeed)) return false;
    command.hasFan = true;
    command.fanSpeed = internalSpeed;
    anyField = true;
  }

  if (doc.containsKey("mode")) {
    int parsedMode = hvac_mode;
    if (!parseModeValue(doc["mode"], parsedMode)) return false;
    command.hasMode = true;
    command.mode = parsedMode;
    anyField = true;
  }

  if (doc.containsKey("setPoint")) {
    float parsedSetPoint = setPoint;
    if (!parseFloatValue(doc["setPoint"], parsedSetPoint)) return false;
    command.hasSetPoint = true;
    command.setPointValue = parsedSetPoint;
    anyField = true;
  }

  return anyField;
}

inline void displayLinkRegisterServerEndpoints() {
  server.on(AM5_INDOOR_DISPLAY_RX_PATH, HTTP_OPTIONS,
            [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(200);
    displayLinkAddCorsHeaders(response);
    request->send(response);
  });

  server.on(
      AM5_INDOOR_DISPLAY_RX_PATH,
      HTTP_POST,
      [](AsyncWebServerRequest* request) {
        String* body = static_cast<String*>(request->_tempObject);
        request->_tempObject = nullptr;

        if (body == nullptr) {
          AsyncWebServerResponse* response = request->beginResponse(
              400, "application/json",
              "{\"status\":\"error\",\"message\":\"No body\"}");
          displayLinkAddCorsHeaders(response);
          request->send(response);
          return;
        }

        AM5DisplayPendingCommand command;
        const bool valid = displayLinkParseIncomingJson(*body, command);

        Serial.print("[DISPLAY][HTTP RX] JSON: ");
        Serial.println(*body);
        delete body;

        if (!valid) {
          AsyncWebServerResponse* response = request->beginResponse(
              400, "application/json",
              "{\"status\":\"error\",\"message\":\"Invalid display command\"}");
          displayLinkAddCorsHeaders(response);
          request->send(response);
          return;
        }

        displayLinkQueueCommand(command);

        AsyncWebServerResponse* response = request->beginResponse(
            202, "application/json",
            "{\"status\":\"queued\"}");
        displayLinkAddCorsHeaders(response);
        request->send(response);
      },
      nullptr,
      [](AsyncWebServerRequest* request,
         uint8_t* data,
         size_t len,
         size_t index,
         size_t total) {
        if (total == 0 || total > AM5_DISPLAY_MAX_BODY_BYTES ||
            index > total || len > total - index) {
          return;
        }

        String* body = static_cast<String*>(request->_tempObject);
        if (body == nullptr) {
          body = new String();
          if (body == nullptr) return;
          body->reserve(total + 1);
          request->_tempObject = body;
        }

        body->concat(reinterpret_cast<const char*>(data), len);
      });

  server.on("/display_link", HTTP_GET, [](AsyncWebServerRequest* request) {
    StaticJsonDocument<384> doc;
    doc["display_ip"] = AM5_DISPLAY_IP;
    doc["display_online"] = am5DisplayOnline ? 1 : 0;
    doc["powersw"] = system_power;
    doc["fanSw"] = fanSpeedToMqttFanSw();
    doc["mode"] = hvac_mode;
    doc["setPoint"] = setPoint;

    String body;
    serializeJson(doc, body);
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", body);
    displayLinkAddCorsHeaders(response);
    request->send(response);
  });

  Serial.print("[DISPLAY] RX endpoint ready: ");
  Serial.println(AM5_INDOOR_DISPLAY_RX_PATH);
}

inline bool displayLinkConsumePending(AM5DisplayPendingCommand& out) {
  portENTER_CRITICAL(&am5DisplayCommandMux);
  if (!am5DisplayCommandPending) {
    portEXIT_CRITICAL(&am5DisplayCommandMux);
    return false;
  }

  out = am5DisplayPendingCommand;
  am5DisplayPendingCommand = AM5DisplayPendingCommand{};
  am5DisplayCommandPending = false;
  portEXIT_CRITICAL(&am5DisplayCommandMux);
  return true;
}

inline void displayLinkApplyPendingCommand() {
  AM5DisplayPendingCommand command;
  if (!displayLinkConsumePending(command)) return;

  const int oldPower = system_power;
  const int oldFan = fan_speed;
  const int oldMode = hvac_mode;
  const float oldSetPoint = setPoint;

  // OFF first, ON last, matching the MQTT control path.
  if (command.hasPower && command.power == 0) setSystemPower(0);
  if (command.hasFan) setFanSpeed(command.fanSpeed);
  if (command.hasMode) setHVACMode(command.mode);
  if (command.hasSetPoint) setPoint = command.setPointValue;
  if (command.hasPower && command.power == 1) setSystemPower(1);

  preferences.begin("values", false);
  if (command.hasFan) preferences.putInt("fan_speed", fan_speed);
  if (command.hasMode) preferences.putInt("hvac_mode", hvac_mode);
  if (command.hasSetPoint) preferences.putFloat("setPoint", setPoint);
  preferences.end();

  const bool changed = oldPower != system_power ||
                       oldFan != fan_speed ||
                       oldMode != hvac_mode ||
                       fabsf(oldSetPoint - setPoint) > 0.001f;

  if (changed) {
    mqtt_publish_requested = true;
    am5DisplayForcePush = true;
    if (oldPower != system_power) rs485_urgent_outdoor_sync = true;
  }

  Serial.print("[DISPLAY][CMD] accepted | powersw=");
  Serial.print(system_power);
  Serial.print(" fanSw=");
  Serial.print(fanSpeedToMqttFanSw());
  Serial.print(" mode=");
  Serial.print(hvac_mode);
  Serial.print(" setPoint=");
  Serial.println(setPoint, 1);
}

inline bool displayLinkStateChanged() {
  return am5DisplayLastPower != system_power ||
         am5DisplayLastFanSw != fanSpeedToMqttFanSw() ||
         am5DisplayLastMode != hvac_mode ||
         am5DisplayLastIndoorSw != indoor_sw ||
         am5DisplayLastOutdoorSw != outdoor_sw ||
         am5DisplayLastStatusOut != effectiveOutdoorStatusForPublish() ||
         isnan(am5DisplayLastSetPoint) ||
         fabsf(am5DisplayLastSetPoint - setPoint) > 0.001f;
}

inline void displayLinkRememberState() {
  am5DisplayLastPower = system_power;
  am5DisplayLastFanSw = fanSpeedToMqttFanSw();
  am5DisplayLastMode = hvac_mode;
  am5DisplayLastIndoorSw = indoor_sw;
  am5DisplayLastOutdoorSw = outdoor_sw;
  am5DisplayLastStatusOut = effectiveOutdoorStatusForPublish();
  am5DisplayLastSetPoint = setPoint;
}

inline bool displayLinkPostAuthoritativeState() {
  if (WiFi.getMode() == WIFI_MODE_NULL || WiFi.softAPgetStationNum() == 0) {
    am5DisplayOnline = false;
    return false;
  }

  StaticJsonDocument<512> doc;
  doc["powersw"] = system_power;
  doc["fanSw"] = fanSpeedToMqttFanSw();
  doc["mode"] = hvac_mode;
  doc["setPoint"] = setPoint;
  doc["indoorsw"] = indoor_sw;
  doc["outdoorsw"] = outdoor_sw;
  doc["supply"] = mqttTemperatureValue(SupplyTempC);
  doc["return"] = mqttTemperatureValue(ReturnTempC);
  doc["statusout"] = effectiveOutdoorStatusForPublish();
  doc["outdoor_online"] = rs485_outdoor_online ? 1 : 0;

  String body;
  serializeJson(doc, body);

  WiFiClient wifiClient;
  HTTPClient http;
  const String url = String("http://") + AM5_DISPLAY_IP + AM5_DISPLAY_RECEIVE_PATH;

  if (!http.begin(wifiClient, url)) {
    Serial.println("[DISPLAY][HTTP TX] http.begin failed");
    am5DisplayOnline = false;
    return false;
  }

  http.setReuse(false);
  http.setConnectTimeout(AM5_DISPLAY_HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(AM5_DISPLAY_HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");

  const int code = http.POST(body);
  http.end();

  if (code < 200 || code >= 300) {
    am5DisplayOnline = false;
    Serial.print("[DISPLAY][HTTP TX] FAILED code=");
    Serial.print(code);
    Serial.print(" JSON=");
    Serial.println(body);
    return false;
  }

  am5DisplayOnline = true;
  am5DisplayLastSuccessMs = millis();
  displayLinkRememberState();

  Serial.print("[DISPLAY][HTTP TX] SUCCESS -> ");
  Serial.print(AM5_DISPLAY_IP);
  Serial.print(" JSON=");
  Serial.println(body);
  return true;
}

inline void displayLinkService() {
  displayLinkApplyPendingCommand();

  const unsigned long now = millis();
  const bool changed = displayLinkStateChanged();
  const bool periodicDue = (now - am5DisplayLastPushMs) >= AM5_DISPLAY_PUSH_INTERVAL_MS;

  if (!am5DisplayForcePush && !changed && !periodicDue) return;
  if (static_cast<int32_t>(now - am5DisplayNextAttemptMs) < 0) return;

  am5DisplayLastPushMs = now;
  const bool ok = displayLinkPostAuthoritativeState();
  am5DisplayForcePush = false;
  am5DisplayNextAttemptMs = now + (ok ? AM5_DISPLAY_PUSH_INTERVAL_MS
                                     : AM5_DISPLAY_OFFLINE_RETRY_MS);
}

#endif
