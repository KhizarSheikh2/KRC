#ifndef OUTDOOR_STATUS_LOGIC_H
#define OUTDOOR_STATUS_LOGIC_H

#include <stdint.h>

// AM6 Outdoor status values shared with the mobile app:
// 0 = Stopped
// 1 = Running
// 2 = High PSI
// 3 = Low PSI
// 4 = Power Fault
// 5 = Overload / Tripped
static constexpr uint8_t AM6_OUT_STATUS_STOPPED = 0;
static constexpr uint8_t AM6_OUT_STATUS_RUNNING = 1;
static constexpr uint8_t AM6_OUT_STATUS_HIGH_PSI = 2;
static constexpr uint8_t AM6_OUT_STATUS_LOW_PSI = 3;
static constexpr uint8_t AM6_OUT_STATUS_POWER_FAULT = 4;
static constexpr uint8_t AM6_OUT_STATUS_OVERLOAD_TRIPPED = 5;

// Each circuit is evaluated independently using the exact same priority:
// High PSI -> Low PSI -> Power Fault -> Overload/Tripped -> Running.
// A switch value of true/1 means healthy; false/0 means that fault is active.
inline uint8_t computeOutdoorCircuitStatusCode(int systemPower,
                                               int outdoorEnable,
                                               bool switchPcfHealthy,
                                               bool relayPcaHealthy,
                                               bool relayPcaConfigured,
                                               bool highPsiHealthy,
                                               bool lowPsiHealthy,
                                               bool overloadHealthy,
                                               bool powerHealthy)
{
  if (systemPower != 1 || outdoorEnable != 1)
    return AM6_OUT_STATUS_STOPPED;

  // If the switch expander is unavailable, neither circuit's protection
  // inputs can be trusted. Report Power Fault for both circuits.
  if (!switchPcfHealthy)
    return AM6_OUT_STATUS_POWER_FAULT;

  if (!highPsiHealthy)
    return AM6_OUT_STATUS_HIGH_PSI;

  if (!lowPsiHealthy)
    return AM6_OUT_STATUS_LOW_PSI;

  // Relay output-expander failure is also a Power Fault because the
  // compressor/condensor pair cannot be controlled reliably.
  if (!powerHealthy || !relayPcaHealthy || !relayPcaConfigured)
    return AM6_OUT_STATUS_POWER_FAULT;

  if (!overloadHealthy)
    return AM6_OUT_STATUS_OVERLOAD_TRIPPED;

  return AM6_OUT_STATUS_RUNNING;
}

inline uint8_t computeOutdoorStatusA(int systemPower,
                                     int outdoorEnable,
                                     bool switchPcfHealthy,
                                     bool relayPcaHealthy,
                                     bool relayPcaConfigured,
                                     const bool switchState[8])
{
  // Circuit A: SW1 High PSI, SW2 Low PSI, SW3 Overload, SW4 Power.
  return computeOutdoorCircuitStatusCode(systemPower, outdoorEnable,
                                         switchPcfHealthy, relayPcaHealthy, relayPcaConfigured,
                                         switchState[0], switchState[1],
                                         switchState[2], switchState[3]);
}

inline uint8_t computeOutdoorStatusB(int systemPower,
                                     int outdoorEnable,
                                     bool switchPcfHealthy,
                                     bool relayPcaHealthy,
                                     bool relayPcaConfigured,
                                     const bool switchState[8])
{
  // Circuit B: SW5 High PSI, SW6 Low PSI, SW7 Overload, SW8 Power.
  return computeOutdoorCircuitStatusCode(systemPower, outdoorEnable,
                                         switchPcfHealthy, relayPcaHealthy, relayPcaConfigured,
                                         switchState[4], switchState[5],
                                         switchState[6], switchState[7]);
}

inline const char* outdoorStatusText(uint8_t status)
{
  switch (status)
  {
    case AM6_OUT_STATUS_STOPPED: return "Stopped";
    case AM6_OUT_STATUS_RUNNING: return "Running";
    case AM6_OUT_STATUS_HIGH_PSI: return "High PSI";
    case AM6_OUT_STATUS_LOW_PSI: return "Low PSI";
    case AM6_OUT_STATUS_POWER_FAULT: return "Power Fault";
    case AM6_OUT_STATUS_OVERLOAD_TRIPPED: return "Overload/Tripped";
    default: return "Unknown";
  }
}

#endif
