#ifndef OUTDOOR_STATUS_LOGIC_H
#define OUTDOOR_STATUS_LOGIC_H

#include <stdint.h>

// AM5 Outdoor statusout values shared with the mobile app:
// 0 = Stopped
// 1 = Running
// 2 = High PSI
// 3 = Low PSI
// 4 = Power Fault
// 5 = Overload / Tripped
static constexpr uint8_t AM5_OUT_STATUS_STOPPED = 0;
static constexpr uint8_t AM5_OUT_STATUS_RUNNING = 1;
static constexpr uint8_t AM5_OUT_STATUS_HIGH_PSI = 2;
static constexpr uint8_t AM5_OUT_STATUS_LOW_PSI = 3;
static constexpr uint8_t AM5_OUT_STATUS_POWER_FAULT = 4;
static constexpr uint8_t AM5_OUT_STATUS_OVERLOAD_TRIPPED = 5;

// switchState[] is the logical/healthy state after SWITCH_ACTIVE_LEVEL is applied:
//   SW1 = High PSI A     SW5 = High PSI B
//   SW2 = Low PSI A      SW6 = Low PSI B
//   SW3 = Overload A     SW7 = Overload B
//   SW4 = Power A        SW8 = Power B
// A value of 1 means healthy; 0 means that protection/fault input is active.
//
// Required priority:
// High PSI -> Low PSI -> Power Fault -> Overload/Tripped -> Running.
// System OFF / Outdoor disabled always reports Stopped.
inline uint8_t computeOutdoorStatusCode(int systemPower,
                                        int outdoorEnable,
                                        bool switchPcfHealthy,
                                        bool relayPcaHealthy,
                                        bool relayPcaConfigured,
                                        const bool switchState[8])
{
  if (systemPower != 1 || outdoorEnable != 1)
    return AM5_OUT_STATUS_STOPPED;

  // If switch data cannot be trusted, do not manufacture a PSI/overload fault
  // from stale/default switch bits. Report a hardware/power fault instead.
  if (!switchPcfHealthy)
    return AM5_OUT_STATUS_POWER_FAULT;

  // Priority 1: High PSI on either circuit.
  if (!switchState[0] || !switchState[4])
    return AM5_OUT_STATUS_HIGH_PSI;

  // Priority 2: Low PSI on either circuit.
  if (!switchState[1] || !switchState[5])
    return AM5_OUT_STATUS_LOW_PSI;

  // Priority 3: Power fault on either circuit. A failed relay output expander
  // is also a power/control fault because commanded relay state cannot be trusted.
  if (!switchState[3] || !switchState[7] ||
      !relayPcaHealthy || !relayPcaConfigured)
    return AM5_OUT_STATUS_POWER_FAULT;

  // Priority 4: Overload / Tripped on either circuit.
  if (!switchState[2] || !switchState[6])
    return AM5_OUT_STATUS_OVERLOAD_TRIPPED;

  return AM5_OUT_STATUS_RUNNING;
}

inline const char* outdoorStatusText(uint8_t status)
{
  switch (status)
  {
    case AM5_OUT_STATUS_STOPPED: return "Stopped";
    case AM5_OUT_STATUS_RUNNING: return "Running";
    case AM5_OUT_STATUS_HIGH_PSI: return "High PSI";
    case AM5_OUT_STATUS_LOW_PSI: return "Low PSI";
    case AM5_OUT_STATUS_POWER_FAULT: return "Power Fault";
    case AM5_OUT_STATUS_OVERLOAD_TRIPPED: return "Overload/Tripped";
    default: return "Unknown";
  }
}

#endif
