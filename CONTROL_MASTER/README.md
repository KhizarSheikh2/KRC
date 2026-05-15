# CONTROL_MASTER

This folder contains the main control and automation suite for water cooling, drainage, and sewerage systems. Each subfolder is a separate Arduino IDE project with its own `*.ino` sketch, networking support, and sensor/actuator logic.

## Included Projects

- `CM_KRC_MAIN/` — Central fan and pump controller with temperature sensing, RTC sync, and cloud connectivity.
- `DRAIN_MASTER/` — Drainage monitoring and pump control with OLED display, keypad, and alarm.
- `SEWERAGE_MASTER_V2/` — Sewerage level controller with local UI, pump automation, and remote monitoring.
- `WT_COOLING_ETH_WIFI/` — Water tank cooler with both Ethernet and Wi-Fi connectivity using W5500.
- `WT_COOLING_Final/` — Final water tank cooling controller featuring keypad, OLED status, and remote control.

## Project READMEs

- `CONTROL_MASTER/CM_KRC_MAIN/README.md`
- `CONTROL_MASTER/DRAIN_MASTER/README.md`
- `CONTROL_MASTER/SEWERAGE_MASTER_V2/README.md`
- `CONTROL_MASTER/WT_COOLING_ETH_WIFI/README.md`
- `CONTROL_MASTER/WT_COOLING_Final/README.md`

## How to Build / Run

1. Open the project `.ino` file in Arduino IDE.
2. Select an ESP32 board such as `ESP32 Dev Module`.
3. Install required libraries using Arduino Library Manager.
4. Configure Wi-Fi, MQTT, and cloud settings in `aws_certificates.h`, `wifithread.h`, or `certificates.h`.
5. Wire the sensors, relays, OLED display, keypad, and additional hardware.
6. Compile and upload the sketch.
7. Open the Serial Monitor at `115200` baud to observe startup logs.

## Required Libraries (common list)

- ArduinoJson
- WiFi
- WiFiClientSecure
- ESPAsyncWebServer
- AsyncTCP
- PubSubClient
- Preferences
- OneWire
- DallasTemperature
- Adafruit_SH110X
- Keypad
- Ethernet
- EthernetUdp
- SSLClient
- PCA9554
- ISL1208_RTC

## Notes

- Each subproject has its own README with details for hardware, wiring, and configuration.
- `WT_COOLING_ETH_WIFI` uses the W5500 Ethernet module and secure MQTT when networked.
- `Preferences` is used in several sketches for persistent user settings.
- Use project-specific header files to update Wi-Fi, AWS, and MQTT credentials.
