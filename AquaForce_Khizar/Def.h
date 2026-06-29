#define SENSOR_DISCONNECTED 888
#define SENSOR_NOT_SELECTED 999
#define MQTT_INTERVAL 3500
#define SENSOR_READ_INTERVAL 1000
#define VFD_FREQ_FACTOR 200

// ---- Master/Slave (Lead-Lag) ----
#define RESTART_LOCKOUT_MS 600000UL  // 600 sec (10 min) restart delay after any compressor STOP, per spec
#define ASSIST_SETTLE_MS 3000UL      // settle time after de-loading MASTER to 50% before starting SLAVE assist

// ---- Loader/Unloader (Capacity Control) ----
#define LOAD_DEADBAND 1  // hysteresis (°/units of ReturnSp) around target before switching LOADING/UNLOADING

// States of the Machine
// #define STOPPED 0
// #define STARTING 1
// #define RUNNING 2
// #define STOPPING 3
// #define TRIPPED 4
// #define AUTO_STOPPED 5
// #define DISABLED 6
// #define SUCTION_TEMP_ALERT 7
// #define DISCHARGE_TEMP_ALERT 8
// #define SPRAY_TEMP_ALERT 12
// #define SUCTION_PSI_ALERT 9
// #define DISCHARGE_PSI_ALERT 10
// #define OIL_PSI_ALERT 13
// #define COMP_AMP_HIGH 11

////////////////////Temperature Sensor DS18B20///////////////////
#define DS18B20_PIN 25

////////////////////Temperature Sensor DS18B20///////////////////
#define DS18B20_PIN_B 26

Preferences preferences;
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);
OneWire oneWire2(DS18B20_PIN_B);
DallasTemperature sensorsB(&oneWire2);

String devicename = "AQF-AAA001";

String device_topic_s_m = "/test/" + devicename + "/#";  // subscribe topic
String device_topic_s_1 = "/test/" + devicename + "/1";  // subscribe topic
String device_topic_s_2 = "/test/" + devicename + "/2";  // subscribe topic
String device_topic_s_3 = "/test/" + devicename + "/3";  // subscribe topic
String device_topic_s_4 = "/test/" + devicename + "/4";  // subscribe topic
String device_topic_s_5 = "/test/" + devicename + "/5";  // subscribe topic
String device_topic_p = "/KRC/" + devicename;            // publish topic   // For MQTT

#define MAX_SENSORS 6

DeviceAddress tempSensorAddresses[MAX_SENSORS];
DeviceAddress tempSensorAddressesB[4];
int numberOfDevices = 0;
int numberOfDevicesB = 0;

DeviceAddress temp1Address;   // Suction A
DeviceAddress temp2Address;   // Discharge A
DeviceAddress temp3Address;   // Sub cool A
DeviceAddress temp4Address;   // Spray A
DeviceAddress temp5Address;   // Supply 5
DeviceAddress temp6Address;   // Return 6
DeviceAddress temp7Address;   // Suction B
DeviceAddress temp8Address;   // Discharge B
DeviceAddress temp9Address;   // Sub cool B
DeviceAddress temp10Address;  // Spray B

bool temp1Assigned = false;
bool temp2Assigned = false;
bool temp3Assigned = false;
bool temp4Assigned = false;
bool temp5Assigned = false;
bool temp6Assigned = false;
bool temp7Assigned = false;
bool temp8Assigned = false;
bool temp9Assigned = false;
bool temp10Assigned = false;


int16_t subcool_temp_offset, spray_temp_offset, discharge_temp_offset, suction_temp_offset, return_air_temp_offset, supply_temp_offset;
int16_t offset, offset1, offset2, offset3, offset4, offset5, offset6;

int16_t subcool_temp_offset2, spray_temp_offset2, discharge_temp_offset2, suction_temp_offset2;
int16_t offsetB, offset7, offset8, offset9, offset10;

int SYSTEM_STATUS = 0;

// From BECA
int16_t Setpoint, Power_state = 0;
float room_temp;
bool beca_Status, prev_status = false;

// For Both Compressors
float SupplyTemp, ReturnTemp = 0;
int ReturnSp;
int SupplyTempF, ReturnTempF = 0;
uint16_t* floatAsRegisters;
int LeadSW = 0;  
// Lead/Lag select: 0 = AUTO (lowest run-hours leads), 1 = MANUAL Comp A forced MASTER, 2 = MANUAL Comp B forced MASTER

int AMP1, AMP2, AMP3 = 0;

unsigned long currentTime = 0;
unsigned long wait_time = 0;
unsigned long previousMillis = 0;  // Min millis
unsigned long pmillis = 0;

int ftoC = 1, psiTobar = 1;
int thermostat_selection = 0;  //  Return Sensor OR Use BECA
int startSw = 0;               // Start/Stop Sw
int ResetAlarm = 0;
bool resetHrsFlag = false;

enum CompressorState : uint8_t {
  COMP_STOPPED,
  COMP_STARTING,
  COMP_RUNNING,
  COMP_STOPPING,
  COMP_TRIPPED,
  COMP_AUTO_STOPPED,
  COMP_DISABLED,
  SUCTION_TEMP_ALERT,
  DISCHARGE_TEMP_ALERT,
  SUCTION_PSI_ALERT,
  DISCHARGE_PSI_ALERT,
  COMP_AMP_HIGH,
  SPRAY_TEMP_ALERT,
  OIL_PSI_ALERT
};

enum EXVState : uint8_t {
  EXV_IDLE,
  EXV_INITIALIZING,
  EXV_READY,
  EXV_RESETTING
};

enum VFDState : uint8_t {
  VFD_NOT_SELECTED,
  VFD_MODE,
  SD_MODE,
  DD_MODE,
  PW_MODE
};

enum LoadState : uint8_t {
  LOAD_IDLE,
  LOAD_LOADING,
  LOAD_MAINTAINING,
  LOAD_UNLOADING
};

// Compressor data structure
struct Compressor {
  // ---- States ----
  CompressorState state;
  VFDState vfdState;
  EXVState exvState;

  // ---- Timing ----
  unsigned long STAR_DELTA_TIME = 0;
  unsigned long COMP_START_TIME = 0;
  unsigned long SWITCH_ALARM_TIME = 0;
  unsigned long VFD_STEP_TIME = 0;

  int COMP_ENABLE = 0;
  int driveSelection = 0;  // SD=0 , DD=1 , PW=2 , VFD=3
  int EXV_ENABLE = 0;
  int gas_index = 0;

  bool startup_flag = false;
  bool alarmFlag = false;
  bool Machine_Shutdown = false;
  bool Switches_Alarm = false;
  bool STAR_TO_DELTA = false;

  int FAN1_ENABLE = 0;  // Fan 1,2
  int FAN3_ENABLE = 0;  // Fan 3,4
  int FAN5_ENABLE = 0;  // Fan 5,6

  // ---- PID ----
  float Setpoint;
  float Input;
  float Output;

  // ---- Runtime ----
  int Starting_delay;     // secs
  int Starting_delay_ms;  //ms
  int VFD_Step_Delay;

  int Run_hrs;
  float step_size;
  int16_t counterMin;

  // ---- VFD ----
  int16_t InvFreq;
  int min_vfd_freq;
  int max_vfd_freq;
  int out_hz;
  int out_amps;
  int out_voltage;

  // ---- Temperatures ----
  int16_t SuctionTempF;
  float SuctionTemp;
  int16_t SprayTempF;
  float SprayTemp;
  int16_t SubCoolTempF;
  float SubCoolTemp;
  int16_t dischargeTempF;
  float dischargeTemp;
  int16_t SuperHeatPV;
  int SuperHeatSV = 0;

  // ---- Pressures ----
  int suctionPressure;
  int dischargePressure;
  int oilPressure;
  int suctionPressureB;
  int dischargePressureB;
  int oilPressureB;

  int suction_pressure_type;
  int suction_pressure_unit;
  int suction_pressure_range;
  int suction_offset;
  int sensorMax;

  int discharge_pressure_type;
  int discharge_pressure_unit;
  int discharge_pressure_range;
  int discharge_offset;
  int sensorMax2;

  int oil_pressure_type;
  int oil_pressure_unit;
  int oil_pressure_range;
  int oil_offset;
  int sensorMax3;

  // ---- CONDENSER FAN SETTINGS ----
  int Fan1_ON_PSI = 0;
  int Fan1_OFF_PSI = 0;
  int Fan3_ON_PSI = 0;
  int Fan3_OFF_PSI = 0;
  int Fan5_ON_PSI = 0;
  int Fan5_OFF_PSI = 0;

  int suc_temp_high_SV = 0;   // Suction Gas Temperature HIGH alarm setpoint (spec alarm list)
  int dis_temp_high_SV = 0;
  int spray_temp_low_SV = 0;
  int suc_pres_low_SV = 0;
  int dis_pres_high_SV = 0;
  int oil_pres_low_SV = 0;    // Oil Pressure LOW alarm setpoint (renamed from oil_pres_high_SV)
  int amps_high_Sp = 0;

  // ALARMS
  bool suc_temp_high_alarm = false;
  bool dis_temp_high_alarm = false;
  bool spray_temp_low_alarm = false;
  bool suc_pres_low_alarm = false;
  bool dis_pres_high_alarm = false;
  bool oil_pres_low_alarm = false;
  bool amp_high_alarm = false;

  // EXTERNAL SAFETY SWITCHES
  bool oil_level_low_alarm = false;
  bool oil_pres_low_alarm_sw = false;
  bool suc_pres_low_alarm_sw = false;
  bool dis_pres_high_alarm_sw = false;

  // OUTPUT STATUS
  bool star_status = false;
  bool delta_status = false;
  bool fan1_status = false;
  bool fan3_status = false;

  // ---- Master/Slave (Lead-Lag) ----
  bool isMaster = false;             // true when this compressor is currently LEAD/MASTER
  bool assistActive = false;         // true while this MASTER has called in the SLAVE to assist
  unsigned long STOP_TIME = 0;       // millis() timestamp of last STOP, drives the 600s restart lockout
  unsigned long ASSIST_STEP_TIME = 0;  // millis() timestamp when MASTER was stepped to 50% before SLAVE start
  int RLA_Limit_Pct = 100;           // operator-selectable load capacity ceiling (HMI) - stepped to 50 during assist hand-off
  bool fullyLoaded = false;          // true when running at/near RLA_Limit_Pct - set by updateCapacityControl() for fixed-speed drives, or frequency heuristic for VFD

  // ---- Loader/Unloader (Capacity Control) - AquaForce N.C. Loader / N.O. Unloader scheme ----
  int RLA_Amps = 0;            // operator setpoint: motor current at 100% rated load (HMI) - basis for load% estimate
  int loadPulseOnMs = 3000;          // Loader/Unloader active-phase duration, selectable 1-7s (ms)
  int loadPulseOffMs = 10000;        // Loader/Unloader rest-phase duration (ms)
  int minLoadPct = 40;               // floor capacity % - target for "fully unloaded"
  LoadState loadState = LOAD_IDLE;
  bool loaderOutput = false;
  bool unloaderOutput = false;
  unsigned long loadPulseTimer = 0;
  bool loadPulsePhaseOn = false;     // current phase of the active pulse cycle
};

struct ExpansionValve {
  int UpperLimit = 100;  // Max. EXV %
  int LowerLimit = 0;    // Min. EXV %
  int exv_step_delay = 0;
  int exv_total_steps = 0;
  int currentStep = 0;

  float Kp = 2;
  float Ki = 0.5;
  float Kd = 0;
  int Mode = 0;
  bool move_direction = false;  // false for Close and true for Open direction

  unsigned long write_exv_millis = 0;
  unsigned long PIDmillis = 0;
};

Compressor CompA;
ExpansionValve EXV1;

Compressor CompB;
ExpansionValve EXV2;

// ============================================================================
//  REFRIGERANT SELECTION  (per AquaForce spec)
//    gas_index 0 = R-134a
//    gas_index 1 = R-22
//    gas_index 2 = R-410   (R-410A thermodynamic data)
//    gas_index 3 = R-407   (R-407C thermodynamic data)
//
//  PT charts are saturated-VAPOUR (dew-point) pressure in PSIG - the correct
//  reference for SUCTION superheat. For the zeotropic blends (R-407C, R-410A)
//  the dew point is used (matters most for R-407C, which has ~7 degF glide).
//
//    array index i  ->  saturation temp = (PT_BASE_TEMP_F + i) degF
//
//  Generated with CoolProp 7.2.0, range -40..150 degF at 1 degF resolution.
//  Re-verify against the gas manufacturer's PT chart before field commissioning.
// ============================================================================
enum Refrigerant : uint8_t { GAS_R134A = 0, GAS_R22 = 1, GAS_R410 = 2, GAS_R407 = 3 };

#define PT_BASE_TEMP_F (-40)

float R134a_PSIG[] = {
    -7.3,   -7.0,   -6.8,   -6.6,   -6.4,   -6.1,   -5.9,   -5.6,   -5.4,   -5.1,
    -4.8,   -4.6,   -4.3,   -4.0,   -3.7,   -3.4,   -3.1,   -2.8,   -2.5,   -2.1,
    -1.8,   -1.5,   -1.1,   -0.8,   -0.4,   -0.0,    0.4,    0.7,    1.1,    1.5,
     1.9,    2.4,    2.8,    3.2,    3.6,    4.1,    4.6,    5.0,    5.5,    6.0,
     6.5,    7.0,    7.5,    8.0,    8.5,    9.1,    9.6,   10.2,   10.8,   11.3,
    11.9,   12.5,   13.1,   13.8,   14.4,   15.0,   15.7,   16.4,   17.0,   17.7,
    18.4,   19.1,   19.9,   20.6,   21.3,   22.1,   22.9,   23.7,   24.5,   25.3,
    26.1,   26.9,   27.8,   28.6,   29.5,   30.4,   31.3,   32.2,   33.1,   34.1,
    35.0,   36.0,   37.0,   38.0,   39.0,   40.1,   41.1,   42.2,   43.2,   44.3,
    45.4,   46.6,   47.7,   48.9,   50.0,   51.2,   52.4,   53.6,   54.9,   56.1,
    57.4,   58.7,   60.0,   61.3,   62.7,   64.0,   65.4,   66.8,   68.2,   69.7,
    71.1,   72.6,   74.1,   75.6,   77.1,   78.7,   80.2,   81.8,   83.4,   85.0,
    86.7,   88.4,   90.0,   91.8,   93.5,   95.2,   97.0,   98.8,  100.6,  102.5,
   104.3,  106.2,  108.1,  110.0,  112.0,  113.9,  115.9,  118.0,  120.0,  122.1,
   124.2,  126.3,  128.4,  130.6,  132.7,  135.0,  137.2,  139.5,  141.7,  144.0,
   146.4,  148.7,  151.1,  153.5,  156.0,  158.4,  160.9,  163.4,  166.0,  168.6,
   171.2,  173.8,  176.5,  179.1,  181.8,  184.6,  187.4,  190.1,  193.0,  195.8,
   198.7,  201.6,  204.6,  207.5,  210.6,  213.6,  216.7,  219.7,  222.9,  226.0,
   229.2,  232.4,  235.7,  239.0,  242.3,  245.7,  249.0,  252.5,  255.9,  259.4,
   262.9
};

float R22_PSIG[] = {
     0.6,    1.0,    1.4,    1.8,    2.2,    2.6,    3.1,    3.5,    4.0,    4.5,
     4.9,    5.4,    5.9,    6.4,    6.9,    7.4,    8.0,    8.5,    9.1,    9.6,
    10.2,   10.8,   11.4,   12.0,   12.6,   13.2,   13.9,   14.5,   15.2,   15.9,
    16.5,   17.2,   17.9,   18.7,   19.4,   20.1,   20.9,   21.7,   22.4,   23.2,
    24.0,   24.9,   25.7,   26.5,   27.4,   28.3,   29.2,   30.1,   31.0,   31.9,
    32.8,   33.8,   34.8,   35.8,   36.8,   37.8,   38.8,   39.9,   40.9,   42.0,
    43.1,   44.2,   45.3,   46.5,   47.6,   48.8,   50.0,   51.2,   52.4,   53.7,
    55.0,   56.2,   57.5,   58.8,   60.2,   61.5,   62.9,   64.3,   65.7,   67.1,
    68.6,   70.0,   71.5,   73.0,   74.5,   76.1,   77.6,   79.2,   80.8,   82.4,
    84.1,   85.7,   87.4,   89.1,   90.8,   92.6,   94.4,   96.1,   98.0,   99.8,
   101.6,  103.5,  105.4,  107.3,  109.3,  111.2,  113.2,  115.3,  117.3,  119.4,
   121.4,  123.5,  125.7,  127.8,  130.0,  132.2,  134.5,  136.7,  139.0,  141.3,
   143.6,  146.0,  148.4,  150.8,  153.2,  155.7,  158.2,  160.7,  163.2,  165.8,
   168.4,  171.0,  173.7,  176.4,  179.1,  181.8,  184.6,  187.4,  190.2,  193.0,
   195.9,  198.8,  201.8,  204.7,  207.7,  210.8,  213.8,  216.9,  220.0,  223.2,
   226.4,  229.6,  232.8,  236.1,  239.4,  242.8,  246.1,  249.5,  253.0,  256.4,
   260.0,  263.5,  267.1,  270.7,  274.3,  278.0,  281.7,  285.4,  289.2,  293.0,
   296.9,  300.8,  304.7,  308.6,  312.6,  316.7,  320.7,  324.8,  329.0,  333.2,
   337.4,  341.6,  345.9,  350.3,  354.6,  359.0,  363.5,  368.0,  372.5,  377.1,
   381.7
};

float R410A_PSIG[] = {
    10.7,   11.3,   12.0,   12.6,   13.3,   14.0,   14.7,   15.5,   16.2,   16.9,
    17.7,   18.5,   19.3,   20.1,   20.9,   21.8,   22.6,   23.5,   24.4,   25.3,
    26.2,   27.1,   28.1,   29.0,   30.0,   31.0,   32.0,   33.1,   34.1,   35.2,
    36.3,   37.4,   38.5,   39.7,   40.8,   42.0,   43.2,   44.4,   45.7,   46.9,
    48.2,   49.5,   50.8,   52.2,   53.5,   54.9,   56.3,   57.8,   59.2,   60.7,
    62.2,   63.7,   65.2,   66.8,   68.4,   70.0,   71.6,   73.3,   74.9,   76.6,
    78.4,   80.1,   81.9,   83.7,   85.5,   87.4,   89.2,   91.1,   93.1,   95.0,
    97.0,   99.0,  101.1,  103.1,  105.2,  107.3,  109.5,  111.7,  113.9,  116.1,
   118.4,  120.6,  123.0,  125.3,  127.7,  130.1,  132.6,  135.0,  137.5,  140.1,
   142.6,  145.2,  147.9,  150.5,  153.2,  156.0,  158.7,  161.5,  164.4,  167.2,
   170.1,  173.1,  176.0,  179.0,  182.1,  185.1,  188.3,  191.4,  194.6,  197.8,
   201.1,  204.4,  207.7,  211.1,  214.5,  217.9,  221.4,  224.9,  228.5,  232.1,
   235.7,  239.4,  243.2,  246.9,  250.7,  254.6,  258.5,  262.4,  266.4,  270.4,
   274.5,  278.6,  282.7,  286.9,  291.2,  295.4,  299.8,  304.1,  308.6,  313.0,
   317.6,  322.1,  326.7,  331.4,  336.1,  340.8,  345.7,  350.5,  355.4,  360.4,
   365.4,  370.4,  375.5,  380.7,  385.9,  391.2,  396.5,  401.9,  407.3,  412.8,
   418.3,  423.9,  429.6,  435.3,  441.0,  446.8,  452.7,  458.7,  464.7,  470.7,
   476.8,  483.0,  489.3,  495.6,  501.9,  508.4,  514.9,  521.4,  528.0,  534.7,
   541.5,  548.3,  555.2,  562.2,  569.2,  576.3,  583.5,  590.8,  598.1,  605.5,
   613.0
};

float R407C_PSIG[] = {
    -2.3,   -1.9,   -1.6,   -1.2,   -0.8,   -0.4,   -0.0,    0.4,    0.8,    1.2,
     1.6,    2.1,    2.5,    3.0,    3.5,    3.9,    4.4,    4.9,    5.4,    5.9,
     6.5,    7.0,    7.6,    8.1,    8.7,    9.3,    9.9,   10.5,   11.1,   11.7,
    12.3,   13.0,   13.7,   14.3,   15.0,   15.7,   16.4,   17.2,   17.9,   18.7,
    19.4,   20.2,   21.0,   21.8,   22.6,   23.5,   24.3,   25.2,   26.1,   27.0,
    27.9,   28.8,   29.8,   30.7,   31.7,   32.7,   33.7,   34.7,   35.7,   36.8,
    37.9,   39.0,   40.1,   41.2,   42.3,   43.5,   44.7,   45.9,   47.1,   48.3,
    49.6,   50.8,   52.1,   53.4,   54.8,   56.1,   57.5,   58.9,   60.3,   61.7,
    63.2,   64.6,   66.1,   67.6,   69.2,   70.7,   72.3,   73.9,   75.5,   77.2,
    78.8,   80.5,   82.2,   84.0,   85.7,   87.5,   89.3,   91.2,   93.0,   94.9,
    96.8,   98.7,  100.7,  102.7,  104.7,  106.7,  108.8,  110.9,  113.0,  115.1,
   117.3,  119.5,  121.7,  124.0,  126.2,  128.5,  130.9,  133.2,  135.6,  138.0,
   140.5,  143.0,  145.5,  148.0,  150.6,  153.2,  155.8,  158.5,  161.2,  163.9,
   166.7,  169.5,  172.3,  175.2,  178.1,  181.0,  183.9,  186.9,  189.9,  193.0,
   196.1,  199.2,  202.4,  205.6,  208.8,  212.1,  215.4,  218.8,  222.1,  225.6,
   229.0,  232.5,  236.1,  239.6,  243.2,  246.9,  250.6,  254.3,  258.1,  261.9,
   265.8,  269.6,  273.6,  277.6,  281.6,  285.6,  289.8,  293.9,  298.1,  302.3,
   306.6,  311.0,  315.3,  319.8,  324.2,  328.7,  333.3,  337.9,  342.6,  347.3,
   352.0,  356.9,  361.7,  366.6,  371.6,  376.6,  381.7,  386.8,  392.0,  397.2,
   402.5
};

int size_R134a = sizeof(R134a_PSIG) / sizeof(R134a_PSIG[0]);
int size_R22   = sizeof(R22_PSIG)   / sizeof(R22_PSIG[0]);
int size_R410  = sizeof(R410A_PSIG) / sizeof(R410A_PSIG[0]);
int size_R407  = sizeof(R407C_PSIG) / sizeof(R407C_PSIG[0]);