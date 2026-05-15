#define DEBUG

#include "RS485_outputs.h"
#include "aws_certificates.h"
#include "time.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <String>
#include <PCA9554.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include "SPIFFS.h"
#include <RBDdimmer.h>
#include <dimmable_light.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>

// =====================================================
// AWS MQTT Configuration
// =====================================================
String devicename = "RMS-AAA001";
const char* mqtt_server = "a31qubhv0f0qec-ats.iot.eu-north-1.amazonaws.com";
const int mqtt_port = 8883;

String device_topic_s = "/test/" + devicename + "/1";
String device_topic_p = "/KRC/" + devicename;

WiFiClientSecure espClient;
PubSubClient mqtt_client(espClient);

// =====================================================
// HMI STATIC AP - ALWAYS ON (192.168.4.1)
// =====================================================
const char* HMI_SSID = "RMS-AAA001";
const char* HMI_PASS = "123456789";

// =====================================================
// Hardware Configuration
// =====================================================
Preferences preferences;
PCA9554 ioCon1(0x27);
#define ONE_WIRE_BUS 19
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
#define outputPin  13
#define outputPin2 18
#define outputPin3 2
#define zerocross  35
DimmableLight light1(outputPin);
DimmableLight light2(outputPin2);
DimmableLight light3(outputPin3);
const int syncPin = 35;

AsyncWebServer server(80);

// =====================================================
// State Variables
// =====================================================
int acSwitch=0, curtainsw_mqtt=0, shuttersw_mqtt=0, smartTv=0;
int Light1=0, Light2=0, roomFan=0, dampersw_mqtt=0;  
int light1intense=10, light2intense=10;               
int curtainintense=10, shutterintense=10;
int season=0, cfm_mqtt=10;
float actemp=22.0, dmptemp=10.0, dmptempsp=22.0;
String dampstate="CLOSED";
String macaddress;
unsigned long lastMqttStatus = 0;

// HMI Bridge Variables
volatile uint16_t light1sw=0, light2sw=0, tvsw=0, damperswitch=0, shuttersw=0, curtainsw=0;
volatile uint16_t light1st=0, light2st=0, tvst=0, shst=0, curtainst=0;
volatile uint16_t light1int=10, light2int=10;
volatile uint16_t scfm[4]={0}, seasonsw=0, SP=22;
volatile uint16_t temper=0, temp1=0;

// WiFi Variables
String ssid, pass;
bool wifi_ap_mode=false;
unsigned long wifi_setting_time=0, lastPublish=0, Clocktimer=0, time3=0;
const unsigned long publishInterval=5000;
String Time="", currentdatetime="";
volatile uint16_t SchScan[5]={0};
String myIP;

// =====================================================
// Forward declarations
// =====================================================
void control_func();
void setupMobileAppServer();
void setupDwinHMIEndpoints();
void printWifiStatus();
void reconnectMQTT();
void publishRMSStatus();
void gettemperature();
void printLocalTime();
bool connect_to_wifi();
void Access_Point();
void setup_wifi_credentials();
void loadAllStatesFromPreferences();
void saveAllStatesToPreferences();
void mqttCallback(char* topic, byte* payload, unsigned int length);

// =====================================================
// SETUP - HMI AP PRIORITY + MQTT
// =====================================================
void setup() {
  Serial.begin(115200);
  Serial.println("=== RMS-AAA001 HMI + MQTT READY ===");

  // RS485 first (safe)
  RS485Serial.begin(MODBUS_BAUD_RATE, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  Serial.println("RS485 OK");

  // LOAD PREFERENCES FIRST - CRITICAL FOR NO JERK
  preferences.begin("rms-states", true);
  loadAllStatesFromPreferences();
  preferences.end();
  Serial.println("States loaded from FLASH");

  // PCA9554 - SET PINS TO SAVED STATES IMMEDIATELY (NO JERK!)
  Wire.begin();
  ioCon1.portMode(ALLOUTPUT);
  
  // Restore ALL relay states from preferences INSTANTLY
  ioCon1.digitalWrite(0, Light1);           // Light1
  ioCon1.digitalWrite(1, Light2);           // Light2  
  ioCon1.digitalWrite(2, acSwitch);         // AC
  ioCon1.digitalWrite(3, roomFan);          // Room Fan
  ioCon1.digitalWrite(4, smartTv);          // Smart TV
  ioCon1.digitalWrite(5, shuttersw_mqtt);   // Shutters
  ioCon1.digitalWrite(6, curtainsw_mqtt);   // Curtains
  ioCon1.digitalWrite(7, dampersw_mqtt);    // Damper
  Serial.println("PCA9554 RESTORED - NO RELAY JERK!");

  // Dimmers only (relays already perfect from step 3)
  control_func();
  Serial.println("Dimmers initialized");

  // 5. Storage & Sensors
  if (!SPIFFS.begin(true)) Serial.println("❌ SPIFFS Failed"); 
  else Serial.println("SPIFFS OK");

  DimmableLight::setSyncPin(syncPin);
  DimmableLight::begin();
  sensors.begin();
  Serial.println("Dimmer/Sensors OK");

  // HMI AP ALWAYS FIRST - 192.168.4.1
  macaddress = WiFi.macAddress();
  Serial.println("MAC: " + macaddress);
  Access_Point();  // HMI AP ALWAYS ON
  
  // Try home WiFi AFTER HMI AP (AP stays running)
  setup_wifi_credentials();

  // MQTT Setup
  mqtt_client.setServer(mqtt_server, mqtt_port);
  mqtt_client.setCallback(mqttCallback);

  // Web Servers
  setupMobileAppServer();
  setupDwinHMIEndpoints();  // NEW DWIN HMI ENDPOINTS
  server.begin();
  
  Serial.println("🚀 RMS READY!");
  Serial.println("📶 HMI AP: RMS-AAA001 / 123456789 → http://192.168.4.1");
  Serial.println("📱 DWIN HMI: /update_param (GET) & /sendtoRMS (POST)");
  Serial.println("📱 Mobile App: MQTT Connected");
}

// =====================================================
// MAIN LOOP - HMI AP ALWAYS ON + MQTT
// =====================================================
void loop() {
  static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 10000) {
    printWifiStatus();
    lastWifiCheck = millis();
  }

  // MQTT (works when WiFi connected)
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqtt_client.connected()) reconnectMQTT();
    else mqtt_client.loop();
  }

  // Auto-publish status every 5s
  if (millis() - lastPublish > publishInterval) {
    publishRMSStatus();
    lastPublish = millis();
  }

  // Temperature every 3s
  if (millis() - time3 > 3000) {
    gettemperature();
    time3 = millis();
  }

  // Clock every 1s
  if (millis() - Clocktimer > 1000 && WiFi.status() == WL_CONNECTED) {
    printLocalTime();
    Clocktimer = millis();
  }

  delay(50);  // HMI AP ALWAYS RUNNING - NO TIMEOUT!
}

// =====================================================
// PREFERENCES FUNCTIONS
// =====================================================
void loadAllStatesFromPreferences() {
  acSwitch = preferences.getUShort("acSwitch", 0); acSwitch = constrain(acSwitch, 0, 1);
  Light1 = preferences.getUShort("Light1", 0); Light1 = constrain(Light1, 0, 1);
  Light2 = preferences.getUShort("Light2", 0); Light2 = constrain(Light2, 0, 1);
  roomFan = preferences.getUShort("roomFan", 0); roomFan = constrain(roomFan, 0, 1);
  smartTv = preferences.getUShort("smartTv", 0); smartTv = constrain(smartTv, 0, 1);
  dampersw_mqtt = preferences.getUShort("dampersw", 0); dampersw_mqtt = constrain(dampersw_mqtt, 0, 1);
  curtainsw_mqtt = preferences.getUShort("curtainsw", 0); curtainsw_mqtt = constrain(curtainsw_mqtt, 0, 1);
  shuttersw_mqtt = preferences.getUShort("shuttersw", 0); shuttersw_mqtt = constrain(shuttersw_mqtt, 0, 1);
  
  light1intense = preferences.getUShort("light1intense", 10); light1intense = constrain(light1intense, 10, 100);
  light2intense = preferences.getUShort("light2intense", 10); light2intense = constrain(light2intense, 10, 100);
  curtainintense = preferences.getUShort("curtainintense", 10); curtainintense = constrain(curtainintense, 10, 100);
  shutterintense = preferences.getUShort("shutterintense", 10); shutterintense = constrain(shutterintense, 10, 100);
  
  cfm_mqtt = preferences.getUShort("cfm", 10); cfm_mqtt = constrain(cfm_mqtt, 10, 100);
  season = preferences.getUShort("season", 0); season = constrain(season, 0, 1);
  dmptempsp = preferences.getUShort("dmptempsp", 22); dmptempsp = constrain(dmptempsp, 15, 30);
  
  // HMI sync
  light1sw = Light1; light1int = light1intense;
  light2sw = Light2; light2int = light2intense;
  tvsw = smartTv;
  damperswitch = dampersw_mqtt;
  curtainsw = curtainsw_mqtt;
  shuttersw = shuttersw_mqtt;
  scfm[1] = cfm_mqtt;
  seasonsw = season;
  SP = (uint16_t)dmptempsp;
}

void saveAllStatesToPreferences() {
  preferences.begin("rms-states", false);
  preferences.putUShort("acSwitch", constrain(acSwitch, 0, 1));
  preferences.putUShort("Light1", constrain(Light1, 0, 1));
  preferences.putUShort("Light2", constrain(Light2, 0, 1));
  preferences.putUShort("roomFan", constrain(roomFan, 0, 1));
  preferences.putUShort("smartTv", constrain(smartTv, 0, 1));
  preferences.putUShort("dampersw", constrain(dampersw_mqtt, 0, 1));
  preferences.putUShort("curtainsw", constrain(curtainsw_mqtt, 0, 1));
  preferences.putUShort("shuttersw", constrain(shuttersw_mqtt, 0, 1));
  preferences.putUShort("light1intense", constrain(light1intense, 10, 100));
  preferences.putUShort("light2intense", constrain(light2intense, 10, 100));
  preferences.putUShort("curtainintense", constrain(curtainintense, 10, 100));
  preferences.putUShort("shutterintense", constrain(shutterintense, 10, 100));
  preferences.putUShort("cfm", constrain(cfm_mqtt, 10, 100));
  preferences.putUShort("season", constrain(season, 0, 1));
  preferences.putUShort("dmptempsp", constrain((uint16_t)dmptempsp, 15, 30));
  preferences.end();
}

// =====================================================
// MQTT CALLBACK - Mobile App → RMS → HMI
// =====================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.println("📨 MQTT RX Topic: " + String(topic));
  Serial.println("📨 MQTT RX: " + message);
  
  if (String(topic) == device_topic_s) {
    Extract_by_json(message);
  }
}

// =====================================================
// MQTT JSON PARSER (Mobile App → RMS → HMI)
// =====================================================
void Extract_by_json(String incomingMessage) {
  StaticJsonDocument<1024> doc;
  
  DeserializationError error = deserializeJson(doc, incomingMessage);
  if (error) {
    Serial.println("❌ JSON ERROR: " + String(error.c_str()));
    return;
  }

  Serial.println("✅ MQTT JSON Parsed OK");

  
  bool changed = false;
  if (doc.containsKey("acSwitch")) { acSwitch = constrain(doc["acSwitch"].as<int>(), 0, 1); changed = true; }
  if (doc.containsKey("curtainsw")) { curtainsw_mqtt = constrain(doc["curtainsw"].as<int>(), 0, 1); curtainsw = curtainsw_mqtt; changed = true; }
  if (doc.containsKey("shuttersw")) { shuttersw_mqtt = constrain(doc["shuttersw"].as<int>(), 0, 1); shuttersw = shuttersw_mqtt; changed = true; }
  if (doc.containsKey("smartTv")) { smartTv = constrain(doc["smartTv"].as<int>(), 0, 1); tvsw = smartTv; changed = true; }
  if (doc.containsKey("Light1")) { Light1 = constrain(doc["Light1"].as<int>(), 0, 1); light1sw = Light1; changed = true; }
  if (doc.containsKey("Light2")) { Light2 = constrain(doc["Light2"].as<int>(), 0, 1); light2sw = Light2; changed = true; }
  if (doc.containsKey("roomFan")) { roomFan = constrain(doc["roomFan"].as<int>(), 0, 1); changed = true; }
  if (doc.containsKey("dampersw")) { dampersw_mqtt = constrain(doc["dampersw"].as<int>(), 0, 1); damperswitch = dampersw_mqtt; changed = true; }
  
  if (doc.containsKey("light1intense")) { light1intense = constrain(doc["light1intense"].as<int>(), 10, 100); light1int = light1intense; changed = true; }
  if (doc.containsKey("light2intense")) { light2intense = constrain(doc["light2intense"].as<int>(), 10, 100); light2int = light2intense; changed = true; }
  if (doc.containsKey("curtainintense")) { curtainintense = constrain(doc["curtainintense"].as<int>(), 10, 100); changed = true; }
  if (doc.containsKey("shutterintense")) { shutterintense = constrain(doc["shutterintense"].as<int>(), 10, 100); changed = true; }
  
  if (doc.containsKey("season")) { season = constrain(doc["season"].as<int>(), 0, 1); seasonsw = season; changed = true; }
  if (doc.containsKey("dmptempsp")) { dmptempsp = constrain(doc["dmptempsp"].as<int>(), 15, 30); SP = (uint16_t)dmptempsp; changed = true; }
  if (doc.containsKey("cfm")) { cfm_mqtt = constrain(doc["cfm"].as<int>(), 10, 100); scfm[1] = cfm_mqtt; changed = true; }

  if (changed) {
    Serial.println("🔄 MQTT Updated: L1=" + String(Light1) + " AC=" + String(acSwitch));
    saveAllStatesToPreferences();
    control_func();
  }
}

// =====================================================
// MQTT STATUS PUBLISH (RMS → Mobile App)
// =====================================================
void publishRMSStatus() {
  if (!mqtt_client.connected()) return;

  StaticJsonDocument<1024> doc;
  doc["acSwitch"] = acSwitch;
  doc["curtainsw"] = curtainsw_mqtt;
  doc["shuttersw"] = shuttersw_mqtt;
  doc["smartTv"] = smartTv;
  doc["Light1"] = Light1;
  doc["Light2"] = Light2;
  doc["roomFan"] = roomFan;
  doc["dampersw"] = dampersw_mqtt;
  doc["light1intense"] = light1intense;
  doc["light2intense"] = light2intense;
  doc["curtainintense"] = curtainintense;
  doc["shutterintense"] = shutterintense;
  doc["actemp"] = actemp;
  doc["season"] = season;
  doc["dmptemp"] = dmptemp;
  doc["dmptempsp"] = dmptempsp;
  doc["cfm"] = cfm_mqtt;
  doc["dampstate"] = dampstate;
  doc["mac_address"] = macaddress;
  doc["ip_address"] = WiFi.localIP().toString();
  doc["ssid"] = WiFi.SSID();
  doc["hmi_ip"] = "192.168.4.1";

  String jsonString;
  serializeJson(doc, jsonString);
  
  bool result = mqtt_client.publish(device_topic_p.c_str(), jsonString.c_str(), true);
  Serial.println("📤 MQTT TX (" + String(result?"OK":"FAIL") + "): " + jsonString.substring(0, 200) + "...");
}

// =====================================================
// HARDWARE CONTROL
// =====================================================
void control_func() {
  Serial.println("⚙️  HARDWARE CONTROL");

  // Light 1 (PCA9554 Pin 0)
  if (light1sw == 0 || light1int == 0) {
    ioCon1.digitalWrite(0, LOW);
    light1.setBrightness(0);
    light1st = 0;
  } else {
    ioCon1.digitalWrite(0, HIGH);
    light1.setBrightness(light1int * 255 / 100);
    light1st = 1;
  }

  // Light 2 (PCA9554 Pin 1)
  if (light2sw == 0 || light2int == 0) {
    ioCon1.digitalWrite(1, LOW);
    light2.setBrightness(0);
    light2st = 0;
  } else {
    ioCon1.digitalWrite(1, HIGH);
    light2.setBrightness(light2int * 255 / 100);
    light2st = 1;
  }

  // ON/OFF Devices
  ioCon1.digitalWrite(2, acSwitch);           // AC
  ioCon1.digitalWrite(3, roomFan);            // Room Fan  
  ioCon1.digitalWrite(4, tvsw); tvst = tvsw;  // Smart TV
  ioCon1.digitalWrite(5, shuttersw); shst = shuttersw;
  ioCon1.digitalWrite(6, curtainsw); curtainst = curtainsw;

  // Status updates 
  dampstate = (damperswitch == 1) ? "OPEN" : "CLOSED";
  actemp = (float)temp1;
  dmptemp = (float)temp1;

  Serial.println("✅ L1:" + String(light1st) + "/" + String(light1int) + 
                 " L2:" + String(light2st) + "/" + String(light2int) +
                 " AC:" + String(acSwitch) + " TV:" + String(tvst));
}

// =====================================================
// UTILITIES
// =====================================================
void gettemperature() {
  sensors.requestTemperatures();
  temper = sensors.getTempCByIndex(0);
  if (temper != DEVICE_DISCONNECTED_C) {
    temp1 = (uint16_t)temper;
  }
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  
  char buf[9];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", 
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  Time = String(buf);
}

void printWifiStatus() {
  Serial.println("📶 WiFi: " + WiFi.localIP().toString() + 
                 " RSSI:" + String(WiFi.RSSI()) + 
                 " SSID:" + WiFi.SSID() + 
                 " | HMI AP: 192.168.4.1 ALWAYS ON");
}

void reconnectMQTT() {
  static unsigned long lastAttempt = 0;
  if (millis() - lastAttempt < 5000) return;
  lastAttempt = millis();

  Serial.println("🔄 MQTT Reconnect...");
  mqtt_client.disconnect();
  
  espClient.setCACert(root_ca);
  espClient.setCertificate(client_cert);
  espClient.setPrivateKey(client_key);
  
  if (mqtt_client.connect(devicename.c_str())) {
    Serial.println("✅ MQTT Connected!");
    mqtt_client.subscribe(device_topic_s.c_str(), 1);
  } else {
    Serial.println("❌ MQTT Fail: " + String(mqtt_client.state()));
  }
}

// =====================================================
// WIFI - HMI AP ALWAYS ON + Home WiFi
// =====================================================
void setup_wifi_credentials() {
  Serial.println("📡 WiFi Setup...");
  preferences.begin("wifi-config", true);
  ssid = preferences.getString("ssid", "");
  pass = preferences.getString("pass", "");
  preferences.end();

  if (ssid.length() > 0 && pass.length() > 0) {
    connect_to_wifi();
  }
}

void Access_Point() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(HMI_SSID, HMI_PASS);
  IPAddress IP = WiFi.softAPIP();
  Serial.println("📶 HMI AP Started:");
  Serial.println("📱 SSID: " + String(HMI_SSID));
  Serial.println("🔐 Password: " + String(HMI_PASS));
  Serial.println("🌐 IP: http://" + IP.toString());
  Serial.println("✅ HMI Connect Now!");
  
  wifi_ap_mode = true;
}

bool connect_to_wifi() {
  Serial.println("🔗 Connecting WiFi: " + ssid);
  WiFi.mode(WIFI_AP_STA);  // ✅ HMI AP + Home WiFi
  WiFi.setHostname(devicename.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    myIP = WiFi.localIP().toString();
    Serial.println("✅ WiFi Connected: " + myIP);
    configTime(0, 0, "pool.ntp.org");
    return true;
  } else {
    Serial.println("❌ WiFi Failed - HMI AP continues");
    return false;
  }
}

// =====================================================
// MOBILE APP WIFI CONFIG
// =====================================================
void setupMobileAppServer() {
  server.on("/wifi_param_by_app", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, data, len);
      
      if (error) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
      }

      const char* new_ssid = doc["ssid"];
      const char* new_pass = doc["password"];

      if (!new_ssid || !new_pass || strlen(new_ssid) == 0) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing credentials\"}");
        return;
      }

      ssid = String(new_ssid);
      pass = String(new_pass);

      bool connected = connect_to_wifi();

      StaticJsonDocument<128> resp;
      resp["status"] = "success";
      resp["wifi_status"] = connected ? 1 : 0;
      resp["hmi_ip"] = "192.168.4.1";
      String respBody;
      serializeJson(resp, respBody);

      request->send(200, "application/json", respBody);

      if (connected) {
        preferences.begin("wifi-config", false);
        preferences.putString("ssid", ssid);
        preferences.putString("pass", pass);
        preferences.end();
        Serial.println("💾 WiFi credentials saved");
      }
    });
}

// =====================================================
// 🔥 NEW DWIN HMI ENDPOINTS
// =====================================================
void setupDwinHMIEndpoints() {
  Serial.println("🔥 DWIN HMI Endpoints Active - http://192.168.4.1");

  // =====================================================
  // SEND DATA TO DWIN (ESP8266 fetchData())
  // =====================================================
  server.on("/update_param", HTTP_GET,
  [](AsyncWebServerRequest *request) {

    StaticJsonDocument<1024> doc;

    // EXACT mapping with DWIN fetchData()
    doc["value1"]  = light1sw;
    doc["value2"]  = light2sw;
    doc["value3"]  = tvsw;
    doc["value4"]  = curtainsw;
    doc["value5"]  = damperswitch;
    doc["value6"]  = shuttersw;

    doc["value7"]  = scfm[1];     // Damper CFM
    doc["value8"]  = light1int;   // Light1 intensity
    doc["value9"]  = light2int;   // Light2 intensity

    doc["value10"] = Time;        // Time string
    doc["value11"] = temp1;       // Temperature
    doc["value12"] = seasonsw;
    doc["value13"] = SP;

    // RTC values
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      doc["value14"] = timeinfo.tm_hour;
      doc["value15"] = timeinfo.tm_min;
      doc["value16"] = timeinfo.tm_mday;
      doc["value17"] = timeinfo.tm_mon + 1;
      doc["value18"] = timeinfo.tm_year + 1900;
    } else {
      doc["value14"] = 0;
      doc["value15"] = 0;
      doc["value16"] = 0;
      doc["value17"] = 0;
      doc["value18"] = 0;
    }

    // Remaining fields expected by DWIN
    doc["value19"] = acSwitch;    // dxcoil
    doc["value20"] = season;      // dxcoilmode
    doc["value21"] = roomFan;     // fan switch
    doc["value22"] = light1int;   // fan intensity

    String json;
    serializeJson(doc, json);

    Serial.println("====== SEND TO DWIN ======");
    Serial.println(json);

    request->send(200, "application/json", json);
  });

  // =====================================================
  // RECEIVE DATA FROM DWIN
  // =====================================================
  server.on("/sendtoRMS", HTTP_POST,
  [](AsyncWebServerRequest *request) {
    Serial.println("🔥 HMI POST RECEIVED!");
  },
  NULL,
  [](AsyncWebServerRequest *request,
     uint8_t *data,
     size_t len,
     size_t index,
     size_t total) {

    StaticJsonDocument<1024> doc;

    auto err = deserializeJson(doc, data, len);

    if (err) {
      Serial.println("DWIN JSON ERROR");
      request->send(400, "text/plain", "Bad JSON");
      return;
    }

    Serial.println("====== DATA FROM DWIN ====");

    // SWITCHES
    light1sw      = doc["SW1"] | 0;
    light2sw      = doc["SW2"] | 0;
    tvsw          = doc["SW3"] | 0;
    curtainsw     = doc["SW4"] | 0;
    damperswitch  = doc["SW5"] | 0;
    shuttersw     = doc["SW6"] | 0;

    // EXTRA SWITCHES
    acSwitch      = doc["SW7"] | 0;
    season        = doc["SW8"] | 0;
    roomFan       = doc["SW9"] | 0;

    // DIMMER VALUES
    scfm[1]       = doc["DAM_CFM"] | 0;
    light1int     = doc["LOAD1_DIM"] | 0;
    light2int     = doc["LOAD2_DIM"] | 0;

    // FAN INTENSITY
    int fan_intensity = doc["LOAD3_DIM"] | 0;

    // TEMP SETTINGS
    SP             = doc["SETPOINT"] | 22;
    seasonsw       = doc["SEASON"] | 0;

    // Sync MQTT/App Variables
    Light1            = light1sw;
    Light2            = light2sw;
    smartTv           = tvsw;
    dampersw_mqtt     = damperswitch;
    curtainsw_mqtt    = curtainsw;
    shuttersw_mqtt    = shuttersw;

    cfm_mqtt          = scfm[1];

    light1intense     = light1int;
    light2intense     = light2int;

    dmptempsp         = SP;

    // Save Preferences
    preferences.begin("rms-states", false);
    saveAllStatesToPreferences();
    preferences.end();

    // Apply Hardware
    control_func();

    Serial.println("L1 SW : " + String(light1sw));
    Serial.println("L2 SW : " + String(light2sw));
    Serial.println("TV    : " + String(tvsw));
    Serial.println("CURTAIN : " + String(curtainsw));
    Serial.println("DAMPER  : " + String(damperswitch));
    Serial.println("SHUTTER : " + String(shuttersw));
    Serial.println("CFM     : " + String(scfm[1]));
    Serial.println("TEMP SP : " + String(SP));

    request->send(
      200,
      "application/json",
      "{\"status\":\"ok\"}"
    );
  });

  // =====================================================
  // OLD ENDPOINTS - BACKWARD COMPATIBILITY
  // =====================================================
  server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "RMS-AAA001 DWIN HMI OK! http://192.168.4.1");
  });

  // Root
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"status\":\"RMS-AAA001 DWIN HMI Ready\",\"ip\":\"192.168.4.1\",\"endpoints\":[\"/update_param\",\"/sendtoRMS\"]}");
  });
}
