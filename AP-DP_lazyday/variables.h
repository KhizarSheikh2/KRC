// ---- MQTT ----
const char* mqtt_server = "a31qubhv0f0qec-ats.iot.eu-north-1.amazonaws.com";
const int mqtt_port = 8883;

// ---- NTP ----
const char* ntpServer = "time.google.com";      // final version kept
const long gmtOffset_sec = 18000;
const int daylightOffset_sec = 0;

unsigned long lastBecaWifiTime = 0;   // Stores the timestamp of the last message
const unsigned long WIFI_TIMEOUT = 5000; // 5-second thermostat communication timeout

// ---- Device Info ----
String devicename = "ZMB-AAA050";
String device_topic_s_m = "/test/" + devicename + "/#";
String device_topic_s_1 = "/test/" + devicename + "/1";
String device_topic_p   = "/KRC/" + devicename;

String Name = "ZoneMaster";
String myID = "16092100096";
String substring1 = myID.substring(2, 6);
String substring2 = myID.substring(9, 11);
String hostname = Name + substring1 + substring2;

bool message_received = false;

// ---- WiFi ----
String ssid = "";
String password = "";
String macaddress = "";
String myIP = "";
bool is_wifi_connected = false;
bool wifi_ap_mode = false;
unsigned long wifi_setting_time = 0;
int wifi_channel = 0;

// ---- JSON ----
StaticJsonDocument<1024> received_doc;

// ---- IO ----
const int Buzzer_Pin = 19;
const int ServoPin = 27;
const int RECV_PIN = 22;
const int POT_PIN = 34;

// ---- Servo ----
bool moveServoFlag = false;
int16_t targetPosition = 0;
int16_t servo_open_pos = 135;
int16_t servo_close_pos = 40;
int16_t last_pos_servo;
int8_t servo_delay = 25;

// ---- BECA / CONTROL ----
bool beca_status = false;
bool prev_beca_status = 1;
bool setpoint_flag = true;
bool remoteEnabled = false;
bool irReceiverStarted = false;

unsigned long cfm_duration = 0;
unsigned long cfm_button_time = 0;
unsigned long previousMillis_1 = 0;
unsigned long previousMillis_2 = 0;
unsigned long previousMillis_3 = 0;
unsigned long previousMillis_4 = 0;
unsigned long wait_update_time = 0;
unsigned long wait_time = 0;

const uint16_t interval = 2000;
bool beca_power = false;
bool beca_power_saved = 1;
uint8_t beca_mode = 0;

int16_t temp_by_beca = 24;
int16_t setpointt = 24;
int16_t setpointt_saved = 24;
uint16_t last_setpoint = 0;

int8_t cfmbutton = 0;
int8_t lst_cfmbutton = 0;
int8_t bp = 1;

int16_t cfm;
bool cfm_flag = false;
uint16_t save_setpointt = 0;

int16_t CFM_max = servo_open_pos;
int16_t new_cfm = servo_open_pos;
int16_t CFM_min = servo_close_pos;
int16_t minval, maxval;

String dataarray = "";

// ---- AP Mode ----
int connected, ping = 0;

uint8_t seasonsw = 0;
int8_t dmptemp = 24;
int8_t dmptempsp = 24;
int8_t prevdmptempsp = 24;

uint8_t dampertsw = 0;
uint8_t prevdampertsw = 1;

uint8_t start_value;
uint8_t end_value = 10;
uint8_t end_value_app = 10;

String supcfm = String(start_value) + "-" + String(end_value);
String retcfm = "0-50";
String timesch = "hoursch=24|daysch=7";
bool timeschen = 0;
bool dampstate = 0;

String timenow = "010120120000";
String lastcmd;

// ---- Time ----
uint8_t mm = 1;
uint8_t dd = 1;
uint16_t yy = 2020;
uint8_t hh = 12;
uint8_t mins = 0;
uint8_t ss = 10;

String month = "Jan";
uint8_t weekDay;
uint8_t current_hour;

bool days_in_num[7];
bool hours_num[24];

String data = "";

// ---- APP CFM ----
unsigned long cfm_duration_app;
bool cfm_settings_app;
