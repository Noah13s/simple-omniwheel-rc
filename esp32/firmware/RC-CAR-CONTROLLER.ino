#include <Wire.h>

#define I2C_SDA 3
#define I2C_SCL 14

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  cameraSetup();
  panTiltSetup();
  batterySetup();
  batteryDisplaySetup();
  motorsSetup();
  wifiSetup();
}

void loop() {
  wifiLoop();
  batteryDisplayLoop();
}