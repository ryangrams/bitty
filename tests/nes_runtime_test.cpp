#include "../firmware/flappy/nes_runtime.h"
#include "../firmware/flappy/tetris_gym.h"
#include "../firmware/flappy/tetris_zero.h"
#include "../firmware/flappy/tetris_viewport.h"

#include <algorithm>
#include <assert.h>
#include <cmath>
#include <cstring>
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

struct VideoTrace {
  const nes_t* owner = nullptr;
  std::vector<uint16_t> pixels = std::vector<uint16_t>(256 * 240, 0);
  std::vector<uint32_t> ppuEvents;
  uint32_t audioHash = 2166136261u;
  unsigned rectangles = 0;
};

static VideoTrace* activeVideoTraces[3] = {};

static VideoTrace& videoTrace(nes_t* nes) {
  for (auto* trace : activeVideoTraces) if (trace && trace->owner == nes) return *trace;
  assert(false);
  return *activeVideoTraces[0];
}

static void tracePatternFetch(nes_t* nes, uint16_t address) {
  videoTrace(nes).ppuEvents.push_back(0x10000000u | uint32_t(nes->scanline) << 16u | address);
  // A fetch changes the next pattern mapping, as a latch mapper would.
  nes->nes_ppu.pattern_table[0] = nes->nes_rom.chr_rom + ((address >> 4u) & 7u) * 1024u;
}

static void traceRenderMode(nes_t* nes, uint8_t background) {
  videoTrace(nes).ppuEvents.push_back(0x20000000u | uint32_t(nes->scanline) << 8u | background);
}

static void traceHsync(nes_t* nes) {
  videoTrace(nes).ppuEvents.push_back(0x30000000u | uint32_t(nes->scanline) << 8u | nes->nes_ppu.ppu_status);
}

static void captureVideo(void* context, int x1, int y1, int x2, int y2, const uint16_t* pixels) {
  auto& trace = *static_cast<VideoTrace*>(context);
  assert(x1 == 0 && x2 == 255 && y2 == y1 + NES_VIDEO_ROWS - 1);
  assert(y1 == int(trace.rectangles % (240 / NES_VIDEO_ROWS)) * NES_VIDEO_ROWS);
  std::copy(pixels, pixels + 256 * NES_VIDEO_ROWS, trace.pixels.begin() + y1 * 256);
  ++trace.rectangles;
}

static void captureAudio(void* context, const int16_t* samples, size_t count, uint32_t rate) {
  auto& trace = *static_cast<VideoTrace*>(context);
  assert(rate == 24000);
  for (size_t i = 0; i < count; ++i) {
    trace.audioHash = (trace.audioHash ^ uint8_t(samples[i])) * 16777619u;
    trace.audioHash = (trace.audioHash ^ uint8_t(uint16_t(samples[i]) >> 8u)) * 16777619u;
  }
}

static void sameMachineState(const NesRuntime& reference, const NesRuntime& candidate) {
  const nes_t* a = reference.core();
  const nes_t* b = candidate.core();
  assert(std::memcmp(&a->nes_cpu, &b->nes_cpu, sizeof(a->nes_cpu)) == 0);
  assert(std::memcmp(&a->nes_apu, &b->nes_apu, sizeof(a->nes_apu)) == 0);
  assert(a->host_dot_remainder == b->host_dot_remainder);
  assert(std::memcmp(&a->timing, &b->timing, sizeof(a->timing)) == 0);
  auto ap = a->nes_ppu;
  auto bp = b->nes_ppu;
  // Presentation caches and pointers differ between instances. PPU registers,
  // VRAM, OAM and palette RAM must remain identical.
  std::memset(ap.bg_opaque, 0, sizeof(ap.bg_opaque));
  std::memset(bp.bg_opaque, 0, sizeof(bp.bg_opaque));
  std::memset(ap.palette, 0, sizeof(ap.palette));
  std::memset(bp.palette, 0, sizeof(bp.palette));
  std::memset(ap.chr_banks, 0, sizeof(ap.chr_banks));
  std::memset(bp.chr_banks, 0, sizeof(bp.chr_banks));
  assert(std::memcmp(&ap, &bp, sizeof(ap)) == 0);
}

static void runtimeVideoWindowsPreserveMachineAndVisiblePixels() {
  auto rom = syntheticNrom();
  for (unsigned i = 0; i < 8192; ++i) rom[16 + 32768 + i] = uint8_t((i * 37u) ^ (i >> 4u));
  for (bool masks : {false, true}) for (bool hooks : {false, true}) {
    NesRuntime reference, windowed, skipped;
    VideoTrace traces[3];
    NesRuntime* runtimes[] = {&reference, &windowed, &skipped};
    for (unsigned i = 0; i < 3; ++i) {
      assert(runtimes[i]->load(rom.data(), rom.size()));
      assert(runtimes[i]->setVideoDivisor(i == 2 ? 3 : 1));
      runtimes[i]->setCallbacks(captureVideo, captureAudio, &traces[i]);
      auto* core = const_cast<nes_t*>(runtimes[i]->core());
      traces[i].owner = core;
      activeVideoTraces[i] = &traces[i];
      core->nes_mapper.mapper_hsync = traceHsync;
      if (hooks) {
        core->nes_mapper.mapper_ppu = tracePatternFetch;
        core->nes_mapper.mapper_ppu_tile_min = 0;
        core->nes_mapper.mapper_ppu_tile_max = 255;
        core->nes_mapper.mapper_render_screen = traceRenderMode;
      }
    }
    for (unsigned frame = 0; frame < 120; ++frame) {
      const int worldTop = 32 + int(frame % 90u);
      uint8_t rowMask[30] = {};
      if (masks) {
        const unsigned heights[] = {1, 80, 135, 180, 240};
        const unsigned height = frame % 13u ? heights[frame % 5u] : 0;
        for (unsigned row = 0; row < height; ++row) {
          const unsigned sourceRow = row * 240u / height;
          rowMask[sourceRow >> 3u] |= uint8_t(1u << (sourceRow & 7u));
        }
        assert(windowed.setVideoRowMask(rowMask, sizeof(rowMask)));
        assert(skipped.setVideoRowMask(rowMask, sizeof(rowMask)));
      } else {
        assert(windowed.setVideoRows(16, 31, worldTop, worldTop + 118));
        assert(skipped.setVideoRows(16, 31, worldTop, worldTop + 118));
      }
      for (unsigned i = 0; i < 3; ++i) {
        auto* core = const_cast<nes_t*>(runtimes[i]->core());
        std::memset(core->nes_ppu.oam_data, 0xff, sizeof(core->nes_ppu.oam_data));
        for (unsigned sprite = 0; sprite < 12; ++sprite) {
          auto& item = core->nes_ppu.sprite_info[sprite];
          item.y = uint8_t((sprite * 19u + frame * 3u) % 238u);
          item.x = uint8_t((sprite * 27u + frame) % 256u);
          item.tile_index_number = uint8_t(sprite * 13u + frame);
          item.attributes = uint8_t((sprite + frame) << 5u);
        }
        core->nes_ppu.ppu_mask = frame % 7u ? 0x1e : 0x18;
        traces[i].ppuEvents.clear();
        assert(runtimes[i]->step(frame & 1u ? NesRuntime::A : 0));
      }
      sameMachineState(reference, windowed);
      sameMachineState(reference, skipped);
      for (unsigned i = 1; i < 3; ++i) {
        assert(traces[0].ppuEvents == traces[i].ppuEvents);
        assert(traces[0].audioHash == traces[i].audioHash);
        if (i == 2 && frame % 3u) continue;
        for (int y = 0; y < 240; ++y) {
          if (masks ? !(rowMask[y >> 3] & (1u << (y & 7))) :
              ((y < 16 || y > 31) && (y < worldTop || y > worldTop + 118))) continue;
          assert(std::equal(traces[0].pixels.begin() + y * 256, traces[0].pixels.begin() + (y + 1) * 256,
                            traces[i].pixels.begin() + y * 256));
        }
      }
    }
    assert(traces[0].rectangles == 120u * (240u / NES_VIDEO_ROWS));
    assert(traces[1].rectangles == traces[0].rectangles);
    assert(traces[2].rectangles == 40u * (240u / NES_VIDEO_ROWS));
    for (auto& trace : activeVideoTraces) trace = nullptr;
  }
}

static void runtimeVideoCadenceAndBounds() {
  NesRuntime runtime;
  uint8_t rowMask[30] = {};
  assert(!runtime.setVideoDivisor(1));
  assert(!runtime.setVideoRows(16, 31));
  assert(!runtime.setVideoRowMask(rowMask, sizeof(rowMask)));
  auto rom = syntheticNrom();
  assert(runtime.load(rom.data(), rom.size()));
  VideoTrace trace;
  runtime.setCallbacks(captureVideo, nullptr, &trace);
  assert(!runtime.setVideoRowMask(nullptr, sizeof(rowMask)));
  assert(!runtime.setVideoRowMask(rowMask, sizeof(rowMask) - 1));
  assert(!runtime.setVideoRowMask(rowMask, sizeof(rowMask) + 1));
  rowMask[0] = 0x81;
  assert(runtime.setVideoRowMask(rowMask, sizeof(rowMask)));
  rowMask[0] = 0;
  assert(runtime.core()->host_video_rows[0] == 0x81);
  assert(!runtime.setVideoDivisor(0));
  assert(runtime.videoDivisor() == NES_VIDEO_DIVISOR);
  for (auto bounds : {std::vector<int>{-1, 5, -1, -1}, {5, 4, -1, -1}, {0, 240, -1, -1},
                     {0, 239, -1, 5}, {0, 239, 5, -1}, {0, 239, 10, 9}, {0, 239, 0, 240}})
    assert(!runtime.setVideoRows(bounds[0], bounds[1], bounds[2], bounds[3]));
  assert(runtime.setVideoRows(0, 0));
  assert(runtime.setVideoRows(0, 239, 0, 239));
  runtime.resetVideoRows();
  assert(!runtime.core()->host_video_window_enabled);
  for (unsigned frame = 0; frame < 7; ++frame) {
    assert(runtime.setVideoDivisor(2));
    assert(runtime.step(0));
  }
  assert(trace.rectangles == 4u * (240u / NES_VIDEO_ROWS));
  assert(runtime.setVideoDivisor(1));
  assert(runtime.step(0));
  assert(trace.rectangles == 5u * (240u / NES_VIDEO_ROWS));
  assert(runtime.setVideoDivisor(3));
  for (unsigned frame = 0; frame < 7; ++frame) assert(runtime.step(0));
  assert(trace.rectangles == 8u * (240u / NES_VIDEO_ROWS));
  assert(runtime.softReset() && runtime.videoDivisor() == 3);
  assert(runtime.step(0));
  assert(trace.rectangles == 9u * (240u / NES_VIDEO_ROWS));
  assert(runtime.load(rom.data(), rom.size()));
  assert(runtime.videoDivisor() == NES_VIDEO_DIVISOR && !runtime.core()->host_video_window_enabled);
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

static void mapper4FourScreenAndExplicitRam() {
  auto rom = syntheticMmc3(4, 32, 32);
  rom[6] = 0x4a;
  rom[7] |= 8;
  rom[8] = 0;
  rom[10] = 0x70;
  rom[12] = 2;
  NesRuntime runtime;
  assert(runtime.load(rom.data(), rom.size()));
  assert(runtime.frameRate() == 60);
  auto* core = const_cast<nes_t*>(runtime.core());
  assert(core->nes_rom.four_screen && core->nes_rom.save_ram && core->nes_rom.sram);
  assert(core->nes_rom.prg_rom == rom.data() + 16);
  assert(core->nes_rom.chr_rom == rom.data() + 16 + 524288);
  for (unsigned page = 0; page < 4; ++page) {
    assert(core->nes_ppu.name_table[page] == core->nes_ppu.ppu_vram[page]);
    assert(core->nes_ppu.name_table_mirrors[page] == core->nes_ppu.ppu_vram[page]);
    core->nes_ppu.v_reg = uint16_t(0x2000 + page * 0x400 + 31);
    nes_write_ppu_register(core, 0x2007, uint8_t(0x31 + page));
  }
  for (uint8_t mirroring : {uint8_t(0), uint8_t(1)}) {
    core->nes_mapper.mapper_write(core, 0xa000, mirroring);
    for (unsigned page = 0; page < 4; ++page) {
      assert(core->nes_ppu.name_table[page] == core->nes_ppu.ppu_vram[page]);
      assert(core->nes_ppu.name_table_mirrors[page] == core->nes_ppu.ppu_vram[page]);
      assert(core->nes_ppu.ppu_vram[page][31] == 0x31 + page);
    }
  }
  assert(runtime.writeRam(0x6000, 0x35) && runtime.writeRam(0x7fff, 0x53));
  assert(runtime.readRam(0x6000) == 0x35 && runtime.readRam(0x7fff) == 0x53);
  assert(runtime.step(0));

  rom = syntheticMmc3(4, 2, 1);
  rom[7] |= 8;
  rom[8] = 0;
  for (bool battery : {false, true}) {
    rom[6] = uint8_t((rom[6] & ~2u) | (battery ? 2u : 0u));
    for (unsigned value = 0; value < 256; ++value) {
      rom[10] = uint8_t(value);
      const bool allowed = value == 0 || value == 7 || (battery && value == 0x70);
      assert(runtime.load(rom.data(), rom.size()) == allowed);
      if (allowed) assert(runtime.core()->nes_rom.sram);
    }
  }
  for (unsigned mapper : {0u, 1u, 37u, 118u}) {
    auto other = mapper == 37 ? syntheticMmc3(mapper, 16, 32) : syntheticMmc3(mapper, 2, 1);
    other[7] |= 8;
    other[8] = 0;
    for (uint8_t value : {uint8_t(7), uint8_t(0x70)}) {
      other[10] = value;
      assert(!runtime.load(other.data(), other.size()));
    }
  }
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

static void cartridgeRandomizerRejectsUnpinnedImages() {
  NesRuntime runtime;
  uint8_t orientation = 0xa5;
  assert(!runtime.runTetrisRandomizer(false, orientation));
  assert(!runtime.runTetrisRandomizer(true, orientation) && orientation == 0xa5);
  auto rom = syntheticNrom();
  assert(runtime.load(rom.data(), rom.size()));
  assert(runtime.step(0));
  runtime.writeRam(0xc0, 4);
  runtime.writeRam(0xa7, 2);
  runtime.writeRam(0xbd, 3);
  runtime.writeRam(0xbe, 2);
  const char signature[] = "T-GYM";
  for (unsigned i = 0; i < 5; ++i) runtime.writeRam(0x75b + i, signature[i]);
  const uint32_t frames = runtime.frames();
  std::vector<uint8_t> before(sizeof(nes_t));
  std::memcpy(before.data(), runtime.core(), before.size());
  for (bool zero : {false, true}) {
    assert(!runtime.runTetrisRandomizer(zero, orientation));
    assert(orientation == 0xa5 && runtime.frames() == frames);
    assert(std::memcmp(before.data(), runtime.core(), before.size()) == 0);
  }
  runtime.unload();
  assert(!runtime.runTetrisRandomizer(false, orientation) && orientation == 0xa5);
}

static void zeroScoreReadsEachPlayersBcdDigits() {
  auto rom = syntheticNrom();
  NesRuntime runtime;
  assert(runtime.load(rom.data(), rom.size()));
  for (uint8_t players : {uint8_t(1), uint8_t(2)}) {
    runtime.writeRam(0xbe, players);
    for (uint8_t player = 1; player <= players; ++player) {
      const unsigned base = players == 2 && player == 1 ? 0x60 : 0x40;
      runtime.writeRam(base + 0x13, 0x56);
      runtime.writeRam(base + 0x14, 0x34);
      runtime.writeRam(base + 0x15, 0x12);
      assert(flappy::TetrisZero::score(runtime, player) == 123456);
      runtime.writeRam(base + 0x13, 0);
      runtime.writeRam(base + 0x14, 0);
      runtime.writeRam(base + 0x15, 0);
      assert(flappy::TetrisZero::score(runtime, player) == 0);
    }
  }
}

int main() {
  cartridgeRandomizerRejectsUnpinnedImages();
  zeroScoreReadsEachPlayersBcdDigits();
  realCpuVideoAudioAndInput();
  runtimeVideoWindowsPreserveMachineAndVisiblePixels();
  runtimeVideoCadenceAndBounds();
  rejectsUnsupportedAndTruncatedImages();
  mapper37BankSwitching();
  mapper118BankingMirroringAndIrq();
  mmc3HeaderAndBankBounds();
  mapper4FourScreenAndExplicitRam();
  mapper1ResetAndRam();
  mapper1HeaderPolicy();
  guardedTetrisConfiguration();
  zeroRoundCompletionDoesNotTreatPauseOrClearsAsGameOver();
  portraitPixelsAndBounds();
  std::cout << "NES CPU, PPU, APU, input, validation and mapper checks passed. Core bytes: "
            << NesRuntime::coreBytes() << "\n";
}
