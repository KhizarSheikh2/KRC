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
extern float OtherTempC;

extern String suctionPressure;
extern String dischargePressure;
extern String suctionPressureB;
extern String dischargePressureB;

extern int remoteCompStatus;
extern int remoteRunOutput;
extern int remoteAlarmOutput;
extern int remoteMqttConnected;
extern int remoteInternetConnected;
extern int remoteMachineShutdown;
extern String remoteHps;
extern String remoteLps;
extern String remoteOps;
extern String remoteDeviceName;

extern bool am4DisplayLinkConnected;
extern unsigned long am4LastStateReceivedMs;
extern uint32_t am4LastStateSequence;

extern bool am4RemoteTimeValid;
extern uint32_t am4RemoteEpochUtc;
extern unsigned long am4RemoteEpochReceivedMs;

extern String am4LastAckCommand;
extern String am4LastAckMessage;
extern bool am4LastAckAccepted;
extern unsigned long am4LastAckMs;

bool am4GetRemotePakistanTime(struct tm& timeInfo);
