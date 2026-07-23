#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#include "DisplayState.h"
#include "DisplayLink.h"
#include "AM3_UI.h"

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("AM3 Display ESP32 starting...");

  initDisplayLink();
  initAM3UI();
}

void loop() {
  serviceDisplayLink();
  updateAM3UI();
  delay(1);
}
