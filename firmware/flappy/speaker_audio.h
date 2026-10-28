#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace flappy {

// StickS3 guidance recommends less than 75% speaker volume on battery.
// Unknown power uses the same cap until an external supply is confirmed.
inline uint8_t speakerVolume(uint8_t requested, bool stickS3, bool externalPower) {
  return stickS3 && !externalPower && requested > 184 ? 184 : requested;
}

// M5Speaker borrows PCM storage and queues two blocks per virtual channel.
// Three rotating buffers keep the producer separate from both borrowed blocks.
// Full output drops a block without blocking the simulation or LCD task.
class SpeakerPcmQueue {
 public:
  enum { Samples = 512, Channel = 1 };

  template <class Speaker>
  bool submit(Speaker& speaker, const int16_t* source, size_t count, uint32_t rate) {
    if (!source || !count || count > Samples || rate < 8000 || rate > 48000) return false;
    if (speaker.isPlaying(Channel) >= 2) { ++dropped_; return false; }
    int16_t* target = buffers_[next_];
    memcpy(target, source, count * sizeof(int16_t));
    if (!speaker.playRaw(target, count, rate, false, 1, Channel, false)) { ++dropped_; return false; }
    next_ = uint8_t((next_ + 1) % 3);
    ++submitted_;
    return true;
  }

  uint32_t submitted() const { return submitted_; }
  uint32_t dropped() const { return dropped_; }

 private:
  int16_t buffers_[3][Samples] = {};
  uint8_t next_ = 0;
  uint32_t submitted_ = 0, dropped_ = 0;
};

}  // namespace flappy
