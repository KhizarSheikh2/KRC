#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#include "DisplayState.h"
#include "DisplayLink.h"
#include "AM4_UI.h"

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("AM4 Display ESP32 starting...");

  initDisplayLink();
  initAM4UI();
}

void loop() {
  serviceDisplayLink();
  updateAM4UI();
  delay(1);
}
