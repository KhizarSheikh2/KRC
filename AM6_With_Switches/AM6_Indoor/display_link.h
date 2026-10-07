#ifndef AM6_DISPLAY_LINK_H
#define AM6_DISPLAY_LINK_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <math.h>

// ============================================================
// AM6 INDOOR -> DISPLAY MONITORING LINK
// Indoor physical switches are now the control authority.
// The Display is monitoring-only and never changes power/fan/mode.
//
// Indoor SoftAP : 192.168.4.1
// Display STA   : 192.168.4.50
// Indoor -> Display: POST http://192.168.4.50/receive
// ============================================================

static const char* AM6_DISPLAY_IP = "192.168.4.50";
static const char* AM6_DISPLAY_RECEIVE_PATH = "/receive";
static const char* AM6_INDOOR_DISPLAY_RX_PATH = "/from_display";
static const uint32_t AM6_DISPLAY_PUSH_INTERVAL_MS = 1000UL;
static const uint32_t AM6_DISPLAY_OFFLINE_RETRY_MS = 1000UL;
static const uint32_t AM6_DISPLAY_HTTP_CONNECT_TIMEOUT_MS = 150UL;
static const uint32_t AM6_DISPLAY_HTTP_TIMEOUT_MS = 300UL;

static unsigned long am6DisplayLastPushMs = 0;
static unsigned long am6DisplayLastSuccessMs = 0;
static unsigned long am6DisplayNextAttemptMs = 0;
static bool am6DisplayOnline = false;
static bool am6DisplayForcePush = true;

static int am6DisplayLastPower = -1;
static int am6DisplayLastFanSw = -1;
static int am6DisplayLastMode = -1;
static int am6DisplayLastIndoorSw = -1;
static int am6DisplayLastOutdoorSw = -1;
static int am6DisplayLastStatusOut = -1;
static int am6DisplayLastStatusOutB = -1;
static int am6DisplayLastOutdoorOnline = -1;
static bool am6DisplayLastSwitch[8] = {false, false, false, false, false, false, false, false};
static bool am6DisplayLastRelay[4] = {false, false, false, false};
static float am6DisplayLastSetPoint = NAN;

inline void displayLinkAddCorsHeaders(AsyncWebServerResponse* response) {
  if (!response) return;
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

inline void displayLinkRegisterServerEndpoints() {
  // Retain the legacy endpoint so an old display build receives a clear
  // response instead of hanging. Commands are intentionally rejected.
  server.on(AM6_INDOOR_DISPLAY_RX_PATH, HTTP_OPTIONS,
            [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(200);
    displayLinkAddCorsHeaders(response);
    request->send(response);
  });

  server.on(AM6_INDOOR_DISPLAY_RX_PATH, HTTP_POST,
            [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(
        403, "application/json",
        "{\"status\":\"monitor_only\",\"message\":\"Physical Indoor switches are authoritative\"}");
    displayLinkAddCorsHeaders(response);
    request->send(response);
  });

  server.on("/display_link", HTTP_GET, [](AsyncWebServerRequest* request) {
    StaticJsonDocument<512> doc;
    doc["display_ip"] = AM6_DISPLAY_IP;
    doc["display_online"] = am6DisplayOnline ? 1 : 0;
    doc["monitor_only"] = 1;
    doc["powersw"] = system_power;
    doc["fanSw"] = fanSpeedToMqttFanSw();
    doc["mode"] = hvac_mode;
    doc["statusout"] = effectiveOutdoorStatusForPublish();
    doc["statusoutB"] = effectiveOutdoorStatusBForPublish();

    String body;
    serializeJson(doc, body);
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", body);
    displayLinkAddCorsHeaders(response);
    request->send(response);
  });

  Serial.println("[DISPLAY] Monitoring-only link enabled");
}

inline bool displayLinkStateChanged() {
  if (am6DisplayLastPower != system_power ||
      am6DisplayLastFanSw != fanSpeedToMqttFanSw() ||
      am6DisplayLastMode != hvac_mode ||
      am6DisplayLastIndoorSw != indoor_sw ||
      am6DisplayLastOutdoorSw != outdoor_sw ||
      am6DisplayLastStatusOut != effectiveOutdoorStatusForPublish() ||
      am6DisplayLastStatusOutB != effectiveOutdoorStatusBForPublish() ||
      am6DisplayLastOutdoorOnline != (rs485_outdoor_online ? 1 : 0) ||
      isnan(am6DisplayLastSetPoint) ||
      fabsf(am6DisplayLastSetPoint - setPoint) > 0.001f) {
    return true;
  }

  const bool relays[4] = {outdoorR1, outdoorR2, outdoorR3, outdoorR4};
  for (uint8_t i = 0; i < 8; ++i) {
    if (am6DisplayLastSwitch[i] != outdoorSwitchState[i]) return true;
  }
  for (uint8_t i = 0; i < 4; ++i) {
    if (am6DisplayLastRelay[i] != relays[i]) return true;
  }
  return false;
}

inline void displayLinkRememberState() {
  am6DisplayLastPower = system_power;
  am6DisplayLastFanSw = fanSpeedToMqttFanSw();
  am6DisplayLastMode = hvac_mode;
  am6DisplayLastIndoorSw = indoor_sw;
  am6DisplayLastOutdoorSw = outdoor_sw;
  am6DisplayLastStatusOut = effectiveOutdoorStatusForPublish();
  am6DisplayLastStatusOutB = effectiveOutdoorStatusBForPublish();
  am6DisplayLastOutdoorOnline = rs485_outdoor_online ? 1 : 0;
  am6DisplayLastSetPoint = setPoint;

  const bool relays[4] = {outdoorR1, outdoorR2, outdoorR3, outdoorR4};
  for (uint8_t i = 0; i < 8; ++i) am6DisplayLastSwitch[i] = outdoorSwitchState[i];
  for (uint8_t i = 0; i < 4; ++i) am6DisplayLastRelay[i] = relays[i];
}

inline bool displayLinkPostAuthoritativeState() {
  if (WiFi.getMode() == WIFI_MODE_NULL || WiFi.softAPgetStationNum() == 0) {
    am6DisplayOnline = false;
    return false;
  }

  StaticJsonDocument<1024> doc;
  doc["powersw"] = system_power;
  doc["fanSw"] = fanSpeedToMqttFanSw();
  doc["mode"] = hvac_mode;
  doc["setPoint"] = setPoint;
  doc["indoorsw"] = indoor_sw;
  doc["outdoorsw"] = outdoor_sw;
  doc["supply"] = mqttTemperatureValue(SupplyTempC);
  doc["return"] = mqttTemperatureValue(ReturnTempC);
  doc["statusout"] = effectiveOutdoorStatusForPublish();
  doc["statusoutB"] = effectiveOutdoorStatusBForPublish();
  doc["outdoor_online"] = rs485_outdoor_online ? 1 : 0;
  doc["outdoor_io_ok"] = outdoorPcfHealthy ? 1 : 0;

  doc["outhipsiA"] = outdoorSwitchState[0] ? 1 : 0;
  doc["outlopsiA"] = outdoorSwitchState[1] ? 1 : 0;
  doc["outoverloadA"] = outdoorSwitchState[2] ? 1 : 0;
  doc["outpowerA"] = outdoorSwitchState[3] ? 1 : 0;
  doc["outhipsiB"] = outdoorSwitchState[4] ? 1 : 0;
  doc["outlopsiB"] = outdoorSwitchState[5] ? 1 : 0;
  doc["outoverloadB"] = outdoorSwitchState[6] ? 1 : 0;
  doc["outpowerB"] = outdoorSwitchState[7] ? 1 : 0;

  doc["outR1"] = outdoorR1 ? 1 : 0;
  doc["outR2"] = outdoorR2 ? 1 : 0;
  doc["outR3"] = outdoorR3 ? 1 : 0;
  doc["outR4"] = outdoorR4 ? 1 : 0;

  String body;
  serializeJson(doc, body);

  WiFiClient wifiClient;
  HTTPClient http;
  const String url = String("http://") + AM6_DISPLAY_IP + AM6_DISPLAY_RECEIVE_PATH;

  if (!http.begin(wifiClient, url)) {
    Serial.println("[DISPLAY][HTTP TX] http.begin failed");
    am6DisplayOnline = false;
    return false;
  }

  http.setReuse(false);
  http.setConnectTimeout(AM6_DISPLAY_HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(AM6_DISPLAY_HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");

  const int code = http.POST(body);
  http.end();

  if (code < 200 || code >= 300) {
    am6DisplayOnline = false;
    Serial.print("[DISPLAY][HTTP TX] FAILED code=");
    Serial.println(code);
    return false;
  }

  am6DisplayOnline = true;
  am6DisplayLastSuccessMs = millis();
  displayLinkRememberState();

  Serial.print("[DISPLAY][HTTP TX] SUCCESS statusout=");
  Serial.print(effectiveOutdoorStatusForPublish());
  Serial.print(" statusoutB=");
  Serial.println(effectiveOutdoorStatusBForPublish());
  return true;
}

inline void displayLinkService() {
  const unsigned long now = millis();
  const bool changed = displayLinkStateChanged();
  const bool periodicDue = (now - am6DisplayLastPushMs) >= AM6_DISPLAY_PUSH_INTERVAL_MS;

  if (!am6DisplayForcePush && !changed && !periodicDue) return;
  if (static_cast<int32_t>(now - am6DisplayNextAttemptMs) < 0) return;

  am6DisplayLastPushMs = now;
  const bool ok = displayLinkPostAuthoritativeState();
  am6DisplayForcePush = false;
  am6DisplayNextAttemptMs = now + (ok ? AM6_DISPLAY_PUSH_INTERVAL_MS
                                     : AM6_DISPLAY_OFFLINE_RETRY_MS);
}

#endif
