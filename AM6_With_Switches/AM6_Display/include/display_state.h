#pragma once

#include <Arduino.h>
#include "config.h"

struct AM6DisplayState {
    int systemPower = 0;
    uint8_t mode = AM6_MODE_COOL;
    uint8_t fanSpeed = AM6_FAN_LOW;

    uint8_t statusA = AM6_OUT_STOPPED;
    uint8_t statusB = AM6_OUT_STOPPED;
    bool outdoorOnline = false;
    bool outdoorIoOk = false;
    bool indoorEnabled = true;
    bool outdoorEnabled = true;

    bool outdoorSwitch[8] = {false, false, false, false, false, false, false, false};
    bool outdoorRelay[4] = {false, false, false, false};

    float supplyTemp = 888.0f;
    float returnTemp = 888.0f;
    float setPoint = 22.0f;

    uint32_t revision = 0;
};

void displayStateBegin();
AM6DisplayState displayStateGet();
uint32_t displayStateRevision();

void displayStateSetPower(int power);
void displayStateSetMode(uint8_t mode);
void displayStateSetFanSpeed(uint8_t fanSpeed);
void displayStateSetStatusA(uint8_t status);
void displayStateSetStatusB(uint8_t status);
void displayStateSetOutdoorOnline(bool online);
void displayStateSetOutdoorIoOk(bool ok);
void displayStateSetIndoorEnabled(bool enabled);
void displayStateSetOutdoorEnabled(bool enabled);
void displayStateSetOutdoorSwitch(uint8_t index, bool state);
void displayStateSetOutdoorRelay(uint8_t index, bool state);
void displayStateSetSupplyTemp(float value);
void displayStateSetReturnTemp(float value);
void displayStateSetSetPoint(float value);
