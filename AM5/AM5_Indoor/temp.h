#ifndef TEMP_H
#define TEMP_H

#include <Preferences.h>
#include <math.h>

// =====================================================
// AM5 INDOOR DS18B20 CONFIGURATION
// Persistent configuration is attached to each DS18B20 ROM address, exactly
// like the Outdoor panel: ROM -> role + offset.
// =====================================================

Preferences indoorTempPreferences;

struct __attribute__((packed)) StoredIndoorSensorConfig {
  uint8_t address[8];
  uint8_t role;
  int16_t offsetHundredths;
};

StoredIndoorSensorConfig storedIndoorSensorConfig[INDOOR_SENSOR_COUNT] = {};
static const char* INDOOR_TEMP_PREF_NAMESPACE = "am5_indoor";
static const char* INDOOR_TEMP_PREF_KEY = "sensor_cfg";

inline bool compareAddresses(const uint8_t* a, const uint8_t* b) {
  for (uint8_t i = 0; i < 8; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

inline bool addressIsEmpty(const uint8_t* address) {
  for (uint8_t i = 0; i < 8; i++) {
    if (address[i] != 0) return false;
  }
  return true;
}

void printAddress(const DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print('0');
    Serial.print(deviceAddress[i], HEX);
  }
}

String getAddressString(const DeviceAddress deviceAddress) {
  String address;
  address.reserve(16);
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) address += '0';
    address += String(deviceAddress[i], HEX);
  }
  address.toLowerCase();
  return address;
}

inline int findIndoorDetectedSensorByAddress(const uint8_t* address) {
  for (uint8_t i = 0; i < INDOOR_SENSOR_COUNT; i++) {
    if (tempSensorPresent[i] && compareAddresses(tempSensorAddresses[i], address)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

inline int findIndoorStoredConfigByAddress(const uint8_t* address) {
  for (uint8_t i = 0; i < INDOOR_SENSOR_COUNT; i++) {
    if (!addressIsEmpty(storedIndoorSensorConfig[i].address) &&
        compareAddresses(storedIndoorSensorConfig[i].address, address)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

inline int findFreeIndoorStoredConfigSlot() {
  for (uint8_t i = 0; i < INDOOR_SENSOR_COUNT; i++) {
    if (addressIsEmpty(storedIndoorSensorConfig[i].address)) {
      return static_cast<int>(i);
    }
  }

  // If all six persistent slots are occupied, reclaim one belonging to a ROM
  // that is no longer physically present. This supports sensor replacement.
  for (uint8_t i = 0; i < INDOOR_SENSOR_COUNT; i++) {
    if (findIndoorDetectedSensorByAddress(storedIndoorSensorConfig[i].address) < 0) {
      return static_cast<int>(i);
    }
  }

  return -1;
}

inline void saveIndoorStoredSensorConfig() {
  indoorTempPreferences.begin(INDOOR_TEMP_PREF_NAMESPACE, false);
  const size_t written = indoorTempPreferences.putBytes(
    INDOOR_TEMP_PREF_KEY,
    storedIndoorSensorConfig,
    sizeof(storedIndoorSensorConfig)
  );
  indoorTempPreferences.end();

  Serial.print("[TEMP-CONFIG] Indoor Preferences saved bytes=");
  Serial.println(written);
}

inline void loadIndoorStoredSensorConfig() {
  memset(storedIndoorSensorConfig, 0, sizeof(storedIndoorSensorConfig));

  indoorTempPreferences.begin(INDOOR_TEMP_PREF_NAMESPACE, true);
  const size_t storedLength = indoorTempPreferences.getBytesLength(INDOOR_TEMP_PREF_KEY);

  if (storedLength == sizeof(storedIndoorSensorConfig)) {
    indoorTempPreferences.getBytes(
      INDOOR_TEMP_PREF_KEY,
      storedIndoorSensorConfig,
      sizeof(storedIndoorSensorConfig)
    );
    Serial.println("[TEMP-CONFIG] Saved Indoor ROM-based configuration loaded");
  }
  else {
    Serial.println("[TEMP-CONFIG] No ROM-based Indoor configuration -> roles unassigned");
  }
  indoorTempPreferences.end();

  // Sanitize persisted data and guarantee role uniqueness.
  bool usedRoles[INDOOR_SENSOR_COUNT + 1] = {false};
  bool changed = false;

  for (uint8_t i = 0; i < INDOOR_SENSOR_COUNT; i++) {
    if (storedIndoorSensorConfig[i].role > INDOOR_SENSOR_COUNT) {
      storedIndoorSensorConfig[i].role = 0;
      changed = true;
    }

    const uint8_t role = storedIndoorSensorConfig[i].role;
    if (role > 0) {
      if (usedRoles[role]) {
        storedIndoorSensorConfig[i].role = 0;
        changed = true;
      }
      else {
        usedRoles[role] = true;
      }
    }
  }

  if (changed) saveIndoorStoredSensorConfig();
}

inline void applyIndoorStoredConfigurationToDetectedSensors() {
  for (uint8_t i = 0; i < INDOOR_SENSOR_COUNT; i++) {
    tempSensorRole[i] = 0;
    tempSensorOffset[i] = 0.0f;

    if (!tempSensorPresent[i]) continue;

    const int cfgIndex = findIndoorStoredConfigByAddress(tempSensorAddresses[i]);
    if (cfgIndex < 0) continue;

    tempSensorRole[i] = storedIndoorSensorConfig[cfgIndex].role;
    tempSensorOffset[i] =
      static_cast<float>(storedIndoorSensorConfig[cfgIndex].offsetHundredths) / 100.0f;
  }
}

inline uint8_t getIndoorAssignmentForAddress(const DeviceAddress address) {
  const int index = findIndoorDetectedSensorByAddress(address);
  return (index >= 0) ? tempSensorRole[index] : 0;
}

inline float getSensorOffsetByAddress(const DeviceAddress address) {
  const int index = findIndoorDetectedSensorByAddress(address);
  return (index >= 0) ? tempSensorOffset[index] : 0.0f;
}

inline bool setIndoorSensorConfig(const uint8_t* address, uint8_t role, float offset) {
  if (role > INDOOR_SENSOR_COUNT) {
    Serial.print("[TEMP-CONFIG] Reject invalid Indoor role=");
    Serial.println(role);
    return false;
  }

  if (!isfinite(offset)) {
    Serial.println("[TEMP-CONFIG] Reject non-finite Indoor offset");
    return false;
  }

  // Keep encoded offset inside the int16 wire/storage range.
  if (offset > 327.67f) offset = 327.67f;
  if (offset < -327.68f) offset = -327.68f;

  const int detectedIndex = findIndoorDetectedSensorByAddress(address);
  if (detectedIndex < 0) {
    Serial.print("[TEMP-CONFIG] Reject unknown Indoor ROM=");
    printAddress(address);
    Serial.println();
    return false;
  }

  // One role may belong to only one sensor. If this role is selected for this
  // ROM, release it from all other stored/detected Indoor sensors first.
  if (role > 0) {
    for (uint8_t i = 0; i < INDOOR_SENSOR_COUNT; i++) {
      if (static_cast<int>(i) != detectedIndex && tempSensorRole[i] == role) {
        tempSensorRole[i] = 0;
      }
    }

    for (uint8_t i = 0; i < INDOOR_SENSOR_COUNT; i++) {
      if (!addressIsEmpty(storedIndoorSensorConfig[i].address) &&
          !compareAddresses(storedIndoorSensorConfig[i].address, address) &&
          storedIndoorSensorConfig[i].role == role) {
        storedIndoorSensorConfig[i].role = 0;
      }
    }
  }

  int cfgIndex = findIndoorStoredConfigByAddress(address);
  if (cfgIndex < 0) cfgIndex = findFreeIndoorStoredConfigSlot();

  if (cfgIndex < 0) {
    Serial.println("[TEMP-CONFIG] ERROR: no free Indoor persistent config slot");
    return false;
  }

  memcpy(storedIndoorSensorConfig[cfgIndex].address, address, 8);
  storedIndoorSensorConfig[cfgIndex].role = role;

  long scaledOffset = lroundf(offset * 100.0f);
  if (scaledOffset > 32767L) scaledOffset = 32767L;
  if (scaledOffset < -32768L) scaledOffset = -32768L;
  storedIndoorSensorConfig[cfgIndex].offsetHundredths =
    static_cast<int16_t>(scaledOffset);

  tempSensorRole[detectedIndex] = role;
  tempSensorOffset[detectedIndex] =
    static_cast<float>(storedIndoorSensorConfig[cfgIndex].offsetHundredths) / 100.0f;

  saveIndoorStoredSensorConfig();

  Serial.print("[TEMP-CONFIG] Indoor ROM=");
  printAddress(address);
  Serial.print(" role=");
  Serial.print(role);
  Serial.print(" offset=");
  Serial.println(tempSensorOffset[detectedIndex], 2);
  return true;
}

#endif
