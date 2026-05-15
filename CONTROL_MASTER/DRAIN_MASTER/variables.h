#ifndef VARIABLES_H
#define VARIABLES_H

#define WDT_TIMEOUT 60

String Name = "DrainMaster";
String devicename = "DM1-AAA001";

const char* mqtt_server = "a31qubhv0f0qec-ats.iot.eu-north-1.amazonaws.com";
const int mqtt_port = 8883;

String device_topic_s = "/test/" + devicename + "/1";  // subscribe topic //For MQTT
String device_topic_p = "/KRC/" + devicename;          // publish topic   // For MQTT

int tmp;
String ssid = "";
String password = "";
String deviceinfo = "";
bool deviceConnected = false;
//--------------------------------------------------------------
//DEVICE DEFAULTS SAVED HERE
//--------------------------------------------------------------
uint8_t newMACAddress[] = { 0x11, 0x18, 0xC2, 0x06, 0x5B, 0x6C };

String myIP = "";
String myID = "16092100075";
String substrin1 = myID.substring(2, 6);
String substrin2 = myID.substring(9, 11);
String hostname = Name + substrin1 + substrin2;
String devversion = "1.0";
int device_type = 7;
int wifi_channel = 0;

//========================================================================================================//
String sdata = "";
int connection_mode = 0;
int retry_count = 10;  //HTTP Begin retry count
int blink = 0;
hw_timer_t* timer = NULL;
volatile bool datainterrupt = false;
long SamplingRate = 1000;  //Read 1000 values in one second.
long milscount = 0;
long ledcounter = 0;
int ledretrycount = 0;
unsigned long dctime, time1, time3;
int wifi_status = 0;
String macaddress = "";
String lastcmd = "";

bool low = false;
bool mid = false;
bool high = false;
String alarm_string = "0";

bool controlStatus = true;

int8_t waterlevel = 0;

uint8_t pumpsw = 0;
uint8_t pumpstate = 0;
uint8_t waterlevelsp = 0;
uint8_t alarmstate = 0;
bool main_control = true, fan_control = true, pump_control = true;

unsigned long lastStableTime = 0;
const unsigned long STABLE_DELAY = 500;  // 500ms debounce

bool value_sent = false;
bool is_wifi_connected = false;

bool wifi_ap_mode = false;
unsigned long wifi_setting_time = 0;

#endif
