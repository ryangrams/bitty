#pragma once

#include <stddef.h>
#include <stdint.h>
#include <nes.h>
#if defined(NES_TEST_PROFILE) && NES_TEST_PROFILE == 1
#include <nes_test.h>
#ifdef ARDUINO
#include <esp_timer.h>
#else
#include <chrono>
#endif
#endif

namespace flappy {

class NesRuntime {
 public:
  enum Pad : uint8_t { A = 1, B = 2, Select = 4, Start = 8, Up = 16, Down = 32, Left = 64, Right = 128 };
  // Video rectangles include both endpoints. Pixel and audio pointers are borrowed for the callback.
  using VideoCallback = void (*)(void* context, int x1, int y1, int x2, int y2, const uint16_t* rgb565);
  using AudioCallback = void (*)(void* context, const int16_t* mono, size_t sampleCount, uint32_t sampleRate);

  NesRuntime() = default;
  ~NesRuntime() { unload(); }
  NesRuntime(const NesRuntime&) = delete;
  NesRuntime& operator=(const NesRuntime&) = delete;

  void setCallbacks(VideoCallback video, AudioCallback audio, void* context) {
    video_ = video;
    audio_ = audio;
    context_ = context;
  }

  bool load(const uint8_t* rom, size_t bytes) {
    unload();
    error_ = nullptr;
    if (!rom || bytes < 16) return fail("ROM header is incomplete");
    if (rom[0] != 'N' || rom[1] != 'E' || rom[2] != 'S' || rom[3] != 0x1a) return fail("ROM header is invalid");
    const unsigned identifier = rom[7] & 0x0c;
    if (identifier != 0 && identifier != 8) return fail("ROM header format is unsupported");
    const bool nes2 = identifier == 8;
    const unsigned mapper = (rom[6] >> 4) | (rom[7] & 0xf0) | (nes2 ? (rom[8] & 15) << 8 : 0);
    if (mapper != 0 && mapper != 1 && mapper != 4 && mapper != 37 && mapper != 118) return fail("Only mappers 0, 1, 4, 37 and 118 are supported");
    if (nes2 && (rom[8] >> 4)) return fail("Cartridge submapper is unsupported");
    if (nes2 && ((rom[9] & 15) == 15 || (rom[9] >> 4) == 15)) return fail("Exponent ROM sizing is unsupported");
    const unsigned prgBanks = rom[4] | (nes2 ? (rom[9] & 15) << 8 : 0);
    const unsigned chrBanks = rom[5] | (nes2 ? (rom[9] >> 4) << 8 : 0);
    if (prgBanks == 0) return fail("Cartridge has no program banks");
    if (mapper == 0 && (prgBanks > 2 || chrBanks > 1)) return fail("NROM bank count is invalid");
    if (mapper == 1 && (prgBanks > 16 || (prgBanks & (prgBanks - 1)) || chrBanks > 16 ||
                       (chrBanks && (chrBanks & (chrBanks - 1))))) return fail("MMC1 bank count is invalid");
    if (mapper == 37 && (prgBanks != 16 || chrBanks != 32)) return fail("Mapper 37 bank count is invalid");
    if ((mapper == 4 || mapper == 118) && (prgBanks < 2 || prgBanks > 32 ||
        (prgBanks & (prgBanks - 1)) || chrBanks > 32 ||
        (chrBanks && (chrBanks & (chrBanks - 1))))) return fail("MMC3 bank count is invalid");
    if (mapper == 118 && (chrBanks == 0 || (rom[6] & 8))) return fail("TxSROM requires CHR ROM and two nametable pages");
    if (rom[7] & 3) return fail("Only standard NES cartridges are supported");
    if (rom[6] & 4) return fail("Cartridge trainers are unsupported");
    if (nes2) {
      if ((rom[12] & 0xfc) || (rom[14] & 0xfc) || (rom[15] & 0xc0)) return fail("NES 2.0 reserved header bits are set");
      if (rom[13] || (rom[14] & 3)) return fail("NES 2.0 cartridge extensions are unsupported");
      if (rom[10]) return fail("Explicit NES 2.0 program RAM is unsupported");
      if (rom[11] && !((mapper == 0 || mapper == 1) && chrBanks == 0 && rom[11] == 7)) return fail("NES 2.0 character RAM size is unsupported");
    } else {
      if ((rom[9] & 0xfe) || rom[10] || rom[11] || rom[12] || rom[13] || rom[14] || rom[15]) return fail("iNES reserved header bytes are set");
      if (rom[8] > 1) return fail("iNES program RAM size is unsupported");
    }
    const unsigned timing = nes2 ? rom[12] & 3 : rom[9] & 1;
    if (timing == 3) return fail("Dendy timing is unsupported");
    const size_t expected = 16 + size_t(prgBanks) * 16384 + size_t(chrBanks) * 8192;
    if (expected > 1024 * 1024 || bytes > 1024 * 1024) return fail("Cartridge exceeds the ROM size limit");
    if (bytes != expected) return fail("ROM length does not match its header");
    core_ = nes_init();
    if (!core_) return fail("NES memory allocation failed");
    if (nes_load_rom(core_, rom) != NES_OK) {
      unload();
      return fail("NES cartridge initialization failed");
    }
    if (mapper != 0 && !core_->nes_mapper.mapper_register) {
      unload();
      return fail("NES mapper allocation failed");
    }
    if ((mapper == 1 || mapper == 4 || mapper == 118) && !core_->nes_rom.sram) {
      unload();
      return fail("NES cartridge RAM allocation failed");
    }
    if (timing == 1) nes_timing_set_pal(core_);
    nes_cpu_reset(core_);
#if defined(NES_TEST_PROFILE) && NES_TEST_PROFILE == 1
    nes_test_profile_begin(core_);
#endif
    frameCount_ = 0;
    dcQ8_ = 0;
    return true;
  }

  void unload() {
    if (active() == this) active() = nullptr;
    if (core_) {
#if defined(NES_TEST_PROFILE) && NES_TEST_PROFILE == 1
      nes_test_profile_begin(nullptr);
#endif
      nes_unload_rom(core_);
      nes_deinit(core_);
      core_ = nullptr;
    }
    frameCount_ = 0;
  }

  bool step(uint8_t pad1, uint8_t pad2 = 0) {
    if (!core_) return false;
    NesRuntime* previous = active();
    active() = this;
    auto& joy = core_->nes_cpu.joypad;
    joy.A1 = (pad1 & A) != 0; joy.B1 = (pad1 & B) != 0;
    joy.SE1 = (pad1 & Select) != 0; joy.ST1 = (pad1 & Start) != 0;
    joy.U1 = (pad1 & Up) != 0; joy.D1 = (pad1 & Down) != 0;
    joy.L1 = (pad1 & Left) != 0; joy.R1 = (pad1 & Right) != 0;
    joy.A2 = (pad2 & A) != 0; joy.B2 = (pad2 & B) != 0;
    joy.SE2 = (pad2 & Select) != 0; joy.ST2 = (pad2 & Start) != 0;
    joy.U2 = (pad2 & Up) != 0; joy.D2 = (pad2 & Down) != 0;
    joy.L2 = (pad2 & Left) != 0; joy.R2 = (pad2 & Right) != 0;
    nes_run_frame(core_);
    ++frameCount_;
    active() = previous;
    return true;
  }

  bool ready() const { return core_ != nullptr; }
  const char* error() const { return error_ ? error_ : ""; }
  uint32_t frames() const { return frameCount_; }
  uint8_t frameRate() const { return core_ ? core_->timing.frame_rate : 60; }
  static size_t coreBytes() { return sizeof(nes_t); }
  const nes_t* core() const { return core_; }

  bool softReset() {
    if (!core_) return false;
    core_->nes_cpu.irq_nmi = core_->nes_cpu.irq_nmi_delay = core_->nes_cpu.irq_pending = 0;
    core_->nes_quit = 0;
    core_->host_dot_remainder = 0;
#if NES_FRAME_SKIP != 0
    core_->nes_frame_skip_count = 0;
#endif
    nes_cpu_reset(core_);
    frameCount_ = 0;
    dcQ8_ = 0;
    return true;
  }

  uint8_t readRam(uint16_t address) const {
    if (!core_) return 0;
    if (address < 0x2000) return core_->nes_cpu.cpu_ram[address & 0x7ff];
    if (address >= 0x6000 && address < 0x8000 && core_->nes_rom.sram) return core_->nes_rom.sram[address - 0x6000];
    return 0;
  }

  bool writeRam(uint16_t address, uint8_t value) {
    if (!core_) return false;
    if (address < 0x2000) { core_->nes_cpu.cpu_ram[address & 0x7ff] = value; return true; }
    if (address >= 0x6000 && address < 0x8000 && core_->nes_rom.sram) {
      core_->nes_rom.sram[address - 0x6000] = value;
      return true;
    }
    return false;
  }

  bool setPal(bool pal) {
    if (!core_) return false;
    if (pal) nes_timing_set_pal(core_);
    else {
      auto& timing = core_->timing;
      timing.line_clocks = 113; timing.line_split = 85;
      timing.remainder_add = 2; timing.remainder_mod = 3;
      timing.vblank_lines = 20; timing.frame_rate = 60;
      timing.apu_frame_divisor = 66; timing.cpu_clock = 1789773;
      timing.samples_per_frame = NES_APU_SAMPLE_RATE / 60;
    }
    return true;
  }

  static int deliverVideo(int x1, int y1, int x2, int y2, const uint16_t* pixels) {
    NesRuntime* runtime = active();
    if (runtime && runtime->video_) runtime->video_(runtime->context_, x1, y1, x2, y2, pixels);
    return NES_OK;
  }

  static int deliverAudio(const uint8_t* buffer, size_t samples) {
    NesRuntime* runtime = active();
    if (!runtime || !runtime->audio_) return NES_OK;
    if (samples > sizeof(runtime->audioBuffer_) / sizeof(runtime->audioBuffer_[0])) return NES_ERROR;
    for (size_t i = 0; i < samples; ++i) {
      const int32_t input = int32_t(buffer[i]) * 256;
      runtime->dcQ8_ += (input - runtime->dcQ8_) / 512;
      int32_t centered = input - runtime->dcQ8_;
      if (centered > 32767) centered = 32767;
      if (centered < -32768) centered = -32768;
      runtime->audioBuffer_[i] = int16_t(centered);
    }
    runtime->audio_(runtime->context_, runtime->audioBuffer_, samples, NES_APU_SAMPLE_RATE);
    return NES_OK;
  }

 private:
  nes_t* core_ = nullptr;
  VideoCallback video_ = nullptr;
  AudioCallback audio_ = nullptr;
  void* context_ = nullptr;
  const char* error_ = nullptr;
  uint32_t frameCount_ = 0;
  int32_t dcQ8_ = 0;
  int16_t audioBuffer_[NES_APU_SAMPLE_PER_SYNC_MAX] = {};

  bool fail(const char* error) { error_ = error; return false; }
  static NesRuntime*& active() { static NesRuntime* current = nullptr; return current; }
};

}  // namespace flappy

#if defined(NES_TEST_PROFILE) && NES_TEST_PROFILE == 1
namespace flappy {
struct NesProfileState {
  nes_test_profile_t counters = {};
  uint64_t started[NES_PROF_COUNT] = {};
  nes_t* owner = nullptr;
  bool regions = true;
};
inline NesProfileState& nesProfileState() {
  static NesProfileState state;
  return state;
}
}  // namespace flappy
#endif

// One firmware translation unit owns the platform hooks.
extern "C" {
#if defined(NES_TEST_PROFILE) && NES_TEST_PROFILE == 1
uint64_t nes_test_time_us(void) {
#ifdef ARDUINO
  return uint64_t(esp_timer_get_time()) + 1;
#else
  return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count()) + 1;
#endif
}
void nes_test_profile_begin(nes_t* nes) {
  auto& state = flappy::nesProfileState();
  state = flappy::NesProfileState{};
  state.owner = nes;
}
const nes_test_profile_t* nes_test_profile_get(void) {
  return &flappy::nesProfileState().counters;
}
void nes_test_profile_regions(int enable) { flappy::nesProfileState().regions = enable != 0; }
int nes_test_profile_regions_enabled(void) { return flappy::nesProfileState().regions; }
void nes_test_profile_region_begin(nes_t* nes, int region) {
  auto& state = flappy::nesProfileState();
  if (state.owner != nes || region < 0 || region >= NES_PROF_COUNT ||
      (!state.regions && region != NES_PROF_FRAME)) return;
  state.started[region] = nes_test_time_us();
}
void nes_test_profile_region_end(nes_t* nes, int region) {
  auto& state = flappy::nesProfileState();
  if (state.owner != nes || region < 0 || region >= NES_PROF_COUNT ||
      (!state.regions && region != NES_PROF_FRAME) || !state.started[region]) return;
  const uint64_t elapsed = nes_test_time_us() - state.started[region];
  state.started[region] = 0;
  state.counters.region_us[region] += elapsed;
  ++state.counters.region_calls[region];
  if (region == NES_PROF_FRAME) {
    ++state.counters.frames;
    state.counters.frame_us += elapsed;
    if (elapsed > state.counters.frame_max_us) state.counters.frame_max_us = elapsed;
  }
}
void nes_test_profile_stream(nes_t* nes, int chr, int hit) {
  auto& state = flappy::nesProfileState();
  if (state.owner != nes) return;
  if (chr) {
    if (hit) ++state.counters.stream_chr_hit;
    else ++state.counters.stream_chr_miss;
  } else {
    if (hit) ++state.counters.stream_prg_hit;
    else ++state.counters.stream_prg_miss;
  }
}
#endif
int nes_initex(nes_t*) { return NES_OK; }
int nes_deinitex(nes_t*) { return NES_OK; }
void nes_frame(nes_t*) {}
int nes_draw(int x1, int y1, int x2, int y2, nes_color_t* pixels) {
  return flappy::NesRuntime::deliverVideo(x1, y1, x2, y2, pixels);
}
int nes_sound_output(uint8_t* samples, size_t count) {
  return flappy::NesRuntime::deliverAudio(samples, count);
}
}
