#include <Adafruit_PWMServoDriver.h>

// Use the constructor that accepts the address AND the Wire object
// 0x40 is the default address for the PCA9685
Adafruit_PWMServoDriver panTiltPwm = Adafruit_PWMServoDriver(0x40);

#define SERVO_MIN 150
#define SERVO_MAX 600

int mapServo(int v) {
  return map(v, 0, 180, SERVO_MIN, SERVO_MAX);
}

bool panTiltSetup() {
  if (!panTiltPwm.begin()) {
    Serial.println("[ERROR] PCA9685 init failed at address 0x40");
    return false;
  }

  panTiltPwm.setPWMFreq(50);
  Serial.println("[OK] PCA9685 initialized");
  return true;
}