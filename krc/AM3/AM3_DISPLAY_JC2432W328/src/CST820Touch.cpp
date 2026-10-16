#include "CST820Touch.h"

CST820Touch::CST820Touch(int8_t sda, int8_t scl, int8_t rst, int8_t interruptPin)
    : sda_(sda), scl_(scl), rst_(rst), int_(interruptPin) {}

bool CST820Touch::probe(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool CST820Touch::begin(uint32_t i2cFrequency) {
  if (rst_ >= 0) {
    pinMode(rst_, OUTPUT);
    digitalWrite(rst_, LOW);
    delay(12);
    digitalWrite(rst_, HIGH);
    delay(120);
  }

  if (int_ >= 0) {
    pinMode(int_, INPUT_PULLUP);
  }

  Wire.begin(sda_, scl_, i2cFrequency);

  constexpr uint8_t candidates[] = {0x15, 0x14, 0x2E, 0x38};
  for (uint8_t candidate : candidates) {
    if (probe(candidate)) {
      address_ = candidate;
      break;
    }
  }

  if (address_ == 0) {
    return false;
  }

  // Vendor driver uses this to prevent automatic low-power sleep.
  writeRegister(0xFE, 0xFF);
  return true;
}

bool CST820Touch::readRegister(uint8_t reg, uint8_t& value) {
  return readRegisters(reg, &value, 1);
}

bool CST820Touch::readRegisters(uint8_t reg, uint8_t* data, size_t length) {
  if (address_ == 0 || data == nullptr || length == 0) {
    return false;
  }

  Wire.beginTransmission(address_);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const size_t received = Wire.requestFrom(static_cast<int>(address_),
                                           static_cast<int>(length));
  if (received != length) {
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }

  for (size_t i = 0; i < length; ++i) {
    data[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

bool CST820Touch::writeRegister(uint8_t reg, uint8_t value) {
  if (address_ == 0) {
    return false;
  }

  Wire.beginTransmission(address_);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool CST820Touch::getTouch(uint16_t& x, uint16_t& y, uint8_t& gesture) {
  uint8_t fingerCount = 0;
  if (!readRegister(0x02, fingerCount) || fingerCount == 0) {
    return false;
  }

  readRegister(0x01, gesture);

  uint8_t point[4] = {0, 0, 0, 0};
  if (!readRegisters(0x03, point, sizeof(point))) {
    return false;
  }

  x = static_cast<uint16_t>(((point[0] & 0x0F) << 8) | point[1]);
  y = static_cast<uint16_t>(((point[2] & 0x0F) << 8) | point[3]);
  return true;
}
