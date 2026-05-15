# WT_COOLING_Final

## Summary

`WT_COOLING_Final` is the production-ready water tank cooling controller in the `CONTROL_MASTER` suite. It supports temperature-based fan and pump control with local OLED/keypad operation and Wi-Fi connectivity.

## What it does

- Monitors tank temperature with a DS18B20 sensor.
- Controls fan and pump outputs for cooling.
- Displays status on an OLED screen.
- Uses a keypad for local mode selection and control.
- Provides buzzer alerts for status and events.
- Supports Wi-Fi for remote command reception.

## Key Files

- `WT_COOLING_Final.ino` — project firmware.
- `variables.h` — pin and system variables.
- `aws_certificates.h` — network/cloud credentials.
- `wifithread.h` — Wi-Fi, MQTT, and remote control helper functions.

## Required Hardware

- ESP32 board
- DS18B20 temperature sensor
- Fan and pump relays
- OLED display (SH110x compatible)
- Keypad matrix
- Buzzer
- Wi-Fi network

## Required Libraries

- WiFi
- WiFiClientSecure
- ESPAsyncWebServer
- AsyncTCP
- PubSubClient
- ArduinoJson
- Preferences
- OneWire
- DallasTemperature
- Wire
- Adafruit_SH110X
- Keypad

## How to Build / Run

1. Open `CONTROL_MASTER/WT_COOLING_Final/WT_COOLING_Final.ino` in Arduino IDE.
2. Select the ESP32 board and proper COM port.
3. Install the required libraries.
4. Configure Wi-Fi and AWS credentials in `aws_certificates.h` and `wifithread.h`.
5. Connect the DS18B20, relays, OLED, keypad, and buzzer.
6. Compile and upload the sketch.
7. Open Serial Monitor at `115200` for startup logs.

## Notes

- This version is intended as the final water tank cooling implementation.
- Confirm the pin mappings in `variables.h` before wiring.
- Local control is available through the keypad and OLED user interface.
