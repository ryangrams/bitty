#include "../firmware/flappy/nes_runtime.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using flappy::NesRuntime;

// This program counts native NMI updates and CPU work between updates.
static std::vector<uint8_t> cartridge(bool nes2, bool pal, unsigned mapper) {
  const unsigned prgBanks = mapper == 37 ? 16 : 2;
  const unsigned chrBanks = mapper == 37 ? 32 : 1;
  std::vector<uint8_t> rom(16 + prgBanks * 16384 + chrBanks * 8192, 0);
  rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1a;
  rom[4] = uint8_t(prgBanks); rom[5] = uint8_t(chrBanks);
  rom[6] = uint8_t((mapper & 15u) << 4);
  rom[7] = uint8_t((mapper & 0xf0u) | (nes2 ? 8u : 0u));
  rom[nes2 ? 12 : 9] = pal ? 1 : 0;

  std::vector<uint8_t> program = {0x78, 0xd8, 0xa2, 0xff, 0x9a, 0xa9, 0x00};
  for (uint8_t address = 0; address < 5; ++address)
    program.insert(program.end(), {0x85, address});
  program.insert(program.end(), {0x8d, 0x00, 0x20, 0x8d, 0x01, 0x20,
                                 0xa9, 0x80, 0x8d, 0x00, 0x20});
  const uint16_t loop = uint16_t(0x8000 + program.size());
  program.insert(program.end(), {0xe6, 0x02, 0xd0, 0xfc, 0xe6, 0x03, 0xd0, 0xf8,
                                 0xe6, 0x04, 0x4c, uint8_t(loop), uint8_t(loop >> 8)});
  const uint8_t nmi[] = {0xe6, 0x00, 0xd0, 0x02, 0xe6, 0x01, 0x40};
  // Identical banks keep reset and interrupt vectors valid on mapper 37.
  for (unsigned bank = 0; bank < prgBanks * 2; ++bank) {
    const size_t base = 16 + bank * 8192;
    std::copy(program.begin(), program.end(), rom.begin() + base);
    std::copy(nmi, nmi + sizeof(nmi), rom.begin() + base + 0x100);
    rom[base + 0x1ffa] = 0; rom[base + 0x1ffb] = 0x81;
    rom[base + 0x1ffc] = 0; rom[base + 0x1ffd] = 0x80;
    rom[base + 0x1ffe] = 0; rom[base + 0x1fff] = 0x81;
  }
  return rom;
}

struct Output {
  size_t samples = 0;
  unsigned audioFrames = 0, videoFrames = 0;
  size_t expectedSamples = 0;
};

static void audio(void* context, const int16_t* samples, size_t count, uint32_t rate) {
  auto& output = *static_cast<Output*>(context);
  assert(samples && rate == 24000 && count == output.expectedSamples);
  output.samples += count;
  ++output.audioFrames;
}

static void video(void* context, int x1, int y1, int x2, int y2, const uint16_t* pixels) {
  assert(pixels && x1 == 0 && x2 == 255 && y1 >= 0 && y2 < 240);
  if (y2 == 239) ++static_cast<Output*>(context)->videoFrames;
}

static uint32_t counter(const NesRuntime& runtime, uint16_t address, unsigned bytes) {
  uint32_t value = 0;
  for (unsigned i = 0; i < bytes; ++i) value |= uint32_t(runtime.readRam(address + i)) << (i * 8);
  return value;
}

struct Work {
  uint32_t loops = 0;
  unsigned frames = 0;
};

static Work oneSecond(NesRuntime& runtime, Output& output, bool pal) {
  const unsigned rate = pal ? 50 : 60;
  assert(runtime.frameRate() == rate);
  assert(runtime.core()->timing.vblank_lines == (pal ? 70 : 20));
  assert(runtime.core()->timing.samples_per_frame == 24000 / rate);
  output.expectedSamples = 24000 / rate;
  for (unsigned frame = 0; frame < 12; ++frame) assert(runtime.step(0));
  const uint32_t updates = counter(runtime, 0, 2);
  const uint32_t loops = counter(runtime, 2, 3);
  const uint32_t frames = runtime.frames();
  assert(updates >= 10);
  output.samples = output.audioFrames = output.videoFrames = 0;
  for (unsigned frame = 0; frame < runtime.frameRate(); ++frame) assert(runtime.step(0));
  assert(counter(runtime, 0, 2) - updates == rate);
  assert(runtime.frames() - frames == rate);
  assert(output.audioFrames == rate && output.samples == 24000);
  assert(output.videoFrames == (rate + NES_VIDEO_DIVISOR - 1) / NES_VIDEO_DIVISOR);
  Work work;
  work.loops = counter(runtime, 2, 3) - loops;
  work.frames = rate;
  assert(work.loops > 100000);
  return work;
}

static Work checkCartridge(bool nes2, bool pal, unsigned mapper) {
  auto rom = cartridge(nes2, pal, mapper);
  NesRuntime runtime;
  Output output;
  runtime.setCallbacks(video, audio, &output);
  assert(runtime.load(rom.data(), rom.size()));
  assert(runtime.core()->nes_rom.mapper_number == mapper);
  const Work work = oneSecond(runtime, output, pal);
  assert(runtime.softReset());
  assert(runtime.frames() == 0 && runtime.frameRate() == (pal ? 50 : 60));
  oneSecond(runtime, output, pal);
  assert(runtime.load(rom.data(), rom.size()));
  assert(runtime.frames() == 0);
  oneSecond(runtime, output, pal);
  return work;
}

int main() {
  Work ntsc, pal;
  for (bool nes2 : {false, true}) {
    ntsc = checkCartridge(nes2, false, 0);
    pal = checkCartridge(nes2, true, 0);
    // PAL's longer blanking interval gives this loop more CPU work per frame.
    const uint64_t palPerFrame = uint64_t(pal.loops) * ntsc.frames;
    const uint64_t ntscPerFrame = uint64_t(ntsc.loops) * pal.frames;
    assert(palPerFrame * 100 > ntscPerFrame * 108);
    assert(palPerFrame * 100 < ntscPerFrame * 116);
  }
  checkCartridge(true, true, 37);
  std::cout << "NES iNES/NES2 timing, NMI updates, audio totals and reset checks passed"
            << " (video divisor " << NES_VIDEO_DIVISOR << ")\n";
}
