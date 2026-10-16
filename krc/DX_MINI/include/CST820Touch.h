#pragma once

#include <Arduino.h>
#include <Wire.h>

class CST820Touch {
public:
  CST820Touch(int8_t sda, int8_t scl, int8_t rst, int8_t intr, uint8_t preferredAddress = 0x15);

  bool begin();
  bool read(uint16_t &x, uint16_t &y);
  bool detected() const { return _detected; }
  uint8_t address() const { return _address; }

private:
  bool probe(uint8_t address);
  bool readReg(uint8_t reg, uint8_t &value);
  bool readRegs(uint8_t reg, uint8_t *data, size_t len);
  bool writeReg(uint8_t reg, uint8_t value);

  int8_t _sda;
  int8_t _scl;
  int8_t _rst;
  int8_t _intr;
  uint8_t _preferredAddress;
  uint8_t _address;
  bool _detected;
};
