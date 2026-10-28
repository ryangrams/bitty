#pragma once
#include "clock_scene.h"
#include "menu_font.h"

namespace flappy {
namespace ClockOverlay {
inline void draw(uint16_t* pixels, int width, int height, const ClockTime& time, ClockDatePosition position) {
  if (!pixels || width < 1 || height < 15 || position == ClockDatePosition::None ||
      uint8_t(position) > uint8_t(ClockDatePosition::Bottom)) return;
  char label[20];
  ClockCalendar::format(label,sizeof(label),time.date,width < 100);
  if (!*label) return;
  const int top = position == ClockDatePosition::Top ? 0 : height - 15;
  for (int y = top; y < top + 15; ++y)
    for (int x = 0; x < width; ++x) pixels[y * width + x] = 0;
  MenuFont::draw(pixels,width,height,label,(width - MenuFont::textWidth(label)) / 2,top + 1,0xce59);
}
}  // namespace ClockOverlay
}  // namespace flappy
