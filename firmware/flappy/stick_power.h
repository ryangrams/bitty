#pragma once
#include <Wire.h>

// Enable the StickC LCD rails before the display controller is identified.
// Rail values follow m5stack/M5StickC AXP192.cpp.
inline bool prepareStickDisplayPower() {
  TwoWire powerBus(1);
  if (!powerBus.begin(21, 22, 100000)) return false;
  auto readRegister = [&](uint8_t reg) -> int {
    powerBus.beginTransmission(0x34);
    powerBus.write(reg);
    if (powerBus.endTransmission(false) != 0) return -1;
    if (powerBus.requestFrom(uint8_t(0x34), uint8_t(1)) != 1) return -1;
    return powerBus.read();
  };
  auto writeRegister = [&](uint8_t reg, uint8_t value) {
    powerBus.beginTransmission(0x34);
    powerBus.write(reg);
    powerBus.write(value);
    return powerBus.endTransmission() == 0;
  };
  bool enabled = false;
  if (readRegister(0x03) == 0x03) {
    const int rails = readRegister(0x12);
    if (rails >= 0) enabled = writeRegister(0x28, 0xcc) && writeRegister(0x12, uint8_t(rails | 0x0c));
  }
  powerBus.end();
  if (enabled) delay(20);
  return enabled;
}
