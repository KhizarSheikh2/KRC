#ifndef WIFI_THREAD_H
#define WIFI_THREAD_H

#include <time.h>

// ============================================================
// AM5 Wi-Fi provisioning + AWS IoT MQTT connectivity
// Indoor hardware is the ONLY Wi-Fi/MQTT node.
// ============================================================

static String wifiProvisionBody;
static bool tlsTimeReady = false;
static bool tlsTimeSyncStarted = false;
static unsigned long lastTlsWaitLogMs = 0;
static int wifiProvisionLastResult = -1; // -1 not tested, 0 failed, 1 connected
static bool mqttClientConfigured = false;
static unsigned long nextMqttConnectAttemptMs = 0;
static unsigned long lastWifiRecoveryAttemptMs = 0;
static bool wifiProvisionInProgress = false;

static const unsigned long MQTT_RECONNECT_BACKOFF_MS = 10000UL;
static const unsigned long WIFI_RECOVERY_INTERVAL_MS = 10000UL;

void addCorsHeaders(AsyncWebServerResponse* response) {
  if (!response) return;
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

void notFound(AsyncWebServerRequest* request) {
  AsyncWebServerResponse* response = request->beginResponse(
      404, "application/json", "{\"status\":\"error\",\"message\":\"Not found\"}");
  addCorsHeaders(response);
  request->send(response);
}

void saveCredentials(const String& newSsid, const String& newPassword) {
  preferences.begin("wifi-creds", false);
  preferences.putString("ssid", newSsid);
  preferences.putString("password", newPassword);
  preferences.end();
}

void startClockSyncForTls() {
  if (WiFi.status() != WL_CONNECTED) {
    tlsTimeReady = false;
    tlsTimeSyncStarted = false;
    return;
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  tlsTimeSyncStarted = true;
  tlsTimeReady = false;
  Serial.println("[TIME] NTP synchronization started (non-blocking)");
}

bool isClockReadyForTls() {
  if (WiFi.status() != WL_CONNECTED) {
    tlsTimeReady = false;
    tlsTimeSyncStarted = false;
    return false;
  }

  const time_t now = time(nullptr);
  const time_t minimumValidEpoch = 1704067200; // 2024-01-01 UTC
  if (now < minimumValidEpoch) return false;

  if (!tlsTimeReady) {
    tlsTimeReady = true;
    struct tm timeInfo;
    localtime_r(&now, &timeInfo);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeInfo);
    Serial.print("[TIME] TLS clock ready: ");
    Serial.println(buffer);
  }
  return true;
}

void ensureVisibleAccessPoint(uint8_t channel = 1) {
  // The previous code used hidden=1. That prevents normal phone discovery.
  // hidden=false keeps AM5-AAA001 visible to both the mobile app and display.
  const IPAddress apIp(192, 168, 4, 1);
  const IPAddress subnetMask(255, 255, 255, 0);
  WiFi.softAPConfig(apIp, apIp, subnetMask);
  const bool apOk = WiFi.softAP(devicename.c_str(), "bitahomes", channel, false, 4);
  wifi_ap_mode = apOk;

  if (apOk) {
    Serial.print("[AP] Visible SSID: ");
    Serial.println(devicename);
    Serial.print("[AP] IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("[AP] Failed to start provisioning AP");
  }
}

void startAccessPoint() {
  Serial.println("[WIFI] Starting AM5 provisioning AP");
  WiFi.mode(WIFI_AP_STA);
  ensureVisibleAccessPoint(1);
  is_wifi_connected = (WiFi.status() == WL_CONNECTED);
}

bool connectStation(const String& targetSsid,
                    const String& targetPassword,
                    uint32_t timeoutMs) {
  if (targetSsid.isEmpty()) return false;

  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(hostname.c_str());
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);

  Serial.print("[WIFI] Connecting to: ");
  Serial.println(targetSsid);

  WiFi.begin(targetSsid.c_str(), targetPassword.c_str());
  const unsigned long started = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - started < timeoutMs) {
    delay(50);
  }

  is_wifi_connected = (WiFi.status() == WL_CONNECTED);
  if (!is_wifi_connected) {
    Serial.print("[WIFI] Connection failed, status=");
    Serial.println(static_cast<int>(WiFi.status()));
    tlsTimeReady = false;
    tlsTimeSyncStarted = false;
    return false;
  }

  myIP = WiFi.localIP().toString();
  wifi_channel = WiFi.channel();

  Serial.print("[WIFI] Connected, IP: ");
  Serial.println(myIP);
  Serial.print("[WIFI] Channel: ");
  Serial.println(wifi_channel);

  // Keep a visible local AM5 AP for provisioning/service access.
  // In AP+STA mode ESP32 keeps both interfaces on the STA channel.
  ensureVisibleAccessPoint(static_cast<uint8_t>(wifi_channel));
  startClockSyncForTls();
  return true;
}

bool wifi_check(const String& targetSsid, const String& targetPassword) {
  wifiProvisionInProgress = true;
  if (targetSsid.isEmpty()) {
    is_wifi_connected = false;
    wifiProvisionLastResult = 0;
    wifiProvisionInProgress = false;
    return false;
  }

  // Match the mobile-app provisioning contract used by the working
  // WT_COOLING project: test the credentials BEFORE returning HTTP 200.
  // Keep AM5 SoftAP alive because the JC2432W328 display uses it.
  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(hostname.c_str());
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);

  // Stop only the previous STA attempt; do NOT tear down the SoftAP.
  WiFi.disconnect(false, false);
  delay(50);

  Serial.println("[APP][WIFI] Checking Wi-Fi credentials");
  Serial.print("[APP][WIFI] SSID: ");
  Serial.println(targetSsid);
  Serial.print("[APP][WIFI] Connecting");

  WiFi.begin(targetSsid.c_str(), targetPassword.c_str());
  const unsigned long started = millis();
  constexpr unsigned long APP_WIFI_CHECK_TIMEOUT_MS = 4000UL;

  while (WiFi.status() != WL_CONNECTED &&
         millis() - started < APP_WIFI_CHECK_TIMEOUT_MS) {
    delay(10);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println();
    Serial.println("[APP][WIFI] FAILED within 4 seconds");
    WiFi.disconnect(false, false);
    is_wifi_connected = false;
    wifiProvisionLastResult = 0;
    tlsTimeReady = false;
    tlsTimeSyncStarted = false;
    wifiProvisionInProgress = false;
    return false;
  }

  is_wifi_connected = true;
  wifiProvisionLastResult = 1;
  myIP = WiFi.localIP().toString();
  wifi_channel = WiFi.channel();

  Serial.println();
  Serial.print("[APP][WIFI] CONNECTED | STA IP=");
  Serial.print(myIP);
  Serial.print(" | channel=");
  Serial.println(wifi_channel);

  // ESP32 AP+STA shares the radio/channel automatically. Do NOT call
  // softAPdisconnect() and do NOT restart the AP here; the HTTP response
  // must reach the mobile app and the display must stay connected/reconnect.
  startClockSyncForTls();
  mqtt_publish_requested = true;
  wifiProvisionInProgress = false;
  return true;
}

void connectToWiFi(const String& targetSsid, const String& targetPassword) {
  if (!connectStation(targetSsid, targetPassword, 12000UL)) {
    Serial.println("[WIFI] Saved credentials did not connect; AP remains available");
    startAccessPoint();
  }
}

bool loadCredentials(String& loadedSsid, String& loadedPassword) {
  preferences.begin("wifi-creds", true);
  loadedSsid = preferences.getString("ssid", "");
  loadedPassword = preferences.getString("password", "");
  preferences.end();
  return !loadedSsid.isEmpty() && !loadedPassword.isEmpty();
}

void sendDeviceStatus(AsyncWebServerRequest* request) {
  StaticJsonDocument<384> doc;
  doc["device"] = devicename;
  doc["ap_ssid"] = devicename;
  doc["ap_ip"] = WiFi.softAPIP().toString();
  doc["wifi_status"] = WiFi.status() == WL_CONNECTED ? 1 : 0;
  doc["mqtt_status"] = client.connected() ? 1 : 0;
  doc["sta_ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
  doc["system_power"] = system_power;
  doc["provision_result"] = wifiProvisionLastResult;

  String body;
  serializeJson(doc, body);
  AsyncWebServerResponse* response = request->beginResponse(200, "application/json", body);
  addCorsHeaders(response);
  request->send(response);
}

void server_setup() {
  // Display uses the same Indoor HTTP server as provisioning.
  displayLinkRegisterServerEndpoints();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendDeviceStatus(request);
  });

  server.on("/device_status", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendDeviceStatus(request);
  });

  server.on("/wifi_param_by_app", HTTP_OPTIONS, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(200);
    addCorsHeaders(response);
    request->send(response);
  });

  server.on(
      "/wifi_param_by_app",
      HTTP_POST,
      [](AsyncWebServerRequest* request) { (void)request; },
      nullptr,
      [](AsyncWebServerRequest* request,
         uint8_t* data,
         size_t len,
         size_t index,
         size_t total) {
        // AsyncWebServer may deliver the JSON body in multiple chunks.
        if (index == 0) {
          wifiProvisionBody = "";
          wifiProvisionBody.reserve(total + 1);
        }

        for (size_t i = 0; i < len; ++i) {
          wifiProvisionBody += static_cast<char>(data[i]);
        }

        if (index + len != total) return;

        StaticJsonDocument<384> doc;
        const DeserializationError error = deserializeJson(doc, wifiProvisionBody);
        wifiProvisionBody = "";

        if (error) {
          AsyncWebServerResponse* response = request->beginResponse(
              400, "application/json",
              "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
          addCorsHeaders(response);
          request->send(response);
          return;
        }

        const String requestedSsid = doc["ssid"] | "";
        const String requestedPassword = doc["password"] | "";

        if (requestedSsid.isEmpty() || requestedPassword.isEmpty()) {
          AsyncWebServerResponse* response = request->beginResponse(
              400, "application/json",
              "{\"status\":\"error\",\"message\":\"SSID or password missing\"}");
          addCorsHeaders(response);
          request->send(response);
          return;
        }

        Serial.print("[APP] Wi-Fi credentials received for SSID: ");
        Serial.println(requestedSsid);

        // IMPORTANT: the existing mobile app expects the same synchronous
        // response contract as WT_COOLING_Final. Test first, then return the
        // final wifi_status (0/1) in this same HTTP request.
        const bool connected = wifi_check(requestedSsid, requestedPassword);

        // Match the working application's expected response keys/message.
        StaticJsonDocument<192> responseDoc;
        responseDoc["status"] = "success";
        responseDoc["message"] = "WiFi parameters saved. Restarting.";
        responseDoc["wifi_status"] = connected ? 1 : 0;

        String responseBody;
        serializeJson(responseDoc, responseBody);

        Serial.print("[APP][WIFI] Response: ");
        Serial.println(responseBody);

        AsyncWebServerResponse* response =
            request->beginResponse(200, "application/json", responseBody);
        addCorsHeaders(response);
        request->send(response);

        // WT_COOLING saves the credentials after returning the result. Keep
        // the same app-facing behavior. AM5 deliberately keeps SoftAP alive
        // for the display instead of calling WiFi.softAPdisconnect().
        ssid = requestedSsid;
        password = requestedPassword;
        saveCredentials(ssid, password);

        if (connected) {
          Serial.println("[APP][WIFI] Provisioning SUCCESS; AM5 AP remains ON for display");
        } else {
          Serial.println("[APP][WIFI] Provisioning FAILED; AM5 AP remains ON for retry/display");
        }
      });

  server.onNotFound(notFound);
  Serial.printf("[HTTP] Heap before server: %u\n", ESP.getFreeHeap());
  server.begin();
  Serial.println("[HTTP] Provisioning server ready");
}

void configureMqttClientOnce() {
  if (mqttClientConfigured) return;

  espClient.setCACert(root_ca);
  espClient.setCertificate(client_cert);
  espClient.setPrivateKey(client_key);

  client.setBufferSize(MQTT_BUFFER_SIZE);
  client.setKeepAlive(30);
  client.setSocketTimeout(5);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  mqttClientConfigured = true;
  Serial.println("[MQTT] TLS certificates/client settings configured once");
}

void serviceWiFiRecovery() {
  if (wifiProvisionInProgress) return;

  if (WiFi.status() == WL_CONNECTED) {
    is_wifi_connected = true;
    return;
  }

  is_wifi_connected = false;
  if (ssid.isEmpty()) return;

  const unsigned long now = millis();
  if (now - lastWifiRecoveryAttemptMs < WIFI_RECOVERY_INTERVAL_MS) return;
  lastWifiRecoveryAttemptMs = now;

  Serial.print("[WIFI] STA offline -> non-blocking reconnect to: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin(ssid.c_str(), password.c_str());

  // Drop stale cloud sockets immediately; the local SoftAP remains alive.
  client.disconnect();
  espClient.stop();
  tlsTimeReady = false;
  tlsTimeSyncStarted = false;
  nextMqttConnectAttemptMs = 0;
}

void reconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    is_wifi_connected = false;
    Serial.println("[MQTT] Wi-Fi is offline");
    return;
  }

  is_wifi_connected = true;
  if (client.connected()) return;

  if (!tlsTimeSyncStarted) startClockSyncForTls();
  if (!isClockReadyForTls()) {
    const unsigned long nowMs = millis();
    if (nowMs - lastTlsWaitLogMs >= 3000UL) {
      lastTlsWaitLogMs = nowMs;
      Serial.println("[MQTT] Waiting for NTP time before AWS TLS (non-blocking)");
    }
    return;
  }

  configureMqttClientOnce();

  const unsigned long nowMs = millis();
  if (static_cast<int32_t>(nowMs - nextMqttConnectAttemptMs) < 0) return;

  // Clear any half-open TLS socket left by the previous failure.
  client.disconnect();
  espClient.stop();

  Serial.print("[MQTT] Connecting to AWS IoT: ");
  Serial.print(mqtt_server);
  Serial.print(':');
  Serial.println(mqtt_port);

  if (!client.connect(devicename.c_str())) {
    Serial.print("[MQTT] Connect failed, PubSubClient state=");
    Serial.println(client.state());
    nextMqttConnectAttemptMs = millis() + MQTT_RECONNECT_BACKOFF_MS;
    espClient.stop();
    return;
  }

  nextMqttConnectAttemptMs = 0;
  Serial.println("[MQTT] Connected");

  // Match the original working AM5/WT-style receive behavior first: subscribe
  // to the wildcard filter. Some AWS IoT policies authorize the topic-filter
  // resource /test/<device>/# specifically; in that case exact subscriptions
  // can be rejected even while publishing continues to work.
  const bool subWildcard = client.subscribe(device_topic_s_m.c_str());
  Serial.print("[MQTT] SUB wildcard ");
  Serial.print(device_topic_s_m);
  Serial.print(": ");
  Serial.println(subWildcard ? "OK" : "FAILED");

  if (!subWildcard) {
    Serial.println("[MQTT] Wildcard subscription failed; trying exact-topic fallback");
    const bool subControl = client.subscribe(device_topic_s_control.c_str());
    const bool subOutdoor = client.subscribe(device_topic_s_outdoor_sensor.c_str());
    const bool subIndoor = client.subscribe(device_topic_s_indoor_sensor.c_str());

    Serial.print("[MQTT] SUB /2 control: "); Serial.println(subControl ? "OK" : "FAILED");
    Serial.print("[MQTT] SUB /3 outdoor config: "); Serial.println(subOutdoor ? "OK" : "FAILED");
    Serial.print("[MQTT] SUB /4 indoor config: "); Serial.println(subIndoor ? "OK" : "FAILED");

    if (!subControl || !subOutdoor || !subIndoor) {
      Serial.println("[MQTT] ERROR: one or more application receive topics are NOT subscribed");
    }
  }

  mqtt_publish_requested = true;
}

#endif
