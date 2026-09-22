#ifndef RS485_H
#define RS485_H

#include <Arduino.h>
#include <math.h>
#include "outdoor_status_logic.h"

// =====================================================
// AM6 OUTDOOR RS485 SLAVE - MATCHED TO LATEST INDOOR
// =====================================================
// Frame: AA 55 DEST SRC TYPE LEN PAYLOAD... CRC_LO CRC_HI
// Indoor master: 0x01
// Outdoor slave: 0x02
// Baud: 19200 8N1
// CRC16-CCITT poly 0x1021 init 0xFFFF
// =====================================================

HardwareSerial RS485Serial(1);

static const uint8_t RS485_SOF_1 = 0xAA;
static const uint8_t RS485_SOF_2 = 0x55;

struct OutdoorRS485Frame
{
  uint8_t dest;
  uint8_t src;
  uint8_t type;
  uint8_t len;
  uint8_t payload[RS485_MAX_PAYLOAD];
  uint16_t receivedCrc;
};

enum OutdoorRS485RxState : uint8_t
{
  RX_WAIT_SOF1,
  RX_WAIT_SOF2,
  RX_DEST,
  RX_SRC,
  RX_TYPE,
  RX_LEN,
  RX_PAYLOAD,
  RX_CRC_LO,
  RX_CRC_HI
};

OutdoorRS485Frame rs485RxFrame = {};
OutdoorRS485RxState rs485RxState = RX_WAIT_SOF1;
uint8_t rs485RxPayloadIndex = 0;
uint8_t rs485RxCrcLow = 0;
unsigned long rs485LastByteMs = 0;

inline void printHexByte(uint8_t value)
{
  if (value < 0x10) Serial.print("0");
  Serial.print(value, HEX);
}

inline uint16_t rs485CrcUpdate(uint16_t crc, uint8_t data)
{
  crc ^= static_cast<uint16_t>(data) << 8;
  for (uint8_t i = 0; i < 8; i++)
  {
    if (crc & 0x8000)
      crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
    else
      crc = static_cast<uint16_t>(crc << 1);
  }
  return crc;
}

inline uint16_t rs485CalculateFrameCrc(const OutdoorRS485Frame &frame)
{
  uint16_t crc = 0xFFFF;
  crc = rs485CrcUpdate(crc, frame.dest);
  crc = rs485CrcUpdate(crc, frame.src);
  crc = rs485CrcUpdate(crc, frame.type);
  crc = rs485CrcUpdate(crc, frame.len);
  for (uint8_t i = 0; i < frame.len; i++)
    crc = rs485CrcUpdate(crc, frame.payload[i]);
  return crc;
}

inline void rs485SetTransmit(bool enable)
{
  digitalWrite(RS485_DE_RE_PIN, enable ? HIGH : LOW);
}

inline void rs485ResetParser()
{
  rs485RxState = RX_WAIT_SOF1;
  rs485RxPayloadIndex = 0;
  rs485RxCrcLow = 0;
  rs485RxFrame.len = 0;
}

inline int16_t rs485GetInt16LE(const uint8_t *buffer, uint8_t index)
{
  return static_cast<int16_t>(
    static_cast<uint16_t>(buffer[index]) |
    (static_cast<uint16_t>(buffer[index + 1]) << 8)
  );
}

inline void rs485PutInt16LE(uint8_t *buffer, uint8_t index, int16_t value)
{
  const uint16_t raw = static_cast<uint16_t>(value);
  buffer[index] = static_cast<uint8_t>(raw & 0xFF);
  buffer[index + 1] = static_cast<uint8_t>((raw >> 8) & 0xFF);
}

inline int16_t rs485EncodeTemperature(float value)
{
  long scaled = lroundf(value * 10.0f);
  if (scaled > 32767L) scaled = 32767L;
  if (scaled < -32768L) scaled = -32768L;
  return static_cast<int16_t>(scaled);
}

inline int16_t rs485EncodeOffset(float value)
{
  long scaled = lroundf(value * 100.0f);
  if (scaled > 32767L) scaled = 32767L;
  if (scaled < -32768L) scaled = -32768L;
  return static_cast<int16_t>(scaled);
}

inline void rs485PrintValidFrame(const OutdoorRS485Frame &frame)
{
  Serial.print("[RS485-RX] CRC OK | DEST=0x");
  printHexByte(frame.dest);
  Serial.print(" SRC=0x");
  printHexByte(frame.src);
  Serial.print(" TYPE=0x");
  printHexByte(frame.type);
  Serial.print(" LEN=");
  Serial.print(frame.len);

  if (frame.len > 0)
  {
    Serial.print(" PAYLOAD=");
    for (uint8_t i = 0; i < frame.len; i++)
    {
      if (i > 0) Serial.print(" ");
      printHexByte(frame.payload[i]);
    }
  }
  Serial.println();
}

inline void rs485SendFrame(uint8_t destination,
                           uint8_t type,
                           const uint8_t *payload,
                           uint8_t length)
{
  if (length > RS485_MAX_PAYLOAD)
  {
    Serial.println("[RS485-TX] ERROR: payload too large");
    return;
  }

  uint16_t crc = 0xFFFF;
  crc = rs485CrcUpdate(crc, destination);
  crc = rs485CrcUpdate(crc, RS485_ADDR_OUTDOOR);
  crc = rs485CrcUpdate(crc, type);
  crc = rs485CrcUpdate(crc, length);
  for (uint8_t i = 0; i < length; i++)
    crc = rs485CrcUpdate(crc, payload[i]);

  Serial.print("[RS485-TX] DEST=0x");
  printHexByte(destination);
  Serial.print(" SRC=0x");
  printHexByte(RS485_ADDR_OUTDOOR);
  Serial.print(" TYPE=0x");
  printHexByte(type);
  Serial.print(" LEN=");
  Serial.print(length);
  Serial.print(" CRC=0x");
  if (crc < 0x1000) Serial.print("0");
  if (crc < 0x0100) Serial.print("0");
  if (crc < 0x0010) Serial.print("0");
  Serial.println(crc, HEX);

  // Give Indoor enough time to release the half-duplex driver.
  delayMicroseconds(600);
  rs485SetTransmit(true);
  delayMicroseconds(150);

  RS485Serial.write(RS485_SOF_1);
  RS485Serial.write(RS485_SOF_2);
  RS485Serial.write(destination);
  RS485Serial.write(RS485_ADDR_OUTDOOR);
  RS485Serial.write(type);
  RS485Serial.write(length);
  if (length > 0 && payload != nullptr)
    RS485Serial.write(payload, length);
  RS485Serial.write(static_cast<uint8_t>(crc & 0xFF));
  RS485Serial.write(static_cast<uint8_t>((crc >> 8) & 0xFF));
  RS485Serial.flush();

  delayMicroseconds(150);
  rs485SetTransmit(false);
}

inline void applySystemPower(uint8_t newPower)
{
  if (newPower > 1)
  {
    Serial.print("[SYSTEM] Invalid power rejected: ");
    Serial.println(newPower);
    return;
  }

  const int oldPower = systemPower;
  systemPower = static_cast<int>(newPower);

  if (oldPower != systemPower)
  {
    Serial.print("[SYSTEM] systemPower mirror changed ");
    Serial.print(oldPower);
    Serial.print(" -> ");
    Serial.println(systemPower);
  }
  else
  {
    Serial.print("[SYSTEM] systemPower mirror confirmed = ");
    Serial.println(systemPower);
  }

  // systemPower is the master Outdoor relay gate. controlRelays() forces
  // R1..R4 OFF when it is 0, then restores normal switch logic when it is 1.
}

inline void applyOutdoorEnable(uint8_t newEnable)
{
  if (newEnable > 1)
  {
    Serial.print("[SYSTEM] Invalid outdoorEnable rejected: ");
    Serial.println(newEnable);
    return;
  }

  const int oldEnable = outdoorEnable;
  outdoorEnable = static_cast<int>(newEnable);
  if (oldEnable != outdoorEnable)
  {
    Serial.print("[SYSTEM] outdoorEnable mirror changed ");
    Serial.print(oldEnable);
    Serial.print(" -> ");
    Serial.println(outdoorEnable);
  }
}

inline void markIndoorPowerCommandAlive()
{
  // Only a valid B0 status/power request authorizes Outdoor relay operation.
  // Sensor-configuration traffic must never extend this safety lease.
  lastValidIndoorCommandMs = millis();
  validIndoorCommandSeen = true;
}

// =====================================================
// 0xB0 STATUS RESPONSE - PROTOCOL V2, EXACTLY 45 BYTES
// =====================================================
inline uint8_t currentOutdoorStatusCode()
{
  return computeOutdoorStatusCode(systemPower,
                                  outdoorEnable,
                                  switchPcfHealthy,
                                  relayPcaHealthy,
                                  relayPcaConfigured,
                                  switchState);
}

inline void sendOutdoorStatusResponse()
{
  uint8_t payload[AM6_RS485_STATUS_RESPONSE_LEN] = {0};
  payload[AM6_RS485_STATUS_VERSION] = AM6_RS485_PROTOCOL_VERSION;
  payload[AM6_RS485_STATUS_SYSTEM_POWER] = systemPower ? 1 : 0;
  payload[AM6_RS485_STATUS_OUTDOOR_ENABLE] = outdoorEnable ? 1 : 0;
  payload[AM6_RS485_STATUS_EFFECTIVE_POWER] = (systemPower && outdoorEnable) ? 1 : 0;
  payload[AM6_RS485_STATUS_CODE] = currentOutdoorStatusCode();

  uint8_t roleValidMask = 0;
  uint8_t physicalValidMask = 0;

  for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
  {
    bool roleValid = false;
    const float roleTemp = getOutdoorRoleTemperature(static_cast<uint8_t>(i + 1), roleValid);
    if (roleValid && isfinite(roleTemp) && roleTemp != TEMP_INVALID_VALUE)
      roleValidMask |= static_cast<uint8_t>(1U << i);

    rs485PutInt16LE(
      payload,
      static_cast<uint8_t>(AM6_RS485_STATUS_ROLE_TEMP_BASE + i * 2),
      rs485EncodeTemperature((roleValid && isfinite(roleTemp)) ? roleTemp : TEMP_INVALID_VALUE)
    );

    const bool physicalValid = tempSensorPresent[i] &&
                               isfinite(temperature[i]) &&
                               temperature[i] != TEMP_INVALID_VALUE;
    const float physicalTemp = physicalValid
      ? (temperature[i] + tempSensorOffset[i])
      : TEMP_INVALID_VALUE;

    if (physicalValid)
      physicalValidMask |= static_cast<uint8_t>(1U << i);

    rs485PutInt16LE(
      payload,
      static_cast<uint8_t>(AM6_RS485_STATUS_PHYSICAL_TEMP_BASE + i * 2),
      rs485EncodeTemperature(physicalTemp)
    );
  }

  payload[AM6_RS485_STATUS_ROLE_VALID_MASK] = roleValidMask;
  payload[AM6_RS485_STATUS_PHYSICAL_VALID_MASK] = physicalValidMask;

  payload[AM6_RS485_STATUS_RELAY_BASE + 0] = R1_State ? 1 : 0;
  payload[AM6_RS485_STATUS_RELAY_BASE + 1] = R2_State ? 1 : 0;
  payload[AM6_RS485_STATUS_RELAY_BASE + 2] = R3_State ? 1 : 0;
  payload[AM6_RS485_STATUS_RELAY_BASE + 3] = R4_State ? 1 : 0;

  payload[AM6_RS485_STATUS_SWITCH_PCF_HEALTHY] = switchPcfHealthy ? 1 : 0;
  payload[AM6_RS485_STATUS_RELAY_PCA_HEALTHY] =
    (relayPcaHealthy && relayPcaConfigured) ? 1 : 0;

  for (uint8_t i = 0; i < SWITCH_COUNT; i++)
    payload[AM6_RS485_STATUS_SWITCH_BASE + i] = switchState[i] ? 1 : 0;

  Serial.print("[OUTDOOR -> INDOOR] B0 TX | v=");
  Serial.print(AM6_RS485_PROTOCOL_VERSION);
  Serial.print(" power="); Serial.print(systemPower);
  Serial.print(" outdoorEnable="); Serial.print(outdoorEnable);
  Serial.print(" status="); Serial.print(payload[AM6_RS485_STATUS_CODE]);
  Serial.print("("); Serial.print(outdoorStatusText(payload[AM6_RS485_STATUS_CODE])); Serial.print(")");
  Serial.print(" R=");
  Serial.print(R1_State ? 1 : 0); Serial.print(R2_State ? 1 : 0);
  Serial.print(R3_State ? 1 : 0); Serial.print(R4_State ? 1 : 0);
  Serial.print(" SW=");
  for (uint8_t i = 0; i < SWITCH_COUNT; i++) Serial.print(switchState[i] ? 1 : 0);
  Serial.print(" LEN="); Serial.println(AM6_RS485_STATUS_RESPONSE_LEN);

  rs485SendFrame(RS485_ADDR_INDOOR,
                 RS485_MSG_OUTDOOR_STATUS_RESPONSE,
                 payload,
                 AM6_RS485_STATUS_RESPONSE_LEN);
}

// =====================================================
// 0xB1 CONFIG ACK
// =====================================================
inline void sendOutdoorConfigAck(bool success)
{
  uint8_t payload[AM6_RS485_CONFIG_ACK_LEN] = {0};
  payload[AM6_RS485_CONFIG_ACK_VERSION] = AM6_RS485_PROTOCOL_VERSION;
  payload[AM6_RS485_CONFIG_ACK_SUCCESS] = success ? 1 : 0;

  Serial.print("[RS485-TX] B1 config ACK success=");
  Serial.println(success ? 1 : 0);

  rs485SendFrame(RS485_ADDR_INDOOR,
                 RS485_MSG_OUTDOOR_CONFIG_ACK,
                 payload,
                 AM6_RS485_CONFIG_ACK_LEN);
}

// =====================================================
// 0xB2 CONFIG SNAPSHOT
// version + count + count*(ROM8 + role1 + offset2 + rawTemp2)
// =====================================================
inline void sendOutdoorConfigSnapshot()
{
  uint8_t payload[RS485_MAX_PAYLOAD] = {0};
  payload[AM6_RS485_CONFIG_SNAPSHOT_VERSION] = AM6_RS485_PROTOCOL_VERSION;

  uint8_t count = 0;
  uint8_t writeIndex = AM6_RS485_CONFIG_SNAPSHOT_HEADER_LEN;

  for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
  {
    if (!tempSensorPresent[i]) continue;

    if (static_cast<uint16_t>(writeIndex) + AM6_RS485_CONFIG_SNAPSHOT_RECORD_LEN > RS485_MAX_PAYLOAD)
      break;

    memcpy(&payload[writeIndex + AM6_RS485_CONFIG_RECORD_ROM_BASE], tempAddress[i], 8);
    payload[writeIndex + AM6_RS485_CONFIG_RECORD_ROLE] = tempSensorRole[i];
    rs485PutInt16LE(payload,
                    static_cast<uint8_t>(writeIndex + AM6_RS485_CONFIG_RECORD_OFFSET_BASE),
                    rs485EncodeOffset(tempSensorOffset[i]));
    rs485PutInt16LE(payload,
                    static_cast<uint8_t>(writeIndex + AM6_RS485_CONFIG_RECORD_TEMP_BASE),
                    rs485EncodeTemperature(temperature[i]));

    writeIndex = static_cast<uint8_t>(writeIndex + AM6_RS485_CONFIG_SNAPSHOT_RECORD_LEN);
    count++;
  }

  payload[AM6_RS485_CONFIG_SNAPSHOT_COUNT] = count;
  const uint8_t payloadLength = static_cast<uint8_t>(
    AM6_RS485_CONFIG_SNAPSHOT_HEADER_LEN + count * AM6_RS485_CONFIG_SNAPSHOT_RECORD_LEN);

  Serial.print("[RS485-TX] B2 config snapshot count=");
  Serial.print(count);
  Serial.print(" len=");
  Serial.println(payloadLength);

  rs485SendFrame(RS485_ADDR_INDOOR,
                 RS485_MSG_OUTDOOR_CONFIG_RESPONSE,
                 payload,
                 payloadLength);
}

// =====================================================
// INCOMING COMMAND HANDLERS
// =====================================================
inline void handleOutdoorStatusRequest(const OutdoorRS485Frame &frame)
{
  // Protocol v2 request is exact and unambiguous:
  // [0] version, [1] system_power, [2] outdoorsw.
  if (frame.len != AM6_RS485_STATUS_REQUEST_LEN)
  {
    Serial.print("[RS485-RX] 0x30 rejected: expected LEN=");
    Serial.print(AM6_RS485_STATUS_REQUEST_LEN);
    Serial.print(" got ");
    Serial.println(frame.len);
    return;
  }

  if (frame.payload[AM6_RS485_STATUS_REQ_VERSION] != AM6_RS485_PROTOCOL_VERSION)
  {
    Serial.print("[RS485-RX] 0x30 rejected: protocol version=");
    Serial.println(frame.payload[AM6_RS485_STATUS_REQ_VERSION]);
    return;
  }

  const uint8_t receivedPower = frame.payload[AM6_RS485_STATUS_REQ_SYSTEM_POWER];
  const uint8_t receivedOutdoorEnable = frame.payload[AM6_RS485_STATUS_REQ_OUTDOOR_ENABLE];
  if (receivedPower > 1 || receivedOutdoorEnable > 1)
  {
    Serial.println("[RS485-RX] 0x30 rejected: invalid power/enable value");
    return;
  }

  markIndoorPowerCommandAlive();
  applySystemPower(receivedPower);
  applyOutdoorEnable(receivedOutdoorEnable);

  // Read the already-working hardware immediately before the reply so Indoor
  // receives current switch and relay states rather than a stale snapshot.
  readSwitches();
  controlRelays();
  sendOutdoorStatusResponse();
}

inline void handleOutdoorConfigSet(const OutdoorRS485Frame &frame)
{
  // [0] version, [1..8] ROM, [9] role 0..6, [10..11] offset x0.01C
  if (frame.len != AM6_RS485_CONFIG_SET_LEN)
  {
    Serial.print("[RS485-RX] 0x31 rejected: expected LEN=12, got ");
    Serial.println(frame.len);
    sendOutdoorConfigAck(false);
    return;
  }

  if (frame.payload[AM6_RS485_CONFIG_SET_VERSION] != AM6_RS485_PROTOCOL_VERSION)
  {
    Serial.println("[RS485-RX] 0x31 rejected: protocol version mismatch");
    sendOutdoorConfigAck(false);
    return;
  }

  const uint8_t *address = &frame.payload[AM6_RS485_CONFIG_SET_ROM_BASE];
  const uint8_t role = frame.payload[AM6_RS485_CONFIG_SET_ROLE];
  const int16_t encodedOffset = rs485GetInt16LE(
    frame.payload, AM6_RS485_CONFIG_SET_OFFSET_BASE);
  const float offset = static_cast<float>(encodedOffset) / 100.0f;

  Serial.print("[RS485-RX] 0x31 config ROM=");
  printAddress(address);
  Serial.print(" role=");
  Serial.print(role);
  Serial.print(" offset=");
  Serial.println(offset, 2);

  const bool success = setOutdoorSensorConfig(address, role, offset);
  sendOutdoorConfigAck(success);
}

inline void handleOutdoorConfigRequest(const OutdoorRS485Frame &frame)
{
  if (frame.len != AM6_RS485_CONFIG_REQUEST_LEN ||
      frame.payload[AM6_RS485_CONFIG_SNAPSHOT_VERSION] != AM6_RS485_PROTOCOL_VERSION)
  {
    Serial.println("[RS485-RX] 0x32 rejected: invalid protocol version/length");
    return;
  }

  sendOutdoorConfigSnapshot();
}

inline void rs485HandleValidFrame(const OutdoorRS485Frame &frame)
{
  rs485PrintValidFrame(frame);

  if (frame.dest != RS485_ADDR_OUTDOOR)
  {
    Serial.print("[RS485-RX] Ignored frame for DEST=0x");
    printHexByte(frame.dest);
    Serial.println();
    return;
  }

  if (frame.src != RS485_ADDR_INDOOR)
  {
    Serial.print("[RS485-RX] Ignored non-Indoor SRC=0x");
    printHexByte(frame.src);
    Serial.println();
    return;
  }

  switch (frame.type)
  {
    case RS485_MSG_OUTDOOR_STATUS_REQUEST:
      handleOutdoorStatusRequest(frame);
      break;

    case RS485_MSG_OUTDOOR_CONFIG_SET:
      handleOutdoorConfigSet(frame);
      break;

    case RS485_MSG_OUTDOOR_CONFIG_REQUEST:
      handleOutdoorConfigRequest(frame);
      break;

    default:
      Serial.print("[RS485-RX] Unsupported TYPE=0x");
      printHexByte(frame.type);
      Serial.println();
      break;
  }
}

inline void rs485ConsumeByte(uint8_t value)
{
  rs485LastByteMs = millis();

  switch (rs485RxState)
  {
    case RX_WAIT_SOF1:
      if (value == RS485_SOF_1) rs485RxState = RX_WAIT_SOF2;
      break;

    case RX_WAIT_SOF2:
      if (value == RS485_SOF_2) rs485RxState = RX_DEST;
      else if (value != RS485_SOF_1) rs485RxState = RX_WAIT_SOF1;
      break;

    case RX_DEST:
      rs485RxFrame.dest = value;
      rs485RxState = RX_SRC;
      break;

    case RX_SRC:
      rs485RxFrame.src = value;
      rs485RxState = RX_TYPE;
      break;

    case RX_TYPE:
      rs485RxFrame.type = value;
      rs485RxState = RX_LEN;
      break;

    case RX_LEN:
      if (value > RS485_MAX_PAYLOAD)
      {
        Serial.print("[RS485-RX] Payload too large LEN=");
        Serial.println(value);
        rs485ResetParser();
        break;
      }
      rs485RxFrame.len = value;
      rs485RxPayloadIndex = 0;
      rs485RxState = (value == 0) ? RX_CRC_LO : RX_PAYLOAD;
      break;

    case RX_PAYLOAD:
      rs485RxFrame.payload[rs485RxPayloadIndex++] = value;
      if (rs485RxPayloadIndex >= rs485RxFrame.len)
        rs485RxState = RX_CRC_LO;
      break;

    case RX_CRC_LO:
      rs485RxCrcLow = value;
      rs485RxState = RX_CRC_HI;
      break;

    case RX_CRC_HI:
    {
      rs485RxFrame.receivedCrc =
        static_cast<uint16_t>(rs485RxCrcLow) |
        (static_cast<uint16_t>(value) << 8);

      const uint16_t calculatedCrc = rs485CalculateFrameCrc(rs485RxFrame);
      if (calculatedCrc == rs485RxFrame.receivedCrc)
      {
        rs485HandleValidFrame(rs485RxFrame);
      }
      else
      {
        Serial.print("[RS485-RX] CRC ERROR received=0x");
        Serial.print(rs485RxFrame.receivedCrc, HEX);
        Serial.print(" calculated=0x");
        Serial.println(calculatedCrc, HEX);
      }

      rs485ResetParser();
      break;
    }
  }
}

inline void serviceIndoorPowerCommandSafety()
{
  // Outdoor is a slave. Stored/local state can never authorize its relays.
  // Until a valid B0 command is received from Indoor, remain forced OFF.
  if (!validIndoorCommandSeen)
  {
    if (systemPower != 0 || outdoorEnable != 0 ||
        R1_State || R2_State || R3_State || R4_State)
    {
      applySystemPower(0);
      applyOutdoorEnable(0);
      forceAllRelaysOff();
    }
    return;
  }

  if (millis() - lastValidIndoorCommandMs > INDOOR_COMMAND_TIMEOUT_MS)
  {
    Serial.print("[RS485][SAFETY] Indoor B0 power command timeout >");
    Serial.print(INDOOR_COMMAND_TIMEOUT_MS);
    Serial.println(" ms -> ALL Outdoor relays OFF; waiting for fresh Indoor command");

    validIndoorCommandSeen = false;
    applySystemPower(0);
    applyOutdoorEnable(0);
    forceAllRelaysOff();
  }
}

inline void rs485Loop()
{
  if (rs485RxState != RX_WAIT_SOF1 &&
      millis() - rs485LastByteMs > RS485_FRAME_TIMEOUT_MS)
  {
    Serial.println("[RS485-RX] Partial frame timeout -> parser reset");
    rs485ResetParser();
  }

  while (RS485Serial.available() > 0)
    rs485ConsumeByte(static_cast<uint8_t>(RS485Serial.read()));
}

inline void initRS485()
{
  Serial.println("[RS485] ------------------------------------------------");
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  rs485SetTransmit(false);

  RS485Serial.begin(RS485_BAUD_RATE,
                    SERIAL_8N1,
                    RS485_RX_PIN,
                    RS485_TX_PIN);

  rs485ResetParser();

  // A reboot/re-init never inherits relay authorization. A fresh valid B0
  // command from Indoor is required before systemPower/outdoorEnable can run.
  validIndoorCommandSeen = false;
  lastValidIndoorCommandMs = 0;
  applySystemPower(0);
  applyOutdoorEnable(0);
  forceAllRelaysOff();

  Serial.println("[RS485] AM6 Outdoor SLAVE initialized");
  Serial.print("[RS485] Indoor=0x"); printHexByte(RS485_ADDR_INDOOR);
  Serial.print(" Outdoor=0x"); printHexByte(RS485_ADDR_OUTDOOR);
  Serial.print(" Baud="); Serial.println(RS485_BAUD_RATE);
  Serial.println("[RS485] Protocol v2: 0x30 LEN=3, 0xB0 LEN=45");
  Serial.println("[RS485] Supported: 0x30 status/power, 0x31 config set, 0x32 config snapshot");
  Serial.println("[RS485] Replies:    0xB0 status, 0xB1 config ACK, 0xB2 config snapshot");
  Serial.println("[RS485] Idle mode = RECEIVE; Outdoor never transmits spontaneously");
  Serial.println("[RS485][SAFETY] Relays require fresh Indoor B0 power=1 + outdoorsw=1");
  Serial.println("[RS485] ------------------------------------------------");
}

#endif
