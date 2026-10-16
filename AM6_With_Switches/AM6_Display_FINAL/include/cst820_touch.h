#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

class CST820Touch {
public:
    bool begin();
    bool read(uint16_t& x, uint16_t& y);
    bool available() const { return ready_; }
    uint8_t address() const { return address_; }

private:
    bool probe(uint8_t address);
    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegister(uint8_t reg, uint8_t& value);
    bool readRegisters(uint8_t reg, uint8_t* data, size_t length);
    void resetController();
    void transformToLandscape(uint16_t rawX, uint16_t rawY, uint16_t& x, uint16_t& y);

    bool ready_ = false;
    uint8_t address_ = 0;
};
