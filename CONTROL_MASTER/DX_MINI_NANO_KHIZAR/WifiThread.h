
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
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    }
    is_wifi_connected = true;
  }
}

//-------THIS CODE IS FOR CREATING API----//
void server_setup() {

  // server.on("/wifi-config", HTTP_POST, [](AsyncWebServerRequest* request) {
  //   if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
  //     Serial.println("WiFi Config From App");
  //     ssid = request->getParam("ssid", true)->value();
  //     password = request->getParam("password", true)->value();

  //     saveCredentials(ssid, password);
  //     request->send(200, "text/plain", "Wi-Fi credentials received and saved. Restarting...");

  //     // Delay and restart
  //     delay(1000);
  //     ESP.restart();

  //   } else {
  //     request->send(400, "text/plain", "Please send both SSID and password.");
  //   }
  // });

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

      if (is_wifi_connected) {
        // WiFi.softAPdisconnect(true);
        // Serial.println("Wifi Soft AP Disconnect");
      } else {
        wifi_ap_mode = true;
        wifi_setting_time = millis();
      }
  });

  server.onNotFound(notFound);
  Serial.printf("\n Heap before server begin: %d\n", ESP.getFreeHeap());
  server.begin();
  Serial.println("Success!");
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
  while (WiFi.status() != WL_CONNECTED && retries < 100) {
    delay(50);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    myIP = WiFi.localIP().toString();
    Serial.print("\nConnected with IP: ");
    Serial.println(myIP);

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    }
    wifi_channel = WiFi.channel();
    WiFi.softAP(devicename.c_str(), "bitahomes", wifi_channel, 1);
  } else {
    Serial.println("\nFailed to Connect, starting AP mode.");
    startAccessPoint();
  }
}