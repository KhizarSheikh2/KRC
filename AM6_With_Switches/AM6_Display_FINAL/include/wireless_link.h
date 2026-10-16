#pragma once

#include <Arduino.h>

enum AM6WirelessUiState : uint8_t {
    AM6_LINK_OFFLINE = 0,
    AM6_LINK_WAITING = 1,
    AM6_LINK_ONLINE = 2,
    AM6_LINK_SYNCING = 3
};

void wirelessLinkBegin();
void wirelessLinkLoop();

AM6WirelessUiState wirelessLinkUiState();

// Compatibility API. This Display build is monitoring-only, so these always
// return false and never send commands to Indoor.
bool wirelessLinkCanControl();
bool wirelessLinkRequestPower(int power);
bool wirelessLinkRequestMode(uint8_t mode);
bool wirelessLinkRequestFan(uint8_t fanSpeed);
