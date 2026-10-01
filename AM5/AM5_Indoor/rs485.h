#ifndef AM5_RS485_H
#define AM5_RS485_H

#include <Arduino.h>
#include <limits.h>
#include <math.h>
#include "am5_rs485_protocol.h"

// =====================================================
// AM5 TWO-NODE RS485 PROTOCOL
// Indoor  0x01 = ONLY MASTER
// Outdoor 0x02 = ONLY SLAVE
// Display is NOT present on this RS485 bus.
// Frame: AA 55 DEST SRC TYPE LEN PAYLOAD CRC_LO CRC_HI
// CRC16-CCITT poly 0x1021, init 0xFFFF
// =====================================================

HardwareSerial AM5_RS485(2);

static const uint8_t RS485_SOF_1 = 0xAA;
static const uint8_t RS485_SOF_2 = 0x55;
struct RS485RxFrame {
  uint8_t dest;
  uint8_t src;
  uint8_t type;
  uint8_t len;
  uint8_t payload[RS485_MAX_PAYLOAD];
  uint16_t receivedCrc;
};

enum RS485RxState : uint8_t {
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

enum RS485MasterTransaction : uint8_t {
  RS485_TXN_IDLE,
  RS485_TXN_WAIT_OUTDOOR_STATUS,
  RS485_TXN_WAIT_OUTDOOR_CONFIG_ACK,
  RS485_TXN_WAIT_OUTDOOR_CONFIG_SNAPSHOT
};

RS485RxFrame rs485RxFrame = {};
RS485RxState rs485RxState = RX_WAIT_SOF1;
RS485MasterTransaction rs485MasterTransaction = RS485_TXN_IDLE;

uint8_t rs485RxPayloadIndex = 0;
uint8_t rs485RxCrcLow = 0;
unsigned long rs485LastByteMs = 0;
unsigned long rs485TransactionStartedMs = 0;
unsigned long rs485LastOutdoorPollMs = 0;

inline uint16_t rs485CrcUpdate(uint16_t crc, uint8_t data) {
  crc ^= static_cast<uint16_t>(data) << 8;
  for (uint8_t i = 0; i < 8; i++) {
    crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                         : static_cast<uint16_t>(crc << 1);
  }
  return crc;
}

inline uint16_t rs485CalculateFrameCrc(const RS485RxFrame& frame) {
  uint16_t crc = 0xFFFF;
  crc = rs485CrcUpdate(crc, frame.dest);
  crc = rs485CrcUpdate(crc, frame.src);
  crc = rs485CrcUpdate(crc, frame.type);
  crc = rs485CrcUpdate(crc, frame.len);
  for (uint8_t i = 0; i < frame.len; i++) crc = rs485CrcUpdate(crc, frame.payload[i]);
  return crc;
}

inline void rs485SetTransmit(bool enable) {
  digitalWrite(RS485_DE_RE_PIN, enable ? HIGH : LOW);
}

inline void rs485ResetParser() {
  rs485RxState = RX_WAIT_SOF1;
  rs485RxPayloadIndex = 0;
  rs485RxCrcLow = 0;
  rs485RxFrame.len = 0;
}

inline void rs485ClearStaleRx() {
  while (AM5_RS485.available() > 0) AM5_RS485.read();
  rs485ResetParser();
}

inline void rs485SendFrame(uint8_t destination, uint8_t type,
                           const uint8_t* payload, uint8_t length) {
  if (length > RS485_MAX_PAYLOAD) {
    Serial.println("[RS485] TX rejected: payload too large");
    return;
  }

  uint16_t crc = 0xFFFF;
  crc = rs485CrcUpdate(crc, destination);
  crc = rs485CrcUpdate(crc, RS485_ADDR_INDOOR);
  crc = rs485CrcUpdate(crc, type);
  crc = rs485CrcUpdate(crc, length);
  for (uint8_t i = 0; i < length; i++) crc = rs485CrcUpdate(crc, payload[i]);

  delayMicroseconds(600);
  rs485SetTransmit(true);
  delayMicroseconds(150);

  AM5_RS485.write(RS485_SOF_1);
  AM5_RS485.write(RS485_SOF_2);
  AM5_RS485.write(destination);
  AM5_RS485.write(RS485_ADDR_INDOOR);
  AM5_RS485.write(type);
  AM5_RS485.write(length);
  if (length > 0 && payload != nullptr) AM5_RS485.write(payload, length);
  AM5_RS485.write(static_cast<uint8_t>(crc & 0xFF));
  AM5_RS485.write(static_cast<uint8_t>((crc >> 8) & 0xFF));
  AM5_RS485.flush();

  delayMicroseconds(150);
  rs485SetTransmit(false);
}

inline int16_t rs485GetInt16LE(const uint8_t* buffer, uint8_t index) {
  const uint16_t raw = static_cast<uint16_t>(buffer[index]) |
                       (static_cast<uint16_t>(buffer[index + 1]) << 8);
  return static_cast<int16_t>(raw);
}

inline int16_t rs485EncodeOffset(float value) {
  long scaled = lroundf(value * 100.0f);
  if (scaled > INT16_MAX) scaled = INT16_MAX;
  if (scaled < INT16_MIN) scaled = INT16_MIN;
  return static_cast<int16_t>(scaled);
}

inline void rs485PutInt16LE(uint8_t* buffer, uint8_t index, int16_t value) {
  const uint16_t raw = static_cast<uint16_t>(value);
  buffer[index] = static_cast<uint8_t>(raw & 0xFF);
  buffer[index + 1] = static_cast<uint8_t>((raw >> 8) & 0xFF);
}

// =====================================================
// OUTDOOR STATUS + AUTHORITATIVE POWER SYNCHRONIZATION
// Protocol v3 status request:
// [0] version, [1] system_power, [2] outdoorsw
// =====================================================
inline void rs485BeginOutdoorStatusTransaction() {
  uint8_t payload[AM5_RS485_STATUS_REQUEST_LEN] = {0};
  payload[AM5_RS485_STATUS_REQ_VERSION] = AM5_RS485_PROTOCOL_VERSION;
  payload[AM5_RS485_STATUS_REQ_SYSTEM_POWER] = static_cast<uint8_t>(system_power == 1 ? 1 : 0);
  payload[AM5_RS485_STATUS_REQ_OUTDOOR_ENABLE] = static_cast<uint8_t>(outdoor_sw == 1 ? 1 : 0);

  rs485ClearStaleRx();
  rs485SendFrame(RS485_ADDR_OUTDOOR,
                 RS485_MSG_OUTDOOR_STATUS_REQUEST,
                 payload,
                 sizeof(payload));
  rs485MasterTransaction = RS485_TXN_WAIT_OUTDOOR_STATUS;
  rs485TransactionStartedMs = millis();
  rs485LastOutdoorPollMs = rs485TransactionStartedMs;
  rs485_urgent_outdoor_sync = false;
}

inline void rs485HandleOutdoorStatus(const RS485RxFrame& frame) {
  // B0 payload length 46 (protocol v3):
  // [0]      version
  // [1]      authoritative system_power echo
  // [2]      outdoorsw echo
  // [3]      effective Outdoor enable = system_power && outdoorsw
  // [4]      statusout  = Circuit A, SW1..SW4 -> R1/R3
  // [5]      statusoutB = Circuit B, SW5..SW8 -> R2/R4
  // [6]      valid mask for configured role temperatures 1..6
  // [7]      valid mask for physical Outdoor temperatures 1..6
  // [8..19]  six configured role temperatures, int16 x0.1 C
  // [20..31] six physical calibrated temperatures, int16 x0.1 C
  // [32..35] R1..R4
  // [36]     switch PCF healthy
  // [37]     relay PCA healthy
  // [38..45] logical SW1..SW8 states
  if (rs485MasterTransaction != RS485_TXN_WAIT_OUTDOOR_STATUS) return;
  rs485MasterTransaction = RS485_TXN_IDLE;

  if (frame.len != AM5_RS485_STATUS_RESPONSE_LEN ||
      frame.payload[AM5_RS485_STATUS_VERSION] != AM5_RS485_PROTOCOL_VERSION) {
    Serial.println("[RS485] Invalid Outdoor status payload");
    return;
  }

  // Snapshot the app-visible Outdoor state before applying this packet so a
  // protection input/status change can be published immediately over MQTT.
  const bool previousOutdoorOnline = rs485_outdoor_online;
  const int previousOutdoorStatusCode = outdoorStatusCode;
  const int previousOutdoorStatusCodeB = outdoorStatusCodeB;
  const bool previousOutdoorR1 = outdoorR1;
  const bool previousOutdoorR2 = outdoorR2;
  const bool previousOutdoorR3 = outdoorR3;
  const bool previousOutdoorR4 = outdoorR4;
  bool previousOutdoorSwitchState[8];
  for (uint8_t i = 0; i < 8; i++) previousOutdoorSwitchState[i] = outdoorSwitchState[i];

  rs485_outdoor_online = true;
  rs485_outdoor_last_seen_ms = millis();

  outdoorPowerEcho = (frame.payload[AM5_RS485_STATUS_SYSTEM_POWER] == 1) ? 1 : 0;
  outdoorEnableEcho = (frame.payload[AM5_RS485_STATUS_OUTDOOR_ENABLE] == 1) ? 1 : 0;
  outdoorEffectivePower = (frame.payload[AM5_RS485_STATUS_EFFECTIVE_POWER] == 1) ? 1 : 0;
  outdoorStatusCode = (frame.payload[AM5_RS485_STATUS_CODE_A] <= static_cast<uint8_t>(OUT_STATUS_OVERLOAD_TRIPPED))
                        ? static_cast<int>(frame.payload[AM5_RS485_STATUS_CODE_A])
                        : static_cast<int>(OUT_STATUS_STOPPED);
  outdoorStatusCodeB = (frame.payload[AM5_RS485_STATUS_CODE_B] <= static_cast<uint8_t>(OUT_STATUS_OVERLOAD_TRIPPED))
                         ? static_cast<int>(frame.payload[AM5_RS485_STATUS_CODE_B])
                         : static_cast<int>(OUT_STATUS_STOPPED);

  const uint8_t roleValidMask = frame.payload[AM5_RS485_STATUS_ROLE_VALID_MASK];
  const uint8_t physicalValidMask = frame.payload[AM5_RS485_STATUS_PHYSICAL_VALID_MASK];

  for (uint8_t i = 0; i < OUTDOOR_SENSOR_COUNT; i++) {
    outdoorRoleTempValid[i] = (roleValidMask & (1U << i)) != 0;
    if (outdoorRoleTempValid[i]) {
      outdoorRoleTempC[i] = static_cast<float>(
        rs485GetInt16LE(frame.payload, static_cast<uint8_t>(AM5_RS485_STATUS_ROLE_TEMP_BASE + i * 2))) / 10.0f;
    }
    else {
      outdoorRoleTempC[i] = SENSOR_DISCONNECTED;
    }

    if ((physicalValidMask & (1U << i)) != 0) {
      outdoorSensorPhysicalTempC[i] = static_cast<float>(
        rs485GetInt16LE(frame.payload, static_cast<uint8_t>(AM5_RS485_STATUS_PHYSICAL_TEMP_BASE + i * 2))) / 10.0f;
    }
    else {
      outdoorSensorPhysicalTempC[i] = SENSOR_DISCONNECTED;
    }
  }

  outdoorR1 = frame.payload[AM5_RS485_STATUS_RELAY_BASE + 0] == 1;
  outdoorR2 = frame.payload[AM5_RS485_STATUS_RELAY_BASE + 1] == 1;
  outdoorR3 = frame.payload[AM5_RS485_STATUS_RELAY_BASE + 2] == 1;
  outdoorR4 = frame.payload[AM5_RS485_STATUS_RELAY_BASE + 3] == 1;
  outdoorSwitchPcfHealthy = frame.payload[AM5_RS485_STATUS_SWITCH_PCF_HEALTHY] == 1;
  outdoorRelayPcfHealthy = frame.payload[AM5_RS485_STATUS_RELAY_PCA_HEALTHY] == 1;
  outdoorPcfHealthy = outdoorSwitchPcfHealthy && outdoorRelayPcfHealthy;

  for (uint8_t i = 0; i < 8; i++) {
    outdoorSwitchState[i] = frame.payload[AM5_RS485_STATUS_SWITCH_BASE + i] == 1;
  }

  bool outdoorStateChanged = !previousOutdoorOnline ||
                             previousOutdoorStatusCode != outdoorStatusCode ||
                             previousOutdoorStatusCodeB != outdoorStatusCodeB ||
                             previousOutdoorR1 != outdoorR1 ||
                             previousOutdoorR2 != outdoorR2 ||
                             previousOutdoorR3 != outdoorR3 ||
                             previousOutdoorR4 != outdoorR4;

  for (uint8_t i = 0; i < 8 && !outdoorStateChanged; i++) {
    if (previousOutdoorSwitchState[i] != outdoorSwitchState[i])
      outdoorStateChanged = true;
  }

  if (outdoorStateChanged) {
    mqtt_publish_requested = true;
    Serial.print("[RS485] Outdoor state changed -> MQTT republish requested | statusout=");
    Serial.print(outdoorStatusCode);
    Serial.print(" statusoutB=");
    Serial.println(outdoorStatusCodeB);
  }

  // Outdoor may have rebooted or missed a master-state change.
  if (outdoorPowerEcho != system_power || outdoorEnableEcho != outdoor_sw) {
    rs485_urgent_outdoor_sync = true;
  }
}

// =====================================================
// OUTDOOR SENSOR CONFIGURATION QUEUE
// App -> MQTT/Wi-Fi Indoor -> RS485 Outdoor
// =====================================================
inline bool rs485AddressesEqual(const uint8_t* a, const uint8_t* b) {
  for (uint8_t i = 0; i < 8; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

inline bool rs485QueueOutdoorSensorConfig(const uint8_t* address,
                                           uint8_t role,
                                           float offset) {
  if (role > OUTDOOR_SENSOR_COUNT) return false;

  for (uint8_t i = 0; i < outdoorConfigQueueCount; i++) {
    if (rs485AddressesEqual(outdoorConfigQueue[i].address, address)) {
      outdoorConfigQueue[i].role = role;
      outdoorConfigQueue[i].offset = offset;
      outdoorConfigQueue[i].retries = 0;
      return true;
    }
  }

  if (outdoorConfigQueueCount >= OUTDOOR_SENSOR_COUNT) {
    Serial.println("[RS485] Outdoor config queue full");
    return false;
  }

  OutdoorConfigCommand& cmd = outdoorConfigQueue[outdoorConfigQueueCount++];
  memcpy(cmd.address, address, 8);
  cmd.role = role;
  cmd.offset = offset;
  cmd.retries = 0;
  return true;
}

inline void rs485PopOutdoorConfigQueue() {
  if (outdoorConfigQueueCount == 0) return;
  for (uint8_t i = 1; i < outdoorConfigQueueCount; i++) {
    outdoorConfigQueue[i - 1] = outdoorConfigQueue[i];
  }
  outdoorConfigQueueCount--;
}

inline void rs485BeginOutdoorConfigSetTransaction() {
  if (outdoorConfigQueueCount == 0) return;

  // Payload: version + ROM[8] + role + offset int16 x0.01 C = 12 bytes.
  uint8_t payload[AM5_RS485_CONFIG_SET_LEN] = {0};
  payload[AM5_RS485_CONFIG_SET_VERSION] = AM5_RS485_PROTOCOL_VERSION;
  memcpy(&payload[AM5_RS485_CONFIG_SET_ROM_BASE], outdoorConfigQueue[0].address, 8);
  payload[AM5_RS485_CONFIG_SET_ROLE] = outdoorConfigQueue[0].role;
  rs485PutInt16LE(payload, AM5_RS485_CONFIG_SET_OFFSET_BASE,
                  rs485EncodeOffset(outdoorConfigQueue[0].offset));

  rs485ClearStaleRx();
  rs485SendFrame(RS485_ADDR_OUTDOOR,
                 RS485_MSG_OUTDOOR_CONFIG_SET,
                 payload,
                 sizeof(payload));
  rs485MasterTransaction = RS485_TXN_WAIT_OUTDOOR_CONFIG_ACK;
  rs485TransactionStartedMs = millis();
}

inline void rs485BeginOutdoorConfigSnapshotTransaction() {
  const uint8_t payload[AM5_RS485_CONFIG_REQUEST_LEN] = {AM5_RS485_PROTOCOL_VERSION};
  rs485ClearStaleRx();
  rs485SendFrame(RS485_ADDR_OUTDOOR,
                 RS485_MSG_OUTDOOR_CONFIG_REQUEST,
                 payload,
                 sizeof(payload));
  rs485MasterTransaction = RS485_TXN_WAIT_OUTDOOR_CONFIG_SNAPSHOT;
  rs485TransactionStartedMs = millis();
  rs485_outdoor_config_last_request_ms = rs485TransactionStartedMs;
  rs485_outdoor_config_snapshot_requested = false;
}

inline void rs485HandleOutdoorConfigAck(const RS485RxFrame& frame) {
  if (rs485MasterTransaction != RS485_TXN_WAIT_OUTDOOR_CONFIG_ACK) return;
  rs485MasterTransaction = RS485_TXN_IDLE;

  if (frame.len != AM5_RS485_CONFIG_ACK_LEN ||
      frame.payload[AM5_RS485_CONFIG_ACK_VERSION] != AM5_RS485_PROTOCOL_VERSION) {
    Serial.println("[RS485] Invalid Outdoor config ACK");
    if (outdoorConfigQueueCount > 0) {
      if (outdoorConfigQueue[0].retries < 2) outdoorConfigQueue[0].retries++;
      else rs485PopOutdoorConfigQueue();
    }
    return;
  }

  rs485_outdoor_online = true;
  rs485_outdoor_last_seen_ms = millis();

  if (frame.payload[AM5_RS485_CONFIG_ACK_SUCCESS] == 1) {
    rs485PopOutdoorConfigQueue();
    // Do not republish the stale pre-change snapshot. Request a fresh snapshot
    // first; rs485HandleOutdoorConfigSnapshot() will trigger the MQTT publish.
    rs485_outdoor_config_snapshot_requested = true;
  }
  else {
    Serial.println("[RS485] Outdoor rejected sensor configuration");
    rs485PopOutdoorConfigQueue();
    rs485_outdoor_config_snapshot_requested = true;
    outdoorConfigAwaitingFreshSnapshot = true;
  }
}

inline void rs485HandleOutdoorConfigSnapshot(const RS485RxFrame& frame) {
  if (rs485MasterTransaction != RS485_TXN_WAIT_OUTDOOR_CONFIG_SNAPSHOT) return;
  rs485MasterTransaction = RS485_TXN_IDLE;

  // B2 payload: version, count, count * 13-byte records.
  // record = address[8], role, offset int16 x0.01 C, raw temp int16 x0.1 C
  if (frame.len < AM5_RS485_CONFIG_SNAPSHOT_HEADER_LEN ||
      frame.payload[AM5_RS485_CONFIG_SNAPSHOT_VERSION] != AM5_RS485_PROTOCOL_VERSION) return;

  const uint8_t count = frame.payload[AM5_RS485_CONFIG_SNAPSHOT_COUNT];
  const uint8_t expectedLength = static_cast<uint8_t>(
    AM5_RS485_CONFIG_SNAPSHOT_HEADER_LEN + count * AM5_RS485_CONFIG_SNAPSHOT_RECORD_LEN);
  if (count > OUTDOOR_SENSOR_COUNT || frame.len != expectedLength) {
    Serial.println("[RS485] Invalid Outdoor config snapshot length");
    return;
  }

  outdoorSensorDetectedCount = count;

  for (uint8_t i = 0; i < OUTDOOR_SENSOR_COUNT; i++) {
    memset(outdoorSensorAddresses[i], 0, 8);
    outdoorSensorPresent[i] = false;
    outdoorSensorRole[i] = 0;
    outdoorSensorOffset[i] = 0.0f;
    outdoorSensorRawTempC[i] = SENSOR_DISCONNECTED;
    outdoorSensorPhysicalTempC[i] = SENSOR_DISCONNECTED;
  }

  for (uint8_t i = 0; i < count; i++) {
    const uint8_t base = static_cast<uint8_t>(
      AM5_RS485_CONFIG_SNAPSHOT_HEADER_LEN + i * AM5_RS485_CONFIG_SNAPSHOT_RECORD_LEN);
    memcpy(outdoorSensorAddresses[i],
           &frame.payload[base + AM5_RS485_CONFIG_RECORD_ROM_BASE], 8);
    outdoorSensorPresent[i] = true;
    outdoorSensorRole[i] = frame.payload[base + AM5_RS485_CONFIG_RECORD_ROLE];
    outdoorSensorOffset[i] = static_cast<float>(
      rs485GetInt16LE(frame.payload,
                      static_cast<uint8_t>(base + AM5_RS485_CONFIG_RECORD_OFFSET_BASE))) / 100.0f;

    const int16_t raw = rs485GetInt16LE(
      frame.payload, static_cast<uint8_t>(base + AM5_RS485_CONFIG_RECORD_TEMP_BASE));
    outdoorSensorRawTempC[i] = (raw == 8880)
                                ? SENSOR_DISCONNECTED
                                : static_cast<float>(raw) / 10.0f;
    outdoorSensorPhysicalTempC[i] =
      (outdoorSensorRawTempC[i] == SENSOR_DISCONNECTED)
        ? SENSOR_DISCONNECTED
        : outdoorSensorRawTempC[i] + outdoorSensorOffset[i];
  }

  outdoorConfigSnapshotValid = true;
  rs485_outdoor_online = true;
  rs485_outdoor_last_seen_ms = millis();

  // This is the authoritative Outdoor role/offset state. Only now republish
  // sensor configuration so the app cannot be reset by stale role=0 data.
  outdoorConfigAwaitingFreshSnapshot = false;
  mqtt_publish_requested = true;
  Serial.println("[RS485] Outdoor config snapshot updated -> MQTT republish requested");
}

// =====================================================
// FRAME ROUTING / PARSER
// =====================================================
inline void rs485HandleFrame(const RS485RxFrame& frame) {
  if (frame.dest != RS485_ADDR_INDOOR || frame.src != RS485_ADDR_OUTDOOR) return;

  switch (frame.type) {
    case RS485_MSG_OUTDOOR_STATUS_RESPONSE:
      rs485HandleOutdoorStatus(frame);
      break;
    case RS485_MSG_OUTDOOR_CONFIG_ACK:
      rs485HandleOutdoorConfigAck(frame);
      break;
    case RS485_MSG_OUTDOOR_CONFIG_RESPONSE:
      rs485HandleOutdoorConfigSnapshot(frame);
      break;
    default:
      break;
  }
}

inline void rs485ConsumeByte(uint8_t value) {
  rs485LastByteMs = millis();

  switch (rs485RxState) {
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
      if (value > RS485_MAX_PAYLOAD) {
        rs485ResetParser();
        break;
      }
      rs485RxFrame.len = value;
      rs485RxPayloadIndex = 0;
      rs485RxState = (value == 0) ? RX_CRC_LO : RX_PAYLOAD;
      break;

    case RX_PAYLOAD:
      rs485RxFrame.payload[rs485RxPayloadIndex++] = value;
      if (rs485RxPayloadIndex >= rs485RxFrame.len) rs485RxState = RX_CRC_LO;
      break;

    case RX_CRC_LO:
      rs485RxCrcLow = value;
      rs485RxState = RX_CRC_HI;
      break;

    case RX_CRC_HI:
      rs485RxFrame.receivedCrc = static_cast<uint16_t>(rs485RxCrcLow) |
                                 (static_cast<uint16_t>(value) << 8);
      if (rs485CalculateFrameCrc(rs485RxFrame) == rs485RxFrame.receivedCrc) {
        rs485HandleFrame(rs485RxFrame);
      }
      else {
        Serial.println("[RS485] CRC error");
      }
      rs485ResetParser();
      break;
  }
}

inline void rs485HandleTransactionTimeout() {
  if (rs485MasterTransaction == RS485_TXN_IDLE) return;
  if (millis() - rs485TransactionStartedMs <= RS485_RESPONSE_TIMEOUT_MS) return;

  rs485_outdoor_online = false;

  if (rs485MasterTransaction == RS485_TXN_WAIT_OUTDOOR_CONFIG_ACK &&
      outdoorConfigQueueCount > 0) {
    if (outdoorConfigQueue[0].retries < 2) {
      outdoorConfigQueue[0].retries++;
    }
    else {
      rs485PopOutdoorConfigQueue();
      rs485_outdoor_config_snapshot_requested = true;
      outdoorConfigAwaitingFreshSnapshot = true;
    }
  }

  if (rs485MasterTransaction == RS485_TXN_WAIT_OUTDOOR_CONFIG_SNAPSHOT) {
    rs485_outdoor_config_snapshot_requested = false;
    rs485_outdoor_config_last_request_ms = millis();
  }

  rs485MasterTransaction = RS485_TXN_IDLE;
  rs485ClearStaleRx();
}

inline void rs485ServiceMasterScheduler() {
  if (rs485MasterTransaction != RS485_TXN_IDLE) return;
  const unsigned long now = millis();

  // Complete-system power synchronization has highest priority.
  if (rs485_urgent_outdoor_sync) {
    rs485BeginOutdoorStatusTransaction();
    return;
  }

  // The Outdoor slave requires a fresh B0 power/status command lease. Keep the
  // normal 500 ms heartbeat ahead of sensor-configuration traffic so a long or
  // retrying config queue cannot accidentally starve relay authorization.
  if (now - rs485LastOutdoorPollMs >= RS485_OUTDOOR_POLL_INTERVAL_MS) {
    rs485BeginOutdoorStatusTransaction();
    return;
  }

  // App-originated Outdoor sensor configuration.
  if (outdoorConfigQueueCount > 0) {
    rs485BeginOutdoorConfigSetTransaction();
    return;
  }

  // Refresh Outdoor ROM addresses / assignments / offsets for the app.
  if (rs485_outdoor_config_snapshot_requested ||
      now - rs485_outdoor_config_last_request_ms >= RS485_OUTDOOR_CONFIG_INTERVAL_MS) {
    rs485BeginOutdoorConfigSnapshotTransaction();
    return;
  }
}

inline void rs485Loop() {
  if (rs485RxState != RX_WAIT_SOF1 &&
      millis() - rs485LastByteMs > RS485_FRAME_TIMEOUT_MS) {
    rs485ResetParser();
  }

  while (AM5_RS485.available() > 0) {
    rs485ConsumeByte(static_cast<uint8_t>(AM5_RS485.read()));
  }

  rs485HandleTransactionTimeout();
  rs485ServiceMasterScheduler();
}

inline void rs485Init() {
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  rs485SetTransmit(false);
  AM5_RS485.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

  rs485ResetParser();
  rs485MasterTransaction = RS485_TXN_IDLE;
  rs485_outdoor_online = false;
  rs485LastOutdoorPollMs = 0;
  rs485_urgent_outdoor_sync = true;
  rs485_outdoor_config_snapshot_requested = true;
  rs485_outdoor_config_last_request_ms = 0;

  Serial.println("[RS485] AM5 two-node bus initialized");
  Serial.printf("[RS485] Indoor MASTER=0x%02X Outdoor SLAVE=0x%02X Baud=%lu\n",
                RS485_ADDR_INDOOR, RS485_ADDR_OUTDOOR, RS485_BAUD);
}

#endif
