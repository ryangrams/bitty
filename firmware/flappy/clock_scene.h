#pragma once
#include <stdint.h>
#include "clock_calendar.h"

namespace flappy {

// Clock scenes use the physical portrait canvas. Their step runs at 60 Hz.
struct ClockTime {
  uint8_t hour = 12, minute = 0, second = 0;
  bool twentyFourHour = true;
  uint32_t ms = 0;
  ClockDate date;
  uint8_t displayHour() const {
    const uint8_t h = hour % 24;
    return twentyFourHour ? h : h % 12 ? h % 12 : 12;
  }
  uint8_t digit(unsigned i) const {
    const uint8_t h = displayHour();
    return i == 0 ? h / 10 : i == 1 ? h % 10 : i == 2 ? (minute % 60) / 10 : minute % 10;
  }
  uint16_t minuteKey() const { return uint16_t((hour % 24) * 60u + minute % 60); }
};

namespace ClockScene {
enum { Width = 135, Height = 240 };
inline uint8_t digitRow(uint8_t digit, unsigned row) {
  static const uint8_t rows[10][5] = {
    {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
    {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7}
  };
  return digit < 10 && row < 5 ? rows[digit][row] : 0;
}
inline bool digitPixel(uint8_t digit, unsigned x, unsigned y) {
  return x < 3 && (digitRow(digit,y) & (4u >> x)) != 0;
}
inline uint32_t random(uint32_t& state) {
  if (!state) state = 1;
  state ^= state << 13; state ^= state >> 17; state ^= state << 5;
  return state;
}
inline void rect(uint16_t* pixels, int x, int y, int w, int h, uint16_t color) {
  if (!pixels || w <= 0 || h <= 0) return;
  const int right = x + w < Width ? x + w : Width, bottom = y + h < Height ? y + h : Height;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  for (int py = y; py < bottom; ++py)
    for (int px = x; px < right; ++px) pixels[py * Width + px] = color;
}
inline void clear(uint16_t* pixels, uint16_t color) { rect(pixels,0,0,Width,Height,color); }
}  // namespace ClockScene
}  // namespace flappy
