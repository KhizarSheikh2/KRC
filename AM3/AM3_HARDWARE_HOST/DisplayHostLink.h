#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <time.h>
#include "DisplayLinkConfig.h"

WiFiServer am3DisplayServer(AM3_DISPLAY_TCP_PORT, 1);
WiFiClient am3DisplayClient;

char am3DisplayRxBuffer[AM3_DISPLAY_RX_BUFFER_SIZE];
size_t am3DisplayRxLength = 0;
bool am3DisplayAuthenticated = false;
uint32_t am3DisplayStateSequence = 0;
unsigned long am3DisplayLastStateMs = 0;
unsigned long am3DisplayLastRxMs = 0;
unsigned long am3DisplayLastStatusMs = 0;
unsigned long am3DisplayTxCount = 0;

void am3SendHardwareState();

void am3CloseDisplayClient(const char* reason) {
  if (am3DisplayClient) {
    am3DisplayClient.stop();
  }
  am3DisplayAuthenticated = false;
  am3DisplayRxLength = 0;
  if (reason != nullptr) {
    Serial.print("[DISPLAY LINK] Disconnected: ");
    Serial.println(reason);
  }
}

bool am3SendDisplayJson(const JsonDocument& document) {
  if (!am3DisplayClient || !am3DisplayClient.connected()) {
#if AM3_LINK_DEBUG
    Serial.println("[DISPLAY TX] No connected TCP client");
#endif
    return false;
  }

  char payload[AM3_DISPLAY_TX_BUFFER_SIZE];
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

  const size_t payloadWritten = am3DisplayClient.write(
    reinterpret_cast<const uint8_t*>(payload), length
  );
  const size_t newlineWritten = am3DisplayClient.write(static_cast<uint8_t>('\n'));

  const bool sent = payloadWritten == length && newlineWritten == 1;
  if (!sent) {
    Serial.print("[DISPLAY TX] Socket write failed. expected=");
    Serial.print(length);
    Serial.print(" actual=");
    Serial.println(payloadWritten);
    return false;
  }

  am3DisplayTxCount++;
#if AM3_LINK_DEBUG
  // Print every state during commissioning so JSON transmission is visible.
  Serial.print("[DISPLAY TX #");
  Serial.print(am3DisplayTxCount);
  Serial.print("] ");
  Serial.println(payload);
#endif
  return true;
}

void am3SendDisplayAck(const char* command, bool accepted, const char* message) {
  StaticJsonDocument<256> response;
  response["v"] = 1;
  response["type"] = "ack";
  response["command"] = command != nullptr ? command : "";
  response["accepted"] = accepted;
  response["message"] = message != nullptr ? message : "";
  am3SendDisplayJson(response);
}

void am3HandleDisplayCommand(const JsonDocument& document) {
  const char* command = document["command"] | "";

  if (strcmp(command, "start") == 0) {
    StartSW_App = 1;
    startSW = true;
    am3SendDisplayAck(command, true, "Start request received by hardware");
    return;
  }

  if (strcmp(command, "stop") == 0) {
    StartSW_App = 0;
    stopSW = true;
    am3SendDisplayAck(command, true, "Stop request received by hardware");
    return;
  }

  if (strcmp(command, "reset") == 0) {
    resetSW = 1;
    am3SendDisplayAck(command, true, "Reset request received by hardware");
    return;
  }

  am3SendDisplayAck(command, false, "Unsupported display command");
}

void am3HandleDisplayMessage(const char* payload, size_t length) {
#if AM3_LINK_DEBUG
  Serial.print("[DISPLAY RX] ");
  Serial.write(reinterpret_cast<const uint8_t*>(payload), length);
  Serial.println();
#endif

  StaticJsonDocument<512> document;
  const DeserializationError error = deserializeJson(document, payload, length);
  if (error) {
    Serial.print("[DISPLAY RX] JSON error: ");
    Serial.println(error.c_str());
    am3SendDisplayAck("", false, "Invalid JSON");
    return;
  }

  const int protocolVersion = document["v"] | 0;
  const char* type = document["type"] | "";
  const char* token = document["token"] | "";

  if (protocolVersion != 1) {
    am3SendDisplayAck("", false, "Unsupported protocol version");
    return;
  }

  if (strcmp(type, "hello") == 0) {
    if (strcmp(token, AM3_DISPLAY_LINK_TOKEN) != 0) {
      am3SendDisplayAck("hello", false, "Authentication failed");
      am3CloseDisplayClient("authentication failed");
      return;
    }

    am3DisplayAuthenticated = true;
    am3DisplayLastRxMs = millis();
    am3SendDisplayAck("hello", true, "Display authenticated");
    Serial.println("[DISPLAY LINK] Authenticated successfully");

    // Do not wait for the periodic timer; send the first state immediately.
    am3DisplayLastStateMs = millis();
    am3SendHardwareState();
    return;
  }

  if (!am3DisplayAuthenticated) {
    am3SendDisplayAck("", false, "Display is not authenticated");
    return;
  }

  if (strcmp(token, AM3_DISPLAY_LINK_TOKEN) != 0) {
    am3SendDisplayAck("", false, "Authentication failed");
    am3CloseDisplayClient("invalid link token");
    return;
  }

  am3DisplayLastRxMs = millis();

  if (strcmp(type, "heartbeat") == 0) {
    return;
  }

  if (strcmp(type, "command") == 0) {
    am3HandleDisplayCommand(document);
  }
}

void am3ReadDisplayClient() {
  if (!am3DisplayClient || !am3DisplayClient.connected()) {
    return;
  }

  size_t bytesProcessed = 0;
  constexpr size_t MAX_BYTES_PER_LOOP = 512;

  while (am3DisplayClient.available() && bytesProcessed < MAX_BYTES_PER_LOOP) {
    const int byteValue = am3DisplayClient.read();
    if (byteValue < 0) {
      break;
    }
    bytesProcessed++;

    const char character = static_cast<char>(byteValue);
    if (character == '\r') {
      continue;
    }

    if (character == '\n') {
      if (am3DisplayRxLength > 0) {
        am3DisplayRxBuffer[am3DisplayRxLength] = '\0';
        am3HandleDisplayMessage(am3DisplayRxBuffer, am3DisplayRxLength);
        am3DisplayRxLength = 0;
      }
      continue;
    }

    if (am3DisplayRxLength >= sizeof(am3DisplayRxBuffer) - 1) {
      am3DisplayRxLength = 0;
      am3SendDisplayAck("", false, "RX message too large");
      continue;
    }

    am3DisplayRxBuffer[am3DisplayRxLength++] = character;
  }
}

void am3SendHardwareState() {
  if (!am3DisplayAuthenticated || !am3DisplayClient.connected()) {
    return;
  }

  StaticJsonDocument<1024> state;
  state["v"] = 1;
  state["type"] = "state";
  state["seq"] = ++am3DisplayStateSequence;
  state["device"] = devicename;
  state["uptimeMs"] = millis();

  state["returnC"] = ReturnTempC;
  state["suctionC"] = SuctionTempC;
  state["dischargeC"] = dischargeTempC;
  state["supplyC"] = SupplyTempC;
  state["oilC"] = OilTempC;

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

  if (!am3SendDisplayJson(state)) {
    am3CloseDisplayClient("state send failed");
  }
}

void initDisplayHostLink() {
  if (!ensureHardwareHostAccessPoint()) {
    Serial.println("[DISPLAY LINK] Cannot start TCP server because AP failed");
    return;
  }

  am3DisplayServer.begin();
  am3DisplayServer.setNoDelay(true);

  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.nist.gov");

  Serial.println("[DISPLAY LINK] TCP SERVER STARTED");
  Serial.print("[DISPLAY LINK] Address: ");
  Serial.print(WiFi.softAPIP());
  Serial.print(':');
  Serial.println(AM3_DISPLAY_TCP_PORT);
  Serial.println("[DISPLAY LINK] Waiting for display ESP32...");
}

void restartDisplayTcpServer() {
  am3CloseDisplayClient("AP/server restart");
  am3DisplayServer.stop();
  delay(10);
  am3DisplayServer.begin();
  am3DisplayServer.setNoDelay(true);
  Serial.println("[DISPLAY LINK] TCP server rebound after AP recovery");
}

void serviceDisplayHostLink() {
  if (serviceHardwareHostAccessPoint()) {
    restartDisplayTcpServer();
  }

  WiFiClient candidate = am3DisplayServer.available();
  if (candidate) {
    if (am3DisplayClient && am3DisplayClient.connected()) {
      am3CloseDisplayClient("new display connection");
    }

    am3DisplayClient = candidate;
    am3DisplayClient.setNoDelay(true);
    am3DisplayClient.setTimeout(2);
    am3DisplayAuthenticated = false;
    am3DisplayRxLength = 0;
    am3DisplayLastRxMs = millis();
    Serial.print("[DISPLAY LINK] TCP client connected from ");
    Serial.println(am3DisplayClient.remoteIP());
  }

  if (am3DisplayClient && !am3DisplayClient.connected()) {
    am3CloseDisplayClient("TCP connection lost");
    return;
  }

  am3ReadDisplayClient();

  const unsigned long now = millis();
  if (am3DisplayClient &&
      now - am3DisplayLastRxMs > AM3_DISPLAY_CLIENT_TIMEOUT_MS) {
    am3CloseDisplayClient(
      am3DisplayAuthenticated ? "heartbeat timeout" : "authentication timeout"
    );
    return;
  }

  if (am3DisplayAuthenticated &&
      now - am3DisplayLastStateMs >= AM3_DISPLAY_STATE_INTERVAL_MS) {
    am3DisplayLastStateMs = now;
    am3SendHardwareState();
  }

#if AM3_LINK_DEBUG
  if (now - am3DisplayLastStatusMs >= AM3_HOST_LINK_STATUS_INTERVAL_MS) {
    am3DisplayLastStatusMs = now;
    Serial.print("[DISPLAY LINK] AP clients=");
    Serial.print(WiFi.softAPgetStationNum());
    Serial.print(" TCP=");
    Serial.print(am3DisplayClient && am3DisplayClient.connected() ? "CONNECTED" : "WAITING");
    Serial.print(" AUTH=");
    Serial.print(am3DisplayAuthenticated ? "YES" : "NO");
    Serial.print(" TX=");
    Serial.println(am3DisplayTxCount);
  }
#endif
}
