# Khizar Office Automation Projects

This repository contains Arduino/ESP32 automation projects developed by Muhammad Khizar for water tank cooling, drainage monitoring, sewerage control, and advanced IoT device management.

Each project folder includes an Arduino IDE sketch (`*.ino`), supporting headers, and a dedicated project README for features, hardware, and build/run instructions.

---

## 📂 Repository Overview

- `CONTROL_MASTER/` — main automation suite with five Arduino IDE projects.
- `DX_MINI_NANO_KHIZAR/` — compact local controller with display, scheduling, and MQTT support.
- `ZMB_Khizar/` — Wi-Fi credential management, API integration, servo control, and remote monitoring.

---

## 📁 Project Documentation

- `CONTROL_MASTER/README.md` — overview of the control suite and build workflow.
- `CONTROL_MASTER/CM_KRC_MAIN/README.md` — fan/pump automation and network control.
- `CONTROL_MASTER/DRAIN_MASTER/README.md` — drainage level control with OLED and keypad.
- `CONTROL_MASTER/SEWERAGE_MASTER_V2/README.md` — sewerage flow control with local display.
- `CONTROL_MASTER/WT_COOLING_ETH_WIFI/README.md` — water tank cooling with Ethernet/Wi-Fi.
- `CONTROL_MASTER/WT_COOLING_Final/README.md` — final water tank cooling controller.
- `DX_MINI_NANO_KHIZAR/README.md` — compact ESP32 controller with scheduler and MQTT.
- `ZMB_Khizar/README.md` — Wi-Fi credential server, API, IR, and servo control.

---

## 🧰 How to Use This Repository

1. Open the repository in Arduino IDE or VS Code with the Arduino extension.
2. Open the project `.ino` file for the folder you want to work on.
3. Install the required Arduino libraries listed in each project README.
4. Configure Wi-Fi, MQTT, and AWS credentials in the appropriate header files (for example `aws_certificates.h`, `aws_cred.h`, `wifithread.h`, or `certificates.h`).
5. Connect the hardware components according to the pin definitions in the sketch and header files.
6. Compile and upload to an ESP32 board.
7. Use Serial Monitor at `115200` baud for device boot logs and debugging.

---

## ⚙️ Common Technology Stack

- **Boards:** ESP32 family
- **Languages:** C / C++
- **Connectivity:** Wi-Fi, MQTT, Ethernet
- **Displays:** OLED (SH110x compatible)
- **Sensors:** DS18B20 temperature sensor, water level sensors
- **Interfaces:** Keypad, buzzer, relay outputs, servo, IR receiver

---

## 🧩 Recommended Workflow

- Start with `CONTROL_MASTER/README.md` for the overall control suite.
- Open the specific subproject README before wiring or uploading.
- Validate library dependencies before compilation.
- Use the `Preferences`-based persistent storage modules to preserve settings across reboots.

---

## 📈 Current Progress

- `CONTROL_MASTER` is organized into five distinct controller projects and includes advanced Wi-Fi/MQTT integration.
- `DX_MINI_NANO_KHIZAR` is functional as a compact local controller with display, scheduler, and relay outputs.
- `ZMB_Khizar` includes Wi-Fi credential management, API fetching, OLED display, and servo control.

---

## 📦 Repository Structure

```text
.
├── CONTROL_MASTER/
│   ├── CM_KRC_MAIN/
│   ├── DRAIN_MASTER/
│   ├── SEWERAGE_MASTER_V2/
│   ├── WT_COOLING_ETH_WIFI/
│   └── WT_COOLING_Final/
├── DX_MINI_NANO_KHIZAR/
└── ZMB_Khizar/
```

---

## 📌 Notes

- Each project is built for Arduino IDE and ESP32-based development.
- Project README files are available inside each folder for more detailed setup information.
- The repository is intended for rapid testing, field validation, and incremental feature expansion.

