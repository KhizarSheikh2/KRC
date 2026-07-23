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
String remoteDeviceName = "AM3";

bool am3DisplayLinkConnected = false;
unsigned long am3LastStateReceivedMs = 0;
uint32_t am3LastStateSequence = 0;

bool am3RemoteTimeValid = false;
uint32_t am3RemoteEpochUtc = 0;
unsigned long am3RemoteEpochReceivedMs = 0;

bool am3GetRemotePakistanTime(struct tm& timeInfo) {
  if (!am3RemoteTimeValid) {
    return false;
  }

  const uint32_t elapsedSeconds = (millis() - am3RemoteEpochReceivedMs) / 1000UL;
  const time_t pakistanEpoch = static_cast<time_t>(am3RemoteEpochUtc)
                             + static_cast<time_t>(elapsedSeconds)
                             + static_cast<time_t>(5 * 60 * 60);
  return gmtime_r(&pakistanEpoch, &timeInfo) != nullptr;
}
