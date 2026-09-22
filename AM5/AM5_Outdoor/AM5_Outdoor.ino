#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "Def.h"
#include "IO.h"
#include "temp.h"
#include "RS485.h"

unsigned long lastSerialStatusMs = 0;

void printPeriodicStatus()
{
  const unsigned long now = millis();
  if (now - lastSerialStatusMs < SERIAL_STATUS_INTERVAL_MS)
    return;

  lastSerialStatusMs = now;

  Serial.println("[STATUS] =================================================");
  Serial.print("[STATUS] Uptime(ms)="); Serial.print(now);
  Serial.print(" | systemPower="); Serial.println(systemPower);

  Serial.print("[STATUS] PCF Switch=0x"); Serial.print(SWITCH_PCF_ADDRESS, HEX);
  Serial.print(switchPcfHealthy ? " OK" : " FAIL");
  Serial.print(" | Relay PCA9554=0x"); Serial.print(RELAY_PCA_ADDRESS, HEX);
  Serial.print(relayPcaHealthy ? " OK" : " FAIL");
  Serial.print(" | Configured="); Serial.println(relayPcaConfigured ? "YES" : "NO");

  Serial.print("[STATUS] Switch RAW=0x");
  if (switchPcfData < 0x10) Serial.print("0");
  Serial.print(switchPcfData, HEX);
  Serial.print(" | ");
  for (uint8_t i = 0; i < SWITCH_COUNT; i++)
  {
    Serial.print("S"); Serial.print(i + 1); Serial.print("=");
    Serial.print(switchState[i] ? 1 : 0);
    if (i < SWITCH_COUNT - 1) Serial.print(" ");
  }
  Serial.println();

  Serial.print("[STATUS] Relays: R1="); Serial.print(R1_State ? 1 : 0);
  Serial.print(" R2="); Serial.print(R2_State ? 1 : 0);
  Serial.print(" R3="); Serial.print(R3_State ? 1 : 0);
  Serial.print(" R4="); Serial.println(R4_State ? 1 : 0);

  const uint8_t statusCode = currentOutdoorStatusCode();
  Serial.print("[STATUS] statusout="); Serial.print(statusCode);
  Serial.print(" ("); Serial.print(outdoorStatusText(statusCode)); Serial.println(")");

  Serial.print("[STATUS] Temps: ");
  for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
  {
    Serial.print("T"); Serial.print(i); Serial.print("=");
    Serial.print(temperature[i], 1); Serial.print("C");
    if (i < TEMP_SENSOR_COUNT - 1) Serial.print(" | ");
  }
  Serial.println();

  Serial.print("[STATUS] Sensor config: ");
  for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
  {
    if (!tempSensorPresent[i]) continue;
    Serial.print("P"); Serial.print(i);
    Serial.print("->Role"); Serial.print(tempSensorRole[i]);
    Serial.print("(off="); Serial.print(tempSensorOffset[i], 2); Serial.print(") ");
  }
  Serial.println();

  Serial.print("[STATUS] Fresh Indoor B0 power command = ");
  Serial.print(validIndoorCommandSeen ? "YES" : "NO");
  if (validIndoorCommandSeen)
  {
    Serial.print(" | last valid request age(ms)=");
    Serial.print(now - lastValidIndoorCommandMs);
  }
  Serial.println();
  Serial.println("[STATUS] =================================================");
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("=========================================================");
  Serial.println(" AM6 OUTDOOR FINAL - PCF8574 SWITCH + PCA9554 RELAY + INDOOR RS485");
  Serial.println("=========================================================");
  Serial.println("[BOOT] Serial Monitor = 115200 baud");
  Serial.println("[BOOT] Starting hardware initialization...");

  initIO();
  initTemperature();
  initRS485();

  Serial.println("[BOOT] Initialization complete");
  Serial.println("[BOOT] Indoor master = 0x01");
  Serial.println("[BOOT] Outdoor slave = 0x02");
  Serial.println("[BOOT] Outdoor replies only after Indoor request");
  Serial.println("[BOOT] SW1..SW8 = Switch PCF8574");
  Serial.println("[BOOT] R1..R4   = Relay PCA9554 @ 0x20 (register based)");
  Serial.println("[BOOT] Waiting for Indoor RS485 commands...");
  Serial.println("=========================================================");
}

void loop()
{
  readSwitches();

  // Safety first: Outdoor is a slave and may operate relays only while a fresh
  // valid B0 power command lease from Indoor is present.
  serviceIndoorPowerCommandSafety();
  controlRelays();

  serviceTemperatures();
  rs485Loop();
  printPeriodicStatus();

  delay(2);
}
