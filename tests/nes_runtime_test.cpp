#include "../firmware/flappy/nes_runtime.h"

#include <algorithm>
#include <assert.h>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

using flappy::NesRuntime;

// This fixture contains only the small test program assembled below.
static std::vector<uint8_t> syntheticNrom() {
  std::vector<uint8_t> rom(16 + 32768 + 8192, 0);
  rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1a;
  rom[4] = 2; rom[5] = 1;
  std::vector<uint8_t> program = {0x78, 0xd8, 0xa2, 0xff, 0x9a, 0xe8, 0x8e, 0x00, 0x20,
                                 0x8e, 0x01, 0x20, 0x2c, 0x02, 0x20, 0x10, 0xfb};
  const auto store = [&program](uint8_t value, uint16_t address) {
    program.insert(program.end(), {0xa9, value, 0x8d, uint8_t(address), uint8_t(address >> 8)});
  };
  store(0x3f, 0x2006); store(0x00, 0x2006);
  store(0x21, 0x2007); store(0x15, 0x2007);
  store(0x00, 0x2005); store(0x00, 0x2005);
  store(0x0a, 0x2001); store(0x80, 0x2000);
  store(0x01, 0x4015); store(0xbf, 0x4000);
  store(0x40, 0x4002); store(0x08, 0x4003);
  const uint16_t loop = uint16_t(0x8000 + program.size());
  program.insert(program.end(), {0x4c, uint8_t(loop), uint8_t(loop >> 8)});
  std::copy(program.begin(), program.end(), rom.begin() + 16);
  const uint8_t nmi[] = {0xe6, 0x00, 0xa9, 0x01, 0x8d, 0x16, 0x40, 0xa9, 0x00,
                         0x8d, 0x16, 0x40, 0xad, 0x16, 0x40, 0x29, 0x01, 0x85, 0x01, 0x40};
  std::copy(nmi, nmi + sizeof(nmi), rom.begin() + 16 + 0x100);
  const size_t vectors = 16 + 32768 - 6;
  rom[vectors] = 0; rom[vectors + 1] = 0x81;
  rom[vectors + 2] = 0; rom[vectors + 3] = 0x80;
  rom[vectors + 4] = 0; rom[vectors + 5] = 0x81;
  for (unsigned row = 0; row < 8; ++row) rom[16 + 32768 + row] = 0xff;
  return rom;
}

struct Output {
  unsigned rectangles = 0;
  unsigned audioFrames = 0;
  size_t audioSamples = 0;
  int audioPeak = 0;
  uint64_t videoSum = 0;
  bool correctRed = false;
  unsigned expectedSamples = 400;
};

static void video(void* context, int x1, int y1, int x2, int y2, const uint16_t* pixels) {
  auto& output = *static_cast<Output*>(context);
  assert(x1 == 0 && x2 == 255);
  assert(y1 == 0 || y1 == 120);
  assert(y2 == y1 + 119);
  assert(y1 == int(output.rectangles % 2) * 120);
  for (int i = 0; i < 256 * 120; ++i) {
    output.videoSum += pixels[i];
    output.correctRed |= pixels[i] == 0xe00b;
  }
  ++output.rectangles;
}

static void audio(void* context, const int16_t* samples, size_t count, uint32_t sampleRate) {
  auto& output = *static_cast<Output*>(context);
  assert(sampleRate == 24000);
  assert(count == output.expectedSamples);
  for (size_t i = 0; i < count; ++i) output.audioPeak = std::max(output.audioPeak, std::abs(int(samples[i])));
  ++output.audioFrames;
  output.audioSamples += count;
}

static void realCpuVideoAudioAndInput() {
  auto rom = syntheticNrom();
  NesRuntime runtime;
  Output output;
  runtime.setCallbacks(video, audio, &output);
  assert(runtime.load(rom.data(), rom.size()));
  assert(runtime.ready());
  assert(runtime.frameRate() == 60);
  assert(runtime.core()->nes_rom.prg_rom == rom.data() + 16);
  assert(NesRuntime::coreBytes() < 80000);
  for (int frame = 0; frame < 12; ++frame) assert(runtime.step(NesRuntime::A));
  assert(runtime.frames() == 12);
  assert(output.rectangles == 24);
  assert(output.videoSum > 0);
  assert(output.correctRed);
  assert(output.audioFrames >= 11);
  assert(output.audioSamples >= 11 * 400);
  assert(output.audioPeak > 100);
  assert(runtime.core()->nes_cpu.cpu_ram[0] >= 8);
  assert(runtime.core()->nes_cpu.cpu_ram[1] == 1);
  assert(runtime.step(0));
  assert(runtime.core()->nes_cpu.cpu_ram[1] == 0);
  runtime.unload();
  assert(!runtime.ready());
  assert(!runtime.step(0));
}

static void rejectsUnsupportedAndTruncatedImages() {
  auto rom = syntheticNrom();
  NesRuntime runtime;
  assert(!runtime.load(nullptr, 0));
  assert(!runtime.load(rom.data(), 15));
  assert(!runtime.load(rom.data(), rom.size() - 1));
  rom[0] = 0;
  assert(!runtime.load(rom.data(), rom.size()));
  rom[0] = 'N';
  rom[6] = 0x20;
  assert(!runtime.load(rom.data(), rom.size()));
  rom[6] = 0x04;
  assert(!runtime.load(rom.data(), rom.size()));
  rom[6] = 0;
  rom[7] = 0x08; rom[8] = 0x10;
  assert(!runtime.load(rom.data(), rom.size()));
  rom[8] = 0; rom[9] = 0x0f;
  assert(!runtime.load(rom.data(), rom.size()));
  rom[9] = 0; rom[12] = 3;
  assert(!runtime.load(rom.data(), rom.size()));
  assert(runtime.error()[0] != 0);
  rom[12] = 1;
  Output output;
  output.expectedSamples = 480;
  runtime.setCallbacks(video, audio, &output);
  assert(runtime.load(rom.data(), rom.size()));
  assert(runtime.error()[0] == 0);
  assert(runtime.frameRate() == 50);
  for (int frame = 0; frame < 5; ++frame) assert(runtime.step(0));
  assert(output.rectangles == 10);
  assert(output.audioSamples >= 4 * 480);
}

static void mapper37BankSwitching() {
  std::vector<uint8_t> rom(16 + 262144 + 262144, 0);
  rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1a;
  rom[4] = 16; rom[5] = 32; rom[6] = 0x50; rom[7] = 0x28; rom[12] = 1;
  for (unsigned bank = 0; bank < 32; ++bank) {
    auto start = rom.begin() + 16 + bank * 8192;
    std::fill(start, start + 8192, uint8_t(bank));
    const uint8_t program[] = {0x78, 0xa9, 0x80, 0x8d, 0x01, 0xa0, 0xa9, 0x04, 0x8d, 0x00, 0x60,
                               0xad, 0x00, 0xb0, 0x85, 0x02, 0x4c, 0x10, 0x80};
    std::copy(program, program + sizeof(program), start);
    start[8192 - 4] = 0; start[8192 - 3] = 0x80;
  }
  NesRuntime runtime;
  assert(runtime.load(rom.data(), rom.size()));
  assert(runtime.frameRate() == 50);
  assert(runtime.step(0));
  assert(runtime.core()->nes_cpu.cpu_ram[2] == 17);
  const auto* base = rom.data() + 16;
  assert(runtime.core()->nes_cpu.prg_banks[0] == base + 16 * 8192);
  assert(runtime.core()->nes_cpu.prg_banks[3] == base + 31 * 8192);
  assert(runtime.core()->nes_ppu.pattern_table[0] == base + 262144 + 128 * 1024);
}

int main() {
  realCpuVideoAudioAndInput();
  rejectsUnsupportedAndTruncatedImages();
  mapper37BankSwitching();
  std::cout << "NES CPU, PPU, APU, input, validation and mapper checks passed. Core bytes: "
            << NesRuntime::coreBytes() << "\n";
}
