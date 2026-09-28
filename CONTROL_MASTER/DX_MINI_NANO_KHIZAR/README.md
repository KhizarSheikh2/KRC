# DX_MINI_NANO_KHIZAR

## Summary

`DX_MINI_NANO_KHIZAR` is a compact ESP32 automation project designed for local control of cooling and wash systems. It includes a display, keypad, scheduler, MQTT integration, and Wi-Fi credential support.

## What it does

- Reads temperature from a DS18B20 sensor.
- Controls relay outputs for evaporative cooling and auto wash.
- Displays status on an OLED screen.
- Supports local keypad input and mode switching.
- Uses MQTT for remote updates and monitoring.
- Stores user settings in non-volatile memory.

## Project Files

- `DX_MINI_NANO_KHIZAR.ino`
- `Def.h`
- `aws_cred.h`
- `temp.h`
- `OLED.h`
- `WifiThread.h`
- `MQTT.h`

## Required Hardware

- ESP32 board
- DS18B20 temperature sensor
- OLED display
- Keypad matrix
- Relay modules
- Buzzer
- External power for relays
- Wi-Fi network

## Required Libraries

- ArduinoJson
- WiFi
- WiFiClientSecure
- OneWire
- DallasTemperature
- PubSubClient
- ESPAsyncWebServer
- Preferences

## How to Build / Run

1. Open `DX_MINI_NANO_KHIZAR/DX_MINI_NANO_KHIZAR.ino` in Arduino IDE.
2. Select `ESP32 Dev Module` or another ESP32-compatible board.
3. Install the listed libraries.
4. Update Wi-Fi credentials in `aws_cred.h`.
5. Confirm device names and pin settings in `Def.h`.
6. Connect the sensor, OLED, keypad, relays, and buzzer.
7. Compile and upload the sketch.
8. Open Serial Monitor at `115200` for logs.

## Notes

- The project uses `Preferences` to preserve power and output states across restarts.
- Wi-Fi AP mode can be used if credentials are not yet configured.
- Refer to the source code headers for detailed pin assignments.
