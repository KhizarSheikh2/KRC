#pragma once

#include "DisplayLinkConfig.h"

// =====================================================
// Persistent Hardware <-> Display Wi-Fi link
// =====================================================
// Hardware always uses AP+STA:
//   AP  = local display link, always available
//   STA = router/internet link for MQTT
const IPAddress AM4_HOST_AP_IP(192, 168, 4, 1);
const IPAddress AM4_HOST_AP_GATEWAY(192, 168, 4, 1);
const IPAddress AM4_HOST_AP_SUBNET(255, 255, 255, 0);

constexpr uint32_t AM4_ROUTER_RETRY_INTERVAL_MS = 10000UL;

unsigned long am4LastHostApHealthCheckMs = 0;
unsigned long am4LastHostNetworkStatusMs = 0;
unsigned long am4LastRouterRetryMs = 0;
bool am4HardwareApConfigured = false;
bool am4LastRouterConnected = false;
uint8_t am4LastDisplayStationCount = 255;

bool isHardwareHostAccessPointActive() {
  const wifi_mode_t mode = WiFi.getMode();
  const bool apModeEnabled = (mode == WIFI_AP || mode == WIFI_AP_STA);
  return apModeEnabled && WiFi.softAPIP() == AM4_HOST_AP_IP;
}

bool ensureHardwareHostAccessPoint() {
  if (WiFi.getMode() != WIFI_AP_STA) {
    WiFi.mode(WIFI_AP_STA);
  }

  // Wi-Fi power saving can add long TCP latency on a continuously updating UI.
  WiFi.setSleep(false);

  if (am4HardwareApConfigured && isHardwareHostAccessPointActive()) {
    return true;
  }

  if (!WiFi.softAPConfig(AM4_HOST_AP_IP, AM4_HOST_AP_GATEWAY, AM4_HOST_AP_SUBNET)) {
    Serial.println("[HOST AP] Failed to configure 192.168.4.1");
    return false;
  }

  // In AP+STA mode the ESP32 radio uses the router's channel when STA is
  // connected. Before that, channel 1 is used and ESP32 migrates the AP when
  // the station joins a router on another channel.
  const uint8_t channel =
    (WiFi.status() == WL_CONNECTED && WiFi.channel() > 0)
      ? static_cast<uint8_t>(WiFi.channel())
      : 1;

  const bool started = WiFi.softAP(
    AM4_HARDWARE_AP_SSID,
    AM4_HARDWARE_AP_PASSWORD,
    channel,
    false,
    4
  );

  am4HardwareApConfigured = started;

  if (started) {
    Serial.println("\n[HOST AP] STARTED");
    Serial.print("[HOST AP] SSID: ");
    Serial.println(AM4_HARDWARE_AP_SSID);
    Serial.print("[HOST AP] IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("[HOST AP] Channel: ");
    Serial.println(channel);
  } else {
    Serial.println("[HOST AP] FAILED TO START");
  }

  return started;
}

bool serviceHardwareHostAccessPoint() {
  const unsigned long now = millis();
  if (now - am4LastHostApHealthCheckMs < AM4_HOST_AP_HEALTH_CHECK_MS) {
    return false;
  }
  am4LastHostApHealthCheckMs = now;

  if (!isHardwareHostAccessPointActive()) {
    Serial.println("[HOST AP] Inactive; restarting AP");
    am4HardwareApConfigured = false;
    return ensureHardwareHostAccessPoint();
  }

  const uint8_t stationCount = WiFi.softAPgetStationNum();
  if (stationCount != am4LastDisplayStationCount) {
    am4LastDisplayStationCount = stationCount;
    Serial.print("[HOST AP] Connected station count: ");
    Serial.println(stationCount);
  }

  return false;
}

void beginRouterConnection(const String& networkSsid, const String& networkPassword) {
  if (networkSsid.isEmpty()) {
    return;
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);

  Serial.print("[ROUTER] Connecting to: ");
  Serial.println(networkSsid);
  WiFi.begin(networkSsid.c_str(), networkPassword.c_str());
  am4LastRouterRetryMs = millis();
}

void serviceRouterWiFi() {
  const unsigned long now = millis();
  const bool connected = WiFi.status() == WL_CONNECTED;

  if (connected != am4LastRouterConnected) {
    am4LastRouterConnected = connected;
    is_wifi_connected = connected;

    if (connected) {
      myIP = WiFi.localIP().toString();
      wifi_channel = WiFi.channel();
      Serial.println("\n[ROUTER] CONNECTED");
      Serial.print("[ROUTER] IP: ");
      Serial.println(myIP);
      Serial.print("[ROUTER] Channel: ");
      Serial.println(wifi_channel);
      // The AP remains active and follows the STA channel automatically.
      ensureHardwareHostAccessPoint();
    } else {
      Serial.println("[ROUTER] DISCONNECTED - local display AP remains active");
    }
  }

  if (!connected && !ssid.isEmpty() &&
      now - am4LastRouterRetryMs >= AM4_ROUTER_RETRY_INTERVAL_MS) {
    am4LastRouterRetryMs = now;
    Serial.println("[ROUTER] Retrying station connection");
    WiFi.begin(ssid.c_str(), password.c_str());
  }

#if AM4_LINK_DEBUG
  if (now - am4LastHostNetworkStatusMs >= AM4_HOST_LINK_STATUS_INTERVAL_MS) {
    am4LastHostNetworkStatusMs = now;
    Serial.print("[NETWORK] mode=");
    Serial.print(static_cast<int>(WiFi.getMode()));
    Serial.print(" AP_IP=");
    Serial.print(WiFi.softAPIP());
    Serial.print(" AP_clients=");
    Serial.print(WiFi.softAPgetStationNum());
    Serial.print(" STA_status=");
    Serial.print(static_cast<int>(WiFi.status()));
    Serial.print(" STA_IP=");
    Serial.println(WiFi.localIP());
  }
#endif
}

void notFound(AsyncWebServerRequest* request) {
  request->send(404, "text/html; charset=UTF-8", "Not found");
}

void saveCredentials(const String& networkSsid, const String& networkPassword) {
  preferences.begin("wifi-creds", false);
  preferences.putString("ssid", networkSsid);
  preferences.putString("password", networkPassword);
  preferences.end();
}

void wifi_check(const String& networkSsid, const String& networkPassword) {
  beginRouterConnection(networkSsid, networkPassword);

  const unsigned long checkStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - checkStart < 5000UL) {
    delay(10);
  }

  is_wifi_connected = WiFi.status() == WL_CONNECTED;
  if (is_wifi_connected) {
    Serial.print("[ROUTER] Connected to ");
    Serial.println(networkSsid);
  } else {
    Serial.println("[ROUTER] Connection still pending; retries continue in loop");
  }

  // Never allow router configuration to disable the display AP.
  ensureHardwareHostAccessPoint();
}

//-------THIS CODE IS FOR CREATING API----//
void server_setup() {
  server.on("/wifi-config", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      Serial.println("WiFi Config From App");
      ssid = request->getParam("ssid", true)->value();
      password = request->getParam("password", true)->value();

      saveCredentials(ssid, password);
      request->send(200, "text/plain", "Wi-Fi credentials received and saved. Restarting...");
      delay(1000);
      ESP.restart();
    } else {
      request->send(400, "text/plain", "Please send both SSID and password.");
    }
  });

  server.on("/wifi_param_by_app", HTTP_OPTIONS, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });

  server.on(
    "/wifi_param_by_app", HTTP_POST,
    [](AsyncWebServerRequest* request) { (void)request; },
    NULL,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len,
       size_t index, size_t total) {
      if (index != 0 || len != total) {
        if (index == 0) {
          request->send(400, "application/json",
                        "{\"status\":\"error\",\"message\":\"Chunked JSON body is not supported\"}");
        }
        return;
      }

      StaticJsonDocument<256> doc;
      if (deserializeJson(doc, data, len)) {
        request->send(400, "application/json",
                      "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
      }

      ssid = doc["ssid"] | "";
      password = doc["password"] | "";

      if (ssid.isEmpty() || password.isEmpty()) {
        request->send(400, "application/json",
                      "{\"status\":\"error\",\"message\":\"SSID or password missing\"}");
        return;
      }

      Serial.print("[ROUTER] Credentials received for: ");
      Serial.println(ssid);
      wifi_check(ssid, password);

      StaticJsonDocument<160> responseDoc;
      responseDoc["status"] = "success";
      responseDoc["message"] = "WiFi parameters saved; connection started.";
      responseDoc["wifi_status"] = is_wifi_connected ? 1 : 0;

      String responseBody;
      serializeJson(responseDoc, responseBody);
      request->send(200, "application/json", responseBody);

      saveCredentials(ssid, password);
      ensureHardwareHostAccessPoint();
    }
  );

  server.onNotFound(notFound);
  Serial.printf("\nHeap before server begin: %d\n", ESP.getFreeHeap());
  server.begin();
  Serial.println("HTTP configuration server started on port 80");
}

bool loadCredentials(String& networkSsid, String& networkPassword) {
  preferences.begin("wifi-creds", true);
  networkSsid = preferences.getString("ssid", "");
  networkPassword = preferences.getString("password", "");
  preferences.end();
  return networkSsid.length() > 0 && networkPassword.length() > 0;
}

void startAccessPoint() {
  Serial.println("[HOST AP] Starting permanent display AP");
  ensureHardwareHostAccessPoint();
}

void connectToWiFi(const String& networkSsid, const String& networkPassword) {
  // Non-blocking: display AP and TCP server remain responsive while router and
  // MQTT connections are established independently.
  beginRouterConnection(networkSsid, networkPassword);
}
