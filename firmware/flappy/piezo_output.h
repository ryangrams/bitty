#pragma once

#include <stdint.h>

namespace flappy {

// LEDC keeps the last pitch and duty active until they change. On ESP32, two
// consecutive duty writes can wait for a full PWM period in the driver.
class PiezoOutput {
 public:
  void reset(uint16_t configuredFrequency, uint16_t duty) {
    configuredFrequency_ = configuredFrequency;
    appliedDuty_ = duty <= 511 ? duty : 511;
    requestedFrequency_ = configuredFrequency;
    requestedDuty_ = appliedDuty_;
  }

  void request(uint16_t frequency, uint16_t duty) {
    requestedFrequency_ = frequency;
    requestedDuty_ = frequency ? (duty <= 511 ? duty : 511) : 0;
  }

  // Driver::setFrequency returns the applied frequency, or zero on failure.
  // Driver::writeDuty returns true on success. Failed changes remain pending.
  template <typename Driver>
  bool service(Driver& driver) {
    if (requestedDuty_ && requestedFrequency_ != configuredFrequency_) {
      if (!driver.setFrequency(requestedFrequency_)) return false;
      configuredFrequency_ = requestedFrequency_;
    }
    if (requestedDuty_ != appliedDuty_) {
      if (!driver.writeDuty(requestedDuty_)) return false;
      appliedDuty_ = requestedDuty_;
    }
    return true;
  }

  uint16_t frequency() const {
    return appliedDuty_ && appliedDuty_ != UINT16_MAX ? configuredFrequency_ : 0;
  }
  uint16_t duty() const { return appliedDuty_ == UINT16_MAX ? 0 : appliedDuty_; }
  uint16_t configuredFrequency() const { return configuredFrequency_; }

 private:
  uint16_t configuredFrequency_ = 0;
  uint16_t appliedDuty_ = UINT16_MAX;
  uint16_t requestedFrequency_ = 0;
  uint16_t requestedDuty_ = 0;
};

}  // namespace flappy
