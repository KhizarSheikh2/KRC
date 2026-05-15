# Room Management System (RMS_Main)

This project contains the Arduino IDE sketch for the Room Management System (RMS), designed to run on an ESP32. It provides comprehensive control over a room's lighting, HVAC, and motorized blinds, featuring both a local HMI (Human-Machine Interface) and remote control via AWS IoT MQTT.

## 🌟 Key Features
- **Lighting Control**: Dimming and ON/OFF control for multiple lights.
- **HVAC Integration**: AC control, room fan speed, and damper management based on temperature setpoints.
- **Motorized Blinds**: Control for shutters and curtains.
- **Connectivity**: 
  - Connects to Wi-Fi for remote AWS MQTT control via mobile app.
  - Maintains an "Always-On" AP (Access Point) for a local DWIN HMI display.
- **State Persistence**: Uses ESP32 `Preferences` to save relay states, preventing sudden changes or "jerks" after a power cycle.
- **Hardware Integration**: Uses PCA9554 I2C I/O expanders for relays, DS18B20 for temperature sensing, and an RS485 interface.

## 🛠 Hardware Required
- **ESP32 Microcontroller**
- **PCA9554** I2C I/O Expander
- **DS18B20** Temperature Sensor (1-Wire)
- **RS485 Transceiver** (for external Modbus/RS485 communication)
- **Zero-cross Dimmers** (for lighting and fan speed control)
- **DWIN HMI Display** (connects to the local AP `RMS-AAA001`)

## 🔌 Pin Configuration

### ESP32 Native Pins
- **Pin 19**: 1-Wire Bus (DS18B20 Temperature Sensor)
- **Pin 13**: Dimmer Output 1 (Light 1)
- **Pin 18**: Dimmer Output 2 (Light 2)
- **Pin 2**: Dimmer Output 3 (Light 3)
- **Pin 35**: Zero-cross Sync Pin (for dimmers)
- **Pin 16**: RS485 RX
- **Pin 17**: RS485 TX

### PCA9554 I2C Expander (Address 0x27)
- **Pin 0**: Light 1 Relay
- **Pin 1**: Light 2 Relay
- **Pin 2**: AC Relay
- **Pin 3**: Room Fan Relay
- **Pin 4**: Smart TV Relay
- **Pin 5**: Shutters Relay
- **Pin 6**: Curtains Relay
- **Pin 7**: Damper Relay

### RS485 Virtual Outputs (DX Coil)
- **Address 0**: Mode 0
- **Address 1**: Mode 1
- **Address 2**: Mode 2
- **Address 3**: Winter Mode (High)
- **Address 4**: Summer Mode (High)

## 🚀 Getting Started

1. **Open the Project:** Open `RMS_Main.ino` in the Arduino IDE.
2. **Install Libraries:** Ensure all required libraries are installed (e.g., `PubSubClient`, `ESPAsyncWebServer`, `PCA9554`, `DallasTemperature`, `ArduinoJson`, `RBDdimmer`).
3. **Configure Certificates:** Update AWS IoT certificates in `aws_certificates.h`.
4. **Compile and Upload:** Upload the sketch to the ESP32.
5. **Initial Setup:** 
   - The device will broadcast an AP named `RMS-AAA001` (Password: `123456789`).
   - Connect to this AP to configure your local Wi-Fi via the mobile app endpoint or allow the HMI to connect automatically.

## 📁 Files Included
- `RMS_Main.ino` - Main application logic, web server, and MQTT setup.
- `aws_certificates.h` - AWS IoT Core certificates and keys.
- `Readme.h` - Pinout definitions and hardware mapping notes.
- `RS485_outputs.h` - RS485 specific configurations and commands.
