#ifndef VARIABLES_H
#define VARIABLES_H

#define SENSOR_DISCONNECTED 888.0f
#define SENSOR_NOT_SELECTED 999.0f
#define MQTT_INTERVAL 3000UL
#define MQTT_BUFFER_SIZE 6144
#define SENSOR_READ_INTERVAL 1000UL
#define TEMP_CONVERSION_TIME_MS 750UL

// =====================================================
// SENSOR COUNTS
// =====================================================
#define INDOOR_SENSOR_COUNT 6
#define OUTDOOR_SENSOR_COUNT 6
#define TOTAL_SENSOR_COUNT 12
#define MAX_SENSORS INDOOR_SENSOR_COUNT

// =====================================================
// RS485 TWO-NODE BUS CONFIGURATION
// AM6 Indoor  = MASTER / main controller (0x01)
// AM6 Outdoor = SLAVE (0x02)
// =====================================================
#define RS485_BAUD                         19200UL
#define RS485_RX_PIN                       16
#define RS485_TX_PIN                       17
#define RS485_DE_RE_PIN                    32
#define RS485_FRAME_TIMEOUT_MS             50UL
#define RS485_RESPONSE_TIMEOUT_MS          250UL
#define RS485_OUTDOOR_POLL_INTERVAL_MS     500UL
#define RS485_OUTDOOR_CONFIG_INTERVAL_MS   5000UL
#define RS485_MAX_PAYLOAD                  96

#define RS485_ADDR_INDOOR                  0x01
#define RS485_ADDR_OUTDOOR                 0x02
#define RS485_ADDR_BROADCAST               0xFF

// =====================================================
// INDOOR HARDWARE
// =====================================================
#define DS18B20_PIN 33

#define RELAY_LOW_PIN      25
#define RELAY_MEDIUM_PIN   26
#define RELAY_HIGH_PIN     27
#define RELAY_HEAT_PIN     14
#define RELAY_ACTIVE_LOW false

// Compile-time protection against accidental GPIO reuse on the Indoor board.
#if (DS18B20_PIN == RELAY_LOW_PIN) || (DS18B20_PIN == RELAY_MEDIUM_PIN) || \
    (DS18B20_PIN == RELAY_HIGH_PIN) || (DS18B20_PIN == RELAY_HEAT_PIN) || \
    (DS18B20_PIN == RS485_RX_PIN) || (DS18B20_PIN == RS485_TX_PIN) || \
    (DS18B20_PIN == RS485_DE_RE_PIN)
#error "AM6 Indoor GPIO conflict: DS18B20 pin overlaps relay/RS485 pin"
#endif

#if (RELAY_HEAT_PIN == RS485_RX_PIN) || (RELAY_HEAT_PIN == RS485_TX_PIN) || \
    (RELAY_HEAT_PIN == RS485_DE_RE_PIN)
#error "AM6 Indoor GPIO conflict: HEAT relay overlaps RS485 pin"
#endif

enum FanSpeed : uint8_t {
  FAN_LOW = 1,
  FAN_MEDIUM = 2,
  FAN_HIGH = 3
};

enum HVACMode : uint8_t {
  MODE_COOL = 0,
  MODE_HEAT = 1
};

// Outdoor status codes published to the application.
enum OutdoorStatusCode : uint8_t {
  OUT_STATUS_STOPPED = 0,
  OUT_STATUS_RUNNING = 1,
  OUT_STATUS_HIGH_PSI = 2,
  OUT_STATUS_LOW_PSI = 3,
  OUT_STATUS_POWER_FAULT = 4,
  OUT_STATUS_OVERLOAD_TRIPPED = 5
};

// =====================================================
// AUTHORITATIVE AM6 SYSTEM STATE
// =====================================================
// system_power is the ONLY authoritative complete-system power variable.
// 0 = OFF, 1 = ON.
int system_power = 0;

// Section enable switches from the mobile application.
// These do not replace or duplicate system_power.
int indoor_sw = 1;
int outdoor_sw = 1;

// Internal fan mapping: 1=LOW, 2=MEDIUM, 3=HIGH.
// MQTT fanSw mapping is translated as 0=LOW, 1=MID, 2=HIGH.
int fan_speed = FAN_LOW;

// HVAC mode: 0=COOL, 1=HEAT.
int hvac_mode = MODE_COOL;

// User setpoint in degrees Celsius. Stored in Preferences.
float setPoint = 22.0f;

// Force an immediate Outdoor RS485 synchronization after master state changes.
bool rs485_urgent_outdoor_sync = true;

AsyncWebServer server(80);
Preferences preferences;
WiFiClientSecure espClient;
PubSubClient client(espClient);
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

String devicename = "AM6-AAA001";

// =====================================================
// MQTT TOPICS - EXACT APPLICATION CONTRACT
// =====================================================
String device_topic_s_m = "/test/" + devicename + "/#";
String device_topic_s_control = "/test/" + devicename + "/2";
String device_topic_s_outdoor_sensor = "/test/" + devicename + "/3";
String device_topic_s_indoor_sensor = "/test/" + devicename + "/4";

String device_topic_p_main = "/KRC/" + devicename;
// Canonical topic uses the device name exactly (hyphen).
String device_topic_p_indoor_temps = "/KRC/" + devicename + "/IndoorTemps";
String device_topic_p_outdoor_temps = "/KRC/" + devicename + "/OutdoorTemps";
String device_topic_p_indoor_sensor = "/KRC/" + devicename + "/indoorSensor";
String device_topic_p_outdoor_sensor = "/KRC/" + devicename + "/outdoorSensor";
String device_topic_p_io = "/KRC/" + devicename + "/AM-Input-Output";

String Name = "AM6";
String myID = "16092100096";
String substring1 = myID.substring(2, 6);
String substring2 = myID.substring(9, 11);
String hostname = Name + substring1 + substring2;

const char* mqtt_server = "a31qubhv0f0qec-ats.iot.eu-north-1.amazonaws.com";
const int mqtt_port = 8883;

// =====================================================
// INDOOR SENSOR DISCOVERY / ROM-BASED CONFIGURATION
// =====================================================
DeviceAddress tempSensorAddresses[INDOOR_SENSOR_COUNT] = {};
bool tempSensorPresent[INDOOR_SENSOR_COUNT] = {false, false, false, false, false, false};
uint8_t tempSensorRole[INDOOR_SENSOR_COUNT] = {0, 0, 0, 0, 0, 0};
float tempSensorOffset[INDOOR_SENSOR_COUNT] = {0, 0, 0, 0, 0, 0};
float indoorSensorRawTempC[INDOOR_SENSOR_COUNT] = {
  SENSOR_DISCONNECTED, SENSOR_DISCONNECTED, SENSOR_DISCONNECTED,
  SENSOR_DISCONNECTED, SENSOR_DISCONNECTED, SENSOR_DISCONNECTED
};
int numberOfDevices = 0;

// =====================================================
// INDOOR ROLE TEMPERATURES
// Role 1 = Suction, 2 = Discharge, 3 = Supply,
// 4 = Return, 5 = Oil, 6 = Other.
// =====================================================
int SuctionTemp = 888;
float SuctionTempC = SENSOR_DISCONNECTED;
int SupplyTemp = 888;
float SupplyTempC = SENSOR_DISCONNECTED;
int ReturnTemp = 888;
float ReturnTempC = SENSOR_DISCONNECTED;
int dischargeTemp = 888;
float dischargeTempC = SENSOR_DISCONNECTED;
int OilTemp = 888;
float OilTempC = SENSOR_DISCONNECTED;
int OtherTemp = 888;
float OtherTempC = SENSOR_DISCONNECTED;

// =====================================================
// OUTDOOR DATA MIRRORED INTO INDOOR MASTER
// =====================================================
DeviceAddress outdoorSensorAddresses[OUTDOOR_SENSOR_COUNT] = {};
bool outdoorSensorPresent[OUTDOOR_SENSOR_COUNT] = {false, false, false, false, false, false};
uint8_t outdoorSensorRole[OUTDOOR_SENSOR_COUNT] = {0, 0, 0, 0, 0, 0};
float outdoorSensorOffset[OUTDOOR_SENSOR_COUNT] = {0, 0, 0, 0, 0, 0};
float outdoorSensorRawTempC[OUTDOOR_SENSOR_COUNT] = {
  SENSOR_DISCONNECTED, SENSOR_DISCONNECTED, SENSOR_DISCONNECTED,
  SENSOR_DISCONNECTED, SENSOR_DISCONNECTED, SENSOR_DISCONNECTED
};
float outdoorSensorPhysicalTempC[OUTDOOR_SENSOR_COUNT] = {
  SENSOR_DISCONNECTED, SENSOR_DISCONNECTED, SENSOR_DISCONNECTED,
  SENSOR_DISCONNECTED, SENSOR_DISCONNECTED, SENSOR_DISCONNECTED
};
float outdoorRoleTempC[OUTDOOR_SENSOR_COUNT] = {
  SENSOR_DISCONNECTED, SENSOR_DISCONNECTED, SENSOR_DISCONNECTED,
  SENSOR_DISCONNECTED, SENSOR_DISCONNECTED, SENSOR_DISCONNECTED
};
bool outdoorRoleTempValid[OUTDOOR_SENSOR_COUNT] = {false, false, false, false, false, false};

bool outdoorR1 = false;
bool outdoorR2 = false;
bool outdoorR3 = false;
bool outdoorR4 = false;
bool outdoorSwitchPcfHealthy = false;
bool outdoorRelayPcfHealthy = false;
bool outdoorPcfHealthy = false;
int outdoorPowerEcho = 0;
int outdoorEnableEcho = 1;
int outdoorEffectivePower = 0;
int outdoorStatusCode = OUT_STATUS_STOPPED;   // Circuit A: SW1..SW4 -> R1/R3 -> statusout
int outdoorStatusCodeB = OUT_STATUS_STOPPED;  // Circuit B: SW5..SW8 -> R2/R4 -> statusoutB
bool outdoorSwitchState[8] = {false, false, false, false, false, false, false, false};

bool outdoorConfigSnapshotValid = false;
uint8_t outdoorSensorDetectedCount = 0;
// True after an app-originated Outdoor config change until a fresh B2 snapshot
// confirms the authoritative role/offset state. Prevents stale app reverts.
bool outdoorConfigAwaitingFreshSnapshot = false;

struct OutdoorConfigCommand {
  uint8_t address[8];
  uint8_t role;
  float offset;
  uint8_t retries;
};
OutdoorConfigCommand outdoorConfigQueue[OUTDOOR_SENSOR_COUNT] = {};
uint8_t outdoorConfigQueueCount = 0;

// =====================================================
// GENERAL / NETWORK STATE
// =====================================================
String ssid = "";
String password = "";
String macaddress = "";
String myIP = "";

bool message_received = false;
bool message_received_config = false;
StaticJsonDocument<4096> received_doc;
bool wifi_ap_mode = false;
unsigned long wifi_setting_time = 0;
int wifi_channel = 0;
bool is_wifi_connected = false;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 18000;
const int daylightOffset_sec = 0;

unsigned long pmillis = 0;
unsigned long wait_time = 0;
bool temperature_conversion_pending = false;
unsigned long temperature_conversion_started_ms = 0;

// RS485 communication health as seen by Indoor master.
bool rs485_outdoor_online = false;
unsigned long rs485_outdoor_last_seen_ms = 0;

bool rs485_outdoor_config_snapshot_requested = true;
unsigned long rs485_outdoor_config_last_request_ms = 0;

// Set when an accepted command/status change should be published immediately.
bool mqtt_publish_requested = false;

#endif
