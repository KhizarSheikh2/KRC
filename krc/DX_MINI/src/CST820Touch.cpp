#include "CST820Touch.h"

CST820Touch::CST820Touch(int8_t sda, int8_t scl, int8_t rst, int8_t intr, uint8_t preferredAddress)
    : _sda(sda), _scl(scl), _rst(rst), _intr(intr), _preferredAddress(preferredAddress),
      _address(preferredAddress), _detected(false) {}

bool CST820Touch::probe(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool CST820Touch::begin() {
  Wire.begin(_sda, _scl);
  Wire.setClock(400000);
  // Bound I2C operations so a noisy/stuck touch bus cannot stall the UI task.
  Wire.setTimeOut(20);

  // Vendor boards expose both INT and RESET.  Pulse INT before reset, then
  // leave it as an input because this driver polls the controller over I2C.
  if (_intr >= 0) {
    pinMode(_intr, OUTPUT);
    digitalWrite(_intr, HIGH);
    delay(1);
    digitalWrite(_intr, LOW);
    delay(1);
  }

  if (_rst >= 0) {
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, LOW);
    delay(12);
    digitalWrite(_rst, HIGH);
    delay(300);
  }

  if (_intr >= 0) pinMode(_intr, INPUT_PULLUP);

  const uint8_t candidates[] = {_preferredAddress, 0x14, 0x2E, 0x38};
  for (uint8_t candidate : candidates) {
    if (probe(candidate)) {
      _address = candidate;
      _detected = true;
      break;
    }
  }

  if (!_detected) return false;

  // Keep the panel awake.  This avoids the sluggish first touch seen on some
  // CST820 modules after they enter low-power mode.
  (void)writeReg(0xFE, 0xFF);
  delay(5);
  return true;
}

bool CST820Touch::readReg(uint8_t reg, uint8_t &value) {
  if (!_detected) return false;
  Wire.beginTransmission(_address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(_address, (uint8_t)1) != 1) return false;
  value = Wire.read();
  return true;
}

bool CST820Touch::readRegs(uint8_t reg, uint8_t *data, size_t len) {
  if (!_detected || !data || len == 0) return false;
  Wire.beginTransmission(_address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;

  const size_t received = Wire.requestFrom(_address, (uint8_t)len);
  if (received != len) {
    while (Wire.available()) (void)Wire.read();
    return false;
  }

  for (size_t i = 0; i < len; ++i) data[i] = Wire.read();
  return true;
}

bool CST820Touch::writeReg(uint8_t reg, uint8_t value) {
  if (!_detected) return false;
  Wire.beginTransmission(_address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool CST820Touch::read(uint16_t &x, uint16_t &y) {
  // Read finger-count + X/Y in ONE I2C burst.  The previous implementation
  // used two independent transactions (0x02, then 0x03..0x06).  On short
  // taps the controller could update between those transactions, which made
  // some presses feel slow or require multiple attempts.
  uint8_t frame[5] = {0, 0, 0, 0, 0};
  if (!readRegs(0x02, frame, sizeof(frame))) return false;

  const uint8_t fingers = frame[0] & 0x0F;
  if (fingers == 0) return false;

  x = (uint16_t)(((frame[1] & 0x0F) << 8) | frame[2]);
  y = (uint16_t)(((frame[3] & 0x0F) << 8) | frame[4]);

  // JC2432W328 CST820 reports portrait-native coordinates (240 x 320).
  // Reject stale/corrupt samples instead of clamping them into another widget.
  return x < 240 && y < 320;
}
