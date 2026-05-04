struct MotorConfig {
  int a1;
  int a2;
  bool isInverted;
};

// Motor Layout: 0 & 2 = LEFT | 1 & 3 = RIGHT
const MotorConfig motors[4] = {
  { 1, 2, true },     // Motor 0 (Left Front)
  { 41, 40, true },   // Motor 1 (Right Front)
  { 38, 37, false },  // Motor 2 (Left Rear)
  { 35, 0, false }    // Motor 3 (Right Rear)
};

#define PWM_FREQ 1000
#define PWM_RES 8

int motorSpeeds[4] = { 0, 0, 0, 0 };
int motorDirs[4] = { 0, 0, 0, 0 };

void applyMotor(int id) {
  int p1 = motors[id].a1;
  int p2 = motors[id].a2;
  int effectiveDir = motors[id].isInverted ? (motorDirs[id] * -1) : motorDirs[id];

  if (effectiveDir == 1) {
    ledcWrite(p1, motorSpeeds[id]);
    ledcWrite(p2, 0);
  } else if (effectiveDir == -1) {
    ledcWrite(p1, 0);
    ledcWrite(p2, motorSpeeds[id]);
  } else {
    ledcWrite(p1, 0);
    ledcWrite(p2, 0);
  }
}

void motorsSetup() {
  for (int i = 0; i < 4; i++) {
    pinMode(motors[i].a1, OUTPUT);
    pinMode(motors[i].a2, OUTPUT);
    ledcAttach(motors[i].a1, PWM_FREQ, PWM_RES);
    ledcAttach(motors[i].a2, PWM_FREQ, PWM_RES);
    applyMotor(i);
  }
}
