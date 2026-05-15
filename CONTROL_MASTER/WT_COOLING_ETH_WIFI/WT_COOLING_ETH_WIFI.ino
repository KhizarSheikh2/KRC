#define DEBUG

// #include <PCA9554.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <PubSubClient.h>  // for MQTT
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "time.h"
#include <WiFi.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_SH110X.h>
#include <Fonts/Org_01.h>
#include <Keypad.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

#define SSL_CLIENT_BUFFER_SIZE 2048
#include <SSLClient.h>

// ── W5500 SPI Pins ────────────────────────────────────────────────
#define W5500_CS    5
#define W5500_MOSI  23
#define W5500_MISO  19
#define W5500_SCLK  18
#define W5500_RST   4

// ── SSLClient entropy: GPIO 34 (floating input-only ADC pin) ─────
#define SSL_ENTROPY_PIN 34

#include "certificates.h"
#include "aws_certificates.h"
#include "variables.h"
#include "wifithread.h"

#define FAN_CONTROL_PIN 13
#define PUMP_CONTROL_PIN 14
#define WATER_LEVEL_PIN 17
#define ONE_WIRE_BUS 16 // Temp sensor pin
#define I2C_SDA 21
#define I2C_SCL 22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
#define BUZZER_PIN 12      // Choose an available GPIO pin
#define BUZZER_FREQ 2000   // Frequency in Hz (2 kHz typical)
#define BUZZER_DURATION 50 // Duration in ms

// PCA9554 ioCon1(0x27);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

enum Mode {
  TEMPERATURE,
  FAN,
  PUMP,
  SCHEDULER,
  NUM_MODES
};

int current_mode = TEMPERATURE;

void beep(int duration = BUZZER_DURATION, int freq = BUZZER_FREQ) {
  tone(BUZZER_PIN, freq, duration);   // Non-blocking tone
  delay(10); // Small delay to ensure it triggers
}

void showCenteredText(const char* text, uint8_t textSize = 2, int yOffset = 0) {
  display.clearDisplay();
  display.setTextSize(textSize);
  display.setFont(); // Uses default font
  display.setTextColor(SH110X_WHITE);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  int y = (SCREEN_HEIGHT - h) / 2 + yOffset;

  display.setCursor(x, y);
  display.print(text);
  display.display();
}

void showStartupScreen() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setFont(&Org_01);
  display.setTextSize(2);

  const char* msg = "Starting";
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  int y = (SCREEN_HEIGHT - h) / 2 - 7;
  display.setCursor(x, y);
  display.print(msg);
  display.display();

  delay(300);
  for (int i = 0; i < 3; i++) {
    display.print(".");
    display.display();
    delay(300);
  }

  display.clearDisplay();
  display.setFont(&Org_01);
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(8, 25);
  display.print("WATER TANK");
  display.setCursor(28, 45);
  display.print("COOLING");
  display.display();
  delay(1500);
}

void updateDisplay() {
  display.clearDisplay();
  display.setFont();  // ALWAYS reset to default font first
  display.setTextColor(SH110X_WHITE);

  // ===== HEADER =====
  display.setTextSize(2);
  String modeName;
  switch (current_mode) {
    case TEMPERATURE: modeName = "WATER TANK"; break;
    case FAN: modeName = "FAN"; break;
    case PUMP: modeName = "PUMP"; break;
    case SCHEDULER: modeName = "SCHEDULER"; break;
  }
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(modeName.c_str(), 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, 4);
  display.print(modeName);
  display.drawFastHLine(2, 22, SCREEN_WIDTH - 4, SH110X_WHITE);

  // ===== MAIN CONTENT =====
  if (current_mode == TEMPERATURE) {
    display.setTextSize(2);
    display.setCursor(9, 27);
    display.print("TEMP:");
    display.setCursor(70, 27);
    display.print(temper);
    display.print((char)247);
    display.print("C");

    display.setTextSize(2);
    display.setCursor(9, 47);
    display.print("S.P :");
    display.setCursor(70, 47);
    display.print(sp);
    display.print((char)247);
    display.print("C");
  } 
  else if (current_mode == FAN) {
    display.setTextSize(2);
    display.setCursor(5, 38);
    display.print(fan_control ? " Auto:" : "Manual:");
    display.setTextSize(2);
    display.setCursor(85, 38);
    display.print(fanstate ? "ON" : "OFF");
  } 
  else if (current_mode == PUMP) {
    display.setTextSize(2);
    display.setCursor(5, 38);
    display.print(pump_control ? " Auto:" : "Manual:");
    display.setTextSize(2);
    display.setCursor(85, 38);
    display.print(pumpstate ? "ON" : "OFF");
  }
  else if (current_mode == SCHEDULER) {
    display.setTextSize(2);
    display.setCursor(25, 27);
    display.print(timeschen ? "Enable" : "Disable");

    if (timeschen) {
      display.setCursor(5, 47);
      // fanstate is the real-time ground truth set by fanScheduler()
      // tmatched from Preferences can be stale at boot, so use fanstate directly
      display.print(fanstate ? "Active" : "Not-Active");
    }
  }
  display.display();
}

void updateControlOutputs() {
  // fanstate and pumpstate are always resolved before this is called.
  // When power is OFF, loopFunction() zeroes both states before calling here.
  digitalWrite(FAN_CONTROL_PIN,  fanstate  ? HIGH : LOW);
  digitalWrite(PUMP_CONTROL_PIN, pumpstate ? HIGH : LOW);
}

void loopFunction() {
  if (datainterrupt) {
    datainterrupt = false;
    if (WiFi.status() == WL_CONNECTED && deviceConnected == false) {
      wifi_status = 1;
      NTP_TIME();
      inputs();
    }
  }

  if (main_control) {

    // ── Fan Control ──────────────────────────────────────────────
    if (timeschen == 1) {
      // fanScheduler() only needs to run once per minute — the schedule
      // is hour-based, and calling it every loop() floods the Serial monitor.
      static unsigned long lastSchedulerRun = 0;
      if (millis() - lastSchedulerRun >= 60000UL || lastSchedulerRun == 0) {
        fanScheduler();
        lastSchedulerRun = millis();
      }
    } else if (fan_control) {
      fan_auto_control();       // Auto mode sets fanstate based on temp
    } else {
      fan_manual_control();     // Manual mode sets fanstate based on fansw
    }

    // ── Pump Control ─────────────────────────────────────────────
    if (pump_control) {
      pump_auto_control();      // Auto mode sets pumpstate based on water level
    } else {
      pump_manual_control();    // Manual mode sets pumpstate based on pumpsw
    }

    // ── Apply outputs once, after all state is resolved ──────────
    updateControlOutputs();

  } else {
    // Power is OFF — force everything off
    fanstate  = 0;
    pumpstate = 0;
    updateControlOutputs();     // Single call handles the LOW writes
  }
}

void getTemperature() {
  if (tmp_req) {
    sensors.setWaitForConversion(false);
    sensors.requestTemperatures();
    sensors.setWaitForConversion(true);
    tmp_req = 0;
    time3 = millis();
    temper = sensors.getTempCByIndex(0);
    if (temper > 0) {
      temp1 = String(temper);
    } else {
      temp1 = "0";
    }
  }
  if (temper != tprev) {
    op = 1;
    tprev = temper;
    time1 = millis();
  }
}

void inputs() {
  if (digitalRead(WATER_LEVEL_PIN) == LOW) {
  #ifdef DEBUG
      Serial.println("==========digitalRead(WATER_LEVEL_PIN) == LOW======");
      Serial.println("Full");
  #endif
      waterlevel = 1;
      waterlevelsp = 1;
    } else {
  #ifdef DEBUG
      Serial.println("==========digitalRead(WATER_LEVEL_PIN) == HIGH======");
      Serial.println("Low");
  #endif
      waterlevel = 0;
      waterlevelsp = 0;
    }
}

void fan_auto_control() {
  if (temper > sp && op == 0) {
    //ioCon1.digitalWrite(4, HIGH);
    // digitalWrite(FAN_CONTROL_PIN, HIGH); // For ESP32 Board
    fanstate = 1;
  } else if (temper <= sp && op == 0) {
    //ioCon1.digitalWrite(4, LOW);
    // digitalWrite(FAN_CONTROL_PIN, LOW); // For ESP32 Board
    fanstate = 0;
  }
  
}
void pump_auto_control() {
  if (waterlevel == 0) {
    //ioCon1.digitalWrite(5, HIGH);
    // digitalWrite(PUMP_CONTROL_PIN, HIGH); // For ESP32 Board
    pumpstate = 1;
  } else if (waterlevel == 1) {
    //ioCon1.digitalWrite(5, LOW);
    // digitalWrite(PUMP_CONTROL_PIN, LOW); // For ESP32 Board
    pumpstate = 0;
  }
  
}
void fan_manual_control() {
  if (fansw == 1) {
    // ioCon1.digitalWrite(4, HIGH);
    // digitalWrite(FAN_CONTROL_PIN, HIGH); // For ESP32 Board
    fanstate = 1;
  } else {
    // ioCon1.digitalWrite(4, LOW);
    // digitalWrite(FAN_CONTROL_PIN, LOW); // For ESP32 Board
    fanstate = 0;
  }
  
}
void pump_manual_control() {
  if (pumpsw == 1) {
    // ioCon1.digitalWrite(5, HIGH);
    // digitalWrite(PUMP_CONTROL_PIN, HIGH); // For ESP32 Board
    pumpstate = 1;
  } else {
    // ioCon1.digitalWrite(5, LOW);
    // digitalWrite(PUMP_CONTROL_PIN, LOW); // For ESP32 Board
    pumpstate = 0;
  }
  
}

void NTP_TIME() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
  #ifdef DEBUG
      Serial.println("Failed to obtain time");
  #endif
      return;
    }
  #ifdef DEBUG
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    Serial.print("   ");
    Serial.print(timeinfo.tm_hour);
    Serial.print(" ");
  #endif
    deviceTime = "";
  #ifdef DEBUG
    Serial.print("day=");
    Serial.println(timeinfo.tm_wday);
  #endif
    tmp = day_app[timeinfo.tm_wday];
    SchScan[0] = tmp;
    SchScan[1] = timeinfo.tm_mday;
    SchScan[2] = timeinfo.tm_mon + 1;
    SchScan[3] = timeinfo.tm_year - 100;
    SchScan[4] = timeinfo.tm_hour;
    SchScan[5] = timeinfo.tm_min;
    SchScan[6] = timeinfo.tm_sec;
  #ifdef DEBUG
    Serial.println("NTP_TIME called");
  #endif
}

bool syncNTPoverEthernet() {
  const int NTP_PACKET_SIZE = 48;
  byte packetBuffer[NTP_PACKET_SIZE];

  // Use direct IPs (NO DNS)
  IPAddress ntpServers[] = {
    IPAddress(216, 239, 35, 0),    // time.google.com
    IPAddress(129, 6, 15, 28),     // time.nist.gov
    IPAddress(162, 159, 200, 1)    // time.cloudflare.com
  };

  for (int s = 0; s < 3; s++) {

    Serial.print("Trying NTP server: ");
    Serial.println(ntpServers[s]);

    ntpUDP.begin(2390);

    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;
    packetBuffer[1] = 0;
    packetBuffer[2] = 6;
    packetBuffer[3] = 0xEC;
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    ntpUDP.beginPacket(ntpServers[s], 123);
    ntpUDP.write(packetBuffer, NTP_PACKET_SIZE);
    ntpUDP.endPacket();

    unsigned long startWait = millis();

    while (millis() - startWait < 3000) {
      int size = ntpUDP.parsePacket();
      if (size >= NTP_PACKET_SIZE) {

        ntpUDP.read(packetBuffer, NTP_PACKET_SIZE);

        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord  = word(packetBuffer[42], packetBuffer[43]);
        unsigned long secsSince1900 = (highWord << 16) | lowWord;

        const unsigned long seventyYears = 2208988800UL;
        unsigned long epoch = secsSince1900 - seventyYears;

        struct timeval tv;
        tv.tv_sec = epoch;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);

        // SET TIMEZONE
        setenv("TZ", "PKT-5", 1);
        tzset();

        struct tm timeinfo;
        getLocalTime(&timeinfo);

        Serial.printf("✓ NTP synced via Ethernet: %02d:%02d:%02d | wday=%d\n",
                      timeinfo.tm_hour,
                      timeinfo.tm_min,
                      timeinfo.tm_sec,
                      timeinfo.tm_wday);

        ntpUDP.stop();
        return true;
      }
      delay(10);
    }

    ntpUDP.stop();
    Serial.println("Timeout, trying next server...");
  }

  Serial.println("✗ All Ethernet NTP servers failed");
  return false;
}

bool syncNTPoverWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("NTP WiFi: not connected, skipping");
    return false;
  }

  configTime(gmtOffset_sec, daylightOffset_sec, "time.google.com", "pool.ntp.org", "time.nist.gov");

  // Wait up to 5 seconds for sync
  struct tm timeinfo;
  unsigned long waitStart = millis();
  while (millis() - waitStart < 5000) {
    if (getLocalTime(&timeinfo)) {
      Serial.printf("✓ NTP synced via WiFi: %02d:%02d:%02d | wday=%d\n",
        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_wday);
      return true;
    }
    delay(200);
  }
  Serial.println("✗ NTP WiFi sync timeout");
  return false;
}

void syncNTP() {
  Serial.println("── NTP Sync ──────────────────────────");
  bool synced = false;

  if (eth_connected) {
    Serial.println("Trying NTP over Ethernet...");
    synced = syncNTPoverEthernet();
  }

  if (!synced && WiFi.status() == WL_CONNECTED) {
    Serial.println("Trying NTP over WiFi...");
    synced = syncNTPoverWiFi();
  }

  if (!synced) {
    Serial.println("✗ NTP sync failed on all interfaces");
  }

  Serial.println("──────────────────────────────────────");
}

void IRAM_ATTR onTimer() {
  if (milscount > 1000) {
    milscount = 0;
    datainterrupt = true;
  }
  if (wifi_status == 0 && deviceConnected == false) {
    if (ledcounter > 50) {
      ledcounter = 0;
      if (blink == 0) {
        // digitalWrite(23, HIGH);
        blink = 1;
      } else {
        // digitalWrite(23, LOW);
        blink = 0;
      }
    }
  } else if (wifi_status == 0 && deviceConnected == true) {
    if (ledcounter > 500) {
      ledretrycount += 1;
      if (ledretrycount > 5) {
        ledcounter = 0;
        ledretrycount = 0;
      } else {
        ledcounter = 400;
      }
      if (blink == 0) {
        // digitalWrite(23, HIGH);
        blink = 1;
      } else {
        // digitalWrite(23, LOW);
        blink = 0;
      }
    }
  } else if (wifi_status == 1 && deviceConnected == false) {
    if (ledcounter > 1001) {
      ledcounter = 0;
      if (blink == 0) {
        // digitalWrite(23, HIGH);
        blink = 1;
      } else {
        // digitalWrite(23, LOW);
        blink = 0;
      }
    }
  }
  ledcounter++;
  milscount++;
}

void parse_schedule() {
  // Clear arrays
  memset(days_in_num, 0, sizeof(days_in_num));
  memset(hours_num, 0, sizeof(hours_num));

  // Find the pipe separator
  int pipeIndex = timesch.indexOf('|');
  if (pipeIndex == -1) return;  // Invalid format, do nothing

  String hoursPart = timesch.substring(0, pipeIndex);
  String daysPart = timesch.substring(pipeIndex + 1);

  // Parse hours
  if (hoursPart.startsWith("hoursch=")) {
    String hstr = hoursPart.substring(8);  // After "hoursch="
    if (hstr == "24") {
      // All hours
      for (int i = 0; i < 24; i++) hours_num[i] = 1;
    } else {
      // Comma-separated list, e.g., "0,1,2"
      int start = 0;
      int comma = hstr.indexOf(',', start);
      while (comma != -1) {
        int h = hstr.substring(start, comma).toInt();
        if (h >= 0 && h < 24) hours_num[h] = 1;
        start = comma + 1;
        comma = hstr.indexOf(',', start);
      }
      // Last value
      int h = hstr.substring(start).toInt();
      if (h >= 0 && h < 24) hours_num[h] = 1;
    }
  }

  // Parse days
  if (daysPart.startsWith("daysch=")) {
    String dstr = daysPart.substring(7);  // After "daysch="
    if (dstr == "7") {
      // All days
      for (int i = 0; i < 7; i++) days_in_num[i] = 1;
    } else {
      // Comma-separated list, e.g., "0,1,2"
      int start = 0;
      int comma = dstr.indexOf(',', start);
      while (comma != -1) {
        int d = dstr.substring(start, comma).toInt();
        if (d >= 0 && d < 7) days_in_num[d] = 1;
        start = comma + 1;
        comma = dstr.indexOf(',', start);
      }
      // Last value
      int d = dstr.substring(start).toInt();
      if (d >= 0 && d < 7) days_in_num[d] = 1;
    }
  }

  #ifdef DEBUG
    Serial.println("Parsed schedule:");
    Serial.print("Hours: ");
    for (int i = 0; i < 24; i++) if (hours_num[i]) Serial.print(String(i) + " ");
    Serial.println();
    Serial.print("Days: ");
    for (int i = 0; i < 7; i++) if (days_in_num[i]) Serial.print(String(i) + " ");
    Serial.println();
  #endif
}

void fanScheduler() {
  if (timeschen == 0) {
    // Serial.println("Scheduler disabled (timeschen = 0)");
    return; // Scheduler disabled
  }

  // Fetch current NTP time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // #ifdef DEBUG
      Serial.println("NTP time unavailable - skipping scheduler");
    // #endif
    return; // Skip if NTP fails (e.g., no WiFi or sync issue)
  }

  int currentHour = timeinfo.tm_hour;  // Current hour from NTP
  int currentDay = timeinfo.tm_wday;   // Current day from NTP

  currentDay = (currentDay == 0) ? 6 : currentDay - 1;

  parse_schedule();

  #ifdef DEBUG
  Serial.printf("Scheduler: Day=%d Hour=%d\n", currentDay, currentHour);
  #endif

  if (days_in_num[currentDay] == 1 && hours_num[currentHour] == 1) {
        tmatched = 1;
        fanstate = 1;
        // Serial.println("SCHEDULE MATCH → FAN ON");
    } else {
        tmatched = 0;
        fanstate = 0;
        // Serial.println("NO MATCH → FAN OFF");
  }
  
  // Save to Preferences (only if changed to avoid wear)
  static int prevTmatched = -1;
  if (tmatched != prevTmatched) {
    preferences.begin("parameters", false);
    preferences.putUInt("tmatched", tmatched);
    preferences.end();
    prevTmatched = tmatched;
  }
}

void handleAdjust(char key) {
  bool stateChanged = false;
  
  if (current_mode == TEMPERATURE) {
    int oldSp = sp;
    if (key == '3' && sp < MAX_TEMP) {
      beep(50, 2000);
      sp++;      
    }
    else if (key == '4' && sp > MIN_TEMP) {
      beep(50, 2000);
      sp--;
    }
    if (sp != oldSp) {
      preferences.begin("parameters", false);
      preferences.putUInt("sp", sp);
      preferences.end();
      stateChanged = true;
    }
  }
  else if (current_mode == FAN && !fan_control && timeschen == 0) {  
    // Only allow manual control when scheduler is OFF
    if (key == '4' || key == '3') {
      beep(50, 2000);
      fansw = !fansw;
      fanstate = fansw;
      
      preferences.begin("parameters", false);
      preferences.putUInt("fansw", fansw);
      preferences.end();
      stateChanged = true;
    }
  } 
  else if (current_mode == PUMP && !pump_control) {
    if (key == '4' || key == '3') {
      beep(50, 2000);
      pumpsw = !pumpsw;
      pumpstate = pumpsw;
      
      preferences.begin("parameters", false);
      preferences.putUInt("pumpsw", pumpsw);
      preferences.end();
      stateChanged = true;
    }
  }
  
  // Only update display immediately
  if (stateChanged) {
    updateDisplay();
    updateControlOutputs();
  }
  // Don't publish here - let the normal publish cycle handle it
}

bool initW5500() {
  // Try to read W5500 version register - should return 0x04 for W5500
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(W5500_CS, LOW);
  
  SPI.transfer(0x00);  // Version register address high byte
  SPI.transfer(0x39);  // Version register address low byte  
  SPI.transfer(0x00);  // Read mode, common register
  byte version = SPI.transfer(0x00);
  
  digitalWrite(W5500_CS, HIGH);
  SPI.endTransaction();
  
  Serial.print("W5500 Version Register: 0x");
  Serial.println(version, HEX);
  
  return (version == 0x04);  // W5500 returns 0x04
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(OLED_ADDRESS, true)) {
    Serial.println("OLED init failed");
    while (1);
  }

  showStartupScreen();

  pinMode(WATER_LEVEL_PIN, INPUT_PULLUP);
  pinMode(FAN_CONTROL_PIN, OUTPUT);
  pinMode(PUMP_CONTROL_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(FAN_CONTROL_PIN, LOW);
  digitalWrite(PUMP_CONTROL_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  keypad.setHoldTime(200);

  #ifdef DEBUG
    Serial.println("\n\nbooting up device...\n\n");
    Serial.print("devicename: ");
    Serial.println(devicename);
  #endif

  DEVICE_INIT();
  Serial.printf("After DEVICE_INIT: main_control=%d, timeschen=%d\n", main_control, timeschen);

  sensors.begin();
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();
  sensors.setWaitForConversion(true);

  //=========================================//
  macaddress = WiFi.macAddress();

  if (loadCredentials(ssid, password)) {
    #ifdef DEBUG
      Serial.println("Loaded Wi-Fi credentials from memory.");
    #endif
    connectToWiFi(ssid.c_str(), password.c_str());
  } else {
    #ifdef DEBUG
      Serial.println("No Wi-Fi credentials found. Starting in AP mode.");
    #endif
    startAccessPoint();
  }

  server_setup();

  // ── Ethernet Init (after WiFi AP) ─────────────────────────────
  Serial.println("\n═══ Ethernet Initialization ═══");
  pinMode(W5500_RST, OUTPUT);
  digitalWrite(W5500_RST, LOW); 
  delay(50);
  digitalWrite(W5500_RST, HIGH); 
  delay(1000);

  SPI.begin(W5500_SCLK, W5500_MISO, W5500_MOSI, W5500_CS);
  pinMode(W5500_CS, OUTPUT);
  digitalWrite(W5500_CS, HIGH);

  // Manual W5500 detection
  if (!initW5500()) {
    Serial.println("✗ W5500 chip not detected on SPI bus");
    Serial.println("  Check: CS, MOSI, MISO, SCLK connections");
    eth_connected = false;
  } else {
    Serial.println("✓ W5500 chip detected");
    
    Ethernet.init(W5500_CS);
    
    // Serial.print("Hardware Status: ");
    switch(Ethernet.hardwareStatus()) {
      case EthernetNoHardware: Serial.println("Not detected"); break;
      case EthernetW5100: Serial.println("W5100"); break;
      case EthernetW5200: Serial.println("W5200"); break;
      case EthernetW5500: Serial.println("W5500"); break;
      default: Serial.println("Unknown"); break;
    }
    
    Serial.print("Initial Link: ");
    Serial.println(Ethernet.linkStatus() == LinkON ? "ON" : "OFF");
    
    // Wait for link
    Serial.print("Waiting for link");
    int linkWaitCount = 0;
    while (Ethernet.linkStatus() != LinkON && linkWaitCount < 50) {
      delay(100);
      linkWaitCount++;
      if (linkWaitCount % 5 == 0) Serial.print(".");
    }
    Serial.println();
    
    if (Ethernet.linkStatus() == LinkON) {
      Serial.println("✓ Cable connected, starting DHCP...");
      
      unsigned long dhcpStart = millis();
      bool dhcpSuccess = Ethernet.begin(newMACAddress);
      
      if (!dhcpSuccess) {
        Serial.println("DHCP timeout, using static IP");
        Ethernet.begin(newMACAddress, IPAddress(192, 168, 0, 117));
        delay(500);
      }
      
      if (Ethernet.localIP() != INADDR_NONE) {
        eth_connected = true;
        myIP = Ethernet.localIP().toString();
        Serial.print("✓ IP: ");
        Serial.println(myIP);
      } else {
        Serial.println("✗ Failed to get IP");
        eth_connected = false;
      }
    } else {
      Serial.println("✗ No link (cable unplugged)");
      eth_connected = false;
    }
  }

  Serial.println(eth_connected ? "→ Using ETHERNET" : "→ Using WiFi");
  if (!eth_connected) myIP = WiFi.localIP().toString();
  Serial.println("═══════════════════════════════\n");

  // ── NTP ──────────────────────────────────────────────────────────
  syncNTP();
  Serial.println("NTP configured");

  // ── Verify entropy source ──────────────────────────────────────
  pinMode(SSL_ENTROPY_PIN, INPUT);
  randomSeed(analogRead(SSL_ENTROPY_PIN));
  Serial.printf("SSL Entropy value: %d\n", analogRead(SSL_ENTROPY_PIN));

  // ── MQTT Setup ────────────────────────────────────────────────
  if (eth_connected) {
    // Configure SSLClient for Ethernet
    sslEthClient.setMutualAuthParams(mTLS);
    sslEthClient.setTimeout(30000);  // 30 second timeout
    
    ethMqttClient.setServer(mqtt_server, mqtt_port);
    ethMqttClient.setCallback(callback);
    ethMqttClient.setBufferSize(2048);  // Increase MQTT buffer
    ethMqttClient.setKeepAlive(60);     // Keep-alive interval
    activeMqttClient = &ethMqttClient;
    
    delay(2000);  // Give SSLClient time to initialize
    Serial.println("Using Ethernet for MQTT");
  } else {
    wifiClient.setCACert(root_ca);
    wifiClient.setCertificate(client_cert);
    wifiClient.setPrivateKey(client_key);
    wifiMqttClient.setServer(mqtt_server, mqtt_port);
    wifiMqttClient.setCallback(callback);
    wifiMqttClient.setBufferSize(2048);
    wifiMqttClient.setKeepAlive(60);
    activeMqttClient = &wifiMqttClient;
    Serial.println("Using WiFi for MQTT");
  }

  delay(1000);  // Brief pause before first connection attempt
  reconnect();

  // ── Initialize Display Based on Power State ──────────────────
  if (main_control) {
    // System is ON - show current mode display
    updateDisplay();
  } else {
    // System is OFF - show power off message
    showCenteredText("POWER OFF", 2);
  }
}

void loop() {
  static char heldKey = NO_KEY;
  bool prev_eth = eth_connected;

  if (millis() - lastInputTime > 2000) {  // Every 2 seconds
    inputs();
    lastInputTime = millis();
  }

  char key = keypad.getKey();

  if (key) {
    Serial.print("Key: ");
    Serial.println(key);

    switch (key) {
      case '1': // Toggle Power
        main_control = !main_control;
        beep(100, 2000);

        preferences.begin("parameters", false);
        preferences.putBool("main_control", main_control);
        preferences.end();

        if (main_control) {
          showCenteredText("POWER ON", 2);
          delay(800);
          updateDisplay();  // Show current mode
        } else {
          fanstate = 0;
          pumpstate = 0;
          updateControlOutputs();
          // display.clearDisplay();
          display.setFont();  // Reset font
          showCenteredText("POWER OFF", 2);
        }
        publishJson();  // Publish state change
        break;

      case '2': // Mode button
        if (main_control) {
          if (!modePressed) {
            modePressed = true;
            modePressStart = millis();
          }
        }
        break;

      case '3':
      case '4': // Adjustments
        if (main_control) {
          // beep();
          if (current_mode == TEMPERATURE) {
            handleAdjust(key);
            heldKey = key;
            lastRepeat = millis();
          } else if (current_mode == FAN || current_mode == PUMP || current_mode == SCHEDULER) {
            handleAdjust(key);
            lastRepeat = millis();
          }
        }
        break;
    }
  }

  // Detect long press WHILE button is held 
  if (modePressed) {
    unsigned long pressDuration = millis() - modePressStart;

    if (pressDuration >= LONG_PRESS_TIME && keypad.getState() == HOLD) {
      modePressed = false;

      if (current_mode == SCHEDULER) {
        timeschen = !timeschen;
        beep(200, 1500);
        if (timeschen == 1 && !fan_control) {
          fan_control = true;
          preferences.begin("parameters", false);
          preferences.putBool("fan_control", fan_control);
          preferences.end();
          Serial.println("Scheduler ON → Fan forced to AUTO");
        }
        preferences.begin("parameters", false);
        preferences.putUInt("timeschen", timeschen);
        preferences.end();

      } else if (current_mode == FAN) {
        if (timeschen == 1) {
          showCenteredText("Disable Scheduler First", 1);
          beep(200, 1500);
          delay(1000);
        } else {
          fan_control = !fan_control;
          preferences.begin("parameters", false);
          preferences.putBool("fan_control", fan_control);
          preferences.end();
          beep(100, 2000);
          showCenteredText(fan_control ? "FAN AUTO" : "FAN MANUAL", 2);
          delay(800);
        }

      } else if (current_mode == PUMP) {
        pump_control = !pump_control;
        preferences.begin("parameters", false);
        preferences.putBool("pump_control", pump_control);
        preferences.end();
        beep(100, 2000);
        showCenteredText(pump_control ? "PUMP AUTO" : "PUMP      MANUAL", 2);
        delay(800);
      }

      updateDisplay();
      publishJson();

    } else if (keypad.getState() == RELEASED) {
      if (pressDuration < LONG_PRESS_TIME) {
        if (main_control) {
          current_mode = (current_mode + 1) % NUM_MODES;
          beep(50, 2000);
          delay(600);
          updateDisplay();
        }
      }
      modePressed = false;
    }
  }

  // Handle continuous key hold
  if (heldKey != NO_KEY && main_control) {
    if (keypad.getState() == HOLD && millis() - lastRepeat > repeatRate) {
      handleAdjust(heldKey);
      lastRepeat = millis();
    }
    if (keypad.getState() == RELEASED) {
      heldKey = NO_KEY;
    }
  }

  // Ethernet link monitoring and maintenance
  static unsigned long lastMaintain = 0;
  static unsigned long lastLinkCheck = 0;

  // Maintain DHCP lease every 30 seconds
  if (millis() - lastMaintain > 30000UL) {
    if (eth_connected) {
      byte maintainResult = Ethernet.maintain();
      if (maintainResult != 0) {
        #ifdef DEBUG
          Serial.printf("Ethernet maintain returned: %d\n", maintainResult);
        #endif
      }
    }
    lastMaintain = millis();
  }

  // Check link status more frequently (every 1 second instead of on-demand)
  if (millis() - lastLinkCheck > 1000UL) {
    bool currentEthStatus = false;
    
    // Only check if we think we're connected OR if chip was detected at startup
    if (eth_connected || Ethernet.hardwareStatus() == EthernetW5500) {
      currentEthStatus = (Ethernet.linkStatus() == LinkON && Ethernet.localIP() != INADDR_NONE);
    }
    
    if (currentEthStatus != eth_connected) {
      eth_connected = currentEthStatus;
      
      Serial.println("");
      Serial.println("╔═════════════════════════════════════╗");
      if (eth_connected) {
        Serial.println("║  🔄 NETWORK SWITCH: WiFi → Ethernet ║");
      } else {
        Serial.println("║  🔄 NETWORK SWITCH: Ethernet → WiFi ║");
      }
      Serial.println("╚═════════════════════════════════════╝");
      
      // Disconnect and cleanup
      if (activeMqttClient->connected()) {
        activeMqttClient->disconnect();
      }
      delay(200);  // Reduced cleanup delay
      
      if (eth_connected) {
        sslEthClient.flush();
        sslEthClient.setMutualAuthParams(mTLS);
        sslEthClient.setTimeout(30000);
        ethMqttClient.setServer(mqtt_server, mqtt_port);
        ethMqttClient.setCallback(callback);
        ethMqttClient.setBufferSize(2048);
        ethMqttClient.setKeepAlive(60);
        activeMqttClient = &ethMqttClient;
        myIP = Ethernet.localIP().toString();
        Serial.print("New IP (Ethernet): ");
        Serial.println(myIP);
        syncNTP();
      } else {
        wifiClient.setCACert(root_ca);
        wifiClient.setCertificate(client_cert);
        wifiClient.setPrivateKey(client_key);
        wifiMqttClient.setServer(mqtt_server, mqtt_port);
        wifiMqttClient.setCallback(callback);
        wifiMqttClient.setBufferSize(2048);
        wifiMqttClient.setKeepAlive(60);
        activeMqttClient = &wifiMqttClient;
        myIP = WiFi.localIP().toString();
        Serial.print("New IP (WiFi): ");
        Serial.println(myIP);
        syncNTP();
      }
      
      delay(1000);
      reconnect();
    }
    
    lastLinkCheck = millis();
  }

  // NTP periodic resync - every hour
  static unsigned long lastNtpResync = 0;
  if (millis() - lastNtpResync > 3600000UL) {
    syncNTP();
    lastNtpResync = millis();
  }

  // MQTT handling
  if (!activeMqttClient->connected()) {
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 10000) {  // Retry every 10s
      reconnect();
      lastReconnect = millis();
    }
  } else {
    // Only call loop() if connected
    activeMqttClient->loop();
    
    static unsigned long lastPublish = 0;
    if (millis() - lastPublish > 3000) {  // Publish every 3s
      publishJson();
      lastPublish = millis();
    }
  }

  // WiFi AP timeout
  if (millis() - wifi_setting_time >= 300000 && wifi_ap_mode == true) {
    WiFi.softAPdisconnect();
    wifi_ap_mode = false;
  }

  // Reset temperature stabilization flag
  if ((millis() - time1) > 8000) {
    op = 0;
  }

  // Temperature reading and display update
  if ((millis() - time3) > 2000) {
    tmp_req = true;
    #ifdef DEBUG
      int numberOfDevices = sensors.getDeviceCount();
      Serial.print("numberOfDevices = ");
      Serial.println(numberOfDevices);
      Serial.println("request temperatures-----------------------------------------");
    #endif
    getTemperature();
    if (current_mode == TEMPERATURE && main_control) {
      updateDisplay();
    }
    time3 = millis();
  }

  // Main control logic
  loopFunction();

  // Update system state
  watertankcoolsw = (pumpstate == 1 || fanstate == 1) ? 2 : 0;

  bool needRefresh = false;
  
  if (prev_fanstate != fanstate) {
    prev_fanstate = fanstate;
    if (current_mode == FAN) needRefresh = true;
  }

  if (prev_pumpstate != pumpstate) {
    prev_pumpstate = pumpstate;
    if (current_mode == PUMP) needRefresh = true;
  }

  if (needRefresh && main_control) {
    updateDisplay();
  }

  delay(15);
}

