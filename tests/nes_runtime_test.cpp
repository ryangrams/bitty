#include "../firmware/flappy/nes_runtime.h"
#include "../firmware/flappy/tetris_gym.h"
#include "../firmware/flappy/tetris_zero.h"
#include "../firmware/flappy/tetris_viewport.h"

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
  assert(y1 >= 0 && y2 < 240);
  assert(y2 == y1 + NES_VIDEO_ROWS - 1);
  assert(y1 == int(output.rectangles % (240 / NES_VIDEO_ROWS)) * NES_VIDEO_ROWS);
  for (int i = 0; i < 256 * NES_VIDEO_ROWS; ++i) {
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
  assert(NesRuntime::coreBytes() < 16000 + 512 * NES_VIDEO_ROWS);
  for (int frame = 0; frame < 12; ++frame) assert(runtime.step(NesRuntime::A));
  assert(runtime.frames() == 12);
  assert(output.rectangles == (240 / NES_VIDEO_ROWS) * ((12 + NES_VIDEO_DIVISOR - 1) / NES_VIDEO_DIVISOR));
  assert(output.videoSum > 0);
  assert(output.correctRed);
  assert(output.audioFrames >= 11);
  assert(output.audioSamples >= 11 * 400);
  assert(output.audioPeak > 100);
  assert(runtime.core()->nes_cpu.cpu_ram[0] >= 8);
  assert(runtime.core()->nes_cpu.cpu_ram[1] == 1);
#if defined(NES_TEST_PROFILE) && NES_TEST_PROFILE == 1
  const auto* profile = nes_test_profile_get();
  assert(profile->frames == 12 && profile->frame_us > 0);
  assert(profile->region_calls[NES_PROF_FRAME] == 12);
  assert(profile->region_calls[NES_PROF_CPU] > 12 * 240);
  assert(profile->region_us[NES_PROF_APU] > 0);
  assert(profile->region_calls[NES_PROF_DRAW] == output.rectangles);
  const uint32_t cpuCalls = profile->region_calls[NES_PROF_CPU];
  nes_test_profile_regions(0);
  assert(runtime.step(NesRuntime::A));
  assert(profile->frames == 13 && profile->region_calls[NES_PROF_CPU] == cpuCalls);
  nes_test_profile_regions(1);
#endif
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
  assert(output.rectangles == (240 / NES_VIDEO_ROWS) * ((5 + NES_VIDEO_DIVISOR - 1) / NES_VIDEO_DIVISOR));
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

// These bank markers and the three-byte loop are generated for this fixture.
static std::vector<uint8_t> syntheticMmc3(unsigned mapper = 118, unsigned prgBanks = 4, unsigned chrBanks = 2) {
  std::vector<uint8_t> rom(16 + prgBanks * 16384 + chrBanks * 8192, 0);
  rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1a;
  rom[4] = uint8_t(prgBanks); rom[5] = uint8_t(chrBanks);
  rom[6] = uint8_t((mapper << 4) | 3); rom[7] = uint8_t(mapper & 0xf0); rom[8] = 1;
  for (unsigned bank = 0; bank < prgBanks * 2; ++bank)
    std::fill(rom.begin() + 16 + bank * 8192, rom.begin() + 16 + (bank + 1) * 8192, uint8_t(bank));
  for (unsigned bank = 0; bank < chrBanks * 8; ++bank)
    std::fill(rom.begin() + 16 + prgBanks * 16384 + bank * 1024,
              rom.begin() + 16 + prgBanks * 16384 + (bank + 1) * 1024, uint8_t(bank));
  const size_t last = 16 + prgBanks * 16384 - 8192;
  rom[last] = 0x4c; rom[last + 1] = 0; rom[last + 2] = 0xe0;
  for (size_t offset = 8192 - 6; offset < 8192; offset += 2) {
    rom[last + offset] = 0; rom[last + offset + 1] = 0xe0;
  }
  return rom;
}

static void mapper118BankingMirroringAndIrq() {
  auto rom = syntheticMmc3();
  NesRuntime runtime;
  assert(runtime.load(rom.data(), rom.size()));
  auto* core = const_cast<nes_t*>(runtime.core());
  assert(core->nes_rom.mapper_number == 118 && core->nes_rom.sram);
  const auto write = [core](uint16_t address, uint8_t value) {
    core->nes_mapper.mapper_write(core, address, value);
  };
  const auto bank = [&write](uint8_t reg, uint8_t value) { write(0x8000, reg); write(0x8001, value); };
  const auto nt = [core](unsigned slot, unsigned page) {
    assert(core->nes_ppu.name_table[slot] == core->nes_ppu.ppu_vram[page]);
    assert(core->nes_ppu.name_table_mirrors[slot] == core->nes_ppu.name_table[slot]);
  };
  const auto* prg = rom.data() + 16;
  const auto* chr = prg + 65536;
  bank(0, 0x82); bank(1, 4); bank(2, 0x85); bank(3, 6); bank(4, 0x87); bank(5, 8);
  const unsigned normal[] = {2, 3, 4, 5, 5, 6, 7, 8};
  for (unsigned slot = 0; slot < 8; ++slot)
    assert(core->nes_ppu.pattern_table[slot] == chr + normal[slot] * 1024);
  nt(0, 1); nt(1, 1); nt(2, 0); nt(3, 0);
  write(0xa000, 0); write(0xa000, 1);
  nt(0, 1); nt(1, 1); nt(2, 0); nt(3, 0);
  write(0x8000, 0x80);
  const unsigned inverted[] = {5, 6, 7, 8, 2, 3, 4, 5};
  for (unsigned slot = 0; slot < 8; ++slot)
    assert(core->nes_ppu.pattern_table[slot] == chr + inverted[slot] * 1024);
  nt(0, 1); nt(1, 0); nt(2, 1); nt(3, 0);
  core->nes_ppu.v_reg = 0x2000;
  nes_write_ppu_register(core, 0x2007, 0xa5);
  assert(core->nes_ppu.ppu_vram[1][0] == 0xa5);
  core->nes_ppu.v_reg = 0;
  const uint8_t tile = core->nes_ppu.pattern_table[0][0];
  nes_write_ppu_register(core, 0x2007, uint8_t(tile ^ 0xff));
  assert(core->nes_ppu.pattern_table[0][0] == tile);
  bank(6, 0xff); bank(7, 0xfe);
  assert(core->nes_cpu.prg_banks[0] == prg + 7 * 8192);
  assert(core->nes_cpu.prg_banks[1] == prg + 6 * 8192);
  write(0x8000, 0x46);
  assert(core->nes_cpu.prg_banks[0] == prg + 6 * 8192);
  assert(core->nes_cpu.prg_banks[2] == prg + 7 * 8192);
  assert(runtime.writeRam(0x6000, 0x5a) && runtime.writeRam(0x7fff, 0xa5));
  assert(runtime.readRam(0x6000) == 0x5a && runtime.readRam(0x7fff) == 0xa5);
  for (unsigned reg = 0; reg < 8; ++reg) for (unsigned value = 0; value < 256; ++value) {
    bank(uint8_t(reg), uint8_t(value));
    for (unsigned slot = 0; slot < 4; ++slot)
      assert(core->nes_cpu.prg_banks[slot] >= prg && core->nes_cpu.prg_banks[slot] + 8192 <= chr);
    for (unsigned slot = 0; slot < 8; ++slot)
      assert(core->nes_ppu.pattern_table[slot] >= chr && core->nes_ppu.pattern_table[slot] + 1024 <= rom.data() + rom.size());
  }
  core->nes_ppu.MASK_b = 1;
  write(0xe000, 0); write(0xc000, 2); write(0xc001, 0); write(0xe001, 0);
  core->nes_mapper.mapper_hsync(core);
  assert(!core->nes_cpu.irq_pending);
  core->nes_mapper.mapper_hsync(core);
  assert(!core->nes_cpu.irq_pending);
  core->nes_mapper.mapper_hsync(core);
  assert(core->nes_cpu.irq_pending);
  write(0xe000, 0);
  assert(!core->nes_cpu.irq_pending);
  write(0xc000, 0); write(0xc001, 0); write(0xe001, 0);
  core->nes_mapper.mapper_hsync(core);
  assert(core->nes_cpu.irq_pending);
  write(0xe000, 0);
  assert(runtime.step(0));
}

static void mmc3HeaderAndBankBounds() {
  NesRuntime runtime;
  for (unsigned mapper : {4u, 118u}) {
    for (unsigned prg : {1u, 3u, 33u}) {
      auto rom = syntheticMmc3(mapper, prg, 2);
      assert(!runtime.load(rom.data(), rom.size()));
    }
    for (unsigned chr : {3u, 33u}) {
      auto rom = syntheticMmc3(mapper, 4, chr);
      assert(!runtime.load(rom.data(), rom.size()));
    }
    auto rom = syntheticMmc3(mapper, 32, 32);
    assert(runtime.load(rom.data(), rom.size()));
    auto* core = const_cast<nes_t*>(runtime.core());
    core->nes_mapper.mapper_write(core, 0x8000, 0);
    core->nes_mapper.mapper_write(core, 0x8001, 0x82);
    assert(core->nes_ppu.pattern_table[0] == rom.data() + 16 + 524288 + 130 * 1024);
    assert(!runtime.load(rom.data(), rom.size() - 1));
  }
  auto rom = syntheticMmc3();
  rom[6] |= 8;
  assert(!runtime.load(rom.data(), rom.size()));
  rom = syntheticMmc3(118, 4, 0);
  assert(!runtime.load(rom.data(), rom.size()));
  rom = syntheticMmc3(4, 4, 0);
  assert(runtime.load(rom.data(), rom.size()));
  assert(runtime.core()->nes_rom.chr_rom && runtime.core()->nes_rom.sram);
}

static std::vector<uint8_t> syntheticMmc1() {
  auto rom = syntheticNrom();
  rom.resize(16 + 32768 + 16384, 0);
  rom[5] = 2;
  rom[6] = 0x12;
  std::vector<uint8_t> program = {0x78, 0xa9, 0x80, 0x8d, 0x00, 0x80};
  const auto serial = [&program](uint16_t address, uint8_t value) {
    for (int bit = 0; bit < 5; ++bit) program.insert(program.end(),
        {0xa9, uint8_t((value >> bit) & 1), 0x8d, uint8_t(address), uint8_t(address >> 8)});
  };
  serial(0x8000, 0x1f);
  serial(0xa000, 2);
  serial(0xc000, 3);
  program.insert(program.end(), {0x4c, 0x00, 0x80});
  std::copy(program.begin(), program.end(), rom.begin() + 16 + 0x200);
  rom[16 + 32768 - 4] = 0;
  rom[16 + 32768 - 3] = 0x82;
  return rom;
}

static void mapper1ResetAndRam() {
  auto rom = syntheticMmc1();
  NesRuntime runtime;
  assert(runtime.load(rom.data(), rom.size()));
  assert(runtime.core()->nes_rom.sram);
  assert(runtime.step(0));
  const auto* chr = rom.data() + 16 + 32768;
  assert(runtime.core()->nes_ppu.pattern_table[0] == chr + 8192);
  assert(runtime.core()->nes_ppu.pattern_table[4] == chr + 12288);
  assert(runtime.writeRam(0x6000, 0xa5));
  assert(runtime.readRam(0x6000) == 0xa5);
  assert(runtime.writeRam(0x1701, 0x5a));
  assert(runtime.readRam(0x701) == 0x5a);
  assert(!runtime.writeRam(0x2000, 0xff));
  assert(!runtime.writeRam(0x8000, 0xff));
  assert(runtime.softReset());
  assert(runtime.frames() == 0);
  assert(runtime.readRam(0x701) == 0x5a);
  assert(runtime.readRam(0x6000) == 0xa5);
  assert(runtime.setPal(true) && runtime.frameRate() == 50);
  assert(runtime.setPal(false) && runtime.frameRate() == 60);
  assert(runtime.step(0));
}

static void mapper1HeaderPolicy() {
  auto rom = syntheticNrom();
  rom.resize(16 + 32768);
  rom[5] = 0; rom[6] = 0x12; rom[7] = 8; rom[11] = 7;
  NesRuntime runtime;
  assert(runtime.load(rom.data(), rom.size()));
  assert(runtime.core()->nes_rom.chr_rom && runtime.core()->nes_rom.sram);
  rom[10] = 0x70;
  assert(!runtime.load(rom.data(), rom.size()));
  rom[10] = 7;
  assert(!runtime.load(rom.data(), rom.size()));
  rom[10] = 0; rom[11] = 0x70;
  assert(!runtime.load(rom.data(), rom.size()));
  rom[11] = 7; rom[4] = 3;
  assert(!runtime.load(rom.data(), rom.size()));
}

static void guardedTetrisConfiguration() {
  auto rom = syntheticMmc1();
  NesRuntime runtime;
  assert(runtime.load(rom.data(), rom.size()));
  flappy::TetrisGym::Options options;
  assert(!flappy::TetrisGym::configureMenu(runtime, options));
  const uint8_t signature[] = {0x54, 0x2d, 0x47, 0x59, 0x4d};
  for (unsigned i = 0; i < sizeof(signature); ++i) runtime.writeRam(0x75b + i, signature[i]);
  runtime.writeRam(0xc0, 3);
  options.level = 255; options.trainer = 6; options.height = 8; options.pal = true;
  assert(flappy::TetrisGym::configureMenu(runtime, options));
  assert(runtime.readRam(0x764) == 1 && runtime.readRam(0x765) == 255);
  assert(runtime.readRam(0x600) == 6 && runtime.readRam(0x770) == 8);
  assert(runtime.readRam(0x789) == 1 && runtime.frameRate() == 50);
  runtime.writeRam(0xc0, 4);
  assert(!flappy::TetrisGym::configureMenu(runtime, options));
  runtime.writeRam(0xbd, 3);
  assert(flappy::TetrisGym::playing(runtime));
  runtime.writeRam(0x50, 0x56); runtime.writeRam(0x51, 12);
  assert(flappy::TetrisGym::lines(runtime) == 1256);
  runtime.writeRam(0x44, 29);
  assert(flappy::TetrisGym::level(runtime) == 29);
  runtime.writeRam(8, 0x40); runtime.writeRam(9, 0xe2); runtime.writeRam(10, 1);
  assert(flappy::TetrisGym::score(runtime) == 123456);
  runtime.writeRam(0x48, 10);
  assert(flappy::TetrisGym::toppedOut(runtime));
  runtime.writeRam(0x50, 0); runtime.writeRam(0x51, 0);
  assert(flappy::TetrisGym::typeBComplete(runtime));
  assert(!flappy::TetrisGym::toppedOut(runtime));
  runtime.writeRam(0x600, 0);
  assert(flappy::TetrisGym::toppedOut(runtime));
  runtime.writeRam(0xc0, 2);
  assert(!flappy::TetrisGym::toppedOut(runtime));
  options.trainer = 2;
  options.seed[0] = 0; options.seed[1] = 1;
  assert(!flappy::TetrisGym::configureMenu(runtime, options));
  options.seed[0] = 0x89; options.seed[1] = 0x88;
  assert(flappy::TetrisGym::configureMenu(runtime, options));
  assert(runtime.readRam(0x37) == 0x89 && runtime.readRam(0x38) == 0x88);
}

static void portraitPixelsAndBounds() {
  flappy::TetrisViewport view;
  assert(view.configure(135, 240));
  assert(!view.configure(130, 240));
  const int stride = 141;
  std::vector<uint16_t> output(stride * 240 + 2, 0xaaaa);
  std::vector<uint16_t> half(256 * 120);
  for (int y = 0; y < 120; ++y) for (int x = 0; x < 256; ++x) half[y * 256 + x] = uint16_t(y * 256 + x);
  assert(!view.consume(0, 0, 255, 119, half.data(), output.data() + 1, true, stride));
  for (int y = 0; y < 120; ++y) for (int x = 0; x < 256; ++x) half[y * 256 + x] = uint16_t((y + 120) * 256 + x);
  assert(view.consume(0, 120, 255, 239, half.data(), output.data() + 1, true, stride));
  const auto at = [&output](int x, int y) { return output[1 + y * stride + x]; };
  assert(at(27, 72) == 48 * 256 + 96);
  assert(at(106, 231) == 207 * 256 + 175);
  assert(at(4, 4) == 56 * 256 + 192);
  assert(at(83, 4) == 104 * 256 + 184);
  assert(at(4, 44) == 160 * 256 + 192);
  assert(output.front() == 0xaaaa && output.back() == 0xaaaa);
  for (int y = 0; y < 240; ++y) assert(output[1 + y * stride + 135] == 0xaaaa);
}

static void zeroRoundCompletionDoesNotTreatPauseOrClearsAsGameOver() {
  auto rom = syntheticNrom();
  NesRuntime runtime;
  assert(runtime.load(rom.data(), rom.size()));
  for (uint8_t players : {uint8_t(1), uint8_t(2)}) {
    runtime.writeRam(0xbe, players);
    runtime.writeRam(0xc0, 4);
    for (uint8_t type : {uint8_t(0), uint8_t(1)}) {
      runtime.writeRam(0xc1, type);
      for (uint8_t phase : {uint8_t(1), uint8_t(2), uint8_t(3), uint8_t(5),
                            uint8_t(6), uint8_t(7), uint8_t(8)}) {
        runtime.writeRam(0x48, phase);
        runtime.writeRam(0x68, phase);
        runtime.writeRam(0xa7, 2);
        assert(!flappy::TetrisZero::finished(runtime));
        runtime.writeRam(0xa7, 7);
        assert(!flappy::TetrisZero::finished(runtime));
      }
      for (uint8_t mode : {uint8_t(4), uint8_t(5)}) {
        runtime.writeRam(0xa7, mode);
        assert(flappy::TetrisZero::finished(runtime) == (type == 1));
      }
      runtime.writeRam(0xa7, 2);
      runtime.writeRam(0x48, 10);
      runtime.writeRam(0x68, 10);
      assert(flappy::TetrisZero::finished(runtime));
      runtime.writeRam(0xc0, 3);
      assert(!flappy::TetrisZero::finished(runtime));
      runtime.writeRam(0xc0, 4);
    }
  }
  runtime.unload();
  assert(!flappy::TetrisZero::finished(runtime));
}

int main() {
  realCpuVideoAudioAndInput();
  rejectsUnsupportedAndTruncatedImages();
  mapper37BankSwitching();
  mapper118BankingMirroringAndIrq();
  mmc3HeaderAndBankBounds();
  mapper1ResetAndRam();
  mapper1HeaderPolicy();
  guardedTetrisConfiguration();
  zeroRoundCompletionDoesNotTreatPauseOrClearsAsGameOver();
  portraitPixelsAndBounds();
  std::cout << "NES CPU, PPU, APU, input, validation and mapper checks passed. Core bytes: "
            << NesRuntime::coreBytes() << "\n";
}
