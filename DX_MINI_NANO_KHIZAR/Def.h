bool dischargeTempAlarm = false;
bool returnTempAlarm = false;
////////////////////Scheduler///////////////////

bool schedulerEnabled = false;
bool PowerState = true;

#define SENSOR_DISCONNECTED 888
#define SENSOR_NOT_SELECTED 999
#define MQTT_INTERVAL 2500
#define SENSOR_READ_INTERVAL 1000

////////////////////Temperature Sensor DS18B20///////////////////
#define DS18B20_PIN 19
/////////////////////////////////////////////////////////////////

// Relay Pins
#define EVAP_COOL_SYSTEM 18  // RELAY 1
#define AUTO_WASH_SYSTEM 5   // RELAY 2
#define UNIT_COMMAND 17      // RELAY 3

// Buzzer Pin
#define BUZZER_PIN 16  // Choose an available GPIO pin

#define BUZZER_FREQ 2000    // Frequency in Hz (2 kHz typical)
#define BUZZER_DURATION 50  // Duration in ms

AsyncWebServer server(80);
Preferences preferences;
WiFiClientSecure espClient;
PubSubClient client(espClient);
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

String devicename = "DXM-AAA001";

String device_topic_s_m = "/test/" + devicename + "/#";  // subscribe topic
String device_topic_s_1 = "/test/" + devicename + "/1";  // subscribe topic
String device_topic_s_2 = "/test/" + devicename + "/2";  // subscribe topic
String device_topic_s_3 = "/test/" + devicename + "/3";  // subscribe topic
String device_topic_p = "/KRC/" + devicename;            // publish topic   // For MQTT

String Name = "DX Master Mini";
String myID = "16092100096";
String substring1 = myID.substring(2, 6);
String substring2 = myID.substring(9, 11);
String hostname = Name + substring1 + substring2;

const char* mqtt_server = "a31qubhv0f0qec-ats.iot.eu-north-1.amazonaws.com";
const int mqtt_port = 8883;
#define MAX_SENSORS 3

DeviceAddress tempSensorAddresses[MAX_SENSORS];
int numberOfDevices = 0;
DeviceAddress temp1Address;
DeviceAddress temp2Address;
DeviceAddress temp3Address;
bool temp1Assigned = false;
bool temp2Assigned = false;
bool temp3Assigned = false;
int16_t discharge_temp_offset, offset, offset1, suction_temp_offset, offset2, return_air_temp_offset, offset3, supply_temp_offset, offset4;

String ssid = "";
String password = "";
String macaddress = "";
String myIP = "";

bool message_received = false;
bool message_received_config = false;
bool message_received_pressure_config = false;
StaticJsonDocument<1024> received_doc;
bool wifi_ap_mode = false;
unsigned long wifi_setting_time = 0;
int wifi_channel = 0;
bool is_wifi_connected = false;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 18000;
const int daylightOffset_sec = 0;

unsigned long wait_update_time, wait_time, time3 = 0;
unsigned long pmillis = 0;

uint8_t weekDay;
uint8_t current_hour;
bool days_in_num[7];
bool hours_num[24];
// Time and Alarm
String timenow = "0101200000";
String timesch = "hoursch=24|daysch=7";
int timeschen = 0;
int tmatched = 0;
// bool timeMatched = false;


int SYSTEM_STATUS = 0;
bool EVAP_COOL_SYSTEM_STATUS = false;  // RELAY 1
bool AUTO_WASH_SYSTEM_STATUS = false;  // RELAY 2
bool UNIT_COMMAND_STATUS = false;      // RELAY 3

float SupplyTemp = 0;
float ReturnTemp = 0;
float dischargeTemp = 0;

// ALERTS
int dischargeSp, ReturnSp = 0;
int EVAP_COOL_SYSTEM_SWITCH = 0;
int AUTO_WASH_SYSTEM_SWITCH = 0;
int UNIT_COMMAND_SWITCH = 1;
int FAULT_SWITCH, MODE_SWITCH = 0;
bool R_A_TEMP_Alarm = false;
