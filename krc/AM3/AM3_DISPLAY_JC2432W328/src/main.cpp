#include <Arduino.h>
#include "AM3_UI.h"
#include "DisplayLink.h"

void setup() {
  Serial.begin(115200);
  delay(120);
  Serial.println();
  Serial.println("========================================");
  Serial.println("AM3 JC2432W328 Display starting");
  Serial.println("TFT_eSPI + LVGL 8 + CST820");
  Serial.println("========================================");

  initDisplayLink();
  initAM3UI();
}

void loop() {
  serviceDisplayLink();
  updateAM3UI();
  delay(2);
}
