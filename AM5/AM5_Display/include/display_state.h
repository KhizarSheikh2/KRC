#pragma once

#include <Arduino.h>
#include "config.h"

// Local display model used until wireless communication is added.
// systemPower is intentionally int: 0 = OFF, 1 = ON.
struct AM5DisplayState {
    int systemPower = 0;
    uint8_t mode = AM5_MODE_COOL;
    uint8_t fanSpeed = AM5_FAN_LOW;
    uint32_t revision = 0;
};

void displayStateBegin();
AM5DisplayState displayStateGet();
uint32_t displayStateRevision();

void displayStateSetPower(int power);
void displayStateSetMode(uint8_t mode);
void displayStateSetFanSpeed(uint8_t fanSpeed);
