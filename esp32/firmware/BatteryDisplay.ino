#include <TM1637Display.h>

// Initialize the display
TM1637Display display(6, 7);

void batteryDisplaySetup() {
  display.setBrightness(0x0f);
  display.clear();
}

void batteryDisplayLoop() {
    float battery = getBatteryPercentage();
    int value = (int)battery;
    display.showNumberDec(value, false, 4, 0);
}