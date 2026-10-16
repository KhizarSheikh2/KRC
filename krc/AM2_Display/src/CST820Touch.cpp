#include "CST820Touch.h"

bool CST820Touch::writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(AM2_TOUCH_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool CST820Touch::readRegs(uint8_t reg, uint8_t* data, size_t len) {
  Wire.beginTransmission(AM2_TOUCH_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;

  const size_t got = Wire.requestFrom(static_cast<uint8_t>(AM2_TOUCH_ADDR), static_cast<uint8_t>(len));
  if (got != len) {
    while (Wire.available()) (void)Wire.read();
    return false;
  }

  for (size_t i = 0; i < len; ++i) data[i] = Wire.read();
  return true;
}

bool CST820Touch::begin() {
  pinMode(AM2_TOUCH_RST, OUTPUT);
  digitalWrite(AM2_TOUCH_RST, LOW);
  delay(12);
  digitalWrite(AM2_TOUCH_RST, HIGH);
  delay(220);

  Wire.begin(AM2_TOUCH_SDA, AM2_TOUCH_SCL, 400000);
  Wire.beginTransmission(AM2_TOUCH_ADDR);
  detected_ = (Wire.endTransmission() == 0);
  if (!detected_) return false;

  // Disable automatic low-power sleep for reliable LVGL polling.
  (void)writeReg(0xFE, 0xFF);
  return true;
}

bool CST820Touch::read(uint16_t& x, uint16_t& y) {
  if (!detected_) return false;

  // CST820: 0x02 = touch count, 0x03..0x06 = X/Y high/low.
  uint8_t data[5] = {0};
  if (!readRegs(0x02, data, sizeof(data))) return false;
  if ((data[0] & 0x0F) == 0) return false;

  uint16_t rawX = static_cast<uint16_t>(((data[1] & 0x0F) << 8) | data[2]);
  uint16_t rawY = static_cast<uint16_t>(((data[3] & 0x0F) << 8) | data[4]);

  // Native touch coordinates are portrait 240x320. The TFT uses rotation=1.
  int32_t sx = rawY;
  int32_t sy = (AM2_SCREEN_H - 1) - static_cast<int32_t>(rawX);

  if (sx < 0) sx = 0;
  if (sx >= AM2_SCREEN_W) sx = AM2_SCREEN_W - 1;
  if (sy < 0) sy = 0;
  if (sy >= AM2_SCREEN_H) sy = AM2_SCREEN_H - 1;

  x = static_cast<uint16_t>(sx);
  y = static_cast<uint16_t>(sy);
  return true;
}
