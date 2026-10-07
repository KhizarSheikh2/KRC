#include "wireless_link.h"

#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include "config.h"
#include "display_state.h"

namespace {
AsyncWebServer displayServer(80);

bool serverStarted = false;
bool wifiWasConnected = false;
uint32_t lastWifiLossMs = 0;
bool wifiConnectInProgress = false;
uint32_t wifiConnectStartedMs = 0;
uint32_t nextWifiAttemptMs = 0;
uint32_t lastIndoorStateMs = 0;
bool haveIndoorState = false;

constexpr size_t kMaxBodyBytes = 1536;

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
    out = static_cast<uint8_t>(appValue + 1);
    return true;
}

bool parseStatus(JsonVariantConst value, uint8_t& out) {
    if (!value.is<int>()) return false;
    const int v = value.as<int>();
    if (v < AM6_OUT_STOPPED || v > AM6_OUT_OVERLOAD) return false;
    out = static_cast<uint8_t>(v);
    return true;
}

bool parseFloatNumber(JsonVariantConst value, float& out) {
    if (!(value.is<float>() || value.is<double>() || value.is<int>() || value.is<long>())) return false;
    out = value.as<float>();
    return true;
}

void addCors(AsyncWebServerResponse* response) {
    if (!response) return;
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

bool processIndoorState(const String& body) {
    StaticJsonDocument<1536> doc;
    const DeserializationError error = deserializeJson(doc, body);
    if (error) {
        Serial.print("[HTTP RX] Invalid Indoor JSON: ");
        Serial.println(error.c_str());
        return false;
    }

    bool accepted = false;

    if (doc.containsKey("powersw")) {
        int value = 0;
        if (!parseZeroOne(doc["powersw"], value)) return false;
        displayStateSetPower(value);
        accepted = true;
    }

    if (doc.containsKey("mode")) {
        uint8_t value = AM6_MODE_COOL;
        if (!parseMode(doc["mode"], value)) return false;
        displayStateSetMode(value);
        accepted = true;
    }

    if (doc.containsKey("fanSw")) {
        uint8_t value = AM6_FAN_LOW;
        if (!parseFanSw(doc["fanSw"], value)) return false;
        displayStateSetFanSpeed(value);
        accepted = true;
    }

    if (doc.containsKey("statusout")) {
        uint8_t value = AM6_OUT_STOPPED;
        if (!parseStatus(doc["statusout"], value)) return false;
        displayStateSetStatusA(value);
        accepted = true;
    }

    if (doc.containsKey("statusoutB")) {
        uint8_t value = AM6_OUT_STOPPED;
        if (!parseStatus(doc["statusoutB"], value)) return false;
        displayStateSetStatusB(value);
        accepted = true;
    }

    if (doc.containsKey("outdoor_online")) {
        int value = 0;
        if (!parseZeroOne(doc["outdoor_online"], value)) return false;
        displayStateSetOutdoorOnline(value == 1);
        accepted = true;
    }

    if (doc.containsKey("outdoor_io_ok")) {
        int value = 0;
        if (!parseZeroOne(doc["outdoor_io_ok"], value)) return false;
        displayStateSetOutdoorIoOk(value == 1);
        accepted = true;
    }

    if (doc.containsKey("indoorsw")) {
        int value = 0;
        if (!parseZeroOne(doc["indoorsw"], value)) return false;
        displayStateSetIndoorEnabled(value == 1);
        accepted = true;
    }

    if (doc.containsKey("outdoorsw")) {
        int value = 0;
        if (!parseZeroOne(doc["outdoorsw"], value)) return false;
        displayStateSetOutdoorEnabled(value == 1);
        accepted = true;
    }

    static const char* switchKeys[8] = {
        "outhipsiA", "outlopsiA", "outoverloadA", "outpowerA",
        "outhipsiB", "outlopsiB", "outoverloadB", "outpowerB"
    };
    for (uint8_t i = 0; i < 8; ++i) {
        if (!doc.containsKey(switchKeys[i])) continue;
        int value = 0;
        if (!parseZeroOne(doc[switchKeys[i]], value)) return false;
        displayStateSetOutdoorSwitch(i, value == 1);
        accepted = true;
    }

    static const char* relayKeys[4] = {"outR1", "outR2", "outR3", "outR4"};
    for (uint8_t i = 0; i < 4; ++i) {
        if (!doc.containsKey(relayKeys[i])) continue;
        int value = 0;
        if (!parseZeroOne(doc[relayKeys[i]], value)) return false;
        displayStateSetOutdoorRelay(i, value == 1);
        accepted = true;
    }

    float floatValue = 0.0f;
    if (doc.containsKey("supply")) {
        if (!parseFloatNumber(doc["supply"], floatValue)) return false;
        displayStateSetSupplyTemp(floatValue);
        accepted = true;
    }
    if (doc.containsKey("return")) {
        if (!parseFloatNumber(doc["return"], floatValue)) return false;
        displayStateSetReturnTemp(floatValue);
        accepted = true;
    }
    if (doc.containsKey("setPoint")) {
        if (!parseFloatNumber(doc["setPoint"], floatValue)) return false;
        displayStateSetSetPoint(floatValue);
        accepted = true;
    }

    if (!accepted) return false;

    lastIndoorStateMs = millis();
    haveIndoorState = true;
    Serial.print("[HTTP RX] Indoor monitoring state: ");
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
        doc["monitor_only"] = 1;
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
    Serial.println("[HTTP SERVER] Display monitoring /receive endpoint ready");
}

void startWifiConnection(uint32_t nowMs) {
    if (WiFi.status() == WL_CONNECTED) return;

    Serial.print("[WIFI] Connecting/reconnecting to Indoor AP: ");
    Serial.println(AM6_INDOOR_AP_SSID);

    // Do not call WiFi.disconnect() on every retry. During a one-second AP/STA
    // channel transition that would tear down an already-recovering station
    // and can create a repeating disconnect/reconnect loop.
    if (wifiWasConnected || lastWifiLossMs != 0) {
        WiFi.reconnect();
    } else {
        WiFi.begin(AM6_INDOOR_AP_SSID, AM6_INDOOR_AP_PASSWORD);
    }
    wifiConnectInProgress = true;
    wifiConnectStartedMs = nowMs;
}

void serviceWifi(uint32_t nowMs) {
    const wl_status_t status = WiFi.status();

    if (status == WL_CONNECTED) {
        wifiConnectInProgress = false;
        if (!wifiWasConnected) {
            wifiWasConnected = true;
            lastWifiLossMs = 0;
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
        lastWifiLossMs = nowMs;
        // Keep the last valid state during a short AP/STA channel transition.
        // The UI shows WAIT instead of flashing OFFLINE and discarding data.
        Serial.print("[WIFI] LOST | status=");
        Serial.println(wifiStatusName(status));
        nextWifiAttemptMs = nowMs + 500UL;
    }

    if (wifiConnectInProgress) {
        if (nowMs - wifiConnectStartedMs >= AM6_WIFI_CONNECT_TIMEOUT_MS) {
            Serial.print("[WIFI] TIMEOUT | status=");
            Serial.println(wifiStatusName(status));
            wifiConnectInProgress = false;
            nextWifiAttemptMs = nowMs + AM6_WIFI_RETRY_MS;
        }
        return;
    }

    if (static_cast<int32_t>(nowMs - nextWifiAttemptMs) >= 0) {
        startWifiConnection(nowMs);
    }
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
    startWifiConnection(millis());
}

void wirelessLinkLoop() {
    const uint32_t nowMs = millis();
    serviceWifi(nowMs);

    if (haveIndoorState &&
        nowMs - lastIndoorStateMs > AM6_INDOOR_STATE_TIMEOUT_MS) {
        haveIndoorState = false;
    }
}

AM6WirelessUiState wirelessLinkUiState() {
    const uint32_t nowMs = millis();

    // The UI represents freshness of the Indoor monitoring data, not a
    // millisecond-by-millisecond Wi-Fi association flag. Preserve ONLINE
    // through a short radio/channel transition while the last Indoor state is
    // still fresh; this prevents the 1-second LINK/WAIT/LINK flicker.
    if (haveIndoorState &&
        (nowMs - lastIndoorStateMs) <= AM6_INDOOR_STATE_TIMEOUT_MS) {
        return AM6_LINK_ONLINE;
    }

    if (WiFi.status() != WL_CONNECTED) {
        if (lastWifiLossMs != 0 && nowMs - lastWifiLossMs <= AM6_WIFI_UI_GRACE_MS)
            return AM6_LINK_WAITING;
        return AM6_LINK_OFFLINE;
    }

    return AM6_LINK_WAITING;
}

bool wirelessLinkCanControl() {
    return false;
}

bool wirelessLinkRequestPower(int /*power*/) {
    return false;
}

bool wirelessLinkRequestMode(uint8_t /*mode*/) {
    return false;
}

bool wirelessLinkRequestFan(uint8_t /*fanSpeed*/) {
    return false;
}
