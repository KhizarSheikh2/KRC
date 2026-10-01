#pragma once

#include <Arduino.h>

enum AM5WirelessUiState : uint8_t {
    AM5_LINK_OFFLINE = 0,
    AM5_LINK_WAITING = 1,
    AM5_LINK_ONLINE = 2,
    AM5_LINK_SYNCING = 3
};

void wirelessLinkBegin();
void wirelessLinkLoop();

AM5WirelessUiState wirelessLinkUiState();
bool wirelessLinkCanControl();

bool wirelessLinkRequestPower(int power);
bool wirelessLinkRequestMode(uint8_t mode);
bool wirelessLinkRequestFan(uint8_t fanSpeed);
