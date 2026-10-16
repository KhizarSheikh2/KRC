#define MODBUS_BAUD_RATE 19200
#define RS485_TX_PIN 17
#define RS485_RX_PIN 16
HardwareSerial RS485Serial(1);

uint16_t calculateCRC16(uint8_t *data, uint16_t length) {
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < length; i++) {
    crc ^= (uint16_t)data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

void operateRS485Relay(uint8_t address, uint8_t bit_set) {
  uint8_t request[8] = {0x0A, 0x05, 0x00, address, bit_set, 0x00, 0, 0};
  uint16_t crc = calculateCRC16(request, 6);
  request[6] = lowByte(crc);
  request[7] = highByte(crc);
  
  RS485Serial.write(request, 8);
  RS485Serial.flush();
  delay(50);
}