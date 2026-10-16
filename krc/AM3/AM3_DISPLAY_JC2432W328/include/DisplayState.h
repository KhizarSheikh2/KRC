#pragma once

#include <Arduino.h>
#include <time.h>

constexpr float SENSOR_DISCONNECTED = 888.0f;
constexpr float SENSOR_NOT_SELECTED = 999.0f;

extern float ReturnTempC;
extern float SuctionTempC;
extern float dischargeTempC;
extern float SupplyTempC;
extern float OilTempC;

extern String suctionPressure;
extern String dischargePressure;
extern String suctionPressureB;
extern String dischargePressureB;

extern int remoteCompStatus;
extern int remoteRunOutput;
extern int remoteAlarmOutput;
extern int remoteMachineShutdown;
extern int remoteMqttConnected;
extern int remoteInternetConnected;
extern String remoteHps;
extern String remoteLps;
extern String remoteOps;
extern String remoteDeviceName;

extern bool am3DisplayLinkConnected;
extern unsigned long am3LastStateReceivedMs;
extern uint32_t am3LastStateSequence;

extern bool am3RemoteTimeValid;
extern uint32_t am3RemoteEpochUtc;
extern unsigned long am3RemoteEpochReceivedMs;

extern String am3LastCommandMessage;
extern unsigned long am3LastCommandAckMs;
extern bool am3LastCommandAccepted;

bool am3GetRemotePakistanTime(struct tm& timeInfo);
