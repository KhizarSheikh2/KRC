#ifndef RELAY_CONTROL_H
#define RELAY_CONTROL_H

#include <Arduino.h>

#if (RELAY_LOW_PIN == RELAY_MEDIUM_PIN) || (RELAY_LOW_PIN == RELAY_HIGH_PIN) || \
    (RELAY_LOW_PIN == RELAY_HEAT_PIN) || (RELAY_MEDIUM_PIN == RELAY_HIGH_PIN) || \
    (RELAY_MEDIUM_PIN == RELAY_HEAT_PIN) || (RELAY_HIGH_PIN == RELAY_HEAT_PIN)
#error "AM6 relay GPIO pins must all be unique"
#endif

inline uint8_t relayOnLevel() { return RELAY_ACTIVE_LOW ? LOW : HIGH; }
inline uint8_t relayOffLevel() { return RELAY_ACTIVE_LOW ? HIGH : LOW; }

inline void relayWrite(uint8_t pin, bool state) {
  digitalWrite(pin, state ? relayOnLevel() : relayOffLevel());
}

inline void prepareRelayPin(uint8_t pin) {
  digitalWrite(pin, relayOffLevel());
  pinMode(pin, OUTPUT);
}

inline bool indoorOutputsEnabled() {
  return system_power == 1 && indoor_sw == 1;
}

inline void allFanRelaysOff() {
  relayWrite(RELAY_LOW_PIN, false);
  relayWrite(RELAY_MEDIUM_PIN, false);
  relayWrite(RELAY_HIGH_PIN, false);
}

inline void applyFanSpeed() {
  if (!indoorOutputsEnabled()) {
    allFanRelaysOff();
    return;
  }

  // Break-before-make: never energize two fan speed taps together.
  allFanRelaysOff();
  delay(100);

  switch (fan_speed) {
    case FAN_LOW:
      relayWrite(RELAY_LOW_PIN, true);
      Serial.println("[FAN] LOW");
      break;
    case FAN_MEDIUM:
      relayWrite(RELAY_MEDIUM_PIN, true);
      Serial.println("[FAN] MEDIUM");
      break;
    case FAN_HIGH:
      relayWrite(RELAY_HIGH_PIN, true);
      Serial.println("[FAN] HIGH");
      break;
    default:
      fan_speed = FAN_LOW;
      relayWrite(RELAY_LOW_PIN, true);
      Serial.println("[FAN] Invalid speed -> LOW");
      break;
  }
}

inline void applyHVACMode() {
  const bool heatOn = indoorOutputsEnabled() && hvac_mode == MODE_HEAT;
  relayWrite(RELAY_HEAT_PIN, heatOn);

  if (indoorOutputsEnabled()) {
    Serial.println(heatOn ? "[MODE] HEAT" : "[MODE] COOL");
  }
}

inline bool setFanSpeed(int requestedSpeed) {
  if (requestedSpeed < FAN_LOW || requestedSpeed > FAN_HIGH) {
    Serial.print("[FAN] Invalid requested speed: ");
    Serial.println(requestedSpeed);
    return false;
  }

  if (fan_speed == requestedSpeed) return true;

  fan_speed = requestedSpeed;
  if (indoorOutputsEnabled()) applyFanSpeed();
  return true;
}

inline bool setHVACMode(int requestedMode) {
  if (requestedMode != MODE_COOL && requestedMode != MODE_HEAT) {
    Serial.print("[MODE] Invalid requested mode: ");
    Serial.println(requestedMode);
    return false;
  }

  if (hvac_mode == requestedMode) return true;

  hvac_mode = requestedMode;
  applyHVACMode();
  return true;
}

inline bool setIndoorSwitch(int state) {
  if (state != 0 && state != 1) return false;
  if (indoor_sw == state) return true;

  indoor_sw = state;

  if (!indoorOutputsEnabled()) {
    allFanRelaysOff();
    relayWrite(RELAY_HEAT_PIN, false);
    Serial.println("[INDOOR] Disabled -> fan/heater relays OFF");
  }
  else {
    Serial.println("[INDOOR] Enabled");
    applyFanSpeed();
    applyHVACMode();
  }
  return true;
}

inline bool setOutdoorSwitch(int state) {
  if (state != 0 && state != 1) return false;
  if (outdoor_sw == state) {
    rs485_urgent_outdoor_sync = true;
    return true;
  }

  outdoor_sw = state;
  rs485_urgent_outdoor_sync = true;
  Serial.print("[OUTDOOR] Application enable = ");
  Serial.println(outdoor_sw);
  return true;
}

inline void saveSystemPowerPreference() {
  preferences.begin("values", false);
  preferences.putInt("power_state", system_power == 1 ? 1 : 0);
  preferences.end();

  Serial.print("[PREF] power_state saved = ");
  Serial.println(system_power);
}

inline void setSystemPower(int state, bool persist = true) {
  // Only 0 (OFF) and 1 (ON) are valid power states.
  state = (state == 1) ? 1 : 0;

  // system_power is the single authoritative complete-system ON/OFF state.
  if (system_power == state) {
    // Re-send the authoritative state to Outdoor even if the requested state
    // did not change. This helps a newly booted Outdoor slave synchronize.
    rs485_urgent_outdoor_sync = true;
    return;
  }

  system_power = state;
  rs485_urgent_outdoor_sync = true;

  // The Indoor/master owns persistent complete-system power state. Outdoor
  // never uses stored state as permission to energize its relays.
  if (persist) saveSystemPowerPreference();

  if (system_power == 0) {
    allFanRelaysOff();
    relayWrite(RELAY_HEAT_PIN, false);
    Serial.println("[SYSTEM] OFF - complete AM6 system disabled");
    return;
  }

  Serial.println("[SYSTEM] ON");

  if (indoor_sw == 1) {
    // No fan-OFF state while the Indoor section is enabled.
    applyFanSpeed();
    applyHVACMode();
  }
  else {
    allFanRelaysOff();
    relayWrite(RELAY_HEAT_PIN, false);
    Serial.println("[INDOOR] Section remains disabled by indoorsw=0");
  }
}

inline void relayInit() {
  prepareRelayPin(RELAY_LOW_PIN);
  prepareRelayPin(RELAY_MEDIUM_PIN);
  prepareRelayPin(RELAY_HIGH_PIN);
  prepareRelayPin(RELAY_HEAT_PIN);

  allFanRelaysOff();
  relayWrite(RELAY_HEAT_PIN, false);

  system_power = 0;
  if (indoor_sw != 0 && indoor_sw != 1) indoor_sw = 1;
  if (outdoor_sw != 0 && outdoor_sw != 1) outdoor_sw = 1;
  if (fan_speed < FAN_LOW || fan_speed > FAN_HIGH) fan_speed = FAN_LOW;
  if (hvac_mode != MODE_COOL && hvac_mode != MODE_HEAT) hvac_mode = MODE_COOL;

  rs485_urgent_outdoor_sync = true;
  Serial.println("[RELAY] AM6 Indoor initialized - complete system OFF");
}

#endif
