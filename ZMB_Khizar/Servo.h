#include <ESP32PWM.h>
#include <ESP32Servo.h>

const int ServoPin = 2;
Servo myservo;
// int pos = 0;


void Int_Servo() {
  ESP32PWM::allocateTimer(0);  // Allocate a timer for the PWM
  myservo.setPeriodHertz(50);  // Standard 50hz servo
  myservo.attach(ServoPin);
}

void MoveServo(uint8_t degrees, int steps, int del) {
  // Serial.println("Move Servo");

  int8_t position = last_pos_servo;
  if (degrees < 0 || degrees > 180) {
    degrees = 0;
  }  //
  else {
    if (last_pos_servo > degrees) {
      Serial.println("if");

      // Before using servo
      // IrReceiver.stop();  // Stop IR receiver to free RMT
      // myservo.attach(ServoPin);  // Now servo can use RMT
      Int_Servo();

      for (uint8_t pos = position; pos >= degrees; pos -= steps) {
        myservo.write(pos);
        delay(del);
        last_pos_servo = pos;
      }
      if (last_pos_servo > degrees) {
        myservo.write(degrees);
      }
      // After moving servo
      myservo.detach();
      IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);  // Start receiver

    } else if (last_pos_servo < degrees){
      Serial.println("else");

      // Before using servo
      IrReceiver.stop();  // Stop IR receiver to free RMT
      // myservo.attach(ServoPin);  // Now servo can use RMT
      Int_Servo();
      for (uint8_t pos = position; pos <= degrees; pos += steps) {
        myservo.write(pos);
        delay(del);
        last_pos_servo = pos;
      }
      // if (last_pos_servo < degrees) {
      //   myservo.write(degrees);
      // }
      // After moving servo
      myservo.detach();
      IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);  // Start receiver
    }
  }
}
