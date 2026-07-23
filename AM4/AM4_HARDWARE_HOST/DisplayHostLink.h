#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <time.h>
#include "DisplayLinkConfig.h"

WiFiServer am4DisplayServer(AM4_DISPLAY_TCP_PORT, 1);
WiFiClient am4DisplayClient;

char am4DisplayRxBuffer[AM4_DISPLAY_RX_BUFFER_SIZE];
size_t am4DisplayRxLength = 0;
bool am4DisplayAuthenticated = false;
uint32_t am4DisplayStateSequence = 0;
unsigned long am4DisplayLastStateMs = 0;
unsigned long am4DisplayLastRxMs = 0;
unsigned long am4DisplayLastStatusMs = 0;
unsigned long am4DisplayTxCount = 0;

void am4SendHardwareState();

void am4CloseDisplayClient(const char* reason) {
  if (am4DisplayClient) {
    am4DisplayClient.stop();
  }
  am4DisplayAuthenticated = false;
  am4DisplayRxLength = 0;
  if (reason != nullptr) {
    Serial.print("[DISPLAY LINK] Disconnected: ");
    Serial.println(reason);
  }
}

bool am4SendDisplayJson(const JsonDocument& document) {
  if (!am4DisplayClient || !am4DisplayClient.connected()) {
#if AM4_LINK_DEBUG
    Serial.println("[DISPLAY TX] No connected TCP client");
#endif
    return false;
  }

  char payload[AM4_DISPLAY_TX_BUFFER_SIZE];
  const size_t requiredLength = measureJson(document);
  if (requiredLength == 0 || requiredLength + 2 > sizeof(payload)) {
    Serial.print("[DISPLAY TX] JSON too large: ");
    Serial.println(requiredLength);
    return false;
  }

  const size_t length = serializeJson(document, payload, sizeof(payload));
  if (length != requiredLength) {
    Serial.println("[DISPLAY TX] JSON serialization failed");
    return false;
  }

  const size_t payloadWritten = am4DisplayClient.write(
    reinterpret_cast<const uint8_t*>(payload), length
  );
  const size_t newlineWritten = am4DisplayClient.write(static_cast<uint8_t>('\n'));

  const bool sent = payloadWritten == length && newlineWritten == 1;
  if (!sent) {
    Serial.print("[DISPLAY TX] Socket write failed. expected=");
    Serial.print(length);
    Serial.print(" actual=");
    Serial.println(payloadWritten);
    return false;
  }

  am4DisplayTxCount++;
#if AM4_LINK_DEBUG
  // Print every state during commissioning so JSON transmission is visible.
  Serial.print("[DISPLAY TX #");
  Serial.print(am4DisplayTxCount);
  Serial.print("] ");
  Serial.println(payload);
#endif
  return true;
}

void am4SendDisplayAck(const char* command, bool accepted, const char* message) {
  StaticJsonDocument<256> response;
  response["v"] = 1;
  response["type"] = "ack";
  response["command"] = command != nullptr ? command : "";
  response["accepted"] = accepted;
  response["message"] = message != nullptr ? message : "";
  am4SendDisplayJson(response);
}

void am4HandleDisplayCommand(const JsonDocument& document) {
  const char* command = document["command"] | "";

  if (strcmp(command, "start") == 0) {
    StartSW_App = 1;
    startSW = true;
    am4SendDisplayAck(command, true, "Start request received by hardware");
    return;
  }

  if (strcmp(command, "stop") == 0) {
    StartSW_App = 0;
    stopSW = true;
    am4SendDisplayAck(command, true, "Stop request received by hardware");
    return;
  }

  if (strcmp(command, "reset") == 0) {
    resetSW = 1;
    am4SendDisplayAck(command, true, "Reset request received by hardware");
    return;
  }

  am4SendDisplayAck(command, false, "Unsupported display command");
}

void am4HandleDisplayMessage(const char* payload, size_t length) {
#if AM4_LINK_DEBUG
  Serial.print("[DISPLAY RX] ");
  Serial.write(reinterpret_cast<const uint8_t*>(payload), length);
  Serial.println();
#endif

  StaticJsonDocument<512> document;
  const DeserializationError error = deserializeJson(document, payload, length);
  if (error) {
    Serial.print("[DISPLAY RX] JSON error: ");
    Serial.println(error.c_str());
    am4SendDisplayAck("", false, "Invalid JSON");
    return;
  }

  const int protocolVersion = document["v"] | 0;
  const char* type = document["type"] | "";
  const char* token = document["token"] | "";

  if (protocolVersion != 1) {
    am4SendDisplayAck("", false, "Unsupported protocol version");
    return;
  }

  if (strcmp(type, "hello") == 0) {
    if (strcmp(token, AM4_DISPLAY_LINK_TOKEN) != 0) {
      am4SendDisplayAck("hello", false, "Authentication failed");
      am4CloseDisplayClient("authentication failed");
      return;
    }

    am4DisplayAuthenticated = true;
    am4DisplayLastRxMs = millis();
    am4SendDisplayAck("hello", true, "Display authenticated");
    Serial.println("[DISPLAY LINK] Authenticated successfully");

    // Do not wait for the periodic timer; send the first state immediately.
    am4DisplayLastStateMs = millis();
    am4SendHardwareState();
    return;
  }

  if (!am4DisplayAuthenticated) {
    am4SendDisplayAck("", false, "Display is not authenticated");
    return;
  }

  if (strcmp(token, AM4_DISPLAY_LINK_TOKEN) != 0) {
    am4SendDisplayAck("", false, "Authentication failed");
    am4CloseDisplayClient("invalid link token");
    return;
  }

  am4DisplayLastRxMs = millis();

  if (strcmp(type, "heartbeat") == 0) {
    return;
  }

  if (strcmp(type, "command") == 0) {
    am4HandleDisplayCommand(document);
  }
}

void am4ReadDisplayClient() {
  if (!am4DisplayClient || !am4DisplayClient.connected()) {
    return;
  }

  size_t bytesProcessed = 0;
  constexpr size_t MAX_BYTES_PER_LOOP = 512;

  while (am4DisplayClient.available() && bytesProcessed < MAX_BYTES_PER_LOOP) {
    const int byteValue = am4DisplayClient.read();
    if (byteValue < 0) {
      break;
    }
    bytesProcessed++;

    const char character = static_cast<char>(byteValue);
    if (character == '\r') {
      continue;
    }

    if (character == '\n') {
      if (am4DisplayRxLength > 0) {
        am4DisplayRxBuffer[am4DisplayRxLength] = '\0';
        am4HandleDisplayMessage(am4DisplayRxBuffer, am4DisplayRxLength);
        am4DisplayRxLength = 0;
      }
      continue;
    }

    if (am4DisplayRxLength >= sizeof(am4DisplayRxBuffer) - 1) {
      am4DisplayRxLength = 0;
      am4SendDisplayAck("", false, "RX message too large");
      continue;
    }

    am4DisplayRxBuffer[am4DisplayRxLength++] = character;
  }
}

void am4SendHardwareState() {
  if (!am4DisplayAuthenticated || !am4DisplayClient.connected()) {
    return;
  }

  StaticJsonDocument<1024> state;
  state["v"] = 1;
  state["type"] = "state";
  state["seq"] = ++am4DisplayStateSequence;
  state["device"] = devicename;
  state["uptimeMs"] = millis();

  state["returnC"] = ReturnTempC;
  state["suctionC"] = SuctionTempC;
  state["dischargeC"] = dischargeTempC;
  state["supplyC"] = SupplyTempC;
  state["oilC"] = OilTempC;
  state["otherC"] = OtherTempC;

  state["suctionPsi"] = suctionPressure;
  state["dischargePsi"] = dischargePressure;
  state["suctionBar"] = suctionPressureB;
  state["dischargeBar"] = dischargePressureB;

  state["Comp_status"] = comp_Status;
  state["runOutput"] = digitalRead(RUN);
  state["alarmOutput"] = digitalRead(ALARM);
  state["hps"] = HPS;
  state["lps"] = LPS;
  state["ops"] = OPS;
  state["machineShutdown"] = Machine_Shutdown ? 1 : 0;
  state["mqttConnected"] = client.connected() ? 1 : 0;
  state["internetConnected"] = WiFi.status() == WL_CONNECTED ? 1 : 0;

  const time_t utcNow = time(nullptr);
  if (utcNow > 1700000000) {
    state["epochUtc"] = static_cast<uint32_t>(utcNow);
  }

  if (!am4SendDisplayJson(state)) {
    am4CloseDisplayClient("state send failed");
  }
}

void initDisplayHostLink() {
  if (!ensureHardwareHostAccessPoint()) {
    Serial.println("[DISPLAY LINK] Cannot start TCP server because AP failed");
    return;
  }

  am4DisplayServer.begin();
  am4DisplayServer.setNoDelay(true);

  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.nist.gov");

  Serial.println("[DISPLAY LINK] TCP SERVER STARTED");
  Serial.print("[DISPLAY LINK] Address: ");
  Serial.print(WiFi.softAPIP());
  Serial.print(':');
  Serial.println(AM4_DISPLAY_TCP_PORT);
  Serial.println("[DISPLAY LINK] Waiting for display ESP32...");
}

void restartDisplayTcpServer() {
  am4CloseDisplayClient("AP/server restart");
  am4DisplayServer.stop();
  delay(10);
  am4DisplayServer.begin();
  am4DisplayServer.setNoDelay(true);
  Serial.println("[DISPLAY LINK] TCP server rebound after AP recovery");
}

void serviceDisplayHostLink() {
  if (serviceHardwareHostAccessPoint()) {
    restartDisplayTcpServer();
  }

  WiFiClient candidate = am4DisplayServer.available();
  if (candidate) {
    if (am4DisplayClient && am4DisplayClient.connected()) {
      am4CloseDisplayClient("new display connection");
    }

    am4DisplayClient = candidate;
    am4DisplayClient.setNoDelay(true);
    am4DisplayClient.setTimeout(2);
    am4DisplayAuthenticated = false;
    am4DisplayRxLength = 0;
    am4DisplayLastRxMs = millis();
    Serial.print("[DISPLAY LINK] TCP client connected from ");
    Serial.println(am4DisplayClient.remoteIP());
  }

  if (am4DisplayClient && !am4DisplayClient.connected()) {
    am4CloseDisplayClient("TCP connection lost");
    return;
  }

  am4ReadDisplayClient();

  const unsigned long now = millis();
  if (am4DisplayClient &&
      now - am4DisplayLastRxMs > AM4_DISPLAY_CLIENT_TIMEOUT_MS) {
    am4CloseDisplayClient(
      am4DisplayAuthenticated ? "heartbeat timeout" : "authentication timeout"
    );
    return;
  }

  if (am4DisplayAuthenticated &&
      now - am4DisplayLastStateMs >= AM4_DISPLAY_STATE_INTERVAL_MS) {
    am4DisplayLastStateMs = now;
    am4SendHardwareState();
  }

#if AM4_LINK_DEBUG
  if (now - am4DisplayLastStatusMs >= AM4_HOST_LINK_STATUS_INTERVAL_MS) {
    am4DisplayLastStatusMs = now;
    Serial.print("[DISPLAY LINK] AP clients=");
    Serial.print(WiFi.softAPgetStationNum());
    Serial.print(" TCP=");
    Serial.print(am4DisplayClient && am4DisplayClient.connected() ? "CONNECTED" : "WAITING");
    Serial.print(" AUTH=");
    Serial.print(am4DisplayAuthenticated ? "YES" : "NO");
    Serial.print(" TX=");
    Serial.println(am4DisplayTxCount);
  }
#endif
}
