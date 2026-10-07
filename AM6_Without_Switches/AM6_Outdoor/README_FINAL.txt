AM6 OUTDOOR - corrected integration build
==========================================

Hardware retained from the supplied working Outdoor project
-------------------------------------------------------------
- DS18B20 bus: GPIO25, up to 6 sensors
- Toggle-switch input expander: PCF8574 @ 0x26
- Relay output expander: PCA9554 @ 0x20
- Third expander @ 0x27 is not used by this firmware
- Relay polarity is ACTIVE HIGH: HIGH = ON, LOW = OFF
- Switch polarity/mapping and the verified local relay logic are unchanged

Verified local Outdoor relay logic retained
-------------------------------------------
- SW1+SW2+SW3+SW4 active -> R1 and R3 ON
- SW5+SW6+SW7+SW8 active -> R2 and R4 ON
- Local switch-to-relay operation remains independent of Indoor system_power,
  matching the supplied Outdoor code that is already working on the hardware.
- Expander failure forces relay outputs OFF.

RS485 connection
----------------
- Indoor master: 0x01
- Outdoor slave: 0x02
- 19200 baud, 8N1
- Outdoor RX GPIO16, TX GPIO17, DE/RE GPIO4
- The Display does NOT use this RS485 bus; it communicates with Indoor by Wi-Fi/HTTP.
- Both Indoor and Outdoor use the identical am6_rs485_protocol.h protocol-v2 contract.
- See RS485_PROTOCOL.txt for exact packet layouts.

Important relay startup correction
----------------------------------
The PCA9554 output latch is now preloaded to the configured OFF electrical level
before pins P0..P3 are changed to outputs. With ACTIVE-HIGH relay hardware this
means the relay pins are preloaded LOW, preventing the old active-low startup
assumption from momentarily commanding the relays ON.

Temperature configuration
-------------------------
- Sensor identity is based on the permanent 8-byte DS18B20 ROM address.
- Role and offset are persisted in Preferences/NVS.
- Indoor changes Outdoor role/offset through RS485 0x31 and receives an ACK.
- Indoor obtains the authoritative Outdoor configuration through the 0xB2 snapshot.
- Invalid/non-finite offsets are rejected; runtime offset exactly matches the
  clamped/rounded value persisted in NVS.

Serial Monitor
--------------
115200 baud. The firmware prints I2C health, switch states, relay states,
temperatures, sensor configuration, and RS485 request/response information.
