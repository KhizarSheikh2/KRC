# SEWERAGE_MASTER_V2

## Summary

`SEWERAGE_MASTER_V2` provides sewerage monitoring and control for fluid level management, pump automation, and user feedback. It is designed for ESP32/Arduino IDE deployment.

## What it does

- Detects sewerage levels using level sensors.
- Controls a pump relay to manage flow.
- Displays status and alerts on an OLED display.
- Supports keypad-based local control.
- Optionally connects to Wi-Fi and MQTT for remote monitoring.

## Key Files

- `SEWERAGE_MASTER_V2.ino` — main project firmware.
- `variables.h` — hardware pin mapping and variables.
- `aws_certificates.h` — Wi-Fi/cloud credential storage.
- `wifithread.h` — networking and MQTT functions.

## Required Hardware

- ESP32 board
- Sewerage/water level sensors
- Pump relay
- OLED display (SH110x compatible)
- Keypad matrix
- Buzzer
- Wi-Fi network

## Required Libraries

- WiFi
- ESPAsyncWebServer
- PubSubClient
- WiFiClientSecure
- ArduinoJson
- Preferences
- Wire
- Adafruit_SH110X
- Keypad

## How to Build / Run

1. Open `CONTROL_MASTER/SEWERAGE_MASTER_V2/SEWERAGE_MASTER_V2.ino` in Arduino IDE.
2. Select the ESP32 board and the correct port.
3. Install the required libraries.
4. Configure Wi-Fi and cloud settings in `aws_certificates.h` and `wifithread.h`.
5. Wire the sensors, pump relay, OLED, keypad, and buzzer.
6. Compile and upload the sketch.
7. Use the OLED display and keypad to observe system status.

## Notes

- Persistent values are stored in `Preferences` for stable behavior.
- Review `variables.h` for pin mappings and adjust if your hardware differs.
