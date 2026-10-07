#ifndef AM6_LOCAL_SWITCHES_H
#define AM6_LOCAL_SWITCHES_H

#include <Arduino.h>

// ============================================================
// AM6 INDOOR LOCAL CONTROL SWITCHES - INDUSTRIAL FAIL-SAFE
// Physical switches are authoritative for:
//   GPIO19 : complete-system power (HIGH=ON)
//   GPIO18 : HVAC mode            (HIGH=HEAT, LOW=COOL)
//   GPIO23 : fan LOW              (HIGH=selected)
//   GPIO22 : fan MEDIUM           (HIGH=selected)
//   GPIO21 : fan HIGH             (HIGH=selected)
//
// Safety behavior:
//   * POWER OFF requires 120 ms stable LOW to reject electrical spikes.
//   * POWER ON must remain HIGH for LOCAL_POWER_ON_QUALIFY_MS.
//   * Any brief LOW/bounce cancels startup and restarts the qualification.
//   * Fan/mode inputs remain debounced independently while power is OFF.
//   * Blank/invalid fan selector positions hold the last valid fan speed.
// ============================================================

static uint8_t localControlStableMask = 0;      // fan + mode only, bits 0..3
static uint8_t localControlCandidateMask = 0;
static unsigned long localControlCandidateSinceMs = 0;
static bool localSwitchInitialized = false;

static bool localPowerHighTiming = false;
static unsigned long localPowerHighSinceMs = 0;
static bool localPowerStartupAnnounced = false;
static bool localPowerLowTiming = false;
static unsigned long localPowerLowSinceMs = 0;

inline uint8_t readLocalSwitchMask() {
  uint8_t mask = 0;
  if (digitalRead(SW_FAN_LOW_PIN) == HIGH)       mask |= 0x01;
  if (digitalRead(SW_FAN_MEDIUM_PIN) == HIGH)    mask |= 0x02;
  if (digitalRead(SW_FAN_HIGH_PIN) == HIGH)      mask |= 0x04;
  if (digitalRead(SW_HEAT_COOL_PIN) == HIGH)     mask |= 0x08;
  if (digitalRead(SW_SYSTEM_POWER_PIN) == HIGH)  mask |= 0x10;
  return mask;
}

inline bool fanSpeedFromLocalMask(uint8_t mask, int& speedOut) {
  switch (mask & 0x07U) {
    case 0x01:
      speedOut = FAN_LOW;
      return true;
    case 0x02:
      speedOut = FAN_MEDIUM;
      return true;
    case 0x04:
      speedOut = FAN_HIGH;
      return true;
    default:
      return false;
  }
}

inline void saveLocalSelectionPreferences(bool saveFan, bool saveMode) {
  if (!saveFan && !saveMode) return;

  preferences.begin("values", false);
  if (saveFan) preferences.putInt("fan_speed", fan_speed);
  if (saveMode) preferences.putInt("hvac_mode", hvac_mode);
  preferences.end();
}

// Apply only fan/mode. Power is deliberately handled by a separate safety
// state machine so a contact transition can never chatter the relays.
inline void applyLocalFanModeMask(uint8_t controlMask, bool force = false) {
  const int requestedMode = (controlMask & 0x08U) ? MODE_HEAT : MODE_COOL;

  int requestedFan = fan_speed;
  const bool fanSelectionValid = fanSpeedFromLocalMask(controlMask, requestedFan);

  const int oldFan = fan_speed;
  const int oldMode = hvac_mode;

  if (fanSelectionValid) {
    setFanSpeed(requestedFan);
  }
  else if (force || (controlMask & 0x07U) != 0) {
    Serial.print("[LOCAL SWITCH] Invalid/blank fan selector LOW/MED/HIGH=");
    Serial.print((controlMask & 0x01U) ? 1 : 0);
    Serial.print("/");
    Serial.print((controlMask & 0x02U) ? 1 : 0);
    Serial.print("/");
    Serial.print((controlMask & 0x04U) ? 1 : 0);
    Serial.print(" -> holding last valid speed ");
    Serial.println(fan_speed);
  }

  setHVACMode(requestedMode);

  const bool fanChanged = oldFan != fan_speed;
  const bool modeChanged = oldMode != hvac_mode;
  saveLocalSelectionPreferences(fanChanged, modeChanged);

  if (fanChanged || modeChanged || force) {
    mqtt_publish_requested = true;

    Serial.print("[LOCAL SWITCH] mode=");
    Serial.print(hvac_mode == MODE_HEAT ? "HEAT" : "COOL");
    Serial.print(" fan=");
    if (fan_speed == FAN_LOW) Serial.print("LOW");
    else if (fan_speed == FAN_MEDIUM) Serial.print("MEDIUM");
    else Serial.print("HIGH");
    Serial.print(" | raw LOW/MED/HIGH=");
    Serial.print((controlMask & 0x01U) ? 1 : 0);
    Serial.print("/");
    Serial.print((controlMask & 0x02U) ? 1 : 0);
    Serial.print("/");
    Serial.println((controlMask & 0x04U) ? 1 : 0);
  }
}

inline void localSwitchesInit() {
  pinMode(SW_FAN_LOW_PIN, INPUT_PULLDOWN);
  pinMode(SW_FAN_MEDIUM_PIN, INPUT_PULLDOWN);
  pinMode(SW_FAN_HIGH_PIN, INPUT_PULLDOWN);
  pinMode(SW_HEAT_COOL_PIN, INPUT_PULLDOWN);
  pinMode(SW_SYSTEM_POWER_PIN, INPUT_PULLDOWN);

  // relayInit() has already forced every Indoor output OFF. Keep it that way
  // through the complete setup sequence even if GPIO19 is physically ON.
  system_power = 0;
  allFanRelaysOff();
  relayWrite(RELAY_HEAT_PIN, false);

  delay(LOCAL_SWITCH_DEBOUNCE_MS);
  uint8_t raw = readLocalSwitchMask();
  localControlCandidateMask = raw & 0x0FU;
  delay(LOCAL_SWITCH_DEBOUNCE_MS);
  raw = readLocalSwitchMask();
  localControlStableMask = raw & 0x0FU;
  localControlCandidateMask = localControlStableMask;
  localControlCandidateSinceMs = millis();

  // Fan/mode may be sampled while the system is OFF; this prepares the desired
  // state without energizing any relay.
  applyLocalFanModeMask(localControlStableMask, true);

  if ((raw & 0x10U) != 0) {
    localPowerHighTiming = true;
    localPowerHighSinceMs = millis();
    Serial.print("[LOCAL POWER] GPIO19 is HIGH at boot -> keeping outputs OFF for ");
    Serial.print(LOCAL_POWER_ON_QUALIFY_MS);
    Serial.println(" ms qualification");
  }
  else {
    localPowerHighTiming = false;
    localPowerHighSinceMs = 0;
    saveSystemPowerPreference(); // physical switch is confirmed OFF
    Serial.println("[LOCAL POWER] GPIO19 is LOW at boot -> system remains OFF");
  }

  localPowerStartupAnnounced = false;
  localPowerLowTiming = false;
  localPowerLowSinceMs = 0;
  localSwitchInitialized = true;
  rs485_urgent_outdoor_sync = true; // first Outdoor command is always OFF

  Serial.println("[LOCAL SWITCH] Physical controls initialized (industrial startup guard enabled)");
  Serial.print("[LOCAL SWITCH] GPIO power/mode/low/med/high = ");
  Serial.print(SW_SYSTEM_POWER_PIN); Serial.print("/");
  Serial.print(SW_HEAT_COOL_PIN); Serial.print("/");
  Serial.print(SW_FAN_LOW_PIN); Serial.print("/");
  Serial.print(SW_FAN_MEDIUM_PIN); Serial.print("/");
  Serial.println(SW_FAN_HIGH_PIN);
}

inline void serviceLocalSwitches() {
  if (!localSwitchInitialized) return;

  const unsigned long now = millis();
  const uint8_t rawMask = readLocalSwitchMask();
  const uint8_t rawControlMask = rawMask & 0x0FU;
  const bool rawPowerHigh = (rawMask & 0x10U) != 0;

  // ------------------------------------------------------------
  // 1) MASTER POWER: OFF NOISE-FILTERED, ON QUALIFIED/DELAYED
  // ------------------------------------------------------------
  if (!rawPowerHigh) {
    // GPIO19 is physically wired through a cabinet toggle switch. Reject very
    // short LOW spikes/noise instead of treating one CPU sample as a real OFF
    // command. A genuine OFF is still acted on after only 120 ms.
    localPowerHighTiming = false;
    localPowerHighSinceMs = 0;
    localPowerStartupAnnounced = false;

    if (system_power != 0) {
      if (!localPowerLowTiming) {
        localPowerLowTiming = true;
        localPowerLowSinceMs = now;
      }
      if (now - localPowerLowSinceMs >= LOCAL_POWER_OFF_DEBOUNCE_MS) {
        Serial.println("[LOCAL POWER] GPIO19 LOW stable -> SYSTEM OFF");
        setSystemPower(0, true);
        mqtt_publish_requested = true;
        rs485_urgent_outdoor_sync = true;
        localPowerLowTiming = false;
      }
    } else {
      localPowerLowTiming = false;
      localPowerLowSinceMs = 0;
    }
  }
  else {
    // Any HIGH immediately cancels a pending false/noisy LOW sample.
    localPowerLowTiming = false;
    localPowerLowSinceMs = 0;

    if (system_power == 0) {
    if (!localPowerHighTiming) {
      localPowerHighTiming = true;
      localPowerHighSinceMs = now;
      localPowerStartupAnnounced = false;
    }

    if (!localPowerStartupAnnounced) {
      Serial.print("[LOCAL POWER] GPIO19 HIGH -> qualifying for ");
      Serial.print(LOCAL_POWER_ON_QUALIFY_MS);
      Serial.println(" ms before startup");
      localPowerStartupAnnounced = true;
    }

    if (now - localPowerHighSinceMs >= LOCAL_POWER_ON_QUALIFY_MS) {
      // Re-apply the stable selector state immediately before start, then
      // energize the Indoor only after all choices are known and stable.
      applyLocalFanModeMask(localControlStableMask, false);
      Serial.println("[LOCAL POWER] Qualification complete -> SYSTEM ON");
      setSystemPower(1, true);
      mqtt_publish_requested = true;
      rs485_urgent_outdoor_sync = true;
      localPowerHighTiming = false;
    }
    }
  }

  // ------------------------------------------------------------
  // 2) FAN / MODE: ordinary debounce, independent from master power
  // ------------------------------------------------------------
  if (rawControlMask != localControlCandidateMask) {
    localControlCandidateMask = rawControlMask;
    localControlCandidateSinceMs = now;
    return;
  }

  if (rawControlMask == localControlStableMask) return;
  if (now - localControlCandidateSinceMs < LOCAL_SWITCH_DEBOUNCE_MS) return;

  localControlStableMask = rawControlMask;
  applyLocalFanModeMask(localControlStableMask, false);
}

#endif
