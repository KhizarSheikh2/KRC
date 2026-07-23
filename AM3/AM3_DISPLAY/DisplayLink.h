#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "DisplayState.h"
#include "DisplayLinkConfig.h"

WiFiClient am3HardwareClient;
char am3HardwareRxBuffer[AM3_DISPLAY_RX_BUFFER_SIZE];
size_t am3HardwareRxLength = 0;
unsigned long am3LastWiFiAttemptMs = 0;
unsigned long am3LastTcpAttemptMs = 0;
unsigned long am3LastHeartbeatMs = 0;
unsigned long am3LastDisplayStatusMs = 0;
uint32_t am3DisplayMessageSequence = 0;
unsigned long am3TcpConnectedMs = 0;
bool am3WasWiFiConnected = false;
bool am3WasTcpConnected = false;

const char* am3WiFiStatusText(wl_status_t status) {
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

bool am3SendHardwareJson(const JsonDocument& document) {
  if (!am3HardwareClient || !am3HardwareClient.connected()) {
    Serial.println("[HARDWARE TX] TCP client is not connected");
    return false;
  }

  char payload[AM3_DISPLAY_TX_BUFFER_SIZE];
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

  const size_t payloadWritten = am3HardwareClient.write(
    reinterpret_cast<const uint8_t*>(payload), length
  );
  const size_t newlineWritten = am3HardwareClient.write(static_cast<uint8_t>('\n'));
  const bool sent = payloadWritten == length && newlineWritten == 1;

#if AM3_LINK_DEBUG
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

void am3SendHello() {
  StaticJsonDocument<192> message;
  message["v"] = 1;
  message["type"] = "hello";
  message["role"] = "display";
  message["token"] = AM3_DISPLAY_LINK_TOKEN;
  message["seq"] = ++am3DisplayMessageSequence;
  am3SendHardwareJson(message);
}

bool am3SendHeartbeat() {
  StaticJsonDocument<192> message;
  message["v"] = 1;
  message["type"] = "heartbeat";
  message["token"] = AM3_DISPLAY_LINK_TOKEN;
  message["seq"] = ++am3DisplayMessageSequence;
  return am3SendHardwareJson(message);
}

bool sendAM3HardwareCommand(const char* command) {
  if (command == nullptr || command[0] == '\0') {
    return false;
  }

  StaticJsonDocument<224> message;
  message["v"] = 1;
  message["type"] = "command";
  message["command"] = command;
  message["token"] = AM3_DISPLAY_LINK_TOKEN;
  message["seq"] = ++am3DisplayMessageSequence;
  return am3SendHardwareJson(message);
}

bool sendAM3StartCommand() { return sendAM3HardwareCommand("start"); }
bool sendAM3StopCommand()  { return sendAM3HardwareCommand("stop"); }
bool sendAM3ResetCommand() { return sendAM3HardwareCommand("reset"); }

void am3ApplyHardwareState(const JsonDocument& state) {
  const uint32_t sequence = state["seq"] | 0;
  if (am3LastStateSequence != 0 && sequence <= am3LastStateSequence) {
    return;
  }
  am3LastStateSequence = sequence;

  ReturnTempC = state["returnC"] | SENSOR_NOT_SELECTED;
  SuctionTempC = state["suctionC"] | SENSOR_NOT_SELECTED;
  dischargeTempC = state["dischargeC"] | SENSOR_NOT_SELECTED;
  SupplyTempC = state["supplyC"] | SENSOR_NOT_SELECTED;
  OilTempC = state["oilC"] | SENSOR_NOT_SELECTED;

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
  remoteDeviceName = state["device"] | "AM3";

  if (state.containsKey("epochUtc")) {
    const uint32_t epoch = state["epochUtc"].as<uint32_t>();
    if (epoch > 1700000000UL) {
      am3RemoteEpochUtc = epoch;
      am3RemoteEpochReceivedMs = millis();
      am3RemoteTimeValid = true;
    }
  }

  am3LastStateReceivedMs = millis();
  am3DisplayLinkConnected = true;
}

void am3HandleHardwareMessage(const char* payload, size_t length) {
#if AM3_LINK_DEBUG
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
    am3ApplyHardwareState(document);
    return;
  }

  if (strcmp(type, "ack") == 0) {
    Serial.print("[HARDWARE ACK] ");
    Serial.print(document["command"] | "");
    Serial.print(" -> ");
    Serial.println(document["message"] | "");
  }
}

void am3ReadHardwareClient() {
  if (!am3HardwareClient || !am3HardwareClient.connected()) {
    return;
  }

  size_t bytesProcessed = 0;
  constexpr size_t MAX_BYTES_PER_LOOP = 640;

  while (am3HardwareClient.available() && bytesProcessed < MAX_BYTES_PER_LOOP) {
    const int byteValue = am3HardwareClient.read();
    if (byteValue < 0) {
      break;
    }
    bytesProcessed++;

    const char character = static_cast<char>(byteValue);
    if (character == '\r') {
      continue;
    }

    if (character == '\n') {
      if (am3HardwareRxLength > 0) {
        am3HardwareRxBuffer[am3HardwareRxLength] = '\0';
        am3HandleHardwareMessage(am3HardwareRxBuffer, am3HardwareRxLength);
        am3HardwareRxLength = 0;
      }
      continue;
    }

    if (am3HardwareRxLength >= sizeof(am3HardwareRxBuffer) - 1) {
      am3HardwareRxLength = 0;
      Serial.println("[HARDWARE RX] Message too large; buffer reset");
      continue;
    }

    am3HardwareRxBuffer[am3HardwareRxLength++] = character;
  }
}

void am3BeginHardwareWiFi() {
  am3HardwareClient.stop();
  am3DisplayLinkConnected = false;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

  // A fixed IP removes DHCP as a possible failure point on the dedicated link.
  if (!WiFi.config(
        AM3_DISPLAY_LOCAL_IP,
        AM3_DISPLAY_GATEWAY,
        AM3_DISPLAY_SUBNET,
        AM3_DISPLAY_DNS)) {
    Serial.println("[DISPLAY WIFI] Static IP configuration failed");
  }

  Serial.print("[DISPLAY WIFI] Connecting to SSID: ");
  Serial.println(AM3_HARDWARE_AP_SSID);
  WiFi.begin(AM3_HARDWARE_AP_SSID, AM3_HARDWARE_AP_PASSWORD);
  am3LastWiFiAttemptMs = millis();
}

void initDisplayLink() {
  am3BeginHardwareWiFi();
}

void serviceDisplayLink() {
  const unsigned long now = millis();
  const wl_status_t wifiStatus = WiFi.status();
  const bool wifiConnected = wifiStatus == WL_CONNECTED;

  if (wifiConnected != am3WasWiFiConnected) {
    am3WasWiFiConnected = wifiConnected;
    if (wifiConnected) {
      Serial.println("\n[DISPLAY WIFI] CONNECTED TO HARDWARE AP");
      Serial.print("[DISPLAY WIFI] Local IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("[DISPLAY WIFI] Gateway: ");
      Serial.println(WiFi.gatewayIP());
      Serial.print("[DISPLAY WIFI] RSSI: ");
      Serial.println(WiFi.RSSI());
      am3LastTcpAttemptMs = 0;  // attempt TCP immediately
    } else {
      Serial.print("[DISPLAY WIFI] Disconnected. Status: ");
      Serial.println(am3WiFiStatusText(wifiStatus));
      am3DisplayLinkConnected = false;
      am3HardwareClient.stop();
    }
  }

  if (!wifiConnected) {
    am3DisplayLinkConnected = false;
    if (am3HardwareClient) {
      am3HardwareClient.stop();
    }

    if (now - am3LastWiFiAttemptMs >= AM3_WIFI_RETRY_MS) {
      Serial.print("[DISPLAY WIFI] Retry. Current status: ");
      Serial.println(am3WiFiStatusText(wifiStatus));
      am3BeginHardwareWiFi();
    }

#if AM3_LINK_DEBUG
    if (now - am3LastDisplayStatusMs >= AM3_DISPLAY_STATUS_INTERVAL_MS) {
      am3LastDisplayStatusMs = now;
      Serial.print("[DISPLAY STATUS] WiFi=");
      Serial.print(am3WiFiStatusText(wifiStatus));
      Serial.println(" TCP=OFF");
    }
#endif
    return;
  }

  if (!am3HardwareClient.connected()) {
    am3DisplayLinkConnected = false;
    if (am3WasTcpConnected) {
      am3WasTcpConnected = false;
      Serial.println("[DISPLAY TCP] Connection lost");
    }

    if (am3LastTcpAttemptMs == 0 ||
        now - am3LastTcpAttemptMs >= AM3_TCP_RETRY_MS) {
      am3LastTcpAttemptMs = now;
      am3HardwareClient.stop();
      Serial.print("[DISPLAY TCP] Connecting to ");
      Serial.print(AM3_HARDWARE_HOST_IP);
      Serial.print(':');
      Serial.println(AM3_DISPLAY_TCP_PORT);

      if (am3HardwareClient.connect(
            AM3_HARDWARE_HOST_IP,
            AM3_DISPLAY_TCP_PORT,
            AM3_TCP_CONNECT_TIMEOUT_MS)) {
        am3HardwareClient.setNoDelay(true);
        am3HardwareClient.setTimeout(2);
        am3HardwareRxLength = 0;
        am3LastStateSequence = 0;
        am3LastStateReceivedMs = 0;
        am3TcpConnectedMs = now;
        am3LastHeartbeatMs = now;
        am3WasTcpConnected = true;
        Serial.println("[DISPLAY TCP] CONNECTED; sending hello");
        am3SendHello();
      } else {
        Serial.println("[DISPLAY TCP] Connect failed; will retry");
      }
    }
    return;
  }

  if (!am3WasTcpConnected) {
    am3WasTcpConnected = true;
    Serial.println("[DISPLAY TCP] CONNECTED");
  }

  am3ReadHardwareClient();

  if (now - am3LastHeartbeatMs >= AM3_HEARTBEAT_INTERVAL_MS) {
    am3LastHeartbeatMs = now;
    if (!am3SendHeartbeat()) {
      am3HardwareClient.stop();
      return;
    }
  }

  const bool waitingForFirstState =
    !am3DisplayLinkConnected && am3LastStateReceivedMs == 0;
  const bool stateTimedOut =
    am3DisplayLinkConnected &&
    now - am3LastStateReceivedMs > AM3_STATE_TIMEOUT_MS;
  const bool firstStateTimedOut =
    waitingForFirstState &&
    now - am3TcpConnectedMs > AM3_STATE_TIMEOUT_MS;

  if (stateTimedOut || firstStateTimedOut) {
    am3DisplayLinkConnected = false;
    am3HardwareClient.stop();
    Serial.println("[DISPLAY TCP] Hardware JSON timeout; reconnecting");
  }

#if AM3_LINK_DEBUG
  if (now - am3LastDisplayStatusMs >= AM3_DISPLAY_STATUS_INTERVAL_MS) {
    am3LastDisplayStatusMs = now;
    Serial.print("[DISPLAY STATUS] WiFi=CONNECTED IP=");
    Serial.print(WiFi.localIP());
    Serial.print(" TCP=");
    Serial.print(am3HardwareClient.connected() ? "CONNECTED" : "OFF");
    Serial.print(" JSON=");
    Serial.print(am3DisplayLinkConnected ? "RECEIVING" : "WAITING");
    Serial.print(" lastSeq=");
    Serial.println(am3LastStateSequence);
  }
#endif
}
