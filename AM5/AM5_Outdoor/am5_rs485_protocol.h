#ifndef AM5_RS485_PROTOCOL_H
#define AM5_RS485_PROTOCOL_H

#include <Arduino.h>

// =====================================================
// AM5 Indoor <-> Outdoor RS485 protocol v3
// This file MUST remain byte-for-byte identical in both projects.
// Frame: AA 55 DEST SRC TYPE LEN PAYLOAD CRC_LO CRC_HI
// CRC16-CCITT: poly 0x1021, init 0xFFFF
// =====================================================

static constexpr uint8_t AM5_RS485_PROTOCOL_VERSION = 3;

// Message types.
static constexpr uint8_t RS485_MSG_OUTDOOR_STATUS_REQUEST  = 0x30;
static constexpr uint8_t RS485_MSG_OUTDOOR_CONFIG_SET      = 0x31;
static constexpr uint8_t RS485_MSG_OUTDOOR_CONFIG_REQUEST  = 0x32;
static constexpr uint8_t RS485_MSG_OUTDOOR_STATUS_RESPONSE = 0xB0;
static constexpr uint8_t RS485_MSG_OUTDOOR_CONFIG_ACK      = 0xB1;
static constexpr uint8_t RS485_MSG_OUTDOOR_CONFIG_RESPONSE = 0xB2;

// 0x30 Indoor -> Outdoor status request.
static constexpr uint8_t AM5_RS485_STATUS_REQUEST_LEN = 3;
static constexpr uint8_t AM5_RS485_STATUS_REQ_VERSION = 0;
static constexpr uint8_t AM5_RS485_STATUS_REQ_SYSTEM_POWER = 1;
static constexpr uint8_t AM5_RS485_STATUS_REQ_OUTDOOR_ENABLE = 2;

// 0xB0 Outdoor -> Indoor status response (46 bytes exactly).
// statusout  = Circuit A (SW1..SW4 -> R1/R3)
// statusoutB = Circuit B (SW5..SW8 -> R2/R4)
static constexpr uint8_t AM5_RS485_STATUS_RESPONSE_LEN = 46;
static constexpr uint8_t AM5_RS485_STATUS_VERSION = 0;
static constexpr uint8_t AM5_RS485_STATUS_SYSTEM_POWER = 1;
static constexpr uint8_t AM5_RS485_STATUS_OUTDOOR_ENABLE = 2;
static constexpr uint8_t AM5_RS485_STATUS_EFFECTIVE_POWER = 3;
static constexpr uint8_t AM5_RS485_STATUS_CODE_A = 4;
static constexpr uint8_t AM5_RS485_STATUS_CODE_B = 5;
static constexpr uint8_t AM5_RS485_STATUS_ROLE_VALID_MASK = 6;
static constexpr uint8_t AM5_RS485_STATUS_PHYSICAL_VALID_MASK = 7;
static constexpr uint8_t AM5_RS485_STATUS_ROLE_TEMP_BASE = 8;      // 6 x int16, 0.1 C
static constexpr uint8_t AM5_RS485_STATUS_PHYSICAL_TEMP_BASE = 20; // 6 x int16, 0.1 C
static constexpr uint8_t AM5_RS485_STATUS_RELAY_BASE = 32;         // R1..R4
static constexpr uint8_t AM5_RS485_STATUS_SWITCH_PCF_HEALTHY = 36;
static constexpr uint8_t AM5_RS485_STATUS_RELAY_PCA_HEALTHY = 37;
static constexpr uint8_t AM5_RS485_STATUS_SWITCH_BASE = 38;        // SW1..SW8

// 0x31 Indoor -> Outdoor sensor config set.
static constexpr uint8_t AM5_RS485_CONFIG_SET_LEN = 12;
static constexpr uint8_t AM5_RS485_CONFIG_SET_VERSION = 0;
static constexpr uint8_t AM5_RS485_CONFIG_SET_ROM_BASE = 1; // 8 bytes
static constexpr uint8_t AM5_RS485_CONFIG_SET_ROLE = 9;
static constexpr uint8_t AM5_RS485_CONFIG_SET_OFFSET_BASE = 10; // int16, 0.01 C

// 0xB1 Outdoor -> Indoor config acknowledgement.
static constexpr uint8_t AM5_RS485_CONFIG_ACK_LEN = 2;
static constexpr uint8_t AM5_RS485_CONFIG_ACK_VERSION = 0;
static constexpr uint8_t AM5_RS485_CONFIG_ACK_SUCCESS = 1;

// 0x32 request / 0xB2 snapshot.
static constexpr uint8_t AM5_RS485_CONFIG_REQUEST_LEN = 1;
static constexpr uint8_t AM5_RS485_CONFIG_SNAPSHOT_HEADER_LEN = 2;
static constexpr uint8_t AM5_RS485_CONFIG_SNAPSHOT_RECORD_LEN = 13;
static constexpr uint8_t AM5_RS485_CONFIG_SNAPSHOT_VERSION = 0;
static constexpr uint8_t AM5_RS485_CONFIG_SNAPSHOT_COUNT = 1;
static constexpr uint8_t AM5_RS485_CONFIG_RECORD_ROM_BASE = 0;     // 8 bytes
static constexpr uint8_t AM5_RS485_CONFIG_RECORD_ROLE = 8;
static constexpr uint8_t AM5_RS485_CONFIG_RECORD_OFFSET_BASE = 9;  // int16, 0.01 C
static constexpr uint8_t AM5_RS485_CONFIG_RECORD_TEMP_BASE = 11;   // int16, 0.1 C

static_assert(AM5_RS485_STATUS_CODE_B + 1 == AM5_RS485_STATUS_ROLE_VALID_MASK,
              "AM5 RS485 dual-status layout mismatch");
static_assert(AM5_RS485_STATUS_ROLE_TEMP_BASE + 6 * 2 == AM5_RS485_STATUS_PHYSICAL_TEMP_BASE,
              "AM5 RS485 role temperature layout mismatch");
static_assert(AM5_RS485_STATUS_PHYSICAL_TEMP_BASE + 6 * 2 == AM5_RS485_STATUS_RELAY_BASE,
              "AM5 RS485 physical temperature layout mismatch");
static_assert(AM5_RS485_STATUS_RELAY_BASE + 4 == AM5_RS485_STATUS_SWITCH_PCF_HEALTHY,
              "AM5 RS485 relay layout mismatch");
static_assert(AM5_RS485_STATUS_SWITCH_BASE + 8 == AM5_RS485_STATUS_RESPONSE_LEN,
              "AM5 RS485 switch layout mismatch");

#endif
