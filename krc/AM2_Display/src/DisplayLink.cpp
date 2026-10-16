#include "DisplayLink.h"
#include <cstring>
#include "BoardConfig.h"

void DisplayLink::begin() {
  pinMode(AM2_RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(AM2_RS485_DE_RE_PIN, LOW);
  serial_.begin(AM2_RS485_BAUD, SERIAL_8N1, AM2_RS485_RX_PIN, AM2_RS485_TX_PIN);
  resetParser();
}

bool DisplayLink::connected() const {
  return lastRxMs_ != 0 && (millis() - lastRxMs_) < 1800UL;
}

void DisplayLink::queueCommand(uint8_t command, int32_t value) {
  pending_.command = command;
  pending_.value = value;
  pendingValid_ = true;
}

void DisplayLink::loop() {
  while (serial_.available() > 0) processByte(static_cast<uint8_t>(serial_.read()));
}

void DisplayLink::resetParser() {
  stage_ = RxStage::Sof1;
  std::memset(&header_, 0, sizeof(header_));
  headerIndex_ = 0;
  payloadIndex_ = 0;
  crcLo_ = 0;
}

void DisplayLink::processByte(uint8_t b) {
  switch (stage_) {
    case RxStage::Sof1:
      if (b == AM2Proto::SOF1) {
        header_.sof1 = b;
        stage_ = RxStage::Sof2;
      }
      break;

    case RxStage::Sof2:
      if (b == AM2Proto::SOF2) {
        header_.sof2 = b;
        headerIndex_ = 0;
        stage_ = RxStage::HeaderRest;
      } else {
        stage_ = (b == AM2Proto::SOF1) ? RxStage::Sof2 : RxStage::Sof1;
      }
      break;

    case RxStage::HeaderRest: {
      uint8_t* rest = reinterpret_cast<uint8_t*>(&header_) + 2;
      rest[headerIndex_++] = b;
      if (headerIndex_ >= sizeof(AM2Proto::FrameHeader) - 2) {
        if (header_.version != AM2Proto::VERSION ||
            header_.address != AM2Proto::DISPLAY_ADDR ||
            header_.length > AM2Proto::MAX_PAYLOAD) {
          resetParser();
        } else if (header_.length == 0) {
          stage_ = RxStage::CrcLo;
        } else {
          payloadIndex_ = 0;
          stage_ = RxStage::Payload;
        }
      }
      break;
    }

    case RxStage::Payload:
      payload_[payloadIndex_++] = b;
      if (payloadIndex_ >= header_.length) stage_ = RxStage::CrcLo;
      break;

    case RxStage::CrcLo:
      crcLo_ = b;
      stage_ = RxStage::CrcHi;
      break;

    case RxStage::CrcHi: {
      const uint16_t received = static_cast<uint16_t>(crcLo_ | (static_cast<uint16_t>(b) << 8));
      acceptFrame(received);
      resetParser();
      break;
    }
  }
}

void DisplayLink::acceptFrame(uint16_t receivedCrc) {
  uint8_t crcData[6 + AM2Proto::MAX_PAYLOAD];
  crcData[0] = header_.version;
  crcData[1] = header_.address;
  crcData[2] = header_.type;
  crcData[3] = header_.seq;
  crcData[4] = static_cast<uint8_t>(header_.length & 0xFF);
  crcData[5] = static_cast<uint8_t>(header_.length >> 8);
  if (header_.length > 0) std::memcpy(crcData + 6, payload_, header_.length);

  if (AM2Proto::crc16(crcData, 6 + header_.length) != receivedCrc) return;
  if (header_.type != AM2Proto::MSG_STATE || header_.length != sizeof(AM2Proto::StatePacket)) return;

  std::memcpy(&state_, payload_, sizeof(state_));
  state_.deviceName[sizeof(state_.deviceName) - 1] = '\0';
  lastRxMs_ = millis();
  ++revision_;

  if (pendingValid_) sendPendingCommand(header_.seq);
}

void DisplayLink::sendPendingCommand(uint8_t requestSeq) {
  // The controller releases the bus immediately after its state frame.
  delayMicroseconds(1500);
  sendFrame(AM2Proto::MSG_COMMAND, requestSeq, &pending_, sizeof(pending_));
  pendingValid_ = false;
}

void DisplayLink::sendFrame(uint8_t type, uint8_t seq, const void* payload, uint16_t length) {
  if (length > AM2Proto::MAX_PAYLOAD) return;

  AM2Proto::FrameHeader h{};
  h.sof1 = AM2Proto::SOF1;
  h.sof2 = AM2Proto::SOF2;
  h.version = AM2Proto::VERSION;
  h.address = AM2Proto::DISPLAY_ADDR;
  h.type = type;
  h.seq = seq;
  h.length = length;

  uint8_t crcData[6 + AM2Proto::MAX_PAYLOAD];
  crcData[0] = h.version;
  crcData[1] = h.address;
  crcData[2] = h.type;
  crcData[3] = h.seq;
  crcData[4] = static_cast<uint8_t>(h.length & 0xFF);
  crcData[5] = static_cast<uint8_t>(h.length >> 8);
  if (length > 0 && payload != nullptr) std::memcpy(crcData + 6, payload, length);
  const uint16_t crc = AM2Proto::crc16(crcData, 6 + length);

  digitalWrite(AM2_RS485_DE_RE_PIN, HIGH);
  delayMicroseconds(80);
  serial_.write(reinterpret_cast<const uint8_t*>(&h), sizeof(h));
  if (length > 0 && payload != nullptr) serial_.write(reinterpret_cast<const uint8_t*>(payload), length);
  serial_.write(static_cast<uint8_t>(crc & 0xFF));
  serial_.write(static_cast<uint8_t>(crc >> 8));
  serial_.flush();
  delayMicroseconds(120);
  digitalWrite(AM2_RS485_DE_RE_PIN, LOW);
}
