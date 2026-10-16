#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "BoardConfig.h"

class CST820Touch {
 public:
  bool begin();
  bool read(uint16_t& x, uint16_t& y);
  bool detected() const { return detected_; }

 private:
  bool writeReg(uint8_t reg, uint8_t value);
  bool readRegs(uint8_t reg, uint8_t* data, size_t len);
  bool detected_ = false;
};
