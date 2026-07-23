
// ACTIVE-HIGH 
#define LOW_PRE 2
#define HIGH_PRE 1
#define OIL_SW 0
#define COMP_PIN 3
#define START 4
#define STOP 5


// PCF8574 Initialization
bool initialize_PCF8574(const uint16_t addresses[], int count) {
  for (int i = 0; i < count; i++) {
    Wire.beginTransmission(addresses[i]);
    if (Wire.endTransmission() == 0) {
      return true;
    }
  }
  return false;
}

// Optimized I/O Initialization
void initInputs() {
  if (!Wire.begin(21, 22)) {
    Serial.println("I2C Bus Initialization Failed!");
    return;
  }
  Serial.println("I2C Bus Initialized Successfully!");

  uint16_t addresses[] = { 0x20 };
  if (!initialize_PCF8574(addresses, 1)) {
    Serial.println("IO PCF8574 Initialization Failed!");
    return;
  }
  Serial.println("IO PCF8574 Initialized Successfully!");

  Wire.beginTransmission(0x20);
  Wire.write(0xFF);  // All pins high (input mode)
  Wire.endTransmission();
}

uint8_t readInputs(uint16_t Address) {
  // PCF8574 inputs idle HIGH. Returning 0xFF on an I2C failure prevents a
  // missing expander from being interpreted as false LOW start/stop commands.
  uint8_t data = 0xFF;
  const uint8_t received = Wire.requestFrom(Address, static_cast<uint8_t>(1));
  if (received == 1 && Wire.available()) {
    data = static_cast<uint8_t>(Wire.read());
  }
  return data;
}
