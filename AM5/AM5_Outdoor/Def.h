#ifndef DEF_H
#define DEF_H

#include "am5_rs485_protocol.h"

// =====================================================
// AM5 OUTDOOR HARDWARE CONFIGURATION
// =====================================================

// Serial debug
#define SERIAL_BAUD_RATE 115200UL
#define SERIAL_STATUS_INTERVAL_MS 2000UL

// DS18B20 - all 6 sensors share one 1-Wire bus
#define DS18B20_PIN 25
#define TEMP_SENSOR_COUNT 6
#define TEMP_INVALID_VALUE 888.0f
#define TEMP_CONVERSION_MS 750UL

// =====================================================
// I2C / SWITCH PCF8574 + RELAY PCA9554
// =====================================================
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// Confirmed hardware mapping:
//   0x26 = PCF8574 used for 8 toggle-switch inputs
//   0x20 = PCA9554 used for 4 relay outputs
//   0x27 = unused I/O expander on this Outdoor board
#define SWITCH_PCF_ADDRESS 0x26
#define RELAY_PCA_ADDRESS  0x20

#define SWITCH_COUNT 8
#define RELAY_COUNT 4

// Toggle switches are wired from PCF pins to common GND.
// Therefore: switch OFF/open = HIGH, switch ON/closed = LOW (active-low).
#define SWITCH_ACTIVE_LEVEL LOW

// Switch PCF8574 mapping
#define SW1 0
#define SW2 1
#define SW3 2
#define SW4 3
#define SW5 4
#define SW6 5
#define SW7 6
#define SW8 7

// Relay PCA9554 mapping (change only if PCB wiring uses different P pins)
#define R1_PCA_PIN 0
#define R2_PCA_PIN 1
#define R3_PCA_PIN 2
#define R4_PCA_PIN 3

// Confirmed hardware polarity: relay ON = HIGH, relay OFF = LOW.
#define RELAY_ACTIVE_LEVEL HIGH

// =====================================================
// RS485 - EXACTLY MATCHES AM5 INDOOR MASTER
// =====================================================
// Indoor master  = 0x01
// Outdoor slave  = 0x02
// Display is NOT on this RS485 bus.
// 19200 8N1, 2-wire half duplex
// Protocol/message/packet layout is centralized in am5_rs485_protocol.h.

#define RS485_ADDR_INDOOR  0x01
#define RS485_ADDR_OUTDOOR 0x02

#define RS485_TX_PIN 17
#define RS485_RX_PIN 16
#define RS485_DE_RE_PIN 4
#define RS485_BAUD_RATE 19200UL

// Must be >= 80 because B2 snapshot can carry 6 * 13 + 2 = 80 bytes.
#define RS485_MAX_PAYLOAD 96
#define RS485_FRAME_TIMEOUT_MS 50UL

// Indoor polls Outdoor every 500 ms. Outdoor is a strict slave: a fresh,
// valid B0 power/status command is required to authorize relay operation.
// If those commands disappear for this long, all four Outdoor relays are
// forced OFF until a new valid B0 command arrives.
#define INDOOR_COMMAND_TIMEOUT_MS 2000UL

// =====================================================
// RUNTIME STATE
// =====================================================
float temperature[TEMP_SENSOR_COUNT] = {
  TEMP_INVALID_VALUE, TEMP_INVALID_VALUE, TEMP_INVALID_VALUE,
  TEMP_INVALID_VALUE, TEMP_INVALID_VALUE, TEMP_INVALID_VALUE
};

bool switchState[SWITCH_COUNT] = {
  false, false, false, false,
  false, false, false, false
};

bool R1_State = false;
bool R2_State = false;
bool R3_State = false;
bool R4_State = false;

// Master enable state received from Indoor over RS485.
// Both must be 1 before the already-verified local SW1..SW8 relay logic is allowed
// to energize R1..R4. systemPower=0 or outdoorEnable=0 forces all relays OFF.
int systemPower = 0;
int outdoorEnable = 0;

// Relay authorization lease from the Indoor master. These start invalid on
// every Outdoor boot and are renewed ONLY by a valid B0 status/power command.
unsigned long lastValidIndoorCommandMs = 0;
bool validIndoorCommandSeen = false;

uint8_t switchPcfData = 0xFF;
uint8_t relayPcaShadow = 0xFF;
bool switchPcfHealthy = false;
bool relayPcaHealthy = false;
bool relayPcaConfigured = false;

#endif
