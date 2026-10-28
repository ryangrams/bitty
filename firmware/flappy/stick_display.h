#pragma once

// Display profiles and AXP192 light behavior adapted from M5GFX 0.2.28.
// Copyright (c) 2021 M5Stack
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <M5Unified.h>
#if defined(CONFIG_IDF_TARGET_ESP32)
#include <lgfx/v1/Light.hpp>
#include <lgfx/v1/panel/Panel_ST7735.hpp>
#include <lgfx/v1/panel/Panel_ST7789.hpp>
#include <lgfx/v1/platforms/esp32/Bus_SPI.hpp>

namespace flappy {
namespace detail {

class StickAxpLight : public lgfx::ILight {
 public:
  bool init(uint8_t brightness) override {
    healthy_ = lgfx::i2c::init(I2C_NUM_1, 21, 22).has_value();
    if (!healthy_) return false;
    healthy_ = lgfx::i2c::writeRegister8(I2C_NUM_1, 0x34, 0x12, 0x4d, 0xff, Frequency).has_value();
    setBrightness(brightness);
    return healthy_;
  }

  void setBrightness(uint8_t brightness) override {
    bool enabled;
    if (brightness) {
      brightness = uint8_t((((brightness >> 1) + 8) / 13) + 5);
      enabled = lgfx::i2c::bitOn(I2C_NUM_1, 0x34, 0x12, 1 << 2, Frequency).has_value();
    } else {
      enabled = lgfx::i2c::bitOff(I2C_NUM_1, 0x34, 0x12, 1 << 2, Frequency).has_value();
    }
    const bool voltage = lgfx::i2c::writeRegister8(
        I2C_NUM_1, 0x34, 0x28, uint8_t(brightness << 4), 0x0f, Frequency).has_value();
    healthy_ = healthy_ && enabled && voltage;
  }

  bool healthy() const { return healthy_; }

 private:
  static constexpr uint32_t Frequency = 400000;
  bool healthy_ = false;
};

class StickSmallPanel : public lgfx::Panel_ST7735S {
 protected:
  const uint8_t* getInitCommands(uint8_t list) const override {
    static constexpr uint8_t gamma[] = {CMD_GAMMASET, 1, 0x08, 0xff, 0xff};
    return list == 2 ? gamma : lgfx::Panel_ST7735S::getInitCommands(list);
  }
};

// Every panel, bus and light outlives copies of this M5GFX object in M5Unified.
class FixedStickDisplay : public M5GFX {
 public:
  bool configure(uint8_t profile) {
    auto bus = bus_.config();
    bus.spi_host = SPI2_HOST;
    bus.dma_channel = 1;
    bus.pin_mosi = 15;
    bus.pin_miso = 14;
    bus.pin_sclk = 13;
    bus.pin_dc = 23;
    bus.spi_3wire = true;
    bus.freq_write = profile == 1 ? 27000000 : 40000000;
    bus.freq_read = profile == 1 ? 14000000 : 15000000;
    bus_.config(bus);

    lgfx::Panel_Device* panel = profile == 1
        ? static_cast<lgfx::Panel_Device*>(&small_)
        : static_cast<lgfx::Panel_Device*>(&plus_);
    auto cfg = panel->config();
    cfg.invert = true;
    cfg.pin_cs = 5;
    cfg.pin_rst = 18;
    cfg.panel_width = profile == 1 ? 80 : 135;
    cfg.panel_height = profile == 1 ? 160 : 240;
    cfg.offset_x = profile == 1 ? 26 : 52;
    cfg.offset_y = profile == 1 ? 1 : 40;
    cfg.offset_rotation = profile == 1 ? 2 : 0;
    panel->config(cfg);
    panel->setBus(&bus_);
    panel->setLight(&light_);
    setPanel(panel);
    _board = profile == 1 ? m5gfx::board_t::board_M5StickC : m5gfx::board_t::board_M5StickCPlus;
    return init();
  }

 protected:
  bool init_impl(bool reset, bool clear) override {
    const bool initialized = lgfx::LGFX_Device::init_impl(reset, clear);
    return initialized && light_.healthy();
  }

 private:
  lgfx::Bus_SPI bus_;
  StickSmallPanel small_;
  lgfx::Panel_ST7789 plus_;
  StickAxpLight light_;
};

}  // namespace detail
}  // namespace flappy

// Call after LCD rail preparation and before M5.begin. Profile zero keeps
// automatic probing; one selects StickC and two selects StickC Plus.
inline bool configureStickDisplay(uint8_t profile) {
  if (profile == 0) return true;
  if (profile > 2) return false;
  static flappy::detail::FixedStickDisplay display;
  if (!display.configure(profile)) return false;
  M5.Display = display;
  return true;
}
#else
inline bool configureStickDisplay(uint8_t profile) { return profile == 0; }
#endif
