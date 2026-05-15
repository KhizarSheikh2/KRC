#ifndef VARIABLES_H
#define VARIABLES_H

#define WDT_TIMEOUT 60

String Name = "ControlMaster";
String devicename = "CM1-AAA014";

const char* mqtt_server = "a31qubhv0f0qec-ats.iot.eu-north-1.amazonaws.com";
const int mqtt_port = 8883;

String device_topic_s = "/test/" + devicename + "/1";  // subscribe topic //For MQTT
String device_topic_p = "/KRC/" + devicename;          // publish topic   // For MQTT

int day_app[7] = { 5, 6, 7, 1, 2, 3, 4 };
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
String devversion = "1.0";
int device_type = 7;
int wifi_channel = 0;

//========================================================================================================//
uint8_t pos, pos1, pos2, Pos, Pos1, comma_index[25], Time_data[32], SchScan[8], formate, values[10], scfm[4], rcfm[4];
String SchString, TimeString[3], STR_buffer, deviceTime;
bool SCH_update = 0;
bool supcfm_update = 0;
bool retcfm_update = 0;
int SP = 0;
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
int wifi_status = 0;
String macaddress = "";
String lastcmd = "";

String timenow = "0101200000";
String timesch = "hoursch=24|daysch=7";
uint8_t timeschen = 0;
String alarm_string = "0";

String dxtemp = "0";
String dxtempsp = "0";
int dxsw = 0;
String tempsp1 = "0";
int comprsw = 0;
int dxstate = 0;

uint8_t seasonsw = 0;
String temp1 = "0";
String temp2 = "0";
String temp1sp = "0";
String temp2sp = "0";
String boilersp = "0";
String coolersp = "0";
String comfortersp = "0";

bool controlStatus = true;

int8_t waterlevel = 0;

uint8_t coolmastersw = 0;
uint8_t sp = 0;
uint8_t fansw = 0;
uint8_t fanstate = 0;
uint8_t fanspeed = 0;
uint8_t pumpsw = 0;
uint8_t pumpstate = 0;
uint8_t waterlevelsp = 0;
bool main_control = true, fan_control = true, pump_control = true;

bool value_sent = false;
bool is_wifi_connected = false;

bool wifi_ap_mode = false;
unsigned long wifi_setting_time = 0;

#endif
