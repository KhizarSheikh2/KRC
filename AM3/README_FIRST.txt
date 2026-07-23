AM3 TWO-ESP32 ARCHITECTURE - LINK FIXED
=======================================

Communication path:

  Display ESP32 <-> Hardware Host ESP32 <-> MQTT <-> Mobile App

HARDWARE HOST
-------------
- Board: DOIT ESP32 DEVKIT V1
- Always operates in WIFI_AP_STA mode.
- Permanent display AP starts before router Wi-Fi and MQTT.
- AP SSID: AM1-7AA010
- AP password: bitahomes
- AP IP: 192.168.4.1
- TCP JSON server: 192.168.4.1:5050
- Router connection is non-blocking.
- MQTT/TLS timeout is bounded so it cannot hold the display link for a long time.

DISPLAY
-------
- Board: DOIT ESP32 DEVKIT V1
- Operates in WIFI_STA mode only.
- Hardcoded AP SSID/password in DisplayLinkConfig.h.
- Static IP: 192.168.4.50
- Connects to hardware TCP server at 192.168.4.1:5050.
- Sends hello/authentication and heartbeat JSON.
- Receives state JSON every 500 ms.

IMPORTANT
---------
Open both Serial Monitors at 115200 baud during the first test.
The code has AM3_LINK_DEBUG enabled, so every connection stage and JSON packet
is printed. After commissioning, set AM3_LINK_DEBUG to 0 in both
DisplayLinkConfig.h files to reduce Serial output.

DISPLAY 1x4 KEYPAD
------------------
The display firmware also supports a 1x4 membrane keypad:
- GPIO27: common row
- GPIO26: Key 1, left / previous page
- GPIO25: Key 2, up
- GPIO33: Key 3, down
- GPIO32: Key 4, right / next page

No external Keypad library is required. Touch swipe navigation remains enabled.
