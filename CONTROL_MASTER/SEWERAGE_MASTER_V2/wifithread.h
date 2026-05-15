const char *ntpServer = "time.google.com";
const long gmtOffset_sec = 18000;
const int daylightOffset_sec = 0;

Preferences preferences;

WiFiClientSecure espClient;
PubSubClient client(espClient);

AsyncWebServer server(80);
// AsyncWebServer OTAserver(81);

void startAccessPoint();
void server_setup();
void showCenteredText(const char*, uint8_t, int);
void updateDisplay();
void updateControlOutputs();

void DEVICE_INIT() {

  preferences.begin("parameters", false);

  pumpsw = preferences.getUInt("pumpsw", 0);
  main_control = preferences.getBool("main_control", false);
  pump_control = preferences.getBool("pump_control", false);

  preferences.end();

  waterlevelsp = waterlevel;

#ifdef DEBUG
  Serial.print("pumpsw: ");
  Serial.println(pumpsw);

  Serial.print("main_control: ");
  Serial.println(main_control);

  Serial.print("pump_control: ");
  Serial.println(pump_control);
#endif
}

//For MQTT
void Extract_by_json(String incomingMessage) {
  StaticJsonDocument<512> doc;

  DeserializationError error = deserializeJson(doc, incomingMessage);
  if (error) {
  #ifdef DEBUG
      Serial.print("JSON deserialization failed: ");
      Serial.println(error.c_str());
  #endif
    return;
  }

  preferences.begin("parameters", false);
  bool mainControlChanged = false;  // Flag to track if main_control changed
  bool pumpSwChanged = false;       // Flag to track if pumpsw changed

  if (doc.containsKey("maincontrol")) {
    int newMainControl = doc["maincontrol"].as<int>();
    if (newMainControl != main_control) {
      main_control = newMainControl;
      preferences.putBool("main_control", main_control);
      mainControlChanged = true;  // Mark as changed
    }
  }
  if (doc.containsKey("pumpcontrol") && doc["pumpcontrol"].as<int>() != pump_control) {
    pump_control = doc["pumpcontrol"].as<int>();
    preferences.putBool("pump_control", pump_control);
  }
  if (doc.containsKey("pumpsw") && doc["pumpsw"].as<int>() != pumpsw) {
    pumpsw = doc["pumpsw"].as<int>();
    preferences.putUInt("pumpsw", pumpsw);
  }

  preferences.end();

  time3 = millis();

#ifdef DEBUG
  // Serial.println("");
  // Serial.println("---------------------------------------------");
  // Serial.print("maincontrol: ");
  // Serial.println(main_control);

  // Serial.print("pumpcontrol: ");
  // Serial.println(pump_control);

  // Serial.print("pumpsw: ");
  // Serial.println(pumpsw);

  // Serial.print("waterlevel: ");
  // Serial.println(waterlevel);

  // Serial.println("------------------------------------------------");
  // Serial.println("");
#endif

  if (mainControlChanged) {
    if (main_control) {
      showCenteredText("POWER ON", 2, 0);
      delay(800);
      updateDisplay();
    } else {
      // Turning power OFF (fully reset states)
      pumpstate = 0;
      updateControlOutputs();
      showCenteredText("POWER OFF", 2, 0);
    }
  }
  else if (main_control) {
    // Only show pump changes if main control is ON
    if (pumpSwChanged) {
      showCenteredText(pumpstate ? "PUMP ON" : "PUMP OFF", 2, 0);
      updateControlOutputs();
      delay(800);
      updateDisplay();
    }
    else {
      updateDisplay();
    }
  }
}

//For MQTT
void callback(char *topic, byte *message, unsigned int length) {
  String messageTemp;

  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }

  if (String(topic) == device_topic_s) {  // /test/CM1-AAA018/1
// #ifdef DEBUG
//     Serial.println("Message Received on: ");
//     Serial.println(String(topic));
// #endif
    Extract_by_json(messageTemp);
  }
}

void saveCredentials(const char *ssid, const char *password) {
  preferences.begin("wifi-creds", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();
}

bool loadCredentials(String &ssid, String &password) {
  preferences.begin("wifi-creds", true);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  preferences.end();
#ifdef DEBUG
  Serial.print("ssid = ");
  Serial.println(ssid);
  Serial.print("password = ");
  Serial.println(password);
#endif
  return (ssid.length() > 0 && password.length() > 0);
}

//For MQTT
void publishJson() {
  StaticJsonDocument<1024> doc;

  doc["maincontrol"] = String(main_control);
  doc["pumpcontrol"] = String(pump_control);
  doc["pumpsw"] = String(pumpsw);
  doc["pumpstate"] = String(pumpstate);
  doc["waterlevel"] = String(waterlevel);
  doc["alarmstate"] = alarmstate;
  // doc["systemdelay"] = delay;
  doc["mac_address"] = macaddress;
  doc["ip_address"] = myIP;
  // doc["ssid"] = ssid;
  // doc["password"] = password;

  // {
  //   "maincontrol": "0", // for power
  //   "pumpcontrol": "0", // for pump auto manual
  //   "pumpsw": "1",
  //   "pumpstate": "1",
  //   "waterlevel": "0",
  //   "waterlevelsp": "0",
  //   "mac_address": "C8:F0:9E:E0:A1:DC",
  //   "ip_address": "192.168.18.155",
  //   "ssid": "BITA HOMES",
  //   "password": "xttok2fb"
  // }

 
  char jsonBuffer[1024];
  size_t n = serializeJson(doc, jsonBuffer);
  client.publish(device_topic_p.c_str(), jsonBuffer, true);

#ifdef DEBUG
  Serial.println("");
  Serial.println(jsonBuffer);
  Serial.println("JSON message published");
#endif
}

void client_loop() {
  client.loop();
}

void reconnect() {
  if (!client.connected()) {
    if (WiFi.status() != WL_CONNECTED) {

#ifdef DEBUG
      Serial.println("Wi-Fi is not connected. Attempting to reconnect...");
#endif
      if (WiFi.status() == WL_CONNECTED) {
        myIP = WiFi.localIP().toString();
#ifdef DEBUG
        Serial.print("connected with IP: ");
        Serial.println(myIP);
#endif
        if (!wifi_ap_mode) {
          wifi_channel = WiFi.channel();
          WiFi.softAP(devicename.c_str(), "bitahomes", wifi_channel, 1);
#ifdef DEBUG
          Serial.println("AP Lauched with ssid and password:");
          Serial.println(devicename);
          Serial.println("bitahomes");
#endif
          wifi_ap_mode = true;
          wifi_setting_time = millis();
        }
      }
      return;
    }
    if (!client.connected()) {

      espClient.setCACert(root_ca);
      espClient.setCertificate(client_cert);
      espClient.setPrivateKey(client_key);

      client.setServer(mqtt_server, mqtt_port);
      client.setCallback(callback);

#ifdef DEBUG
      Serial.print("Attempting AWS MQTT connection...");
#endif
      if (client.connect(devicename.c_str())) {
#ifdef DEBUG
        Serial.println("connected");
#endif
        client.subscribe(device_topic_s.c_str());
      } else {
#ifdef DEBUG
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 10 milliseconds");
#endif
      }
    }
  }
}

bool wifi_check(const char *ssid, const char *password) {
  unsigned long check_time = millis();

  WiFi.begin(ssid, password);

#ifdef DEBUG
  Serial.println("Checking Wifi Connection");
  Serial.println("SSID: " + String(ssid));
  Serial.println("Password: " + String(password));
  Serial.print("Connecting to WiFi..");
#endif
  while (WiFi.status() != WL_CONNECTED) {

    if (millis() - check_time >= 4000) {
// Timeout reached, stop trying to connect
#ifdef DEBUG
      Serial.println();
      Serial.println("Failed to connect to WiFi within 4 Seconds.");
#endif
      is_wifi_connected = 0;
      return false;
    }
    delay(10);
#ifdef DEBUG
    Serial.print(".");
#endif
  }
#ifdef DEBUG
  Serial.println();
  Serial.print("Connected to ");
  Serial.println(ssid);
#endif
  is_wifi_connected = 1;
  return true;
}

bool connectToWiFi(const char *ssid, const char *password) {

  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.setHostname(devicename.c_str());

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);

  unsigned long start_time = millis();

  WiFi.begin(ssid, password);

#ifdef DEBUG
  Serial.println("Saved settings:");
  Serial.println("SSID: " + String(ssid));
  Serial.println("Password: " + String(password));
  Serial.print("Connecting to WiFi..");
#endif
  while (WiFi.status() != WL_CONNECTED) {

    if (millis() - start_time >= 5000) {
#ifdef DEBUG
      Serial.println();
      Serial.println("Failed to connect to WiFi within 5 Seconds.");
#endif
      startAccessPoint();
      return false;
    }
    delay(10);
#ifdef DEBUG
    Serial.print(".");
#endif
  }

  myIP = WiFi.localIP().toString();

#ifdef DEBUG
  Serial.println();
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("connected with IP: ");
  Serial.println(myIP);
#endif
  is_wifi_connected = 1;

  wifi_channel = WiFi.channel();
  WiFi.softAP(devicename.c_str(), "bitahomes", wifi_channel, 1);

#ifdef DEBUG
  Serial.println("AP Lauched with ssid and password:");
  Serial.println(devicename);
  Serial.println("bitahomes");
#endif
  wifi_ap_mode = true;
  wifi_setting_time = millis();

  return true;
}


void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/html; charset=UTF-8", "Not found");
}


void server_setup() {

  // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send_P(200, "text/html", control);
  // });

  server.on("/wifi_param_by_app", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });

  server.on(
    "/wifi_param_by_app", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, data, len);

      if (error) {
#ifdef DEBUG
        Serial.print("JSON Parsing Error: ");
        Serial.println(error.c_str());
#endif
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
      }

      const char *ssid = doc["ssid"];
      const char *password = doc["password"];

      if (!ssid || !password || strlen(ssid) == 0 || strlen(password) == 0) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"SSID or password missing\"}");
        return;
      }

#ifdef DEBUG
      Serial.println("Received Wifi Credentials:");
      Serial.println("SSID: " + String(ssid));
      Serial.println("Password: " + String(password));
#endif

      wifi_check(ssid, password);

      StaticJsonDocument<128> responseDoc;
      responseDoc["status"] = "success";
      responseDoc["message"] = "WiFi parameters saved. Restarting.";
      switch (is_wifi_connected) {
        case 0:
          responseDoc["wifi_status"] = 0;
          break;
        case 1:
          responseDoc["wifi_status"] = 1;
          break;
        default:
          break;
      }
      String responseBody;

      serializeJson(responseDoc, responseBody);

#ifdef DEBUG
      Serial.println(responseBody);
#endif

      request->send(200, "application/json", responseBody);

      saveCredentials(ssid, password);


      delay(500);

      if (is_wifi_connected) {
        WiFi.softAPdisconnect();
      } else {
        wifi_ap_mode = true;
        wifi_setting_time = millis();
      }

    });

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  // AsyncElegantOTA.begin(&OTAserver);
  server.onNotFound(notFound);
  // OTAserver.onNotFound(notFound);
  server.begin();
  // OTAserver.begin();
}

void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(devicename.c_str(), "bitahomes", 1, 1);  // Set your desired SSID and password

  IPAddress IP = WiFi.softAPIP();
#ifdef DEBUG
  Serial.print("AP IP address: ");
  Serial.println(IP);
#endif
}
