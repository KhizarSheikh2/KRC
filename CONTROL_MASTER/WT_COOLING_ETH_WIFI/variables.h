#ifndef VARIABLES_H
#define VARIABLES_H

#define WDT_TIMEOUT 60

String Name = "ControlMaster";
String devicename = "WTC-AAA001";

const char* mqtt_server = "a31qubhv0f0qec-ats.iot.eu-north-1.amazonaws.com";
const int mqtt_port = 8883;

String device_topic_s = "/test/" + devicename + "/1";  // subscribe topic //For MQTT
String device_topic_p = "/KRC/" + devicename;          // publish topic   // For MQTT

int day_app[7] = {6, 0, 1, 2, 3, 4, 5};
int for_schrout[8] = { 0, 3, 4, 5, 6, 7, 1, 2 };
RTC_DATA_ATTR int counter = 0;
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
// String devversion = "1.0";
// int device_type = 7;
int wifi_channel = 0;

//============================================================================//

uint8_t pos, pos1, pos2, Pos, Pos1, comma_index[25], Time_data[32], SchScan[8], formate, values[10], scfm[4], rcfm[4];
// String SchString, TimeString[3], STR_buffer;
String deviceTime;
// bool SCH_update = 0;
// bool supcfm_update = 0;
// bool retcfm_update = 0;
int SP = 0;

//===========================================================================//

// String sdata = "";
// int connection_mode = 0;
// int retry_count = 10;  //HTTP Begin retry count
int blink = 0;
hw_timer_t* timer = NULL;
volatile bool datainterrupt = false;
long SamplingRate = 1000;  //Read 1000 values in one second.
long milscount = 0;
long ledcounter = 0;
int ledretrycount = 0;
unsigned long time1, time3;
int wifi_status = 0;
String macaddress = "";

//==============================================================================

byte i;
int count = 0;
byte data[12];
byte addr[8];
int temper;
int tprev = 0;
bool op = 0, tmp_req = 0;

const byte ROWS = 1;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'2', '1', '3', '4'}
};

byte rowPins[ROWS] = {27};
byte colPins[COLS] = {26, 25, 33, 32}; 

const int MIN_TEMP = 10;
const int MAX_TEMP = 50;

String timesch = "hoursch=24|daysch=7";  // fan schedule config
uint8_t timeschen = 0;               // 0 = off, 1 = enabled

int days_in_num[7] = {0};          // Array for selected days
int hours_num[24] = {0};           // Array for selected hours (0-23)
bool timeMatched = false;          // Flag for time match
int tmatched = 0;

static unsigned long lastRepeat = 0;

unsigned long lastScheduleCheck = 0;
int lastCheckedHour = -1;
int lastCheckedMinute = -1;
String lastParsedSchedule = "";

const unsigned long repeatRate = 150;
static unsigned long lastInputTime = 0;

unsigned long keyPressStart = 0;  // for long press detection
bool keyHeld = false;

unsigned long modePressStart = 0;
bool modePressed = false;
const unsigned long LONG_PRESS_TIME = 2000; // 2 seconds

String temp1 = "0";
String temp1sp = "0";

int8_t waterlevel = 0;

uint8_t watertankcoolsw = 0;
uint8_t sp = 25;
uint8_t fansw = 0;
uint8_t fanstate = 0;
uint8_t fanspeed = 0;
uint8_t pumpsw = 0;
uint8_t pumpstate = 0;
uint8_t waterlevelsp = 0;
bool fan_control = true, pump_control = true;
bool main_control = true;
uint8_t prev_fanstate = 255;
uint8_t prev_pumpstate = 255;

bool is_wifi_connected = false;

bool wifi_ap_mode = false;
unsigned long wifi_setting_time = 0;

bool eth_connected = false;  // Ethernet link status
EthernetClient ethClient;    // For Ethernet-based MQTT
SSLClient sslEthClient(ethClient, TAs, (size_t)TAs_NUM, SSL_ENTROPY_PIN);  // SSL over Ethernet
PubSubClient ethMqttClient(sslEthClient);  // MQTT client for Ethernet

// Keep existing WiFi MQTT client
WiFiClientSecure wifiClient;
PubSubClient wifiMqttClient(wifiClient);
PubSubClient* activeMqttClient = &wifiMqttClient;  // Pointer to active client (starts with WiFi)

EthernetUDP ntpUDP;

#endif