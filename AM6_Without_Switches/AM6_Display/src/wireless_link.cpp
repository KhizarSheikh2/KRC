#include "wireless_link.h"

#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include <cmath>

#include "config.h"
#include "display_state.h"

namespace {
AsyncWebServer displayServer(80);

struct PendingCommand {
    bool hasPower = false;
    int power = 0;
    bool hasMode = false;
    uint8_t mode = AM6_MODE_COOL;
    bool hasFan = false;
    uint8_t fan = AM6_FAN_LOW;
};

PendingCommand pending;
bool commandPending = false;
bool awaitingConfirmation = false;
uint32_t commandSentAtMs = 0;
uint32_t nextCommandAttemptMs = 0;

bool serverStarted = false;
bool wifiWasConnected = false;
bool wifiConnectInProgress = false;
uint32_t wifiConnectStartedMs = 0;
uint32_t nextWifiAttemptMs = 0;
uint32_t lastIndoorStateMs = 0;
bool haveIndoorState = false;

constexpr size_t kMaxBodyBytes = 768;

IPAddress displayLocalIp() {
    return IPAddress(AM6_DISPLAY_IP_0, AM6_DISPLAY_IP_1,
                     AM6_DISPLAY_IP_2, AM6_DISPLAY_IP_3);
}

IPAddress indoorIp() {
    return IPAddress(AM6_INDOOR_IP_0, AM6_INDOOR_IP_1,
                     AM6_INDOOR_IP_2, AM6_INDOOR_IP_3);
}

IPAddress subnet() {
    return IPAddress(255, 255, 255, 0);
}

const char* wifiStatusName(wl_status_t status) {
    switch (status) {
        case WL_IDLE_STATUS: return "IDLE";
        case WL_NO_SSID_AVAIL: return "SSID_NOT_FOUND";
        case WL_SCAN_COMPLETED: return "SCAN_COMPLETED";
        case WL_CONNECTED: return "CONNECTED";
        case WL_CONNECT_FAILED: return "CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "CONNECTION_LOST";
        case WL_DISCONNECTED: return "DISCONNECTED";
        default: return "UNKNOWN";
    }
}

bool parseZeroOne(JsonVariantConst value, int& out) {
    if (value.is<bool>()) {
        out = value.as<bool>() ? 1 : 0;
        return true;
    }
    if (value.is<int>()) {
        const int v = value.as<int>();
        if (v == 0 || v == 1) {
            out = v;
            return true;
        }
    }
    return false;
}

bool parseMode(JsonVariantConst value, uint8_t& out) {
    if (!value.is<int>()) return false;
    const int v = value.as<int>();
    if (v != AM6_MODE_COOL && v != AM6_MODE_HEAT) return false;
    out = static_cast<uint8_t>(v);
    return true;
}

bool parseFanSw(JsonVariantConst value, uint8_t& out) {
    if (!value.is<int>()) return false;
    const int appValue = value.as<int>();
    if (appValue < 0 || appValue > 2) return false;
    out = static_cast<uint8_t>(appValue + 1); // 0..2 -> internal 1..3
    return true;
}

void addCors(AsyncWebServerResponse* response) {
    if (!response) return;
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

bool processIndoorState(const String& body) {
    StaticJsonDocument<768> doc;
    const DeserializationError error = deserializeJson(doc, body);
    if (error) {
        Serial.print("[HTTP RX] Invalid Indoor JSON: ");
        Serial.println(error.c_str());
        return false;
    }

    bool accepted = false;

    if (doc.containsKey("powersw")) {
        int power = 0;
        if (!parseZeroOne(doc["powersw"], power)) return false;
        displayStateSetPower(power);
        accepted = true;
    }

    if (doc.containsKey("mode")) {
        uint8_t mode = AM6_MODE_COOL;
        if (!parseMode(doc["mode"], mode)) return false;
        displayStateSetMode(mode);
        accepted = true;
    }

    if (doc.containsKey("fanSw")) {
        uint8_t fan = AM6_FAN_LOW;
        if (!parseFanSw(doc["fanSw"], fan)) return false;
        displayStateSetFanSpeed(fan);
        accepted = true;
    }

    if (!accepted) return false;

    lastIndoorStateMs = millis();
    haveIndoorState = true;

    // Indoor sends an authoritative state immediately after applying a display
    // command. Any valid state received after our command POST is therefore the
    // confirmation point.
    if (awaitingConfirmation &&
        static_cast<int32_t>(lastIndoorStateMs - commandSentAtMs) >= 0) {
        awaitingConfirmation = false;
    }

    Serial.print("[HTTP RX] Indoor state: ");
    Serial.println(body);
    return true;
}

void setupServer() {
    if (serverStarted) return;

    displayServer.on(AM6_DISPLAY_RECEIVE_PATH, HTTP_OPTIONS,
                     [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse(200);
        addCors(response);
        request->send(response);
    });

    displayServer.on(
        AM6_DISPLAY_RECEIVE_PATH,
        HTTP_POST,
        [](AsyncWebServerRequest* request) {
            String* body = static_cast<String*>(request->_tempObject);
            request->_tempObject = nullptr;

            if (body == nullptr) {
                request->send(400, "application/json",
                              "{\"status\":\"error\",\"message\":\"No body\"}");
                return;
            }

            const bool ok = processIndoorState(*body);
            delete body;

            AsyncWebServerResponse* response = request->beginResponse(
                ok ? 200 : 400,
                "application/json",
                ok ? "{\"status\":\"ok\"}"
                   : "{\"status\":\"error\",\"message\":\"Invalid state\"}");
            addCors(response);
            request->send(response);
        },
        nullptr,
        [](AsyncWebServerRequest* request,
           uint8_t* data,
           size_t len,
           size_t index,
           size_t total) {
            if (total == 0 || total > kMaxBodyBytes ||
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

    displayServer.on("/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        StaticJsonDocument<384> doc;
        doc["device"] = "AM6 Display";
        doc["wifi"] = WiFi.status() == WL_CONNECTED ? 1 : 0;
        doc["indoor_link"] = wirelessLinkUiState() == AM6_LINK_ONLINE ? 1 : 0;
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -127;

        String body;
        serializeJson(doc, body);
        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", body);
        addCors(response);
        request->send(response);
    });

    displayServer.begin();
    serverStarted = true;
    Serial.println("[HTTP SERVER] Display /receive endpoint ready");
}

void startWifiConnection(uint32_t nowMs) {
    if (WiFi.status() == WL_CONNECTED) return;

    Serial.print("[WIFI] Connecting to Indoor AP: ");
    Serial.println(AM6_INDOOR_AP_SSID);

    WiFi.disconnect(false, false);
    delay(20);
    WiFi.begin(AM6_INDOOR_AP_SSID, AM6_INDOOR_AP_PASSWORD);
    wifiConnectInProgress = true;
    wifiConnectStartedMs = nowMs;
}

void serviceWifi(uint32_t nowMs) {
    const wl_status_t status = WiFi.status();

    if (status == WL_CONNECTED) {
        wifiConnectInProgress = false;
        if (!wifiWasConnected) {
            wifiWasConnected = true;
            Serial.print("[WIFI] CONNECTED | IP=");
            Serial.print(WiFi.localIP());
            Serial.print(" | RSSI=");
            Serial.println(WiFi.RSSI());
            setupServer();
        }
        return;
    }

    if (wifiWasConnected) {
        wifiWasConnected = false;
        haveIndoorState = false;
        awaitingConfirmation = false;
        Serial.print("[WIFI] LOST | status=");
        Serial.println(wifiStatusName(status));
        nextWifiAttemptMs = nowMs + 500UL;
    }

    if (wifiConnectInProgress) {
        if (nowMs - wifiConnectStartedMs >= AM6_WIFI_CONNECT_TIMEOUT_MS) {
            Serial.print("[WIFI] TIMEOUT | status=");
            Serial.println(wifiStatusName(status));
            WiFi.disconnect(false, false);
            wifiConnectInProgress = false;
            nextWifiAttemptMs = nowMs + AM6_WIFI_RETRY_MS;
        }
        return;
    }

    if (static_cast<int32_t>(nowMs - nextWifiAttemptMs) >= 0) {
        startWifiConnection(nowMs);
    }
}

bool postPendingCommand(uint32_t nowMs) {
    if (!commandPending || WiFi.status() != WL_CONNECTED) return false;

    StaticJsonDocument<256> doc;
    if (pending.hasPower) doc["powersw"] = pending.power;
    if (pending.hasMode) doc["mode"] = pending.mode;
    if (pending.hasFan) doc["fanSw"] = static_cast<int>(pending.fan) - 1;

    String body;
    serializeJson(doc, body);

    WiFiClient client;
    HTTPClient http;
    const String url = String("http://") + indoorIp().toString() + AM6_INDOOR_COMMAND_PATH;

    if (!http.begin(client, url)) {
        Serial.println("[HTTP TX] http.begin failed");
        nextCommandAttemptMs = nowMs + AM6_HTTP_RETRY_MS;
        return false;
    }

    http.setReuse(false);
    http.setConnectTimeout(AM6_HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(AM6_HTTP_RESPONSE_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/json");

    const int code = http.POST(body);
    http.end();

    if (code < 200 || code >= 300) {
        Serial.print("[HTTP TX] Display command FAILED code=");
        Serial.print(code);
        Serial.print(" JSON=");
        Serial.println(body);
        nextCommandAttemptMs = nowMs + AM6_HTTP_RETRY_MS;
        return false;
    }

    Serial.print("[HTTP TX] Display command SUCCESS JSON=");
    Serial.println(body);

    pending = PendingCommand{};
    commandPending = false;
    awaitingConfirmation = true;
    commandSentAtMs = nowMs;
    nextCommandAttemptMs = nowMs;
    return true;
}

bool linkFresh(uint32_t nowMs) {
    return haveIndoorState &&
           WiFi.status() == WL_CONNECTED &&
           (nowMs - lastIndoorStateMs) <= AM6_INDOOR_STATE_TIMEOUT_MS;
}

void queuePower(int power) {
    pending.hasPower = true;
    pending.power = (power == 1) ? 1 : 0;
    commandPending = true;
    nextCommandAttemptMs = 0;
}

void queueMode(uint8_t mode) {
    pending.hasMode = true;
    pending.mode = mode;
    commandPending = true;
    nextCommandAttemptMs = 0;
}

void queueFan(uint8_t fan) {
    pending.hasFan = true;
    pending.fan = fan;
    commandPending = true;
    nextCommandAttemptMs = 0;
}
} // namespace

void wirelessLinkBegin() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.disconnect(false, false);
    delay(30);

    const bool configured = WiFi.config(displayLocalIp(), indoorIp(), subnet());
    Serial.print("[WIFI] Static Display IP ");
    Serial.print(displayLocalIp());
    Serial.println(configured ? " configured" : " configuration FAILED");

    nextWifiAttemptMs = millis();
    nextCommandAttemptMs = 0;
    // IMPORTANT: start AsyncWebServer only after WL_CONNECTED. This mirrors
    // the proven AP-TH pattern and prevents a server object initialized before
    // the STA interface is ready from becoming unreachable at 192.168.4.50.
    startWifiConnection(millis());
}

void wirelessLinkLoop() {
    const uint32_t nowMs = millis();
    serviceWifi(nowMs);

    if (haveIndoorState &&
        nowMs - lastIndoorStateMs > AM6_INDOOR_STATE_TIMEOUT_MS) {
        haveIndoorState = false;
        awaitingConfirmation = false;
        commandPending = false;
        pending = PendingCommand{};
    }

    if (commandPending && WiFi.status() == WL_CONNECTED &&
        static_cast<int32_t>(nowMs - nextCommandAttemptMs) >= 0) {
        postPendingCommand(nowMs);
    }
}

AM6WirelessUiState wirelessLinkUiState() {
    const uint32_t nowMs = millis();
    if (WiFi.status() != WL_CONNECTED) return AM6_LINK_OFFLINE;
    if (!linkFresh(nowMs)) return AM6_LINK_WAITING;
    if (commandPending || awaitingConfirmation) return AM6_LINK_SYNCING;
    return AM6_LINK_ONLINE;
}

bool wirelessLinkCanControl() {
    // Allow a newer user selection while a previous command is still syncing.
    // The PendingCommand structure safely coalesces the latest requested state.
    return WiFi.status() == WL_CONNECTED && linkFresh(millis());
}

bool wirelessLinkRequestPower(int power) {
    if (!wirelessLinkCanControl()) return false;
    const int normalized = power == 1 ? 1 : 0;
    displayStateSetPower(normalized); // optimistic UI; Indoor confirms it
    queuePower(normalized);
    return true;
}

bool wirelessLinkRequestMode(uint8_t mode) {
    if (!wirelessLinkCanControl()) return false;
    if (mode != AM6_MODE_COOL && mode != AM6_MODE_HEAT) return false;
    displayStateSetMode(mode);
    queueMode(mode);
    return true;
}

bool wirelessLinkRequestFan(uint8_t fanSpeed) {
    if (!wirelessLinkCanControl()) return false;
    if (fanSpeed < AM6_FAN_LOW || fanSpeed > AM6_FAN_HIGH) return false;
    displayStateSetFanSpeed(fanSpeed);
    queueFan(fanSpeed);
    return true;
}
