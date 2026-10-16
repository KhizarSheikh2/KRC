#include <Arduino.h>

#include "DisplayLink.h"
#include "AM4_UI.h"

void setup() {
  Serial.begin(115200);
  delay(80);
  Serial.println();
  Serial.println("AM4 JC2432W328 Display starting...");

  // Display is brought up first so the UI is visible immediately.
  // The hardware host firmware/protocol is unchanged.
  initAM4UI();
  initDisplayLink();
}

void loop() {
  serviceDisplayLink();
  updateAM4UI();
  delay(1);
}
