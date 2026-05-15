# CM_KRC_MAIN

## Summary

`CM_KRC_MAIN` is the core automation engine inside `CONTROL_MASTER`. It is an ESP32-based controller for fan and pump operation, temperature monitoring, local control, and cloud-enabled remote management.

## What it does

- Reads temperature from a DS18B20 sensor.
- Controls fan and pump outputs in manual and automatic modes.
- Uses a real-time clock (NTP + RTC) for scheduled operations.
- Connects to Wi-Fi and supports MQTT and HTTP-based control.
- Stores operational settings in non-volatile memory.

## Key Files

- `CM_KRC_MAIN.ino` — main sketch and loop logic.
- `variables.h` — system variables and pin definitions.
- `aws_certificates.h` — cloud and certificate settings.
- `wifithread.h` — Wi-Fi, MQTT, and configuration helper functions.

## Required Hardware

- ESP32 board
- DS18B20 temperature sensor
- Relay drivers for fan and pump outputs
- PCA9554 I/O expander (if used)
- RTC module support via `ISL1208_RTC`
- Wi-Fi network

## Required Libraries

- ArduinoJson
- WiFi
- WiFiClientSecure
- ESPAsyncWebServer
- AsyncTCP
- PubSubClient
- Preferences
- OneWire
- DallasTemperature
- PCA9554
- ISL1208_RTC

## How to Build / Run

1. Open `CONTROL_MASTER/CM_KRC_MAIN/CM_KRC_MAIN.ino` in Arduino IDE.
2. Select `ESP32 Dev Module` or compatible ESP32 board.
3. Install the listed libraries using Library Manager.
4. Configure Wi-Fi and MQTT/cloud parameters in `aws_certificates.h` and `wifithread.h`.
5. Connect the DS18B20, fan/pump relays, and optional I/O expander.
6. Compile and upload the sketch.
7. Open Serial Monitor at `115200` for boot and debug messages.

## Notes

- The firmware is built for modular deployment in the control suite.
- `Preferences` retains setpoints and control state across restarts.
- Review the pin definitions in `variables.h` before wiring.
