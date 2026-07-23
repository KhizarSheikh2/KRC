#define SENSOR_DISCONNECTED 888
#define SENSOR_NOT_SELECTED 999
#define MQTT_INTERVAL 2500
#define TEMP_OFFSET 0
#define SENSOR_READ_INTERVAL 1000

////////////////////Temperature Sensor DS18B20///////////////////
#define ONE_WIRE_BUS 19
constexpr uint8_t MAX_ONEWIRE_DEVICES = 10;
/////////////////////////////////////////////////////////////////

#define ALARM 32
#define RUN 33

// #define COMP_PIN 34
// #define OIL_SW 18
// #define HIGH_PRE 13
// #define LOW_PRE 35

String devicename = "AM3-AAA001";
String device_topic_s_m = "/test/" + devicename + "/#";  // subscribe topic
String device_topic_s_1 = "/test/" + devicename + "/1";  // subscribe topic
String device_topic_s_2 = "/test/" + devicename + "/2";  // subscribe topic
String device_topic_p = "/KRC/" + devicename;            // publish topic

String Name = "AM3";
String myID = "16092100096";
String substring1 = myID.substring(2, 6);
String substring2 = myID.substring(9, 11);
String hostname = Name + substring1 + substring2;

AsyncWebServer server(80);
Preferences preferences;
WiFiClientSecure espClient;
PubSubClient client(espClient);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

const char* mqtt_server = "a31qubhv0f0qec-ats.iot.eu-north-1.amazonaws.com";
const int mqtt_port = 8883;

DeviceAddress tempSensorAddresses[MAX_ONEWIRE_DEVICES];
int numberOfDevices = 0;
uint8_t inputs = 0;
DeviceAddress temp1Address;
DeviceAddress temp2Address;
DeviceAddress temp3Address;
DeviceAddress temp4Address;
DeviceAddress temp5Address;
bool temp1Assigned = false;
bool temp2Assigned = false;
bool temp3Assigned = false;
bool temp4Assigned = false;
bool temp5Assigned = false;
int16_t discharge_temp_offset, offset, offset1, suction_temp_offset, offset2, return_air_temp_offset, offset3, supply_temp_offset, offset4, oil_temp_offset, offset5;


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

// States of the Machine
#define STOPPED 0
#define RUNNING 1
#define AUTO_STOPPED 2
#define RESTARTING 4
#define RETURN_TEMP_ALERT 5
#define SUCTION_TEMP_ALERT 6
#define DISCHARGE_TEMP_ALERT 7
#define LPS_ALARM 12
#define HPS_ALARM 13
#define OPS_ALARM 14
#define COMP_FAIL_TO_RUN 15
#define TEMP_SENSOR_ERROR 16
#define MC_STUCK 17

int comp_Status = 0;

int SuctionTemp = 0;
float SuctionTempC = 0;
int SupplyTemp = 0;
float SupplyTempC = 0;
int ReturnTemp = 0;
float ReturnTempC = 0;
int dischargeTemp = 0;
float dischargeTempC = 0;
int OilTemp = 0;
float OilTempC = 0;

String suctionPressure;     // in psi
String dischargePressure;   // in psi
String suctionPressureB;    // in bar
String dischargePressureB;  // in bar


int Comp_Start_delay = 0;
int Comp_restart_delay = 0;
unsigned long Comp_restart_delay_ms = 0;

unsigned long COMP_START_TIME = 0;
unsigned long SWITCH_ALARM_HOLD_TIME = 0;
unsigned long COMP_TRIP_HOLD_TIME = 0;
unsigned long COMP_FB_STUCK_TIME = 0;
unsigned long currentMillis = 0;
unsigned long wait_time = 0;
unsigned long RESTART_TIME = 0;
unsigned long pmillis = 0;

// ALERTS
int suctionAlertSp, dischargeAlertSp, ReturnAlertSp, ReturnSp = 0;
int oilSp = 0;
int suctionPsiSp, dischargePsiSp = 0;
int gas_selection = 0;

String HPS = "LOW", LPS = "HIGH", OPS = "LOW";
int OPS_SW, HPS_SW, LPS_SW = 0;
bool comp_alarm, oil_alarm, dis_alarm, suction_alarm = false;
bool machineStopped = false;  // for MC Stuck
bool startup_flag, alarmFlag = false;
bool Machine_Shutdown, Switches_alarm = false;

bool AutoAlarmReset = false;
int autoReset = 0;
int ftoc = 1, psitobar = 1;
// Kept for mobile-app protocol compatibility. BECA has been removed, so
// the hardware always uses its assigned DS18B20 return sensor.
int returnTempSelection = 0;
int modesw = 0;
bool startSW, stopSW = false;
int startSWEnable, StartSW_App, resetSW = 0;

float r22_PSI[] = {
  24.0, 24.9, 25.7, 26.5, 27.4, 28.3, 29.2, 30.1, 31.0, 31.9,
  32.8, 33.8, 34.8, 35.8, 36.8, 37.8, 38.8, 39.9, 40.9, 42.0,
  43.1, 44.2, 45.3, 46.5, 47.6, 48.8, 50.0, 51.2, 52.4, 53.7,
  55.0, 56.2, 57.5, 58.8, 60.2, 61.5, 62.9, 64.3, 65.7, 67.1,
  68.6, 70.0, 71.5, 73.0, 74.5, 76.1, 77.6, 79.2, 80.8, 82.4,
  84.1, 85.7, 87.4, 89.1, 90.8, 92.6, 94.4, 96.1, 98.0, 99.8,
  101.6, 103.5, 105.4, 107.3, 109.3, 111.3, 113.2, 115.3, 117.3,
  119.4, 121.4, 123.6, 125.7, 127.8, 130.0, 132.2, 134.5, 136.7,
  139.0, 141.3, 143.6, 146.0, 148.4, 150.8, 153.2, 155.7, 158.2,
  160.7, 163.2, 165.8, 168.4, 171.0, 173.7, 176.4, 179.1, 181.8,
  184.6, 187.4, 190.2, 193.0, 195.9, 198.8, 201.8, 204.7, 207.7,
  210.8, 213.8, 216.9, 220.0, 223.2, 226.4, 229.6, 232.8, 236.1,
  239.4, 242.8, 246.1, 249.5, 253.0, 256.5, 260.0, 263.5, 267.1,
  270.7, 274.3, 278.0, 281.7, 285.4, 289.2, 293.0, 296.9, 300.8,
  304.7, 308.7, 312.6, 316.7, 320.7, 324.8, 329.0, 333.2, 337.4,
  341.6, 345.9, 350.3, 354.6, 359.0, 363.5, 368.0, 372.5, 377.1,
  381.7
};

float r32_PSI[] = {
  49.3,    // 0°F
  50.6,    // 1°F
  51.9,    // 2°F
  53.3,    // 3°F
  54.7,    // 4°F
  56.1,    // 5°F
  57.5,    // 6°F
  59.0,    // 7°F
  60.4,    // 8°F
  62.0,    // 9°F
  63.5,    // 10°F
  65.05,   // 11°F
  66.6,    // 12°F
  68.2,    // 13°F
  69.8,    // 14°F
  71.45,   // 15°F
  73.1,    // 16°F
  74.8,    // 17°F
  76.5,    // 18°F
  78.25,   // 19°F
  80.0,    // 20°F
  81.8,    // 21°F
  83.6,    // 22°F
  85.45,   // 23°F
  87.3,    // 24°F
  89.2,    // 25°F
  91.1,    // 26°F
  93.05,   // 27°F
  95.0,    // 28°F
  97.05,   // 29°F
  99.1,    // 30°F
  101.05,  // 31°F
  103.0,   // 32°F
  105.0,   // 33°F
  107.0,   // 34°F
  109.5,   // 35°F
  112.0,   // 36°F
  114.0,   // 37°F
  116.0,   // 38°F
  118.5,   // 39°F
  121.0,   // 40°F
  123.5,   // 41°F
  126.0,   // 42°F
  128.5,   // 43°F
  131.0,   // 44°F
  133.0,   // 45°F
  135.0,   // 46°F
  138.0,   // 47°F
  141.0,   // 48°F
  143.5,   // 49°F
  146.0,   // 50°F
  148.5,   // 51°F
  151.0,   // 52°F
  154.0,   // 53°F
  157.0,   // 54°F
  159.5,   // 55°F
  162.0,   // 56°F
  165.0,   // 57°F
  168.0,   // 58°F
  171.0,   // 59°F
  174.0,   // 60°F
  177.0,   // 61°F
  180.0,   // 62°F
  183.0,   // 63°F
  186.0,   // 64°F
  189.5,   // 65°F
  193.0,   // 66°F
  196.0,   // 67°F
  199.0,   // 68°F
  202.5,   // 69°F
  206.0,   // 70°F
  209.0,   // 71°F
  212.0,   // 72°F
  215.0,   // 73°F
  218.0,   // 74°F
  221.0,   // 75°F
  224.0,   // 76°F
  227.0,   // 77°F
  230.5,   // 78°F
  234.0,   // 79°F
  238.0,   // 80°F
  242.0,   // 81°F
  245.5,   // 82°F
  249.0,   // 83°F
  253.0,   // 84°F
  257.0,   // 85°F
  261.0,   // 86°F
  265.0,   // 87°F
  269.0,   // 88°F
  273.0,   // 89°F
  277.0,   // 90°F
  281.0,   // 91°F
  285.5,   // 92°F
  290.0,   // 93°F
  294.0,   // 94°F
  298.0,   // 95°F
  302.5,   // 96°F
  307.0,   // 97°F
  311.5,   // 98°F
  316.0,   // 99°F
  320.5,   // 100°F
  325.0,   // 101°F
  330.0,   // 102°F
  335.0,   // 103°F
  340.0,   // 104°F
  345.0,   // 105°F
  349.5,   // 106°F
  354.0,   // 107°F
  359.0,   // 108°F
  364.0,   // 109°F
  369.5,   // 110°F
  375.0,   // 111°F
  380.0,   // 112°F
  385.0,   // 113°F
  390.5,   // 114°F
  396.0,   // 115°F
  401.5,   // 116°F
  407.0,   // 117°F
  412.5,   // 118°F
  418.0,   // 119°F
  423.5,   // 120°F
  429.0,   // 121°F
  435.0,   // 122°F
  441.0,   // 123°F
  447.0,   // 124°F
  453.0,   // 125°F
  459.0,   // 126°F
  465.0,   // 127°F
  471.0,   // 128°F
  477.0,   // 129°F
  483.0,   // 130°F
  489.0,   // 131°F
  495.5,   // 132°F
  502.0,   // 133°F
  508.5,   // 134°F
  515.0,   // 135°F
  521.5,   // 136°F
  528.0,   // 137°F
  535.0,   // 138°F
  542.0,   // 139°F
  549.0,   // 140°F
  556.0,   // 141°F
  563.0,   // 142°F
  570.0,   // 143°F
  577.0,   // 144°F
  584.0,   // 145°F
  591.5,   // 146°F
  599.0,   // 147°F
  606.5,   // 148°F
  614.0,   // 149°F
  621.5    // 150°F
};

float r407_vapor_PSI[] = {  // Discharge
  19.4, 20.2, 21.0, 21.8, 22.6, 23.5, 24.3, 25.2, 26.1, 27.0, 27.9, 28.8, 29.8,
  30.7, 31.7, 32.7, 33.7, 34.7, 35.7, 36.8, 37.9, 39.0, 40.1, 41.2, 42.3, 43.5,
  44.7, 45.9, 47.1, 48.3, 49.6, 50.8, 52.1, 53.4, 54.8, 56.1, 57.5, 58.9, 60.3,
  61.7, 63.2, 64.6, 66.1, 67.6, 69.2, 70.7, 72.3, 73.9, 75.5, 77.2, 78.8, 80.5,
  82.2, 84.0, 85.7, 87.5, 89.3, 91.2, 93.0, 94.9, 96.8, 98.7, 100.7, 102.7, 104.7,
  106.7, 108.8, 110.9, 113.0, 115.1, 117.3, 119.5, 121.7, 124.0, 126.2, 128.6,
  130.9, 133.3, 135.6, 138.1, 140.5, 143.0, 145.5, 148.1, 150.6, 153.2, 155.9,
  158.5, 161.2, 163.9, 166.7, 169.5, 172.3, 175.2, 178.1, 181.0, 184.0, 186.9,
  190.0, 193.0, 196.1, 199.3, 202.4, 205.6, 208.9, 212.1, 215.4, 218.8, 222.2,
  225.6, 229.0, 232.5, 236.1, 239.7, 243.3, 246.9, 250.6, 254.3, 258.1, 261.9,
  265.8, 269.7, 273.6, 277.6, 281.6, 285.7, 289.8, 293.9, 298.1, 302.4, 306.7,
  311.0, 315.4, 319.8, 324.2, 328.8, 333.3, 337.9, 342.6, 347.3, 352.1, 356.9,
  361.7, 366.6, 371.6, 376.6, 381.7, 386.8, 392.0, 397.2, 402.5
};

float r410_vapor_PSI[] = {  // Discharge
  48.2, 49.5, 50.8, 52.2, 53.5, 54.9, 56.3, 57.8, 59.2, 60.7, 62.2, 63.7, 65.2,
  66.8, 68.4, 70.0, 71.6, 73.3, 74.9, 76.6, 78.4, 80.1, 81.9, 83.7, 85.5, 87.4,
  89.2, 91.1, 93.1, 95.0, 97.0, 99.0, 101.1, 103.1, 105.2, 107.3, 109.5, 111.7,
  113.9, 116.1, 118.4, 120.7, 123.0, 125.3, 127.7, 130.1, 132.6, 135.0, 137.5,
  140.1, 142.6, 145.2, 147.9, 150.5, 153.2, 156.0, 158.7, 161.5, 164.4, 167.2,
  170.1, 173.1, 176.0, 179.0, 182.1, 185.2, 188.3, 191.4, 194.6, 197.8, 201.1,
  204.4, 207.7, 211.1, 214.5, 217.9, 221.4, 224.9, 228.5, 232.1, 235.8, 239.4,
  243.2, 246.9, 250.7, 254.6, 258.5, 262.4, 266.4, 270.4, 274.5, 278.6, 282.7,
  286.9, 291.2, 295.5, 299.8, 304.2, 308.6, 313.1, 317.6, 322.1, 326.7, 331.4,
  336.1, 340.9, 345.7, 350.5, 355.4, 360.4, 365.4, 370.4, 375.5, 380.7, 385.9,
  391.2, 396.5, 401.9, 407.3, 412.8, 418.3, 423.9, 429.5, 435.2, 441.0, 446.8,
  452.7, 458.6, 464.6, 470.7, 476.8, 483.0, 489.2, 495.5, 501.9, 508.3, 514.8,
  521.4, 528.0, 534.7, 541.4, 548.3, 555.2, 562.1, 569.2, 576.3, 583.5, 590.7,
  598.1, 605.5, 613.0
};
