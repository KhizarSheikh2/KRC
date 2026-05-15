const char* ntpServer = "time.google.com";

const long gmtOffset_sec = 18000;
const int daylightOffset_sec = 0;

Preferences preferences;

// WiFiClientSecure espClient;
// PubSubClient client(espClient);

AsyncWebServer server(80);
// AsyncWebServer OTAserver(81);

void startAccessPoint();
void server_setup();
void showCenteredText(const char*, uint8_t, int);
void updateDisplay();
void updateControlOutputs();

void DEVICE_INIT() {

  preferences.begin("parameters", false);

  sp = preferences.getUInt("sp", 20);
  fansw = preferences.getUInt("fansw", 0);
  pumpsw = preferences.getUInt("pumpsw", 0);
  main_control = preferences.getBool("main_control", false);
  fan_control  = preferences.getBool("fan_control", true);
  pump_control = preferences.getBool("pump_control", true);
  timesch = preferences.getString("timesch", "hoursch=24|daysch=7");
  timeschen = preferences.getUInt("timeschen", 0);
  tmatched = 0;           // Always start as 0; fanScheduler() will set the real value
  timeMatched = false;    // once it runs and evaluates the current time

  preferences.end();

  waterlevelsp = waterlevel;
  temp1sp = String(sp);

#ifdef DEBUG
  Serial.print("sp: ");
  Serial.println(sp);

  Serial.print("fansw: ");
  Serial.println(fansw);

  Serial.print("pumpsw: ");
  Serial.println(pumpsw);

  Serial.print("main_control: ");
  Serial.println(main_control);

  Serial.print("fan_control: ");
  Serial.println(fan_control);

  Serial.print("pump_control: ");
  Serial.println(pump_control);

  Serial.print("Time Schedule: ");
  Serial.println(timesch);

  Serial.print("Time Schedule Enable: ");
  Serial.println(timeschen);

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
  bool mainControlChanged = false;
  bool fanSwChanged = false;        // Flag to track if fansw changed
  bool pumpSwChanged = false;       // Flag to track if pumpsw changed
  bool fanControlChanged = false;   // Flag to track if fancontrol (Auto/Manual) changed
  bool pumpControlChanged = false;  // Flag to track if pumpcontrol (Auto/Manual) changed
  
  if (doc.containsKey("maincontrol")) {
    int newMainControl = doc["maincontrol"].as<int>();
    if (newMainControl != main_control) {
      main_control = newMainControl;
      preferences.putBool("main_control", main_control);
      mainControlChanged = true;
    }
  }
    
  if (doc.containsKey("fancontrol")) {
    int newFanControl = doc["fancontrol"].as<int>();
    if (newFanControl != fan_control) {
      // Block switching to manual if scheduler is ON
      if (timeschen == 1 && newFanControl == 0) {
        Serial.println("MQTT: Blocked fan→manual, scheduler is ON");
      } else {
        fan_control = newFanControl;
        preferences.putBool("fan_control", fan_control);
        fanControlChanged = true;  // ← trigger OLED flash for Auto/Manual change
      }
    }
  }
  
  if (doc.containsKey("pumpcontrol")) {
    int newPumpControl = doc["pumpcontrol"].as<int>();
    if (newPumpControl != pump_control) {
      pump_control = newPumpControl;
      preferences.putBool("pump_control", pump_control);
      pumpControlChanged = true;  // ← trigger OLED flash for Auto/Manual change
    }
  }
  
  if (doc.containsKey("temp1sp")) {
    int newSp = doc["temp1sp"].as<int>();
    if (newSp != sp) {
      temp1sp = String(newSp);
      sp = newSp;
      preferences.putUInt("sp", sp);
    }
  }
  
  // Only update manual switch if scheduler is OFF and fan is in manual mode
  if (doc.containsKey("fansw")) {
    int newFansw = doc["fansw"].as<int>();
    if (newFansw != fansw) {
      fansw = newFansw;
      // Only directly update fanstate if we're in manual mode AND scheduler is off
      if (!fan_control && timeschen == 0) {
        fanstate = fansw;
      }
      preferences.putUInt("fansw", fansw);
      fanSwChanged = true;   // ← trigger OLED flash for fan on/off via MQTT
    }
  }
  
  if (doc.containsKey("pumpsw")) {
    int newPumpsw = doc["pumpsw"].as<int>();
    if (newPumpsw != pumpsw) {
      pumpsw = newPumpsw;
      // Only directly update pumpstate if we're in manual mode
      if (!pump_control) {
        pumpstate = pumpsw;
      }
      preferences.putUInt("pumpsw", pumpsw);
      pumpSwChanged = true;  // ← trigger OLED flash for pump on/off via MQTT
    }
  }
  
  if (doc.containsKey("timesch")) {
    String newTimesch = doc["timesch"].as<String>();
    if (newTimesch != timesch) {
      timesch = newTimesch;
      preferences.putString("timesch", timesch);
    }
  }
  
  if (doc.containsKey("timeschen")) {
    int newTimeschen = doc["timeschen"].as<int>();
    if (newTimeschen != timeschen) {
      timeschen = newTimeschen;
      preferences.putUInt("timeschen", timeschen);
      
      // Force fan to AUTO when scheduler is enabled via MQTT too
      if (timeschen == 1 && !fan_control) {
        fan_control = true;
        preferences.putBool("fan_control", fan_control);
        Serial.println("MQTT: Scheduler ON → Fan forced to AUTO");
      }
    }
  }
  
  if (doc.containsKey("tmatched")) {
    int newTmatched = doc["tmatched"].as<int>();
    if (newTmatched != tmatched) {
      tmatched = newTmatched;
      timeMatched = (tmatched == 1);
      preferences.putUInt("tmatched", tmatched);
      #ifdef DEBUG
        Serial.print("tmatched set and saved to: ");
        Serial.println(tmatched);
      #endif
    }
  }

  if (main_control) {
    updateDisplay();
  }

  preferences.end();
  time3 = millis();

  if (mainControlChanged) {
    if (main_control) {
      showCenteredText("POWER ON", 2, 0);
      delay(800);
      updateDisplay();
    } else {
      fanstate = 0;
      pumpstate = 0;
      updateControlOutputs();
      showCenteredText("POWER OFF", 2, 0);
    }
  }
  else if (main_control) {
    // Only show fan/pump changes if main control is ON
    if (fanSwChanged) {
      // current_mode = FAN;
      showCenteredText(fanstate ? "FAN ON" : "FAN OFF", 2, 0);
      updateControlOutputs();
      delay(800);
      updateDisplay();
    }
    else if (pumpSwChanged) {
      // current_mode = PUMP;
      showCenteredText(pumpstate ? "PUMP ON" : "PUMP OFF", 2, 0);
      updateControlOutputs();
      delay(800);
      updateDisplay();
    }
    else if (fanControlChanged) {
      // current_mode = FAN;
      showCenteredText(fan_control ? "FAN AUTO" : "FAN MANUAL", 2, 0);
      delay(800);
      updateDisplay();
    }
    else if (pumpControlChanged) {
      // current_mode = PUMP;
      showCenteredText(pump_control ? "PUMP AUTO" : "PUMP      MANUAL", 2, 0);
      delay(800);
      updateDisplay();
    }
    else {
      updateDisplay();
    }
  }
  
  #ifdef DEBUG
    Serial.print("Updated timesch: ");
    Serial.println(timesch);
    Serial.print("Updated timeschen: ");
    Serial.println(timeschen);
  #endif
}

//For MQTT
void callback(char *topic, byte *message, unsigned int length) {
  String messageTemp;

  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }

  #ifdef DEBUG
    Serial.println("");
    Serial.println("═════════════════════════════════════");
    Serial.print("   RECEIVED via: ");
    Serial.println(eth_connected ? "ETHERNET" : "WiFi");
    Serial.print("   IP: ");
    Serial.println(myIP);
    Serial.print("   Topic: ");
    Serial.println(String(topic));
    Serial.print("   Message: ");
    Serial.println(messageTemp);
    Serial.println("═════════════════════════════════════");
  #endif

  if (String(topic) == device_topic_s) {
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
  doc["fancontrol"] = String(fan_control);
  doc["pumpcontrol"] = String(pump_control);
  doc["temp1"] = temp1;
  doc["temp1sp"] = String(sp);
  doc["fansw"] = String(fansw);
  doc["fanstate"] = String(fanstate);
  doc["pumpsw"] = String(pumpsw);
  doc["pumpstate"] = String(pumpstate);
  doc["waterlevel"] = String(waterlevel);
  doc["timesch"] = timesch;
  doc["timeschen"] = String(timeschen);
  doc["tmatched"] = tmatched;
  doc["mac_address"] = macaddress;
  doc["ip_address"] = myIP;
  // doc["ssid"] = ssid;
  // doc["password"] = password;

  char jsonBuffer[1024];
  size_t n = serializeJson(doc, jsonBuffer);
  
  bool publishSuccess = activeMqttClient->publish(device_topic_p.c_str(), jsonBuffer, true);

  #ifdef DEBUG
    Serial.println("");
    Serial.println("─────────────────────────────────────");
    Serial.print("   PUBLISHING via: ");
    Serial.println(eth_connected ? "ETHERNET" : "WiFi");
    Serial.print("   IP: ");
    Serial.println(myIP);
    Serial.print("   Topic: ");
    Serial.println(device_topic_p);
    Serial.print("   Status: ");
    Serial.println(publishSuccess ? "✓ SUCCESS" : "✗ FAILED");
    Serial.println(jsonBuffer);
    Serial.println("─────────────────────────────────────");
  #endif
}

void reconnect() {
  if (!activeMqttClient->connected()) {
    if (!eth_connected && WiFi.status() != WL_CONNECTED) {
      Serial.println("No network – skipping MQTT");
      return;
    }

    Serial.println("─────────────────────────────────────");
    Serial.print("🔌 Connecting to AWS MQTT via: ");
    Serial.println(eth_connected ? "ETHERNET" : "WiFi");
    Serial.printf("   Server: %s:%d\n", mqtt_server, mqtt_port);
    Serial.print("   IP: ");
    Serial.println(myIP);
    
    // Clear any existing errors for SSLClient
    if (eth_connected) {
      sslEthClient.flush();  // Clear buffers
      delay(100);
    }
    
    String clientId = devicename + "-" + String(random(0xffff), HEX);
    
    if (activeMqttClient->connect(clientId.c_str())) {
      Serial.println("   ✓ MQTT CONNECTED");
      Serial.println("─────────────────────────────────────");
      activeMqttClient->subscribe(device_topic_s.c_str());
      delay(100);
      publishJson();
    } else {
      int state = activeMqttClient->state();
      Serial.printf("   ✗ MQTT FAILED rc=%d", state);
      
      // Decode MQTT errors
      switch(state) {
        case -4: Serial.println(" (Connection timeout)"); break;
        case -3: Serial.println(" (Connection lost)"); break;
        case -2: Serial.println(" (Connect failed)"); break;
        case -1: Serial.println(" (Disconnected)"); break;
        case 1: Serial.println(" (Bad protocol)"); break;
        case 2: Serial.println(" (Bad client ID)"); break;
        case 3: Serial.println(" (Unavailable)"); break;
        case 4: Serial.println(" (Bad credentials)"); break;
        case 5: Serial.println(" (Unauthorized)"); break;
        default: Serial.println(" (Unknown)"); break;
      }
      Serial.println("─────────────────────────────────────");
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
