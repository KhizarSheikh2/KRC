#include "display_state.h"

#include <cmath>

namespace {
AM6DisplayState state;
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

bool validMode(uint8_t mode) {
    return mode == AM6_MODE_COOL || mode == AM6_MODE_HEAT;
}

bool validFanSpeed(uint8_t speed) {
    return speed >= AM6_FAN_LOW && speed <= AM6_FAN_HIGH;
}

bool validOutdoorStatus(uint8_t status) {
    return status <= AM6_OUT_OVERLOAD;
}

void bumpRevisionLocked() {
    ++state.revision;
    if (state.revision == 0) state.revision = 1;
}

bool floatChanged(float a, float b) {
    if (std::isnan(a) != std::isnan(b)) return true;
    if (std::isnan(a)) return false;
    return std::fabs(a - b) > 0.01f;
}
} // namespace

void displayStateBegin() {
    portENTER_CRITICAL(&stateMux);
    state = AM6DisplayState{};
    state.revision = 1;
    portEXIT_CRITICAL(&stateMux);
}

AM6DisplayState displayStateGet() {
    portENTER_CRITICAL(&stateMux);
    AM6DisplayState copy = state;
    portEXIT_CRITICAL(&stateMux);
    return copy;
}

uint32_t displayStateRevision() {
    portENTER_CRITICAL(&stateMux);
    const uint32_t revision = state.revision;
    portEXIT_CRITICAL(&stateMux);
    return revision;
}

void displayStateSetPower(int power) {
    const int normalized = power == 1 ? 1 : 0;
    portENTER_CRITICAL(&stateMux);
    if (state.systemPower != normalized) {
        state.systemPower = normalized;
        bumpRevisionLocked();
    }
    portEXIT_CRITICAL(&stateMux);
}

void displayStateSetMode(uint8_t mode) {
    if (!validMode(mode)) return;
    portENTER_CRITICAL(&stateMux);
    if (state.mode != mode) {
        state.mode = mode;
        bumpRevisionLocked();
    }
    portEXIT_CRITICAL(&stateMux);
}

void displayStateSetFanSpeed(uint8_t fanSpeed) {
    if (!validFanSpeed(fanSpeed)) return;
    portENTER_CRITICAL(&stateMux);
    if (state.fanSpeed != fanSpeed) {
        state.fanSpeed = fanSpeed;
        bumpRevisionLocked();
    }
    portEXIT_CRITICAL(&stateMux);
}

void displayStateSetStatusA(uint8_t status) {
    if (!validOutdoorStatus(status)) return;
    portENTER_CRITICAL(&stateMux);
    if (state.statusA != status) {
        state.statusA = status;
        bumpRevisionLocked();
    }
    portEXIT_CRITICAL(&stateMux);
}

void displayStateSetStatusB(uint8_t status) {
    if (!validOutdoorStatus(status)) return;
    portENTER_CRITICAL(&stateMux);
    if (state.statusB != status) {
        state.statusB = status;
        bumpRevisionLocked();
    }
    portEXIT_CRITICAL(&stateMux);
}

void displayStateSetOutdoorOnline(bool online) {
    portENTER_CRITICAL(&stateMux);
    if (state.outdoorOnline != online) {
        state.outdoorOnline = online;
        bumpRevisionLocked();
    }
    portEXIT_CRITICAL(&stateMux);
}

void displayStateSetOutdoorIoOk(bool ok) {
    portENTER_CRITICAL(&stateMux);
    if (state.outdoorIoOk != ok) {
        state.outdoorIoOk = ok;
        bumpRevisionLocked();
    }
    portEXIT_CRITICAL(&stateMux);
}

void displayStateSetIndoorEnabled(bool enabled) {
    portENTER_CRITICAL(&stateMux);
    if (state.indoorEnabled != enabled) {
        state.indoorEnabled = enabled;
        bumpRevisionLocked();
    }
    portEXIT_CRITICAL(&stateMux);
}

void displayStateSetOutdoorEnabled(bool enabled) {
    portENTER_CRITICAL(&stateMux);
    if (state.outdoorEnabled != enabled) {
        state.outdoorEnabled = enabled;
        bumpRevisionLocked();
    }
    portEXIT_CRITICAL(&stateMux);
}

void displayStateSetOutdoorSwitch(uint8_t index, bool value) {
    if (index >= 8) return;
    portENTER_CRITICAL(&stateMux);
    if (state.outdoorSwitch[index] != value) {
        state.outdoorSwitch[index] = value;
        bumpRevisionLocked();
    }
    portEXIT_CRITICAL(&stateMux);
}

void displayStateSetOutdoorRelay(uint8_t index, bool value) {
    if (index >= 4) return;
    portENTER_CRITICAL(&stateMux);
    if (state.outdoorRelay[index] != value) {
        state.outdoorRelay[index] = value;
        bumpRevisionLocked();
    }
    portEXIT_CRITICAL(&stateMux);
}

void displayStateSetSupplyTemp(float value) {
    portENTER_CRITICAL(&stateMux);
    if (floatChanged(state.supplyTemp, value)) {
        state.supplyTemp = value;
        bumpRevisionLocked();
    }
    portEXIT_CRITICAL(&stateMux);
}

void displayStateSetReturnTemp(float value) {
    portENTER_CRITICAL(&stateMux);
    if (floatChanged(state.returnTemp, value)) {
        state.returnTemp = value;
        bumpRevisionLocked();
    }
    portEXIT_CRITICAL(&stateMux);
}

void displayStateSetSetPoint(float value) {
    portENTER_CRITICAL(&stateMux);
    if (floatChanged(state.setPoint, value)) {
        state.setPoint = value;
        bumpRevisionLocked();
    }
    portEXIT_CRITICAL(&stateMux);
}
