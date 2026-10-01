#ifndef TEMP_H
#define TEMP_H

#include <Preferences.h>
#include <math.h>

OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

DeviceAddress tempAddress[TEMP_SENSOR_COUNT] = {};
bool tempSensorPresent[TEMP_SENSOR_COUNT] = {false, false, false, false, false, false};
uint8_t tempSensorRole[TEMP_SENSOR_COUNT] = {0, 0, 0, 0, 0, 0};
float tempSensorOffset[TEMP_SENSOR_COUNT] = {0, 0, 0, 0, 0, 0};
uint8_t detectedSensorCount = 0;

unsigned long lastTempRequestMs = 0;
bool tempConversionPending = false;

Preferences outdoorPreferences;

struct __attribute__((packed)) StoredSensorConfig
{
  uint8_t address[8];
  uint8_t role;
  int16_t offsetHundredths;
};

StoredSensorConfig storedSensorConfig[TEMP_SENSOR_COUNT] = {};
static const char *OUTDOOR_PREF_NAMESPACE = "AM5_outdoor";
static const char *OUTDOOR_PREF_KEY = "sensor_cfg";

inline bool tempAddressesEqual(const uint8_t *a, const uint8_t *b)
{
  for (uint8_t i = 0; i < 8; i++)
    if (a[i] != b[i]) return false;
  return true;
}

inline bool tempAddressIsEmpty(const uint8_t *address)
{
  for (uint8_t i = 0; i < 8; i++)
    if (address[i] != 0) return false;
  return true;
}

void printAddress(const DeviceAddress address)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (address[i] < 16) Serial.print("0");
    Serial.print(address[i], HEX);
  }
}

inline int findDetectedSensorByAddress(const uint8_t *address)
{
  for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
  {
    if (tempSensorPresent[i] && tempAddressesEqual(tempAddress[i], address))
      return i;
  }
  return -1;
}

inline int findStoredConfigByAddress(const uint8_t *address)
{
  for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
  {
    if (!tempAddressIsEmpty(storedSensorConfig[i].address) &&
        tempAddressesEqual(storedSensorConfig[i].address, address))
      return i;
  }
  return -1;
}

inline int findFreeStoredConfigSlot()
{
  // Prefer an actually empty slot.
  for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
    if (tempAddressIsEmpty(storedSensorConfig[i].address)) return i;

  // If all six slots belong to old/replaced sensors, reclaim one that is not
  // physically present now. This keeps sensor replacement serviceable.
  for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
    if (findDetectedSensorByAddress(storedSensorConfig[i].address) < 0) return i;

  return -1;
}

inline void saveStoredSensorConfig()
{
  outdoorPreferences.begin(OUTDOOR_PREF_NAMESPACE, false);
  const size_t written = outdoorPreferences.putBytes(
    OUTDOOR_PREF_KEY,
    storedSensorConfig,
    sizeof(storedSensorConfig)
  );
  outdoorPreferences.end();

  Serial.print("[TEMP-CONFIG] Preferences saved bytes=");
  Serial.println(written);
}

inline void loadStoredSensorConfig()
{
  memset(storedSensorConfig, 0, sizeof(storedSensorConfig));

  outdoorPreferences.begin(OUTDOOR_PREF_NAMESPACE, true);
  const size_t storedLength = outdoorPreferences.getBytesLength(OUTDOOR_PREF_KEY);

  if (storedLength == sizeof(storedSensorConfig))
  {
    outdoorPreferences.getBytes(
      OUTDOOR_PREF_KEY,
      storedSensorConfig,
      sizeof(storedSensorConfig)
    );
    Serial.println("[TEMP-CONFIG] Saved Outdoor sensor configuration loaded");
  }
  else
  {
    Serial.println("[TEMP-CONFIG] No valid saved configuration -> all roles unassigned");
  }

  outdoorPreferences.end();

  // Sanitize persisted data.
  bool usedRoles[TEMP_SENSOR_COUNT + 1] = {false};
  for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
  {
    if (storedSensorConfig[i].role > TEMP_SENSOR_COUNT)
      storedSensorConfig[i].role = 0;

    if (storedSensorConfig[i].role > 0)
    {
      if (usedRoles[storedSensorConfig[i].role])
        storedSensorConfig[i].role = 0;
      else
        usedRoles[storedSensorConfig[i].role] = true;
    }
  }
}

inline void applyStoredConfigurationToDetectedSensors()
{
  for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
  {
    tempSensorRole[i] = 0;
    tempSensorOffset[i] = 0.0f;

    if (!tempSensorPresent[i]) continue;

    const int cfgIndex = findStoredConfigByAddress(tempAddress[i]);
    if (cfgIndex < 0) continue;

    tempSensorRole[i] = storedSensorConfig[cfgIndex].role;
    tempSensorOffset[i] = static_cast<float>(storedSensorConfig[cfgIndex].offsetHundredths) / 100.0f;
  }
}

inline bool setOutdoorSensorConfig(const uint8_t *address, uint8_t role, float offset)
{
  if (!isfinite(offset))
  {
    Serial.println("[TEMP-CONFIG] Reject non-finite offset");
    return false;
  }

  if (role > TEMP_SENSOR_COUNT)
  {
    Serial.print("[TEMP-CONFIG] Reject invalid role=");
    Serial.println(role);
    return false;
  }

  const int detectedIndex = findDetectedSensorByAddress(address);
  if (detectedIndex < 0)
  {
    Serial.print("[TEMP-CONFIG] Reject unknown ROM=");
    printAddress(address);
    Serial.println();
    return false;
  }

  // One role can belong to only one physical sensor on this panel.
  if (role > 0)
  {
    for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
    {
      if (i != detectedIndex && tempSensorRole[i] == role)
        tempSensorRole[i] = 0;
    }

    for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
    {
      if (!tempAddressIsEmpty(storedSensorConfig[i].address) &&
          !tempAddressesEqual(storedSensorConfig[i].address, address) &&
          storedSensorConfig[i].role == role)
      {
        storedSensorConfig[i].role = 0;
      }
    }
  }

  tempSensorRole[detectedIndex] = role;

  int cfgIndex = findStoredConfigByAddress(address);
  if (cfgIndex < 0) cfgIndex = findFreeStoredConfigSlot();

  if (cfgIndex < 0)
  {
    Serial.println("[TEMP-CONFIG] ERROR: no free persistent config slot");
    return false;
  }

  memcpy(storedSensorConfig[cfgIndex].address, address, 8);
  storedSensorConfig[cfgIndex].role = role;

  long scaledOffset = lroundf(offset * 100.0f);
  if (scaledOffset > 32767L) scaledOffset = 32767L;
  if (scaledOffset < -32768L) scaledOffset = -32768L;
  storedSensorConfig[cfgIndex].offsetHundredths = static_cast<int16_t>(scaledOffset);
  // Use the exact persisted value at runtime as well, so reboot cannot change
  // the effective calibration by a rounding/clamping difference.
  tempSensorOffset[detectedIndex] = static_cast<float>(storedSensorConfig[cfgIndex].offsetHundredths) / 100.0f;

  saveStoredSensorConfig();

  Serial.print("[TEMP-CONFIG] Updated ROM=");
  printAddress(address);
  Serial.print(" role=");
  Serial.print(role);
  Serial.print(" offset=");
  Serial.println(offset, 2);

  return true;
}

inline float getOutdoorRoleTemperature(uint8_t role, bool &valid)
{
  valid = false;
  if (role < 1 || role > TEMP_SENSOR_COUNT) return TEMP_INVALID_VALUE;

  for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
  {
    if (!tempSensorPresent[i] || tempSensorRole[i] != role) continue;
    if (temperature[i] == TEMP_INVALID_VALUE) return TEMP_INVALID_VALUE;

    valid = true;
    return temperature[i] + tempSensorOffset[i];
  }

  return TEMP_INVALID_VALUE;
}

void requestNextTemperatureConversion()
{
  sensors.requestTemperatures();
  lastTempRequestMs = millis();
  tempConversionPending = true;
}

void initTemperature()
{
  Serial.println("[TEMP] -------------------------------------------------");
  Serial.print("[TEMP] DS18B20 bus GPIO = ");
  Serial.println(DS18B20_PIN);

  sensors.begin();
  sensors.setWaitForConversion(false);

  loadStoredSensorConfig();

  const int found = sensors.getDeviceCount();
  detectedSensorCount = 0;

  Serial.print("[TEMP] Devices found = ");
  Serial.println(found);

  for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
  {
    tempSensorPresent[i] = sensors.getAddress(tempAddress[i], i);

    Serial.print("[TEMP] Physical #");
    Serial.print(i);

    if (tempSensorPresent[i])
    {
      detectedSensorCount++;
      sensors.setResolution(tempAddress[i], 12);
      Serial.print(" FOUND | ROM=");
      printAddress(tempAddress[i]);
      Serial.println(" | resolution=12-bit");
    }
    else
    {
      memset(tempAddress[i], 0, 8);
      temperature[i] = TEMP_INVALID_VALUE;
      Serial.println(" NOT FOUND -> raw value=888.0");
    }
  }

  applyStoredConfigurationToDetectedSensors();

  for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
  {
    if (!tempSensorPresent[i]) continue;
    Serial.print("[TEMP-CONFIG] Physical #");
    Serial.print(i);
    Serial.print(" ROM=");
    printAddress(tempAddress[i]);
    Serial.print(" role=");
    Serial.print(tempSensorRole[i]);
    Serial.print(" offset=");
    Serial.println(tempSensorOffset[i], 2);
  }

  Serial.println("[TEMP] Non-blocking conversion started");
  Serial.println("[TEMP] -------------------------------------------------");
  requestNextTemperatureConversion();
}

void serviceTemperatures()
{
  if (!tempConversionPending)
  {
    requestNextTemperatureConversion();
    return;
  }

  if (millis() - lastTempRequestMs < TEMP_CONVERSION_MS)
    return;

  Serial.print("[TEMP] Raw update: ");

  for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
  {
    if (!tempSensorPresent[i])
    {
      temperature[i] = TEMP_INVALID_VALUE;
    }
    else
    {
      const float value = sensors.getTempC(tempAddress[i]);
      if (value == DEVICE_DISCONNECTED_C)
        temperature[i] = TEMP_INVALID_VALUE;
      else
        temperature[i] = value;
    }

    Serial.print("P");
    Serial.print(i);
    Serial.print("=");
    Serial.print(temperature[i], 1);
    Serial.print("C");
    if (i < TEMP_SENSOR_COUNT - 1) Serial.print(" | ");
  }

  Serial.println();

  tempConversionPending = false;
  requestNextTemperatureConversion();
}

#endif
