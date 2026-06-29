#include <Adafruit_ADS1X15.h>

/*
  Voltage= ADC value × FULL-SCALE VOLTAGE /  RESOLUTION
  where :
  FULL-SCALE VOLTAGE => 4.096V
  RESOLUTION => 2^16 / 2 = 32768 i.e MAX ADC VALUE => 32768
*/

// for 4-20mA sensors
#define SENSOR_MAX_I 23950  // 20mA or 3.0V
#define SENSOR_MIN_I 4800   // 4mA or 0.6V

// for 0-10v sensors (Left Jumper .-. .)
// #define SENSOR_MAX_ANALOG 20000  // 10.0V
#define SENSOR_MAX_V 12800  //5.04V
#define SENSOR_MIN_V 30     // 0.1V


#define ADC_Ports 4  // 4 Channels for each ADS1115

Adafruit_ADS1115 ads1; /* Use this for the 16-bit version */
Adafruit_ADS1115 ads2; /* Use this for the 16-bit version */

void adcInit() {
  // Wire.begin();
  if (!ads1.begin(0x49)) {
    Serial.println("ADS1115 # 1 not found at 0x48");
  } else {
    Serial.println("ADS1115 # 1 Initialized Successfully!");
  }

  if (!ads2.begin(0x48)) {
    Serial.println("ADS1115 # 2 not found at 0x49");
  } else {
    Serial.println("ADS1115 # 2 Initialized Successfully!");
  }
  ads1.setGain(GAIN_ONE);  // 1x gain (±4.096V)
  ads2.setGain(GAIN_ONE);  // 1x gain (±4.096V)
  // ads1.setDataRate(RATE_ADS1115_250SPS);
  // ads2.setDataRate(RATE_ADS1115_250SPS);
}

void scanADC(Compressor &comp) {
  uint8_t suctionId = (&comp == &CompA) ? 0 : 2;
  uint8_t dischargeId = (&comp == &CompA) ? 1 : 3;

  long raw[4] = { 0, 0, 0, 0 };
  for (int channel = 0; channel < ADC_Ports; channel++) {  // to read only CH0 & CH1
    for (int j = 0; j < 10; j++) {
      raw[channel] += ads1.readADC_SingleEnded(channel);
      delayMicroseconds(300);  // Small delay to reduce noise
    }
    raw[channel] /= 10;  // Average of 10 samples
    // Serial.print("\t Channel : ");
    // Serial.print(channel);
    // Serial.print(" Raw Value : ");
    // Serial.print(raw[channel]);
  }
  // Serial.println();

  comp.suctionPressure = 888;
  comp.dischargePressure = 888;
  comp.oilPressure = 888;

  if (raw[suctionId] > 0) {
    // 0-5V sensor
    if (comp.suction_pressure_type == 1) comp.suctionPressure = map(raw[suctionId], SENSOR_MIN_V, SENSOR_MAX_V, 0, comp.sensorMax);
    // 4-20mA sensor
    else if (comp.suction_pressure_type == 2) comp.suctionPressure = map(raw[suctionId], SENSOR_MIN_I, SENSOR_MAX_I, 0, comp.sensorMax);
    comp.suctionPressure += comp.suction_offset;
  }
  if (raw[dischargeId] > 0) {
    // 0-5V sensor
    if (comp.discharge_pressure_type == 1) comp.dischargePressure = map(raw[dischargeId], SENSOR_MIN_V, SENSOR_MAX_V, 0, comp.sensorMax2);
    // 4-20mA sensor
    else if (comp.discharge_pressure_type == 2) comp.dischargePressure = map(raw[dischargeId], SENSOR_MIN_I, SENSOR_MAX_I, 0, comp.sensorMax2);
    comp.dischargePressure += comp.discharge_offset;
  }

  // ---- Oil pressure (second ADS1115 'ads2', previously unused) ----
  // NOTE / WIRING ASSUMPTION: oil-pressure transducers are read from ads2,
  //   Comp A -> ads2 channel 0, Comp B -> ads2 channel 1.
  // Confirm this against the actual board wiring and change oilCh if different.
  uint8_t oilCh = (&comp == &CompA) ? 0 : 1;
  long oilRaw = 0;
  for (int j = 0; j < 10; j++) {
    oilRaw += ads2.readADC_SingleEnded(oilCh);
    delayMicroseconds(300);
  }
  oilRaw /= 10;  // average of 10 samples

  if (oilRaw > 0) {
    // 0-5V sensor
    if (comp.oil_pressure_type == 1) comp.oilPressure = map(oilRaw, SENSOR_MIN_V, SENSOR_MAX_V, 0, comp.sensorMax3);
    // 4-20mA sensor
    else if (comp.oil_pressure_type == 2) comp.oilPressure = map(oilRaw, SENSOR_MIN_I, SENSOR_MAX_I, 0, comp.sensorMax3);
    comp.oilPressure += comp.oil_offset;
  }
  // else: leaves comp.oilPressure at 888 (SENSOR_DISCONNECTED) -> alarm is suppressed
}