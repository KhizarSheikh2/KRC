// #include "lwip/tcpip.h"


void notFound(AsyncWebServerRequest* request) {
  request->send(404, "text/html; charset=UTF-8", "Not found");
}

void saveCredentials(const String& ssid, const String& password) {
  preferences.begin("wifi-creds", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();
}

void wifi_check(const String& ssid, const String& password) {
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long check_time = millis();

  Serial.println("Network Credentials From App");
  Serial.println("SSID: " + ssid);
  Serial.println("Password: " + password);
  Serial.print("Connecting to WiFi..");

  while (WiFi.status() != WL_CONNECTED) {  // ~5s max
    if (millis() - check_time >= 5000) {
      Serial.println("Failed to connect to WiFi within 5 Seconds.");
      is_wifi_connected = false;
      return;
    }
    delay(10);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nConnected to ");
    Serial.println(ssid);
    // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    // struct tm timeinfo;
    // if (getLocalTime(&timeinfo)) {
    //   Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    // }
    is_wifi_connected = true;
  }
}


void Send_Data_to_BECA() {
  HTTPClient http;
  String serverPath = "http://192.168.4.2:80/receive";

  http.begin(serverPath);
  StaticJsonDocument<512> jsonDoc;

  jsonDoc["setpoint"] = setpointt;
  // jsonDoc["CFM"] = end_value;
  jsonDoc["season"] = beca_mode;
  jsonDoc["powerstate"] = beca_power;

  String jsonData;
  serializeJson(jsonDoc, jsonData);

  Serial.print("TO APTH -> JSON payload: ");
  Serial.println(jsonData);

  if (jsonDoc.overflowed()) {
    Serial.println("ERROR: JSON buffer overflow!");
    http.end();
    return;
  }

  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(jsonData);

  if (httpCode > 0) {
    // Serial.println("Data sent successfully to DWIN");
  } else {
    Serial.print("HTTP error: ");
    Serial.println(httpCode);
  }

  http.end();
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

      // Delay and restart
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
    "/wifi_param_by_app", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<256> doc;
      if (deserializeJson(doc, data, len)) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
      }

      ssid = doc["ssid"] | "";
      password = doc["password"] | "";

      if (ssid.isEmpty() || password.isEmpty()) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"SSID or password missing\"}");
        return;
      }

      Serial.println("Received Wifi Credentials:");
      Serial.println("SSID: " + String(ssid));
      Serial.println("Password: " + String(password));

      wifi_check(ssid, password);

      StaticJsonDocument<128> responseDoc;
      responseDoc["status"] = "success";
      responseDoc["message"] = "WiFi parameters saved. Restarting.";
      responseDoc["wifi_status"] = is_wifi_connected ? 1 : 0;

      String responseBody;
      serializeJson(responseDoc, responseBody);
      request->send(200, "application/json", responseBody);

      saveCredentials(ssid, password);
      delay(500);

      // if (is_wifi_connected) {
      //   WiFi.softAPdisconnect(true);
      //   Serial.println("Wifi Soft AP Disconnect");
      // } else {
      //   wifi_ap_mode = true;
      //   wifi_setting_time = millis();
      // }
    });

  server.on(
    "/from_therm", HTTP_POST,
    [](AsyncWebServerRequest* request) {
      // 1. Check if object exists
      if (!request->_tempObject) {
        request->send(400, "text/plain", "No Body");
        return;
      }

      // 2. Cast the temporary object back to a String pointer
      String* jsonData = (String*)request->_tempObject;

      // 3. Parse JSON directly from the accumulated string object
      StaticJsonDocument<1024> jsonDoc;
      DeserializationError error = deserializeJson(jsonDoc, *jsonData);

      Serial.println("\n ===== JSON RECEIVED FROM APTH =====");
      Serial.println(*jsonData);

      // 4. CRITICAL: Free heap memory immediately to prevent crashes
      delete jsonData;
      request->_tempObject = nullptr;

      // 5. Handle parse errors
      if (error) {
        Serial.print("JSON Parse Failed: ");
        Serial.println(error.c_str());
        request->send(400, "text/plain", "Invalid JSON");
        return;
      }

      // humidity
      // supply_temp

      temp_by_beca = jsonDoc["return_temp"].as<int>();
      setpointt = jsonDoc["setpoint"].as<int>();
      // end_value = jsonDoc["CFM"].as<int>();
      beca_mode = jsonDoc["season"].as<int>();
      beca_power = jsonDoc["powerstate"].as<int>();

      // Serial.print("SP -> ");
      // Serial.println(setpointt);

      lastBecaWifiTime = millis();  // Refresh the timestamp
      beca_status = true;           // Device is actively communicating
      request->send(200, "text/plain", "Data processed");
    },

    // ON BODY RECEIVED (chunked)
    NULL,

    // Chunk handler
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      if (!request->_tempObject) {
        request->_tempObject = new String();
      }
      String* jsonData = (String*)request->_tempObject;
      jsonData->reserve(total);
      jsonData->concat((char*)data, len);
  });

  server.onNotFound(notFound);
  Serial.printf("\n Heap before server begin: %d\n", ESP.getFreeHeap());

  server.begin();
  Serial.println("Server Started!");
}

bool loadCredentials(String& ssid, String& password) {
  preferences.begin("wifi-creds", true);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  preferences.end();
  // ssid = "BITA DEV";
  // password = "xttok2fb";
  return (ssid.length() > 0 && password.length() > 0);
}

void startAccessPoint() {
  Serial.println("Starting in AP mode");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(devicename.c_str(), "bitahomes", 1, 1);  // Set your desired SSID and password

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
}

void connectToWiFi(const String& ssid, const String& password) {
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(hostname.c_str());
  WiFi.setAutoReconnect(true);

  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid.c_str(), password.c_str());

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    myIP = WiFi.localIP().toString();
    Serial.print("\nConnected with IP: ");
    Serial.println(myIP);

    // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    // struct tm timeinfo;
    // if (getLocalTime(&timeinfo)) {
    //   Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    // }

    wifi_channel = WiFi.channel();
    WiFi.softAP(devicename.c_str(), "bitahomes", wifi_channel, 1);
    Serial.print("AP Mode IP : ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("\nFailed to Connect, starting AP mode.");
    startAccessPoint();
  }
}
