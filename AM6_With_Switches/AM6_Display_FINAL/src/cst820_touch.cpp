#include "cst820_touch.h"

namespace {
constexpr uint8_t REG_FINGER_COUNT  = 0x02;
constexpr uint8_t REG_TOUCH_DATA    = 0x03;
constexpr uint8_t REG_DISABLE_SLEEP = 0xFE;
constexpr uint8_t ADDRESS_CANDIDATES[] = {0x15, 0x14, 0x2E, 0x38};
}

void CST820Touch::resetController() {
    // Match the vendor CST820 reset sequence used by working JC2432W328C
    // examples: hold RESET low briefly, then release high and wait for boot.
    pinMode(AM6_TOUCH_RST, OUTPUT);
    digitalWrite(AM6_TOUCH_RST, LOW);
    delay(10);
    digitalWrite(AM6_TOUCH_RST, HIGH);
    delay(300);
}

bool CST820Touch::probe(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

bool CST820Touch::begin() {
    Wire.begin(AM6_TOUCH_SDA, AM6_TOUCH_SCL, AM6_TOUCH_I2C_HZ);
    Wire.setTimeOut(20);
    resetController();

    for (uint8_t candidate : ADDRESS_CANDIDATES) {
        if (probe(candidate)) {
            address_ = candidate;
            break;
        }
    }

    if (address_ == 0) {
        ready_ = false;
        Serial.println("[TOUCH] CST820 not detected");
        return false;
    }

    // Disable CST820 automatic low-power mode. This is also done in the
    // vendor driver and prevents delayed/missed wake-ups between touches.
    if (!writeRegister(REG_DISABLE_SLEEP, 0xFF)) {
        Serial.println("[TOUCH] Warning: failed to disable auto sleep");
    }

    ready_ = true;
    Serial.print("[TOUCH] CST820 detected at 0x");
    Serial.print(address_, HEX);
    Serial.print(" | I2C=");
    Serial.print(AM6_TOUCH_I2C_HZ);
    Serial.println(" Hz");
    return true;
}

bool CST820Touch::writeRegister(uint8_t reg, uint8_t value) {
    if (address_ == 0) return false;

    for (uint8_t attempt = 0; attempt < AM6_TOUCH_READ_RETRIES; ++attempt) {
        Wire.beginTransmission(address_);
        Wire.write(reg);
        Wire.write(value);
        if (Wire.endTransmission() == 0) return true;
        delayMicroseconds(150);
    }
    return false;
}

bool CST820Touch::readRegister(uint8_t reg, uint8_t& value) {
    return readRegisters(reg, &value, 1);
}

bool CST820Touch::readRegisters(uint8_t reg, uint8_t* data, size_t length) {
    if (address_ == 0 || data == nullptr || length == 0 || length > 16) {
        return false;
    }

    for (uint8_t attempt = 0; attempt < AM6_TOUCH_READ_RETRIES; ++attempt) {
        Wire.beginTransmission(address_);
        Wire.write(reg);

        if (Wire.endTransmission(false) == 0) {
            const size_t received = Wire.requestFrom(
                address_, static_cast<uint8_t>(length));

            if (received == length) {
                for (size_t i = 0; i < length; ++i) {
                    if (!Wire.available()) return false;
                    data[i] = static_cast<uint8_t>(Wire.read());
                }
                return true;
            }
        }

        while (Wire.available()) {
            (void)Wire.read();
        }
        delayMicroseconds(150);
    }

    return false;
}

void CST820Touch::transformToLandscape(uint16_t rawX,
                                       uint16_t rawY,
                                       uint16_t& x,
                                       uint16_t& y) {
    auto clampSigned = [](int32_t value, uint16_t maximum) -> uint16_t {
        if (value <= 0) return 0;
        if (value >= static_cast<int32_t>(maximum)) return maximum;
        return static_cast<uint16_t>(value);
    };

#if AM6_DISPLAY_ROTATION == 1
    // Native portrait: 240 x 320. Landscape rotation 1 is equivalent to
    // swap_xy + mirror_y.
    x = (rawY >= AM6_SCREEN_WIDTH)
            ? static_cast<uint16_t>(AM6_SCREEN_WIDTH - 1)
            : rawY;
    y = clampSigned(239 - static_cast<int32_t>(rawX),
                    static_cast<uint16_t>(AM6_SCREEN_HEIGHT - 1));
#elif AM6_DISPLAY_ROTATION == 3
    x = clampSigned(319 - static_cast<int32_t>(rawY),
                    static_cast<uint16_t>(AM6_SCREEN_WIDTH - 1));
    y = (rawX >= AM6_SCREEN_HEIGHT)
            ? static_cast<uint16_t>(AM6_SCREEN_HEIGHT - 1)
            : rawX;
#elif AM6_DISPLAY_ROTATION == 2
    x = clampSigned(239 - static_cast<int32_t>(rawX), 239);
    y = clampSigned(319 - static_cast<int32_t>(rawY), 319);
#else
    x = (rawX > 239U) ? 239U : rawX;
    y = (rawY > 319U) ? 319U : rawY;
#endif
}

bool CST820Touch::read(uint16_t& x, uint16_t& y) {
    if (!ready_) return false;

    uint8_t fingers = 0;
    if (!readRegister(REG_FINGER_COUNT, fingers)) return false;
    if ((fingers & 0x0F) == 0) return false;

    uint8_t data[4] = {0, 0, 0, 0};
    if (!readRegisters(REG_TOUCH_DATA, data, sizeof(data))) return false;

    const uint16_t rawX = static_cast<uint16_t>(
        ((static_cast<uint16_t>(data[0]) & 0x0F) << 8) | data[1]);
    const uint16_t rawY = static_cast<uint16_t>(
        ((static_cast<uint16_t>(data[2]) & 0x0F) << 8) | data[3]);

    // Reject corrupted/transient samples instead of clamping them onto a UI
    // edge where they could create a false press.
    if (rawX > 239U || rawY > 319U) return false;

    transformToLandscape(rawX, rawY, x, y);
    return true;
}
