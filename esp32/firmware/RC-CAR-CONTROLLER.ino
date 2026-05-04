#include <Wire.h>

#define I2C_SDA 5
#define I2C_SCL 4

void setup() {
  Serial.begin(115200);
  // cameraSetup();
  Wire.begin(I2C_SDA, I2C_SCL);
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