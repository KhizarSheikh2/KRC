#include "display_state.h"

namespace {
AM6DisplayState state;

bool validMode(uint8_t mode) {
    return mode == AM6_MODE_COOL || mode == AM6_MODE_HEAT;
}

bool validFanSpeed(uint8_t speed) {
    return speed >= AM6_FAN_LOW && speed <= AM6_FAN_HIGH;
}

void bumpRevision() {
    ++state.revision;
    if (state.revision == 0) state.revision = 1;
}
} // namespace

void displayStateBegin() {
    state.systemPower = 0;
    state.mode = AM6_MODE_COOL;
    state.fanSpeed = AM6_FAN_LOW;
    state.revision = 1;
}

AM6DisplayState displayStateGet() {
    return state;
}

uint32_t displayStateRevision() {
    return state.revision;
}

void displayStateSetPower(int power) {
    const int normalized = (power == 1) ? 1 : 0;
    if (state.systemPower == normalized) return;
    state.systemPower = normalized;
    bumpRevision();
}

void displayStateSetMode(uint8_t mode) {
    if (!validMode(mode) || state.mode == mode) return;
    state.mode = mode;
    bumpRevision();
}

void displayStateSetFanSpeed(uint8_t fanSpeed) {
    if (!validFanSpeed(fanSpeed) || state.fanSpeed == fanSpeed) return;
    state.fanSpeed = fanSpeed;
    bumpRevision();
}
