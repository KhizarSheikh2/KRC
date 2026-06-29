#include <Wire.h>

#define OUTPUT_Port 0x20
#define INPUT_PortA 0x26
#define INPUT_PortB 0x27

// EXV Stepper Pins
// STEPPER 1 (15,27)
// STEPPER 2 (13,14)
#define DIR_A 27
#define PLS_A 15
#define DIR_B 14
#define PLS_B 13

// PCF8574 O/P BIT #
#define CompA_STAR 3
#define CompA_DELTA 4
#define CompA_Condenser_Fan1 0
#define CompA_Condenser_Fan2 1
#define CompB_STAR 5
#define CompB_DELTA 6
#define CompB_Condenser_Fan1 7
#define CompB_Condenser_Fan2 2

// PCF8574 I/P BIT #
// 1. Oil Level Low Switch (N.C. = OK)
// 2. Oil Differential Pressure Low Switch (N.C. = OK)
// 3. Suction Pressure Low Switch (N.C. = OK)
// 4. Discharge Pressure High Switch (N.C. = OK)
#define Oil_level_Sw_C1 2
#define Oil_Pres_Low_Sw_C1 3
#define Suc_Pres_Low_Sw_C1 1
#define Dis_Pres_High_Sw_C1 0

#define Oil_level_Sw_C2 6
#define Oil_Pres_Low_Sw_C2 7
#define Suc_Pres_Low_Sw_C2 5
#define Dis_Pres_High_Sw_C2 4

// Internal variables
uint8_t outputs = 0;
uint8_t EXV_DIR_PIN[2] = { DIR_A, DIR_B };
uint8_t EXV_PLS_PIN[2] = { PLS_A, PLS_B };
uint8_t STAR_PIN[2] = { CompA_STAR, CompB_STAR };
uint8_t DELTA_PIN[2] = { CompA_DELTA, CompB_DELTA };
// uint8_t FAN1[2] = { CompA_Condenser_Fan1, CompB_Condenser_Fan1 };

void EXV_CONFIG() {
  pinMode(DIR_A, OUTPUT);
  digitalWrite(DIR_A, LOW);
  pinMode(PLS_A, OUTPUT);
  digitalWrite(PLS_A, LOW);

  pinMode(DIR_B, OUTPUT);
  digitalWrite(DIR_B, HIGH);
  pinMode(PLS_B, OUTPUT);
  digitalWrite(PLS_B, LOW);
}

void write_on_EXV(ExpansionValve &exv, int targetSteps, unsigned long now, uint16_t exv_step_delay) {
  uint8_t id = (&exv == &EXV1) ? 0 : 1;
  bool stepPending = false;
  // ---------- Decide movement ----------
  if (exv.currentStep < targetSteps && exv.currentStep < exv.exv_total_steps) {
    exv.move_direction = true;  // OPEN
    exv.currentStep++;
    stepPending = true;
  }
  //
  else if (exv.currentStep > targetSteps && exv.currentStep > 0) {
    exv.move_direction = false;  // CLOSE
    exv.currentStep--;
    stepPending = true;
  }

  // ---------- Execute step (timed) ----------
  if (stepPending && (now - exv.write_exv_millis >= exv_step_delay)) {
    exv.write_exv_millis = now;

    digitalWrite(EXV_DIR_PIN[id], exv.move_direction ? HIGH : LOW);

    digitalWrite(EXV_PLS_PIN[id], HIGH);
    delayMicroseconds(800);  // ✔ short, safe
    digitalWrite(EXV_PLS_PIN[id], LOW);
    delayMicroseconds(800);
  }
}

void Open(int compID, int stepdelay) {
  if (compID == 1) {
    digitalWrite(DIR_A, HIGH);
    digitalWrite(PLS_A, HIGH);
    delay(stepdelay);
    digitalWrite(PLS_A, LOW);
    delay(stepdelay);
  }

  else if (compID == 2) {
    digitalWrite(DIR_B, HIGH);
    digitalWrite(PLS_B, HIGH);
    delay(stepdelay);
    digitalWrite(PLS_B, LOW);
    delay(stepdelay);
  }
}

void Close(int compID, int stepdelay) {
  if (compID == 1) {
    digitalWrite(DIR_A, LOW);
    digitalWrite(PLS_A, HIGH);
    delay(stepdelay);
    digitalWrite(PLS_A, LOW);
    delay(stepdelay);
  }

  else if (compID == 2) {
    digitalWrite(DIR_B, LOW);
    digitalWrite(PLS_B, HIGH);
    delay(stepdelay);
    digitalWrite(PLS_B, LOW);
    delay(stepdelay);
  }
}

uint8_t readInputs(uint16_t Address) {
  uint8_t data = 0;
  Wire.requestFrom(Address, 1);
  if (Wire.available()) {
    data = Wire.read();
  }
  return data;
}

// Function to write all outputs to the PCF8574
void Output_Write(uint8_t pin, bool state) {
  // uint8_t outputs = readInputs(OUTPUT_Port);
  if (state == 1) {
    outputs |= (1 << pin);
  } else {
    outputs &= ~(1 << pin);  // Bit Clear
  }

  Wire.beginTransmission(OUTPUT_Port);
  Wire.write(outputs);
  Wire.endTransmission();
}

// PCF8574 Initialization
bool initialize_PCF8574(const uint16_t addresses[], int count) {
  for (int i = 0; i < count; i++) {
    Wire.beginTransmission(addresses[i]);
    uint8_t result = Wire.endTransmission();
    if (result != 0) {
      // Return false if any device fails to respond
      return false;
    }
  }
  // All devices responded successfully
  return true;
}

// Optimized I/O Initialization
void initGPIOs() {
  if (!Wire.begin(21, 22)) {
    Serial.println("I2C Bus Initialization Failed!");
    return;
  }
  Serial.println("I2C Bus Initialized Successfully!");

  Wire.beginTransmission(OUTPUT_Port);
  uint8_t error = Wire.endTransmission();
  if (error == 0) {
    Serial.println("PCF8574 for Relays Initialized Successfully!");
  } else {
    Serial.println("PCF8574 for Relays Initialized Failed");
  }

  Wire.beginTransmission(OUTPUT_Port);
  Wire.write(0x00);  // All pins low (output mode)
  Wire.endTransmission();


  uint16_t addresses[] = { INPUT_PortA, INPUT_PortB };
  if (!initialize_PCF8574(addresses, 2)) {
    Serial.println("IO PCF8574 Initialization Failed!");
    return;
  }
  Serial.println("IO PCF8574 Initialized Successfully!");


  Wire.beginTransmission(INPUT_PortA);
  Wire.write(0xFF);  // All pins high (input mode)
  Wire.endTransmission();

  Wire.beginTransmission(INPUT_PortB);
  Wire.write(0xFF);  // All pins high (input mode)
  Wire.endTransmission();

  EXV_CONFIG();
}
