#include <Arduino.h>
#include <math.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "esp_task_wdt.h"

#include "aws_certificates.h"
#include "variables.h"
#include "temp.h"
#include "relay_control.h"
#include "rs485.h"
#include "mqtt.h"
#include "display_link.h"
#include "wifithread.h"

int convertToFahrenheit(float temp) {
  if (temp == SENSOR_DISCONNECTED || isnan(temp)) return 888;
  return static_cast<int>(roundf(((temp * 9.0f) / 5.0f) + 32.0f));
}

void resetPublishedTemperatures() {
  SuctionTempC = SENSOR_DISCONNECTED;
  dischargeTempC = SENSOR_DISCONNECTED;
  SupplyTempC = SENSOR_DISCONNECTED;
  ReturnTempC = SENSOR_DISCONNECTED;
  OilTempC = SENSOR_DISCONNECTED;
  OtherTempC = SENSOR_DISCONNECTED;

  SuctionTemp = 888;
  dischargeTemp = 888;
  SupplyTemp = 888;
  ReturnTemp = 888;
  OilTemp = 888;
  OtherTemp = 888;
}

void refreshTemperatureSensorList() {
  numberOfDevices = 0;

  for (uint8_t i = 0; i < INDOOR_SENSOR_COUNT; i++) {
    DeviceAddress address = {0};
    const bool present = sensors.getAddress(address, i);

    tempSensorPresent[i] = present;
    if (present) {
      memcpy(tempSensorAddresses[i], address, 8);
      numberOfDevices++;
    }
    else {
      memset(tempSensorAddresses[i], 0, 8);
      tempSensorRole[i] = 0;
      tempSensorOffset[i] = 0.0f;
      indoorSensorRawTempC[i] = SENSOR_DISCONNECTED;
    }
  }

  const int detectedDevices = sensors.getDeviceCount();
  if (detectedDevices > INDOOR_SENSOR_COUNT) {
    Serial.println("More than 6 Indoor sensors detected; first six are used");
  }

  // Re-attach role/offset settings by ROM address after every bus refresh.
  applyIndoorStoredConfigurationToDetectedSensors();
}

void readAssignedTemperatures() {
  resetPublishedTemperatures();

  for (uint8_t i = 0; i < INDOOR_SENSOR_COUNT; i++) {
    if (!tempSensorPresent[i]) {
      indoorSensorRawTempC[i] = SENSOR_DISCONNECTED;
      continue;
    }

    const float rawTemperature = sensors.getTempC(tempSensorAddresses[i]);
    if (rawTemperature == DEVICE_DISCONNECTED_C || isnan(rawTemperature)) {
      indoorSensorRawTempC[i] = SENSOR_DISCONNECTED;
      continue;
    }

    indoorSensorRawTempC[i] = rawTemperature;
    const float corrected = rawTemperature + tempSensorOffset[i];

    switch (tempSensorRole[i]) {
      case 1:
        SuctionTempC = corrected;
        SuctionTemp = convertToFahrenheit(corrected);
        break;
      case 2:
        dischargeTempC = corrected;
        dischargeTemp = convertToFahrenheit(corrected);
        break;
      case 3:
        SupplyTempC = corrected;
        SupplyTemp = convertToFahrenheit(corrected);
        break;
      case 4:
        ReturnTempC = corrected;
        ReturnTemp = convertToFahrenheit(corrected);
        break;
      case 5:
        OilTempC = corrected;
        OilTemp = convertToFahrenheit(corrected);
        break;
      case 6:
        OtherTempC = corrected;
        OtherTemp = convertToFahrenheit(corrected);
        break;
      default:
        break; // 0 = physical sensor present but unassigned
    }
  }
}

void startTemperatureConversion() {
  refreshTemperatureSensorList();
  sensors.requestTemperatures();
  temperature_conversion_started_ms = millis();
  temperature_conversion_pending = true;
  pmillis = temperature_conversion_started_ms;
}

void serviceTemperatureSensors() {
  if (temperature_conversion_pending) {
    if (millis() - temperature_conversion_started_ms >= TEMP_CONVERSION_TIME_MS) {
      readAssignedTemperatures();
      temperature_conversion_pending = false;
    }
    return;
  }

  if (millis() - pmillis >= SENSOR_READ_INTERVAL) {
    startTemperatureConversion();
  }
}

void setup() {
  esp_task_wdt_init(10, true);
  Serial.begin(115200);

  relayInit();

  sensors.begin();
  sensors.setWaitForConversion(false);

  Serial.println("\nBooting AM6 Indoor MASTER...");
  Serial.print("Device Name: ");
  Serial.println(devicename);
  Serial.print("[PINS] DS18B20=");
  Serial.print(DS18B20_PIN);
  Serial.print(" | FAN LOW/MED/HIGH=");
  Serial.print(RELAY_LOW_PIN);
  Serial.print("/");
  Serial.print(RELAY_MEDIUM_PIN);
  Serial.print("/");
  Serial.print(RELAY_HIGH_PIN);
  Serial.print(" | HEAT=");
  Serial.println(RELAY_HEAT_PIN);

  DEVICE_INIT();
  refreshTemperatureSensorList();
  rs485Init();

  macaddress = WiFi.macAddress();

  if (loadCredentials(ssid, password)) {
    Serial.println("Loaded Wi-Fi credentials from memory");
    connectToWiFi(ssid, password);
  }
  else {
    Serial.println("No Wi-Fi credentials found; starting AP mode");
    startAccessPoint();
  }

  server_setup();

  // Configure/establish MQTT immediately when Wi-Fi is already available.
  if (WiFi.status() == WL_CONNECTED) {
    reconnect();
  }

  resetPublishedTemperatures();
  startTemperatureConversion();
  wait_time = millis();
}

void loop() {
  // Actively recover the router/STA link without tearing down the local AM6 AP.
  serviceWiFiRecovery();

  // Keep network status synchronized for app diagnostics.
  is_wifi_connected = (WiFi.status() == WL_CONNECTED);

  // MQTT RX is serviced FIRST so app power/config commands are not delayed by
  // a failed Display HTTP connection attempt.
  if (WiFi.status() == WL_CONNECTED && client.connected()) {
    client.loop();
  }

  // Keep Outdoor RS485, sensors and Display link responsive.
  rs485Loop();
  // If an Outdoor sensor-selection packet arrived before the ROM snapshot,
  // replay it as soon as RS485 supplies that snapshot.
  servicePendingOutdoorSensorConfig();
  serviceTemperatureSensors();
  displayLinkService();

  // Service MQTT again after local hardware/network work.
  if (WiFi.status() == WL_CONNECTED && client.connected()) {
    client.loop();

    if (mqtt_publish_requested) {
      publishAllMqttState();
      mqtt_publish_requested = false;
    }
  }

  if (millis() - wait_time >= MQTT_INTERVAL) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!client.connected()) reconnect();
      if (client.connected()) {
        publishAllMqttState();
      }
    }

    message_received = false;
    message_received_config = false;
    wait_time = millis();
  }
}
