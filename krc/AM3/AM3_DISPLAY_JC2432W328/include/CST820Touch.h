#pragma once

#include <Arduino.h>
#include <Wire.h>

class CST820Touch {
 public:
  CST820Touch(int8_t sda, int8_t scl, int8_t rst, int8_t interruptPin = -1);

  bool begin(uint32_t i2cFrequency = 400000UL);
  bool getTouch(uint16_t& x, uint16_t& y, uint8_t& gesture);
  bool online() const { return address_ != 0; }
  uint8_t address() const { return address_; }

 private:
  bool probe(uint8_t address);
  bool readRegister(uint8_t reg, uint8_t& value);
  bool readRegisters(uint8_t reg, uint8_t* data, size_t length);
  bool writeRegister(uint8_t reg, uint8_t value);

  int8_t sda_;
  int8_t scl_;
  int8_t rst_;
  int8_t int_;
  uint8_t address_ = 0;
};
