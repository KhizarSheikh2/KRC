#pragma once
#include <Arduino.h>
#include "AM2DisplayProtocol.h"

class DisplayLink {
 public:
  void begin();
  void loop();
  void queueCommand(uint8_t command, int32_t value = 0);

  bool connected() const;
  const AM2Proto::StatePacket& state() const { return state_; }
  uint32_t revision() const { return revision_; }
  uint32_t lastRxMs() const { return lastRxMs_; }

 private:
  enum class RxStage : uint8_t { Sof1, Sof2, HeaderRest, Payload, CrcLo, CrcHi };

  void resetParser();
  void processByte(uint8_t b);
  void acceptFrame(uint16_t receivedCrc);
  void sendPendingCommand(uint8_t requestSeq);
  void sendFrame(uint8_t type, uint8_t seq, const void* payload, uint16_t length);

  HardwareSerial serial_{2};
  AM2Proto::StatePacket state_{};
  AM2Proto::CommandPacket pending_{};
  bool pendingValid_ = false;
  uint32_t revision_ = 0;
  uint32_t lastRxMs_ = 0;

  RxStage stage_ = RxStage::Sof1;
  AM2Proto::FrameHeader header_{};
  uint8_t headerIndex_ = 0;
  uint8_t payload_[AM2Proto::MAX_PAYLOAD]{};
  uint16_t payloadIndex_ = 0;
  uint8_t crcLo_ = 0;
};
