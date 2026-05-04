#include <INA219.h>

INA219 ina1(0x41);
bool ina219_enabled = false;

float voltageToPercent(float v) {
  if (v >= 12.6) return 100;
  if (v <= 9.6) return 0;
  return (v - 9.6) * 100.0 / (12.6 - 9.6);
}

float getBatteryPercentage() {
  if (!ina219_enabled) return 0.0f;
  float busVoltage = ina1.getBusVoltage_V();
  return voltageToPercent(busVoltage);
}

bool batterySetup() {
  if (!i2cDevicePresent(0x41)) {
    Serial.println("[ERROR] INA219 not detected at 0x41");
    ina219_enabled = false;
    return false;
  }

  ina1.begin();
  Serial.println("[OK] INA219 initialized");
  ina219_enabled = true;
  return true;
}

bool i2cDevicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return (Wire.endTransmission() == 0);
}