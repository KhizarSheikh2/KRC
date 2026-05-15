# DRAIN_MASTER

## Summary

`DRAIN_MASTER` is an ESP32-based drainage controller that monitors water level sensors and operates a pump to prevent overflow. It includes a local OLED interface, keypad control, and alarm notification.

## What it does

- Reads water level sensors for low/medium/high states.
- Activates a pump relay to drain excess water.
- Displays operational status on an OLED screen.
- Allows local mode and status control via keypad.
- Generates buzzer alerts for fault or overflow conditions.
- Supports remote connectivity through Wi-Fi and MQTT.

## Key Files

- `DRAIN_MASTER.ino` — main sketch.
- `variables.h` — pin assignments and shared variables.
- `aws_certificates.h` — Wi-Fi/cloud credentials.
- `wifithread.h` — network and MQTT helper functions.

## Required Hardware

- ESP32 board
- Water level sensors
- Pump relay module
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

1. Open `CONTROL_MASTER/DRAIN_MASTER/DRAIN_MASTER.ino` in Arduino IDE.
2. Select the ESP32 board and correct USB port.
3. Install the required libraries.
4. Configure Wi-Fi and cloud credentials in `aws_certificates.h` and `wifithread.h`.
5. Connect the OLED, keypad, pump relay, water level sensors, and buzzer.
6. Compile and upload the sketch.
7. Use the display and keypad for local control and status feedback.

## Notes

- Check the pin definitions in `variables.h` before wiring the hardware.
- The display and keypad simplify local operation when Wi-Fi is unavailable.
