#include "../firmware/flappy/speaker_audio.h"

#include <assert.h>
#include <deque>
#include <iostream>
#include <vector>

struct Speaker {
  struct Block { const int16_t* data; std::vector<int16_t> copy; };
  std::deque<Block> borrowed;
  bool reject = false;
  unsigned calls = 0;

  size_t isPlaying(unsigned channel) const {
    assert(channel == 1);
    return borrowed.size();
  }
  bool playRaw(const int16_t* data, size_t count, uint32_t rate, bool stereo,
               unsigned repeat, unsigned channel, bool interrupt) {
    assert(rate == 24000 && !stereo && repeat == 1 && channel == 1 && !interrupt);
    ++calls;
    if (reject) return false;
    for (const auto& block : borrowed) assert(block.data != data);
    borrowed.push_back({data, std::vector<int16_t>(data, data + count)});
    return true;
  }
  void verify() const {
    for (const auto& block : borrowed)
      for (size_t i = 0; i < block.copy.size(); ++i) assert(block.data[i] == block.copy[i]);
  }
};

int main() {
  flappy::SpeakerPcmQueue queue;
  Speaker speaker;
  int16_t samples[512];
  // Exercise changing DMA consumption and producer backpressure. Every sample
  // still borrowed by the speaker must survive later submissions unchanged.
  for (unsigned frame = 0; frame < 1000; ++frame) {
    const unsigned count = frame % 2 ? 400 : 480;
    for (unsigned i = 0; i < 512; ++i) samples[i] = int16_t(frame * 13 + i);
    speaker.verify();
    if (frame % 3 && !speaker.borrowed.empty()) speaker.borrowed.pop_front();
    const size_t occupied = speaker.borrowed.size();
    assert(queue.submit(speaker, samples, count, 24000) == (occupied < 2));
    speaker.verify();
  }
  assert(queue.submitted() > 600 && queue.dropped() > 300);
  speaker.borrowed.clear();
  speaker.reject = true;
  assert(!queue.submit(speaker, samples, 400, 24000));
  speaker.reject = false;
  assert(queue.submit(speaker, samples, 400, 24000));
  speaker.verify();
  const unsigned calls = speaker.calls;
  assert(!queue.submit(speaker, nullptr, 400, 24000));
  assert(!queue.submit(speaker, samples, 0, 24000));
  assert(!queue.submit(speaker, samples, 513, 24000));
  assert(!queue.submit(speaker, samples, 400, 0));
  assert(speaker.calls == calls);
  for (unsigned level = 0; level <= 255; ++level) {
    assert(flappy::speakerVolume(level, false, false) == level);
    assert(flappy::speakerVolume(level, true, true) == level);
    assert(flappy::speakerVolume(level, true, false) == (level > 184 ? 184 : level));
  }
  std::cout << "Speaker PCM ownership, backpressure and volume checks passed\n";
}
