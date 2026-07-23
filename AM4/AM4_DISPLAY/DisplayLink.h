#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "DisplayState.h"
#include "DisplayLinkConfig.h"

WiFiClient am4HardwareClient;
char am4HardwareRxBuffer[AM4_DISPLAY_RX_BUFFER_SIZE];
size_t am4HardwareRxLength = 0;
unsigned long am4LastWiFiAttemptMs = 0;
unsigned long am4LastTcpAttemptMs = 0;
unsigned long am4LastHeartbeatMs = 0;
unsigned long am4LastDisplayStatusMs = 0;
uint32_t am4DisplayMessageSequence = 0;
unsigned long am4TcpConnectedMs = 0;
bool am4WasWiFiConnected = false;
bool am4WasTcpConnected = false;

const char* am4WiFiStatusText(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:     return "IDLE";
    case WL_NO_SSID_AVAIL:   return "SSID_NOT_FOUND";
    case WL_SCAN_COMPLETED:  return "SCAN_COMPLETED";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED:    return "DISCONNECTED";
    default:                 return "UNKNOWN";
  }
}

bool am4SendHardwareJson(const JsonDocument& document) {
  if (!am4HardwareClient || !am4HardwareClient.connected()) {
    Serial.println("[HARDWARE TX] TCP client is not connected");
    return false;
  }

  char payload[AM4_DISPLAY_TX_BUFFER_SIZE];
  const size_t requiredLength = measureJson(document);
  if (requiredLength == 0 || requiredLength + 2 > sizeof(payload)) {
    Serial.print("[HARDWARE TX] JSON too large: ");
    Serial.println(requiredLength);
    return false;
  }

  const size_t length = serializeJson(document, payload, sizeof(payload));
  if (length != requiredLength) {
    Serial.println("[HARDWARE TX] JSON serialization failed");
    return false;
  }

  const size_t payloadWritten = am4HardwareClient.write(
    reinterpret_cast<const uint8_t*>(payload), length
  );
  const size_t newlineWritten = am4HardwareClient.write(static_cast<uint8_t>('\n'));
  const bool sent = payloadWritten == length && newlineWritten == 1;

#if AM4_LINK_DEBUG
  if (sent) {
    Serial.print("[HARDWARE TX] ");
    Serial.println(payload);
  }
#endif

  if (!sent) {
    Serial.println("[HARDWARE TX] Socket write failed");
  }
  return sent;
}

void am4SendHello() {
  StaticJsonDocument<192> message;
  message["v"] = 1;
  message["type"] = "hello";
  message["role"] = "display";
  message["token"] = AM4_DISPLAY_LINK_TOKEN;
  message["seq"] = ++am4DisplayMessageSequence;
  am4SendHardwareJson(message);
}

bool am4SendHeartbeat() {
  StaticJsonDocument<192> message;
  message["v"] = 1;
  message["type"] = "heartbeat";
  message["token"] = AM4_DISPLAY_LINK_TOKEN;
  message["seq"] = ++am4DisplayMessageSequence;
  return am4SendHardwareJson(message);
}

bool sendAM4HardwareCommand(const char* command) {
  if (command == nullptr || command[0] == '\0') {
    return false;
  }

  StaticJsonDocument<224> message;
  message["v"] = 1;
  message["type"] = "command";
  message["command"] = command;
  message["token"] = AM4_DISPLAY_LINK_TOKEN;
  message["seq"] = ++am4DisplayMessageSequence;
  return am4SendHardwareJson(message);
}

bool sendAM4StartCommand() { return sendAM4HardwareCommand("start"); }
bool sendAM4StopCommand()  { return sendAM4HardwareCommand("stop"); }
bool sendAM4ResetCommand() { return sendAM4HardwareCommand("reset"); }

void am4ApplyHardwareState(const JsonDocument& state) {
  const uint32_t sequence = state["seq"] | 0;
  if (am4LastStateSequence != 0 && sequence <= am4LastStateSequence) {
    return;
  }
  am4LastStateSequence = sequence;

  ReturnTempC = state["returnC"] | SENSOR_NOT_SELECTED;
  SuctionTempC = state["suctionC"] | SENSOR_NOT_SELECTED;
  dischargeTempC = state["dischargeC"] | SENSOR_NOT_SELECTED;
  SupplyTempC = state["supplyC"] | SENSOR_NOT_SELECTED;
  OilTempC = state["oilC"] | SENSOR_NOT_SELECTED;
  OtherTempC = state["otherC"] | SENSOR_NOT_SELECTED;

  suctionPressure = state["suctionPsi"] | "999";
  dischargePressure = state["dischargePsi"] | "999";
  suctionPressureB = state["suctionBar"] | "999";
  dischargePressureB = state["dischargeBar"] | "999";

  remoteCompStatus = state["Comp_status"] | 0;
  remoteRunOutput = state["runOutput"] | 0;
  remoteAlarmOutput = state["alarmOutput"] | 0;
  remoteMqttConnected = state["mqttConnected"] | 0;
  remoteInternetConnected = state["internetConnected"] | 0;
  remoteHps = state["hps"] | "";
  remoteLps = state["lps"] | "";
  remoteOps = state["ops"] | "";
  remoteDeviceName = state["device"] | "AM4";

  if (state.containsKey("epochUtc")) {
    const uint32_t epoch = state["epochUtc"].as<uint32_t>();
    if (epoch > 1700000000UL) {
      am4RemoteEpochUtc = epoch;
      am4RemoteEpochReceivedMs = millis();
      am4RemoteTimeValid = true;
    }
  }

  am4LastStateReceivedMs = millis();
  am4DisplayLinkConnected = true;
}

void am4HandleHardwareMessage(const char* payload, size_t length) {
#if AM4_LINK_DEBUG
  Serial.print("[HARDWARE RX] ");
  Serial.write(reinterpret_cast<const uint8_t*>(payload), length);
  Serial.println();
#endif

  StaticJsonDocument<1024> document;
  const DeserializationError error = deserializeJson(document, payload, length);
  if (error) {
    Serial.print("[HARDWARE RX] JSON error: ");
    Serial.println(error.c_str());
    return;
  }

  const int protocolVersion = document["v"] | 0;
  const char* type = document["type"] | "";
  if (protocolVersion != 1) {
    Serial.println("[HARDWARE RX] Unsupported protocol version");
    return;
  }

  if (strcmp(type, "state") == 0) {
    am4ApplyHardwareState(document);
    return;
  }

  if (strcmp(type, "ack") == 0) {
    Serial.print("[HARDWARE ACK] ");
    Serial.print(document["command"] | "");
    Serial.print(" -> ");
    Serial.println(document["message"] | "");
  }
}

void am4ReadHardwareClient() {
  if (!am4HardwareClient || !am4HardwareClient.connected()) {
    return;
  }

  size_t bytesProcessed = 0;
  constexpr size_t MAX_BYTES_PER_LOOP = 640;

  while (am4HardwareClient.available() && bytesProcessed < MAX_BYTES_PER_LOOP) {
    const int byteValue = am4HardwareClient.read();
    if (byteValue < 0) {
      break;
    }
    bytesProcessed++;

    const char character = static_cast<char>(byteValue);
    if (character == '\r') {
      continue;
    }

    if (character == '\n') {
      if (am4HardwareRxLength > 0) {
        am4HardwareRxBuffer[am4HardwareRxLength] = '\0';
        am4HandleHardwareMessage(am4HardwareRxBuffer, am4HardwareRxLength);
        am4HardwareRxLength = 0;
      }
      continue;
    }

    if (am4HardwareRxLength >= sizeof(am4HardwareRxBuffer) - 1) {
      am4HardwareRxLength = 0;
      Serial.println("[HARDWARE RX] Message too large; buffer reset");
      continue;
    }

    am4HardwareRxBuffer[am4HardwareRxLength++] = character;
  }
}

void am4BeginHardwareWiFi() {
  am4HardwareClient.stop();
  am4DisplayLinkConnected = false;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

  // A fixed IP removes DHCP as a possible failure point on the dedicated link.
  if (!WiFi.config(
        AM4_DISPLAY_LOCAL_IP,
        AM4_DISPLAY_GATEWAY,
        AM4_DISPLAY_SUBNET,
        AM4_DISPLAY_DNS)) {
    Serial.println("[DISPLAY WIFI] Static IP configuration failed");
  }

  Serial.print("[DISPLAY WIFI] Connecting to SSID: ");
  Serial.println(AM4_HARDWARE_AP_SSID);
  WiFi.begin(AM4_HARDWARE_AP_SSID, AM4_HARDWARE_AP_PASSWORD);
  am4LastWiFiAttemptMs = millis();
}

void initDisplayLink() {
  am4BeginHardwareWiFi();
}

void serviceDisplayLink() {
  const unsigned long now = millis();
  const wl_status_t wifiStatus = WiFi.status();
  const bool wifiConnected = wifiStatus == WL_CONNECTED;

  if (wifiConnected != am4WasWiFiConnected) {
    am4WasWiFiConnected = wifiConnected;
    if (wifiConnected) {
      Serial.println("\n[DISPLAY WIFI] CONNECTED TO HARDWARE AP");
      Serial.print("[DISPLAY WIFI] Local IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("[DISPLAY WIFI] Gateway: ");
      Serial.println(WiFi.gatewayIP());
      Serial.print("[DISPLAY WIFI] RSSI: ");
      Serial.println(WiFi.RSSI());
      am4LastTcpAttemptMs = 0;  // attempt TCP immediately
    } else {
      Serial.print("[DISPLAY WIFI] Disconnected. Status: ");
      Serial.println(am4WiFiStatusText(wifiStatus));
      am4DisplayLinkConnected = false;
      am4HardwareClient.stop();
    }
  }

  if (!wifiConnected) {
    am4DisplayLinkConnected = false;
    if (am4HardwareClient) {
      am4HardwareClient.stop();
    }

    if (now - am4LastWiFiAttemptMs >= AM4_WIFI_RETRY_MS) {
      Serial.print("[DISPLAY WIFI] Retry. Current status: ");
      Serial.println(am4WiFiStatusText(wifiStatus));
      am4BeginHardwareWiFi();
    }

#if AM4_LINK_DEBUG
    if (now - am4LastDisplayStatusMs >= AM4_DISPLAY_STATUS_INTERVAL_MS) {
      am4LastDisplayStatusMs = now;
      Serial.print("[DISPLAY STATUS] WiFi=");
      Serial.print(am4WiFiStatusText(wifiStatus));
      Serial.println(" TCP=OFF");
    }
#endif
    return;
  }

  if (!am4HardwareClient.connected()) {
    am4DisplayLinkConnected = false;
    if (am4WasTcpConnected) {
      am4WasTcpConnected = false;
      Serial.println("[DISPLAY TCP] Connection lost");
    }

    if (am4LastTcpAttemptMs == 0 ||
        now - am4LastTcpAttemptMs >= AM4_TCP_RETRY_MS) {
      am4LastTcpAttemptMs = now;
      am4HardwareClient.stop();
      Serial.print("[DISPLAY TCP] Connecting to ");
      Serial.print(AM4_HARDWARE_HOST_IP);
      Serial.print(':');
      Serial.println(AM4_DISPLAY_TCP_PORT);

      if (am4HardwareClient.connect(
            AM4_HARDWARE_HOST_IP,
            AM4_DISPLAY_TCP_PORT,
            AM4_TCP_CONNECT_TIMEOUT_MS)) {
        am4HardwareClient.setNoDelay(true);
        am4HardwareClient.setTimeout(2);
        am4HardwareRxLength = 0;
        am4LastStateSequence = 0;
        am4LastStateReceivedMs = 0;
        am4TcpConnectedMs = now;
        am4LastHeartbeatMs = now;
        am4WasTcpConnected = true;
        Serial.println("[DISPLAY TCP] CONNECTED; sending hello");
        am4SendHello();
      } else {
        Serial.println("[DISPLAY TCP] Connect failed; will retry");
      }
    }
    return;
  }

  if (!am4WasTcpConnected) {
    am4WasTcpConnected = true;
    Serial.println("[DISPLAY TCP] CONNECTED");
  }

  am4ReadHardwareClient();

  if (now - am4LastHeartbeatMs >= AM4_HEARTBEAT_INTERVAL_MS) {
    am4LastHeartbeatMs = now;
    if (!am4SendHeartbeat()) {
      am4HardwareClient.stop();
      return;
    }
  }

  const bool waitingForFirstState =
    !am4DisplayLinkConnected && am4LastStateReceivedMs == 0;
  const bool stateTimedOut =
    am4DisplayLinkConnected &&
    now - am4LastStateReceivedMs > AM4_STATE_TIMEOUT_MS;
  const bool firstStateTimedOut =
    waitingForFirstState &&
    now - am4TcpConnectedMs > AM4_STATE_TIMEOUT_MS;

  if (stateTimedOut || firstStateTimedOut) {
    am4DisplayLinkConnected = false;
    am4HardwareClient.stop();
    Serial.println("[DISPLAY TCP] Hardware JSON timeout; reconnecting");
  }

#if AM4_LINK_DEBUG
  if (now - am4LastDisplayStatusMs >= AM4_DISPLAY_STATUS_INTERVAL_MS) {
    am4LastDisplayStatusMs = now;
    Serial.print("[DISPLAY STATUS] WiFi=CONNECTED IP=");
    Serial.print(WiFi.localIP());
    Serial.print(" TCP=");
    Serial.print(am4HardwareClient.connected() ? "CONNECTED" : "OFF");
    Serial.print(" JSON=");
    Serial.print(am4DisplayLinkConnected ? "RECEIVING" : "WAITING");
    Serial.print(" lastSeq=");
    Serial.println(am4LastStateSequence);
  }
#endif
}
