#pragma once
#include <stdint.h>
#include <stddef.h>

namespace flappy {
// M5PM1 manual v1.9, BTN_CFG/BTN_CFG_2 and BTN_Status. Keep download mode intact.
class StickPowerButton {
 public:
  template <typename Bus> bool begin(Bus& bus) {
    ready_ = false;
    if (!bus.read(0,identity_,4) || identity_[0] != 0x50 || identity_[1] != 0x20 ||
        identity_[2] != 5 || (identity_[3] != 6 && identity_[3] != 0x53)) return false;
    uint8_t before[2], verify[2];
    if (!bus.read(0x49,before,2)) return false;
    const uint8_t first = before[0] | 1u, second = before[1] | 1u;
    if (!bus.write(0x49,first) || !bus.write(0x4a,second) || !bus.read(0x49,verify,2) ||
        verify[0] != first || verify[1] != second) {
      bus.write(0x49,before[0]); bus.write(0x4a,before[1]);
      return false;
    }
    uint8_t initial = 1;
    bus.read(0x48,&initial,1);
    pressed_ = (initial & 1u) != 0;
    polled_ = false;
    return ready_ = true;
  }
  template <typename Bus> bool poll(Bus& bus, uint32_t nowMs) {
    if (!ready_ || (polled_ && uint32_t(nowMs - lastPoll_) < 8)) return false;
    polled_ = true; lastPoll_ = nowMs;
    uint8_t state = 0;
    if (!bus.read(0x48,&state,1)) return false;
    const bool down = (state & 1u) != 0;
    const bool clicked = !pressed_ && ((state & 0x80u) || down);
    pressed_ = down;
    return clicked;
  }
  bool ready() const { return ready_; }
  bool pressed() const { return pressed_; }
  uint8_t revision(unsigned i) const { return i < 4 ? identity_[i] : 0; }
 private:
  uint32_t lastPoll_ = 0;
  uint8_t identity_[4] = {};
  bool ready_ = false, pressed_ = false, polled_ = false;
};
} // namespace flappy
