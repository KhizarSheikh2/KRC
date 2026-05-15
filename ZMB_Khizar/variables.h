// const char* mqtt_server = "192.168.18.112";

const char* mqtt_server = "a31qubhv0f0qec-ats.iot.eu-north-1.amazonaws.com";
const int mqtt_port = 8883;

String Name = "ZoneMaster";
String myID = "00000000000";
String substrin1 = myID.substring(2, 6);
String substrin2 = myID.substring(9, 11);
String hostname = Name + substrin1 + substrin2;
String devicename = "ZMB-AAA015";
String savedatacommand = "";
int device_type = 3;
String devversion = "1.0";
String macaddress = "";

String device_topic_s = "/test/" + devicename + "/1";  // subscribe topic
String device_topic_p = "/KRC/" + devicename;          // publish topic

bool message_received = false;

#define I2C_SDA 21      // for nano 18 / for esp 21
#define I2C_SCL 22      // for nano 19 / for esp 22
#define ONE_WIRE_BUS 19  // temp sensor

const int RECV_PIN = 12;
bool remoteEnabled = false;

const int Buzzer_Pin = 23;

long lastMsg = 0;
char msg[50];
int value = 0;

bool moveServoFlag = false;
int16_t targetPosition = 0;
int16_t servo_open_pos = 135;
int16_t servo_close_pos = 40;
int16_t last_pos_servo;
int8_t servo_delay = 40;
// bool beca_status = false;
// bool prev_beca_status = 1;
bool setpoint_flag = true;

unsigned long cfm_duration = 0;
unsigned long cfm_button_time = 0;
unsigned long previousMillis = 0;
unsigned long previousMillis_1 = 0;
unsigned long previousMillis_2 = 0;
unsigned long previousMillis_3 = 0;
unsigned long previousMillis_4 = 0;
unsigned long wait_update_time = 0;
unsigned long wait_time = 0;
bool show_time = false;
const uint16_t interval = 2000;
bool power = false;
bool power_saved = 1;
uint8_t season = 0;
int16_t temp = 24;
int16_t setpointt = 24;
int16_t setpointt_saved = 24;
uint16_t last_setpoint = 0;
// int8_t cfmbutton = 0;
// int8_t lst_cfmbutton = 0;
int8_t bp = 1;
// int16_t cfm;
bool cfm_flag = false;
// uint16_t save_setpointt = 0;
int16_t CFM_max = servo_open_pos;
int16_t new_cfm = servo_open_pos;
int16_t CFM_min = servo_close_pos;
int16_t minval, maxval;
// bool update_from_pref = true;


// const int16_t SERVO_CLOSE_ORIGIN = 40;
// const int16_t SERVO_OPEN_ORIGIN  = 135;

String dataarray = "";

uint8_t seasonsw = 0;
int8_t dmptemp = 24;
int8_t dmptempsp = 24;
int8_t prevdmptempsp = 24;
uint8_t dampertsw = 0;
uint8_t prevdampertsw = 1;
uint8_t start_value = 0;
uint8_t end_value = 10;
uint8_t end_value_app = 10;

String supcfm = String(start_value) + "-" + String(end_value);
String retcfm = "0-50";
String timesch = "hoursch=24|daysch=7";
bool timeschen = 0;
bool dampstate = 0;
String flapstate = "";
String alarm_string = "0";
String timenow = "010120120000";
String myIP = "";
long packet_sequence = 0;
String lastcmd;

// String keys[] = { "seasonsw", "dmptemp", "dmptempsp", "dampertsw", "supcfm", "retcfm", "hoursch", "timeschen", "dampstate", "alarm", "timenow", "ip_address", "packet_id" };

const char* ntpServer = "time.google.com";
const long gmtOffset_sec = 18000;
const int daylightOffset_sec = 0;

uint8_t mm = 1;      //month
uint8_t dd = 1;      //date
uint16_t yy = 2020;  //year
uint8_t hh = 12;     //hour
uint8_t mins = 0;    //minute
uint8_t ss = 10;     //second
String month = "Jan";
uint8_t weekDay;
uint8_t current_hour;
bool days_in_num[7];
bool hours_num[24];


enum displaystate {
  datee,
  timee
};
displaystate display_state = datee;


// Keypad setup (1x4 membrane)
const byte ROWS = 1;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'2', '1', '3', '4'}
};
byte rowPins[ROWS] = {27};
byte colPins[COLS] = {26, 25, 33, 32};


// Keypad variables
bool modePressed = false;
unsigned long modePressStart = 0;
unsigned long lastRepeat = 0;
const unsigned long repeatDelay = 200;  // Repeat delay for held keys
char lastHeldKey = 0;  // tracks which key is currently held down


float temperature;  // From DS18B20
// float Setpoint;     // User-set value
// bool season;        // false = Summer, true = Winter
int CFM;

unsigned long setpointStartTime = 0;
const unsigned long setpointDuration = 5000;  // 5 seconds

String data = "";
bool is_wifi_connected = 0;

bool wifi_ap_mode = false;
unsigned long wifi_setting_time = 0;

unsigned long cfm_duration_app;
bool cfm_settings_app;
