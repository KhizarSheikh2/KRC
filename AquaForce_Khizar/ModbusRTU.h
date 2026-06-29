/*
    MODBUS RTU - AS A Master DEVICE USING RS485 to communicate with Adam4015 & INVT VFDs

    Function Code 0x03 – Read Holding Registers
    Function Code 0x06 – Write Single Register
*/

// Define Modbus RS485 parameters
#define VFD_1_ID 0x01
#define VFD_2_ID 0x02

#define MODBUS_TIMEOUT 1000
#define RS485_BAUD_RATE 9600
#define RS485_TX_PIN 17
#define RS485_RX_PIN 16
#define RS485_DE_RE_PIN 4  // DE and RE pins tied together and connected to GPIO 4

// BECA THERMOSTAT
#define BECA_SLAVE_ID 0x02
#define MODBUS_REGISTER_ADDRESS 0x0000

// CT MODULE
#define MODBUS_SLAVE_ID 0x01
#define MODBUS_FUNCTION_READ_HOLDING_REGISTERS 0x03
#define CT_REGISTER_ADDRESS 0x0103

// INVT VFD
#define Control_Address 0x1000
#define Freq_Address 0x2000  // Set Hz
#define VFD_Run 1
#define VFD_Stop 5

HardwareSerial RS485Serial(1);

bool readHoldingRegisters(uint8_t slaveId, uint16_t startAddress, uint16_t registerCount, uint16_t *buffer);
void writeSingleRegister(uint8_t slaveId, uint16_t address, uint16_t value);

uint16_t calculateCRC(uint8_t *buffer, uint16_t length) {
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < length; i++) {
    crc ^= buffer[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xA001;
      else
        crc >>= 1;
    }
  }
  return crc;
}

void clearSerialBuffer() {
  uint8_t flush;
  while (RS485Serial.available()) {
    flush = RS485Serial.read();  // Clear any old data
  }
}

void sendModbusRequest(uint8_t slaveId, uint8_t functionCode, uint16_t startAddress, uint16_t registerCount) {
  digitalWrite(RS485_DE_RE_PIN, HIGH);  // Set DE/RE pin high for transmit mode
  uint8_t request[8];
  request[0] = slaveId;
  request[1] = functionCode;
  request[2] = startAddress >> 8;
  request[3] = startAddress & 0xFF;
  request[4] = registerCount >> 8;
  request[5] = registerCount & 0xFF;
  uint16_t crc = calculateCRC(request, 6);
  request[6] = crc & 0xFF;
  request[7] = crc >> 8;

  RS485Serial.write(request, sizeof(request));
  RS485Serial.flush();                 // Ensure all data is sent
  // clearSerialBuffer();                 // Ensure buffer is empty
  digitalWrite(RS485_DE_RE_PIN, LOW);  // Set DE/RE pin low for receive mode
  delay(10);
}

bool readModbusResponse(uint8_t *response, uint16_t length) {
  uint32_t timeout = millis() + MODBUS_TIMEOUT;
  uint16_t index = 0;
  while (millis() < timeout) {
    if (RS485Serial.available()) {
      response[index++] = RS485Serial.read();
      if (index >= length) return true;
    }
  }
  // Serial.println("Timeout or Incomplete Response!");
  return false;
}

bool readHoldingRegisters(uint8_t slaveId, uint16_t startAddress, uint16_t registerCount, uint16_t *buffer) {
  sendModbusRequest(slaveId, 0x03, startAddress, registerCount);
  uint8_t response[5 + 2 * registerCount];
  delay(400);
  if (readModbusResponse(response, sizeof(response))) {
    uint16_t crc = (response[sizeof(response) - 1] << 8) | response[sizeof(response) - 2];
    if (calculateCRC(response, sizeof(response) - 2) == crc) {
      for (int i = 0; i < registerCount; i++) {
        buffer[i] = (response[3 + i * 2] << 8) | response[4 + i * 2];
      }
      return true;
    } else {
      Serial.println("CRC error!");
      RS485Serial.end();
      delay(50);
      RS485Serial.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
      delay(50);
    }
  } else {
    Serial.println("Modbus RTU : No response or timeout!");
  }
  return false;
}

void writeSingleRegister(uint8_t slaveId, uint16_t address, uint16_t value) {
  sendModbusRequest(slaveId, 0x06, address, value);
  delay(150);
  // uint16_t registers[10];
  // readHoldingRegisters(slaveId, 0x0000, 10, registers);
}
