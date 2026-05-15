#define DEBUG

#include <ArduinoJson.h>

#include <WiFi.h>
// #include <esp_task_wdt.h>
#include <ISL1208_RTC.h>
#include <PCA9554.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <PubSubClient.h>  // for MQTT
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "time.h"
// #include <WiFi.h>
// #include <AsyncElegantOTA.h>
// #include <esp_wifi.h>
#include <Preferences.h>

PCA9554 ioCon1(0x27);

#define ONE_WIRE_BUS 19
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

ISL1208_RTC myRtc = ISL1208_RTC();

DeviceAddress sensor1 = { 0x28, 0xF9, 0x5B, 0x75, 0xD0, 0x1, 0x3C, 0xF6 };

byte i;
int count = 0;
byte data[12];
byte addr[8];
float celsius;
int temper;
int tprev = 0;
bool op = 0, SHED = 0, tmp_req = 0;
unsigned long dctime, time1, time3;
int democount = 0;

#include "variables.h"
#include "aws_certificates.h"
#include "wifithread.h"

void loopFunction() {
  if (datainterrupt) {
    // esp_task_wdt_reset();
    datainterrupt = false;
    if (WiFi.status() == WL_CONNECTED && deviceConnected == false) {
      wifi_status = 1;

      //RTC_ADJUST();
      NTP_TIME();
      inputs();
    }
  }
  if (main_control) {
    if (fan_control) {
      fan_auto_control();
    } else {
      fan_manual_control();
    }
    if (pump_control) {
      pump_auto_control();
    } else {
      pump_manual_control();
    }
  } else {
    ioCon1.digitalWrite(4, LOW);
    // digitalWrite(2, HIGH); // For ESP32 Board
    fanstate = 0;

    ioCon1.digitalWrite(5, LOW);
    // digitalWrite(4, HIGH); // For ESP32 Board
    pumpstate = 0;
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

void RTC_ADJUST() {
  if (SCH_update == 1) {
    myRtc.yearValue = TimeString[2].substring(5, 7).toInt();
    myRtc.monthValue = TimeString[2].substring(3, 5).toInt();
    myRtc.dateValue = TimeString[2].substring(1, 3).toInt();
    myRtc.minuteValue = TimeString[2].substring(9, 11).toInt();
    myRtc.secondValue = TimeString[2].substring(11, 13).toInt();
    if (TimeString[2].substring(7, 9).toInt() >= 12) {
      myRtc.periodValue = 1;
      myRtc.hourValue = TimeString[2].substring(7, 9).toInt() - 12;
    } else {
      myRtc.periodValue = 0;
      myRtc.hourValue = TimeString[2].substring(7, 9).toInt();
    }
    myRtc.dayValue = TimeString[2].substring(0, 1).toInt();
    myRtc.updateTime();
    SCH_update = 0;
  }
}

void inputs() {

  if (digitalRead(26) == LOW) {
#ifdef DEBUG
    Serial.println("==========digitalRead(26) == LOW======");
    Serial.println("Full");
#endif
    waterlevel = 1;
    waterlevelsp = 1;
  } else {
#ifdef DEBUG
    Serial.println("==========digitalRead(26) == HIGH======");
    Serial.println("Low");
#endif
    waterlevel = 0;
    waterlevelsp = 0;
  }
}

void fan_auto_control() {
  if (temper > sp && op == 0) {
    ioCon1.digitalWrite(4, HIGH);
    // digitalWrite(2, LOW); // For ESP32 Board
    fanstate = 1;
  } else if (temper <= sp && op == 0) {
    ioCon1.digitalWrite(4, LOW);
    // digitalWrite(2, HIGH); // For ESP32 Board

    fanstate = 0;
  }
}
void pump_auto_control() {

  if (waterlevel == 0) {
    ioCon1.digitalWrite(5, HIGH);
    // digitalWrite(4, LOW); // For ESP32 Board
    pumpstate = 1;
  } else if (waterlevel == 1) {
    ioCon1.digitalWrite(5, LOW);
    // digitalWrite(4, HIGH); // For ESP32 Board
    pumpstate = 0;
  }
}

void fan_manual_control() {
  if (fansw == 1) {
    ioCon1.digitalWrite(4, HIGH);
    // digitalWrite(2, LOW); // For ESP32 Board
    fanstate = 1;
  } else {
    ioCon1.digitalWrite(4, LOW);
    // digitalWrite(2, HIGH); // For ESP32 Board
    fanstate = 0;
  }
}


void pump_manual_control() {
  if (pumpsw == 1) {
    ioCon1.digitalWrite(5, HIGH);
    // digitalWrite(4, LOW); // For ESP32 Board
    pumpstate = 1;
  } else {
    ioCon1.digitalWrite(5, LOW);
    // digitalWrite(4, HIGH); // For ESP32 Board
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

void setup() {

  Wire.begin();

  ioCon1.digitalWrite(0, LOW);
  ioCon1.digitalWrite(1, LOW);
  ioCon1.digitalWrite(2, LOW);
  ioCon1.digitalWrite(3, LOW);
  ioCon1.digitalWrite(4, LOW);
  ioCon1.digitalWrite(5, LOW);
  ioCon1.digitalWrite(6, LOW);
  ioCon1.digitalWrite(7, LOW);

  ioCon1.portMode(ALLOUTPUT);

  pinMode(26, INPUT);
  pinMode(27, INPUT);

#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("\n\nbooting up device...\n\n");
  Serial.print("devicename: ");
  Serial.println(devicename);
#endif

  DEVICE_INIT();

  sensors.begin();
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();
  sensors.setWaitForConversion(true);
  myRtc.begin();

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

  /* For TIO444 Board*/

  // iocon  pin set to output
  // pinMode(2, OUTPUT);
  // pinMode(13, OUTPUT);

  // digitalWrite(2, HIGH); // For ESP32 Board
  // digitalWrite(13, HIGH); // For ESP32 Board
  /*-----------------*/

  /* For ESP32 Board*/
  // pinMode(2, OUTPUT);
  // pinMode(4, OUTPUT);

  // digitalWrite(2, HIGH);
  // digitalWrite(4, HIGH);
  /*-----------------*/


  // timer = timerBegin(0, 80, true);                    //Begin timer with 1 MHz frequency (80MHz/80)
  // timerAttachInterrupt(timer, &onTimer, true);        //Attach the interrupt to Timer1
  // unsigned int timerFactor = 1000000 / SamplingRate;  //Calculate the time interval between two readings, or more accurately, the number of cycles between two readings
  // timerAlarmWrite(timer, timerFactor, true);          //Initialize the timer
  // timerAlarmEnable(timer);

  // esp_task_wdt_init(WDT_TIMEOUT, true);  //enable panic so ESP32 restarts
  // esp_task_wdt_add(NULL);                //add current thread to WDT watch

  reconnect();
}


void loop() {

  if (millis() - wifi_setting_time >= 300000 && wifi_ap_mode == true) {  // Ap mode will off after 5 Minutes 300000 = 5 * 60 * 1000 seconds
    WiFi.softAPdisconnect();
    wifi_ap_mode = false;
  }

  if ((millis() - time1) > 8000) {
    op = 0;
  }
  if ((millis() - time3) > 2000) {
    tmp_req = true;
#ifdef DEBUG
    sensors.begin();
    int numberOfDevices = sensors.getDeviceCount();
    Serial.print("numberOfDevices = ");
    Serial.println(numberOfDevices);
    Serial.println("request temperatures-----------------------------------------");
    getTemperature();
#endif
    if (!client.connected()) {
      reconnect();
    }
    publishJson();
    time3 = millis();
  }
  loopFunction();
  client.loop();

  if (pumpstate == 1 || fanstate == 1) {
    coolmastersw = 2;
  }
  if (pumpstate == 0 && fanstate == 0) {
    coolmastersw = 0;
  }
}