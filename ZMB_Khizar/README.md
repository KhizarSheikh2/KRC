# ZMB_Khizar

## Summary

`ZMB_Khizar` is an ESP32-based device controller designed for remote monitoring, Wi-Fi credential management, servo/damper control, and API-driven automation.

## What it does

- Reads temperature from a DS18B20 sensor.
- Controls a servo for damper or mechanical movement.
- Receives IR input using `IRremote`.
- Hosts a Wi-Fi credential setup server.
- Publishes and subscribes via MQTT over a secure client.
- Fetches external data from an API.
- Shows status on an OLED display.
- Stores settings in `Preferences`.

## Project Files

- `ZMB_Khizar.ino`
- `GET_from_API.h`
- `Servo.h`
- `ServerForWiFiCredentials.h`
- `OLED_Display.h`
- `aws_cred.h`
- `variables.h`

## Required Hardware

- ESP32 board
- DS18B20 temperature sensor
- Servo motor
- IR receiver
- OLED display
- Keypad matrix
- Buzzer
- Wi-Fi network

## Required Libraries

- IRremote
- WiFi
- WiFiClientSecure
- PubSubClient
- ArduinoJson
- Wire
- Keypad
- OneWire
- DallasTemperature
- Preferences

## How to Build / Run

1. Open `ZMB_Khizar/ZMB_Khizar.ino` in Arduino IDE.
2. Select the ESP32 board and correct serial port.
3. Install the listed libraries.
4. Update Wi-Fi credentials in `aws_cred.h`.
5. Verify API settings in `GET_from_API.h`.
6. Connect the temperature sensor, servo, IR receiver, OLED, keypad, and buzzer.
7. Compile and upload the sketch.
8. Open Serial Monitor at `115200` for logs.

## Notes

- Use the internal Wi-Fi credential server when network settings are not preconfigured.
- The API integration and remote control features rely on secure MQTT and Wi-Fi connectivity.
