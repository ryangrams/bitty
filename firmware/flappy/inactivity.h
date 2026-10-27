#pragma once

#include <stdint.h>

namespace flappy {

enum class PowerSource : uint8_t { Unknown, Battery, External };

inline PowerSource axp192PowerSource(bool readOk, uint8_t inputStatus, uint8_t batteryStatus) {
  if (!readOk) return PowerSource::Unknown;
  if (inputStatus & 0xa0u) return PowerSource::External;
  return batteryStatus & 0x20u ? PowerSource::Battery : PowerSource::Unknown;
}

class InactivityPolicy {
 public:
  void activity(uint32_t nowMs) { lastActivity_ = nowMs; initialized_ = true; }

  void resumed(uint32_t nowMs) {
    activity(nowMs);
    wakeInputBlocked_ = true;
  }

  // Suppress the wake press and the first released sample before rearming input.
  bool consumeWakeInput(bool anyInputHeld, uint32_t nowMs) {
    if (!wakeInputBlocked_) return false;
    activity(nowMs);
    if (!anyInputHeld) wakeInputBlocked_ = false;
    return true;
  }

  bool shouldSleep(uint32_t nowMs, uint32_t timeoutMs, PowerSource source,
                   bool inputActive = false, bool inhibited = false) {
    if (!initialized_ || source != source_ || timeoutMs != timeout_ ||
        source != PowerSource::Battery || !timeoutMs || inputActive || inhibited || wakeInputBlocked_) {
      activity(nowMs);
    }
    source_ = source;
    timeout_ = timeoutMs;
    return source == PowerSource::Battery && timeoutMs && !inputActive &&
           !inhibited && !wakeInputBlocked_ && uint32_t(nowMs - lastActivity_) >= timeoutMs;
  }

  uint32_t idleMs(uint32_t nowMs) const { return initialized_ ? uint32_t(nowMs - lastActivity_) : 0; }
  bool wakeInputBlocked() const { return wakeInputBlocked_; }

 private:
  uint32_t lastActivity_ = 0, timeout_ = 0;
  PowerSource source_ = PowerSource::Unknown;
  bool initialized_ = false, wakeInputBlocked_ = false;
};

}  // namespace flappy
