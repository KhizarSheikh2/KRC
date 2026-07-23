#pragma once

#include <Arduino.h>
#include <time.h>

constexpr float SENSOR_DISCONNECTED = 888.0f;
constexpr float SENSOR_NOT_SELECTED = 999.0f;

float ReturnTempC = SENSOR_NOT_SELECTED;
float SuctionTempC = SENSOR_NOT_SELECTED;
float dischargeTempC = SENSOR_NOT_SELECTED;
float SupplyTempC = SENSOR_NOT_SELECTED;
float OilTempC = SENSOR_NOT_SELECTED;
float OtherTempC = SENSOR_NOT_SELECTED;

String suctionPressure = "999";
String dischargePressure = "999";
String suctionPressureB = "999";
String dischargePressureB = "999";

int remoteCompStatus = 0;
int remoteRunOutput = 0;
int remoteAlarmOutput = 0;
int remoteMqttConnected = 0;
int remoteInternetConnected = 0;
String remoteHps = "";
String remoteLps = "";
String remoteOps = "";
String remoteDeviceName = "AM4";

bool am4DisplayLinkConnected = false;
unsigned long am4LastStateReceivedMs = 0;
uint32_t am4LastStateSequence = 0;

bool am4RemoteTimeValid = false;
uint32_t am4RemoteEpochUtc = 0;
unsigned long am4RemoteEpochReceivedMs = 0;

bool am4GetRemotePakistanTime(struct tm& timeInfo) {
  if (!am4RemoteTimeValid) {
    return false;
  }

  const uint32_t elapsedSeconds = (millis() - am4RemoteEpochReceivedMs) / 1000UL;
  const time_t pakistanEpoch = static_cast<time_t>(am4RemoteEpochUtc)
                             + static_cast<time_t>(elapsedSeconds)
                             + static_cast<time_t>(5 * 60 * 60);
  return gmtime_r(&pakistanEpoch, &timeInfo) != nullptr;
}
