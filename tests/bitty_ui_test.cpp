#include "../firmware/flappy/bitty_ui.h"
#include <assert.h>
#include <vector>

using namespace flappy;

int main() {
  for (int mv : {-1,0,2499,4501,5000}) assert(batteryStatus(true,mv,PowerSource::External,true).percent == -1);
  assert(batteryStatus(false,4100,PowerSource::External).percent == -1);
  assert(batteryStatus(true,2500,PowerSource::Battery).percent == 0);
  assert(batteryStatus(true,3300,PowerSource::Battery).percent == 0);
  assert(batteryStatus(true,3700,PowerSource::Battery).percent == 50);
  assert(batteryStatus(true,4100,PowerSource::Battery).percent == 100);
  assert(batteryStatus(true,4200,PowerSource::External,true).charging);
  assert(!batteryStatus(true,4200,PowerSource::Battery,true).charging);
  assert(!batteryStatus(false,4200,PowerSource::External,true).charging);
  int previous = -1;
  for (int mv = 2500; mv <= 4500; ++mv) {
    const auto state = batteryStatus(true,mv,PowerSource::Battery);
    assert(state.percent >= previous && state.percent <= 100 && state.percent % 5 == 0);
    previous = state.percent;
  }

  const int width = 144, height = 256, size = width * height;
  std::vector<uint16_t> storage(size + 16,0x5aa5);
  auto* pixels = storage.data() + 8;
  const auto guards = [&]() {
    for (int i = 0; i < 8; ++i) assert(storage[i] == 0x5aa5 && storage[size + 8 + i] == 0x5aa5);
  };
  const auto nativeCrop = [&]() {
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
      if (x < 4 || x >= 139 || y < 8 || y >= 248) assert(pixels[y * width + x] == BittyUi::Black);
  };
  for (int percent : {-1,0,5,15,50,95,100}) for (auto source : {PowerSource::Unknown,PowerSource::Battery,PowerSource::External}) {
    BittyUi::rect(pixels,width,height,0,0,width,height,BittyUi::Black);
    BatteryStatus state; state.percent = int8_t(percent); state.source = source;
    state.charging = source == PowerSource::External;
    BittyUi::header(pixels,width,height,state);
    guards(); nativeCrop();
    assert(pixels[14 * width + 10] == BittyUi::Glow);
  }
  // Four-row website mark: continuous left stem, open bowl, violet lower corners.
  BittyUi::rect(pixels,width,height,0,0,width,height,BittyUi::Black);
  BittyUi::mark(pixels,width,height,20,40,6,1);
  for (int y = 40; y < 67; ++y) assert(pixels[y*width+20] == BittyUi::Glow);
  assert(pixels[40*width+27] == BittyUi::Black);
  assert(pixels[47*width+27] == BittyUi::Glow);
  assert(pixels[54*width+27] == BittyUi::Black);
  assert(pixels[61*width+27] == BittyUi::Glow);
  assert(pixels[54*width+34] == BittyUi::Glow);
  std::vector<uint16_t> finished;
  assert(BittyUi::SplashMs == 2400);
  for (unsigned elapsed = 0; elapsed <= BittyUi::SplashMs + 100; elapsed += 10) {
    BittyUi::splash(pixels,width,height,elapsed);
    guards(); nativeCrop();
    if (elapsed == 900) finished.assign(pixels,pixels + size);
    if (elapsed > 900) assert(finished == std::vector<uint16_t>(pixels,pixels + size));
  }
  // The smaller domain suffix sits under the centered native wordmark.
  const char* url = ".games";
  const int left = (width - MenuFont::textWidth(url)) / 2, top = 24 + 20*4 + 2*3 + 16 + 37;
  for (int i = 0; url[i]; ++i) for (int y = 0; y < MenuFont::Height; ++y)
    for (int x = 0; x < MenuFont::Width; ++x)
      if (MenuFont::glyph(url[i])[y] & (1u << (MenuFont::Width - 1 - x)))
        assert(pixels[(top + y) * width + left + i * MenuFont::Advance + x] == BittyUi::Muted);
  BittyUi::splash(nullptr,width,height,900);
  BittyUi::header(nullptr,width,height,BatteryStatus());
  BittyUi::rect(pixels,width,height,-20,-30,40,50,BittyUi::Violet);
  guards();
}
