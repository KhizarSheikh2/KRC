#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

#include <ESP32PWM.h>
#include <ESP32Servo.h>

Servo myservo;

void Int_Servo() {
  ESP32PWM::allocateTimer(0);
  myservo.setPeriodHertz(50);  // Standard RC-servo frequency
  myservo.attach(ServoPin);
}

void MoveServo(int16_t degrees, int16_t steps, int16_t moveDelayMs) {
  degrees = constrain(degrees, 0, 180);
  if (steps <= 0) steps = 1;
  if (moveDelayMs < 0) moveDelayMs = 0;

  // Nothing to do if we are already at the requested logical position.
  if (last_pos_servo == degrees) return;

  // The IR receiver and ESP32Servo can compete for timing/RMT resources on
  // some ESP32 library combinations. Pause IR only when it has actually
  // been started; startup servo movement happens before IR is enabled.
  const bool restartIrAfterMove = irReceiverStarted;
  if (restartIrAfterMove) {
    IrReceiver.stop();
  }

  if (!myservo.attached()) {
    myservo.setPeriodHertz(50);
    myservo.attach(ServoPin);
  }

  const int16_t startPosition = constrain(last_pos_servo, 0, 180);

  if (startPosition < degrees) {
    for (int16_t pos = startPosition; pos < degrees; pos += steps) {
      myservo.write(pos);
      last_pos_servo = pos;
      delay(moveDelayMs);
    }
  } else {
    for (int16_t pos = startPosition; pos > degrees; pos -= steps) {
      myservo.write(pos);
      last_pos_servo = pos;
      delay(moveDelayMs);
    }
  }

  // Always land exactly on the target even when the step size does not divide
  // the movement range evenly.
  myservo.write(degrees);
  last_pos_servo = degrees;
  delay(moveDelayMs);

  myservo.detach();

  if (restartIrAfterMove) {
    IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);
  }
}

#endif
