# WT_COOLING_ETH_WIFI

## Summary

`WT_COOLING_ETH_WIFI` is a water tank cooling controller with both Ethernet and Wi-Fi connectivity. It is designed for robust, networked operation using an ESP32 and W5500 module.

## What it does

- Reads water temperature from a DS18B20 sensor.
- Controls fan and pump relays to cool the tank.
- Supports Ethernet connectivity with the W5500 module.
- Falls back to Wi-Fi for alternate connectivity.
- Displays status on an OLED interface.
- Accepts user input via keypad and buzzer alerts.
- Uses AWS certificate-based cloud/MQTT integration.

## Key Files

- `WT_COOLING_ETH_WIFI.ino` — main firmware.
- `certificates.h` — SSL certificate definitions.
- `aws_certificates.h` — cloud connection credentials.
- `variables.h` — pin definitions and settings.
- `wifithread.h` — network and MQTT helpers.

## Required Hardware

- ESP32 board
- W5500 Ethernet module
- DS18B20 temperature sensor
- Fan and pump relays
- OLED display (SH110x compatible)
- Keypad matrix
- Buzzer
- Ethernet cable
- Wi-Fi network (optional)

## Required Libraries

- ArduinoJson
- WiFi
- Ethernet
- EthernetUdp
- SSLClient
- WiFiClientSecure
- ESPAsyncWebServer
- AsyncTCP
- PubSubClient
- Preferences
- OneWire
- DallasTemperature
- Adafruit_SH110X
- Keypad

## How to Build / Run

1. Open `CONTROL_MASTER/WT_COOLING_ETH_WIFI/WT_COOLING_ETH_WIFI.ino` in Arduino IDE.
2. Select `ESP32 Dev Module` or compatible board.
3. Install the required libraries.
4. Set up network and AWS credentials in `certificates.h`, `aws_certificates.h`, and `wifithread.h`.
5. Wire the W5500 module, DS18B20, relays, OLED, keypad, and buzzer.
6. Compile and upload the sketch.
7. Open Serial Monitor at `115200` for logs.

## Notes

- Use the Ethernet connection for stable network access.
- Wi-Fi is supported as an alternate connectivity path.
- Verify the hardware pin mapping in `variables.h` before wiring.
