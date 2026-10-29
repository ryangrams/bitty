#pragma once
#include <stdint.h>
#include <stdio.h>
#include "battery_status.h"
#include "menu_font.h"
#include "bitty_logo.h"

namespace flappy {
namespace BittyUi {

constexpr uint16_t color(unsigned r, unsigned g, unsigned b) {
  return uint16_t(((r & 248) << 8) | ((g & 252) << 3) | (b >> 3));
}
constexpr uint16_t Black = color(16,17,22), Panel = color(26,27,34);
constexpr uint16_t Rule = color(46,47,56), Violet = color(109,75,245);
constexpr uint16_t Glow = color(180,160,255), Amber = color(232,163,61);
constexpr uint16_t Text = color(240,240,243), Muted = color(185,188,196);
constexpr uint16_t Low = color(255,107,44);
constexpr uint32_t SplashMs = 2400;

inline void rect(uint16_t* output, int width, int height, int x, int y,
                 int w, int h, uint16_t ink) {
  if (!output || width <= 0 || height <= 0) return;
  const int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
  const int right = x + w < width ? x + w : width;
  const int bottom = y + h < height ? y + h : height;
  for (int row = top; row < bottom; ++row)
    for (int col = left; col < right; ++col) output[row * width + col] = ink;
}

// The website mark has a continuous four-row stem and an open upper bowl.
inline void mark(uint16_t* output, int width, int height, int x, int y,
                 int cell, int gap, uint32_t elapsed = SplashMs, bool animate = false) {
  const uint8_t xs[] = {0,1,2,1,2}, ys[] = {0,1,1,3,2}, spans[] = {4,1,1,1,2};
  for (unsigned i = 0; i < 5; ++i) {
    const uint32_t start = i * 55;
    if (animate && elapsed < start) continue;
    const uint32_t age = animate ? elapsed - start : 220;
    const int remaining = age < 220 ? int(220 - age) : 0;
    const int displacement = remaining * remaining / 1600;
    const int dx = animate && i % 2 ? displacement : 0;
    const int dy = animate && !(i % 2) ? -displacement : 0;
    rect(output,width,height,x + xs[i] * (cell + gap) + dx,
         y + ys[i] * (cell + gap) + dy,cell,spans[i] * cell + (spans[i] - 1) * gap,Glow);
  }
}

inline void bitmap(uint16_t* output, int width, int height, int x, int y,
                   const char* pixels, int w, int h, int reveal = 32767, int columns = 32767) {
  if (!output) return;
  for (int row = 0; row < h; ++row) for (int col = 0; col < w && col < reveal && col < columns; ++col) {
    const int dx = x + col, dy = y + row;
    if (dx >= 0 && dy >= 0 && dx < width && dy < height) {
      const char pixel = pixels[row * w + col];
      const unsigned index = pixel <= '9' ? unsigned(pixel - '0') : unsigned(pixel - 'a' + 10);
      if (index < 15) output[dy * width + dx] = BittyLogo::Palette[index];
    }
  }
}

inline void header(uint16_t* output, int width, int height, const BatteryStatus& battery) {
  rect(output,width,height,4,10,width - 8,21,Black);
  mark(output,width,height,10,12,3,1);
  bitmap(output,width,height,26,13,BittyLogo::HeaderPixels,BittyLogo::HeaderWidth,BittyLogo::HeaderHeight);
  const int x = width - 60;
  if (battery.source == PowerSource::External) {
    // A plug means external power. It does not claim that charging is active.
    rect(output,width,height,x - 10,15,6,5,Glow);
    rect(output,width,height,x - 9,13,1,2,Glow);
    rect(output,width,height,x - 6,13,1,2,Glow);
    rect(output,width,height,x - 8,20,2,3,Glow);
  }
  const uint16_t ink = battery.percent >= 0 && battery.percent <= 15 ? Low : Muted;
  rect(output,width,height,x,14,18,10,ink);
  rect(output,width,height,x + 18,17,2,4,ink);
  rect(output,width,height,x + 1,15,16,8,Black);
  if (battery.percent >= 0)
    rect(output,width,height,x + 2,16,14 * battery.percent / 100,6,
         battery.charging ? Amber : ink);
  else MenuFont::draw(output,width,height,"?",x + 6,12,Muted);
  char label[5];
  if (battery.percent < 0) snprintf(label,sizeof(label),"--%%");
  else snprintf(label,sizeof(label),"%u%%",unsigned(battery.percent));
  MenuFont::draw(output,width,height,label,width - 10 - MenuFont::textWidth(label),12,Text);
  rect(output,width,height,8,29,width - 16,1,Rule);
}

inline void splash(uint16_t* output, int width, int height, uint32_t elapsed) {
  rect(output,width,height,0,0,width,height,Black);
  const int cell=width>=128 ? 20 : 14, gap=2;
  const int top=height>=224 ? 24 : 12, wordY=top + cell*4 + gap*3 + 16;
  const int wordWidth=68;
  mark(output,width,height,(width-(cell*3+gap*2))/2,top,cell,gap,elapsed,true);
  const int reveal=elapsed<280 ? 0 : int((elapsed-280)*wordWidth/300);
  bitmap(output,width,height,(width-wordWidth)/2,wordY,BittyLogo::SplashPixels,
      BittyLogo::SplashWidth,BittyLogo::SplashHeight,reveal,wordWidth);
  MenuFont::draw(output,width,height,".games",(width-MenuFont::textWidth(".games"))/2,wordY+37,Muted);
  for (int i = 0; i < 5; ++i)
    rect(output,width,height,width / 2 - 18 + i * 8,height-(height>=224 ? 30 : 9),4,3,
         elapsed >= uint32_t((i + 1) * 150) ? Violet : Rule);
  if(width==144 && height==256) {
    rect(output,width,height,0,0,width,8,Black);rect(output,width,height,0,248,width,8,Black);
    rect(output,width,height,0,0,4,height,Black);rect(output,width,height,139,0,5,height,Black);
  }
}

}  // namespace BittyUi
}  // namespace flappy
