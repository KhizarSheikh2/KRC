#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#include "DisplayLink.h"
#include "DisplayState.h"
#include "DisplayLinkConfig.h"

namespace {
WiFiClient hardwareClient;
char rxBuffer[AM4_DISPLAY_RX_BUFFER_SIZE];
size_t rxLength = 0;
unsigned long lastWiFiAttemptMs = 0;
unsigned long lastTcpAttemptMs = 0;
unsigned long lastHeartbeatMs = 0;
unsigned long lastStatusMs = 0;
uint32_t messageSequence = 0;
unsigned long tcpConnectedMs = 0;
bool wasWiFiConnected = false;
bool wasTcpConnected = false;

const char* wifiStatusText(wl_status_t status) {
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

bool sendJson(const JsonDocument& document) {
  if (!hardwareClient || !hardwareClient.connected()) {
    return false;
  }

  char payload[AM4_DISPLAY_TX_BUFFER_SIZE];
  const size_t requiredLength = measureJson(document);
  if (requiredLength == 0 || requiredLength + 2 > sizeof(payload)) {
    Serial.printf("[HARDWARE TX] JSON too large: %u\n", static_cast<unsigned>(requiredLength));
    return false;
  }

  const size_t length = serializeJson(document, payload, sizeof(payload));
  if (length != requiredLength) {
    Serial.println("[HARDWARE TX] JSON serialization failed");
    return false;
  }

  const size_t payloadWritten = hardwareClient.write(
    reinterpret_cast<const uint8_t*>(payload), length
  );
  const size_t newlineWritten = hardwareClient.write(static_cast<uint8_t>('\n'));
  const bool sent = payloadWritten == length && newlineWritten == 1;

#if AM4_LINK_DEBUG
  if (sent) {
    Serial.print("[HARDWARE TX] ");
    Serial.println(payload);
  }
#endif

  return sent;
}

void sendHello() {
  StaticJsonDocument<192> message;
  message["v"] = 1;
  message["type"] = "hello";
  message["role"] = "display";
  message["token"] = AM4_DISPLAY_LINK_TOKEN;
  message["seq"] = ++messageSequence;
  sendJson(message);
}

bool sendHeartbeat() {
  StaticJsonDocument<192> message;
  message["v"] = 1;
  message["type"] = "heartbeat";
  message["token"] = AM4_DISPLAY_LINK_TOKEN;
  message["seq"] = ++messageSequence;
  return sendJson(message);
}

void applyHardwareState(const JsonDocument& state) {
  const uint32_t sequence = state["seq"] | 0U;
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
  remoteMachineShutdown = state["machineShutdown"] | 0;
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

void handleMessage(const char* payload, size_t length) {
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
    applyHardwareState(document);
    return;
  }

  if (strcmp(type, "ack") == 0) {
    am4LastAckCommand = document["command"] | "";
    am4LastAckAccepted = document["accepted"] | false;
    am4LastAckMessage = document["message"] | "";
    am4LastAckMs = millis();

    Serial.print("[HARDWARE ACK] ");
    Serial.print(am4LastAckCommand);
    Serial.print(" -> ");
    Serial.println(am4LastAckMessage);
  }
}

void readClient() {
  if (!hardwareClient || !hardwareClient.connected()) {
    return;
  }

  size_t bytesProcessed = 0;
  constexpr size_t MAX_BYTES_PER_LOOP = 640;

  while (hardwareClient.available() && bytesProcessed < MAX_BYTES_PER_LOOP) {
    const int byteValue = hardwareClient.read();
    if (byteValue < 0) {
      break;
    }
    ++bytesProcessed;

    const char character = static_cast<char>(byteValue);
    if (character == '\r') {
      continue;
    }

    if (character == '\n') {
      if (rxLength > 0) {
        rxBuffer[rxLength] = '\0';
        handleMessage(rxBuffer, rxLength);
        rxLength = 0;
      }
      continue;
    }

    if (rxLength >= sizeof(rxBuffer) - 1) {
      rxLength = 0;
      Serial.println("[HARDWARE RX] Message too large; buffer reset");
      continue;
    }

    rxBuffer[rxLength++] = character;
  }
}

void beginHardwareWiFi() {
  hardwareClient.stop();
  am4DisplayLinkConnected = false;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

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
  lastWiFiAttemptMs = millis();
}
}  // namespace

bool sendAM4HardwareCommand(const char* command) {
  if (command == nullptr || command[0] == '\0') {
    return false;
  }

  StaticJsonDocument<224> message;
  message["v"] = 1;
  message["type"] = "command";
  message["command"] = command;
  message["token"] = AM4_DISPLAY_LINK_TOKEN;
  message["seq"] = ++messageSequence;
  return sendJson(message);
}

bool sendAM4StartCommand() { return sendAM4HardwareCommand("start"); }
bool sendAM4StopCommand()  { return sendAM4HardwareCommand("stop"); }
bool sendAM4ResetCommand() { return sendAM4HardwareCommand("reset"); }

void initDisplayLink() {
  beginHardwareWiFi();
}

void serviceDisplayLink() {
  const unsigned long now = millis();
  const wl_status_t wifiStatus = WiFi.status();
  const bool wifiConnected = wifiStatus == WL_CONNECTED;

  if (wifiConnected != wasWiFiConnected) {
    wasWiFiConnected = wifiConnected;
    if (wifiConnected) {
      Serial.println("\n[DISPLAY WIFI] CONNECTED TO HARDWARE AP");
      Serial.print("[DISPLAY WIFI] Local IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("[DISPLAY WIFI] RSSI: ");
      Serial.println(WiFi.RSSI());
      lastTcpAttemptMs = 0;
    } else {
      Serial.print("[DISPLAY WIFI] Disconnected. Status: ");
      Serial.println(wifiStatusText(wifiStatus));
      am4DisplayLinkConnected = false;
      hardwareClient.stop();
    }
  }

  if (!wifiConnected) {
    am4DisplayLinkConnected = false;
    if (hardwareClient) {
      hardwareClient.stop();
    }

    if (now - lastWiFiAttemptMs >= AM4_WIFI_RETRY_MS) {
      beginHardwareWiFi();
    }

#if AM4_LINK_DEBUG
    if (now - lastStatusMs >= AM4_DISPLAY_STATUS_INTERVAL_MS) {
      lastStatusMs = now;
      Serial.print("[DISPLAY STATUS] WiFi=");
      Serial.print(wifiStatusText(wifiStatus));
      Serial.println(" TCP=OFF");
    }
#endif
    return;
  }

  if (!hardwareClient.connected()) {
    am4DisplayLinkConnected = false;
    if (wasTcpConnected) {
      wasTcpConnected = false;
      Serial.println("[DISPLAY TCP] Connection lost");
    }

    if (lastTcpAttemptMs == 0 || now - lastTcpAttemptMs >= AM4_TCP_RETRY_MS) {
      lastTcpAttemptMs = now;
      hardwareClient.stop();

      if (hardwareClient.connect(
            AM4_HARDWARE_HOST_IP,
            AM4_DISPLAY_TCP_PORT,
            AM4_TCP_CONNECT_TIMEOUT_MS)) {
        hardwareClient.setNoDelay(true);
        hardwareClient.setTimeout(2);
        rxLength = 0;
        am4LastStateSequence = 0;
        am4LastStateReceivedMs = 0;
        tcpConnectedMs = now;
        lastHeartbeatMs = now;
        wasTcpConnected = true;
        Serial.println("[DISPLAY TCP] CONNECTED; sending hello");
        sendHello();
      }
    }
    return;
  }

  if (!wasTcpConnected) {
    wasTcpConnected = true;
    Serial.println("[DISPLAY TCP] CONNECTED");
  }

  readClient();

  if (now - lastHeartbeatMs >= AM4_HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = now;
    if (!sendHeartbeat()) {
      hardwareClient.stop();
      return;
    }
  }

  const bool waitingForFirstState = !am4DisplayLinkConnected && am4LastStateReceivedMs == 0;
  const bool stateTimedOut = am4DisplayLinkConnected &&
                             now - am4LastStateReceivedMs > AM4_STATE_TIMEOUT_MS;
  const bool firstStateTimedOut = waitingForFirstState &&
                                  now - tcpConnectedMs > AM4_STATE_TIMEOUT_MS;

  if (stateTimedOut || firstStateTimedOut) {
    am4DisplayLinkConnected = false;
    hardwareClient.stop();
    Serial.println("[DISPLAY TCP] Hardware JSON timeout; reconnecting");
  }

#if AM4_LINK_DEBUG
  if (now - lastStatusMs >= AM4_DISPLAY_STATUS_INTERVAL_MS) {
    lastStatusMs = now;
    Serial.print("[DISPLAY STATUS] WiFi=CONNECTED IP=");
    Serial.print(WiFi.localIP());
    Serial.print(" TCP=");
    Serial.print(hardwareClient.connected() ? "CONNECTED" : "OFF");
    Serial.print(" JSON=");
    Serial.print(am4DisplayLinkConnected ? "RECEIVING" : "WAITING");
    Serial.print(" lastSeq=");
    Serial.println(am4LastStateSequence);
  }
#endif
}
