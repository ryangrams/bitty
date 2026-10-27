#pragma once

#include <stddef.h>
#include <stdint.h>
#include <nes.h>

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
    if (mapper != 0 && mapper != 4 && mapper != 37) return fail("Only mappers 0, 4 and 37 are supported");
    if (nes2 && (rom[8] >> 4)) return fail("Cartridge submapper is unsupported");
    if (nes2 && ((rom[9] & 15) == 15 || (rom[9] >> 4) == 15)) return fail("Exponent ROM sizing is unsupported");
    const unsigned prgBanks = rom[4] | (nes2 ? (rom[9] & 15) << 8 : 0);
    const unsigned chrBanks = rom[5] | (nes2 ? (rom[9] >> 4) << 8 : 0);
    if (prgBanks == 0) return fail("Cartridge has no program banks");
    if (mapper == 0 && (prgBanks > 2 || chrBanks > 1)) return fail("NROM bank count is invalid");
    if (mapper == 37 && (prgBanks != 16 || chrBanks != 32)) return fail("Mapper 37 bank count is invalid");
    if (rom[7] & 3) return fail("Only standard NES cartridges are supported");
    if (rom[6] & 4) return fail("Cartridge trainers are unsupported");
    if (nes2) {
      if ((rom[12] & 0xfc) || (rom[14] & 0xfc) || (rom[15] & 0xc0)) return fail("NES 2.0 reserved header bits are set");
      if (rom[13] || (rom[14] & 3)) return fail("NES 2.0 cartridge extensions are unsupported");
      if (rom[10]) return fail("Explicit NES 2.0 program RAM is unsupported");
      if (rom[11] && !(mapper == 0 && chrBanks == 0 && rom[11] == 7)) return fail("NES 2.0 character RAM size is unsupported");
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
    if (timing == 1) nes_timing_set_pal(core_);
    nes_cpu_reset(core_);
    frameCount_ = 0;
    dcQ8_ = 0;
    return true;
  }

  void unload() {
    if (active() == this) active() = nullptr;
    if (core_) {
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

// One firmware translation unit owns the platform hooks.
extern "C" {
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
