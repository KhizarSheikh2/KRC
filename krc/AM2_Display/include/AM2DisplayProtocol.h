#pragma once
#include <stdint.h>
#include <stddef.h>

namespace AM2Proto {

static constexpr uint8_t SOF1 = 0xA5;
static constexpr uint8_t SOF2 = 0x5A;
static constexpr uint8_t VERSION = 1;
static constexpr uint8_t DISPLAY_ADDR = 0x03;
static constexpr uint8_t MSG_STATE = 0x10;
static constexpr uint8_t MSG_COMMAND = 0x21;
static constexpr uint16_t MAX_PAYLOAD = 128;

#pragma pack(push, 1)
struct FrameHeader {
  uint8_t sof1;
  uint8_t sof2;
  uint8_t version;
  uint8_t address;
  uint8_t type;
  uint8_t seq;
  uint16_t length;
};

struct StatePacket {
  uint32_t uptimeSec;
  char deviceName[16];

  int16_t returnTemp10;
  int16_t supplyTemp10;
  int16_t suctionTemp10;
  int16_t dischargeTemp10;

  int16_t suctionPsi;
  int16_t dischargePsi;
  int16_t oilPsi;
  int16_t suctionBar;
  int16_t dischargeBar;
  int16_t oilBar;

  int16_t amp1;
  int16_t amp2;
  int16_t amp3;

  int16_t returnSp;
  int16_t returnAlertSp;
  int16_t suctionAlertSp;
  int16_t dischargeAlertSp;
  int16_t suctionPsiSp;
  int16_t dischargePsiSp;
  int16_t oilPsiSp;
  int16_t ampsLimit;
  int16_t restartDelaySec;

  uint8_t compStatus;
  uint8_t mode;
  uint8_t systemEnabled;
  uint8_t runOutput;
  uint8_t alarmOutput;
  uint8_t alarmLatched;
  uint8_t wifiConnected;
  uint8_t mqttConnected;

  uint8_t autoReset;
  uint8_t ctEnabled;
  uint8_t lpsEnable;
  uint8_t hpsEnable;
  uint8_t opsEnable;
  uint8_t suctionPressureEnable;
  uint8_t dischargePressureEnable;
  uint8_t oilPressureEnable;

  uint8_t lpsInputActive;
  uint8_t hpsInputActive;
  uint8_t opsInputActive;
  uint8_t compressorFeedback;
  uint8_t reserved[4];
};

struct CommandPacket {
  uint8_t command;
  int32_t value;
};
#pragma pack(pop)

enum Command : uint8_t {
  CMD_NONE = 0,
  CMD_START = 1,
  CMD_STOP = 2,
  CMD_RETURN_SP = 3,
  CMD_RETURN_ALERT_SP = 4,
  CMD_SUCTION_ALERT_SP = 5,
  CMD_DISCHARGE_ALERT_SP = 6,
  CMD_SUCTION_PSI_SP = 7,
  CMD_DISCHARGE_PSI_SP = 8,
  CMD_OIL_PSI_SP = 9,
  CMD_AMPS_LIMIT = 10,
  CMD_MODE = 11,
  CMD_AUTO_RESET = 12,
  CMD_CT_ENABLE = 13,
  CMD_LPS_ENABLE = 14,
  CMD_HPS_ENABLE = 15,
  CMD_OPS_ENABLE = 16,
  CMD_SUCTION_PRESSURE_ENABLE = 17,
  CMD_DISCHARGE_PRESSURE_ENABLE = 18,
  CMD_OIL_PRESSURE_ENABLE = 19,
  CMD_RESTART_DELAY = 20
};

inline uint16_t crc16(const uint8_t* data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 1U) ? static_cast<uint16_t>((crc >> 1U) ^ 0xA001U)
                       : static_cast<uint16_t>(crc >> 1U);
    }
  }
  return crc;
}

static_assert(sizeof(StatePacket) <= MAX_PAYLOAD, "AM2 state packet exceeds RS485 payload buffer");
static_assert(sizeof(CommandPacket) == 5, "AM2 command packet packing changed");

}  // namespace AM2Proto
