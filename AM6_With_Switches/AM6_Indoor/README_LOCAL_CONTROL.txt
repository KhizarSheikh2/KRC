AM6 INDOOR - LOCAL PHYSICAL CONTROL MASTER
==========================================

Physical inputs (active HIGH, configured INPUT_PULLDOWN):
- GPIO19 = complete-system POWER: 1 ON, 0 OFF
- GPIO18 = HVAC MODE: 1 HEAT, 0 COOL
- GPIO23 = FAN LOW select
- GPIO22 = FAN MEDIUM select
- GPIO21 = FAN HIGH select

The three fan inputs are a one-of-three selector. Exactly one HIGH selects that
speed. If none or multiple are HIGH after debounce, firmware holds the last
valid speed so airflow is not dropped while the system is ON.

Control authority:
- Physical GPIO switches own powersw, fanSw and mode.
- Mobile app still receives/publishes these values for monitoring, but incoming
  powersw/fanSw/mode commands do not override the physical switches.
- App indoorsw, outdoorsw, setPoint and temperature sensor configuration remain
  supported.
- Display is monitoring-only.

Power persistence:
- power_state is saved in Preferences when the physical GPIO19 state changes.
- Saved power is never used to override GPIO19 at boot.
- Outdoor remains a slave and requires a fresh valid Indoor RS485 command
  before any Outdoor relay may energize.

Relay polarity:
- Indoor relay active level remains HIGH (RELAY_ACTIVE_LOW=false).
