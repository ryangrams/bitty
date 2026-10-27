#pragma once
#include <stdint.h>

namespace flappy {
// The NES picture uses its conventional 4:3 display aspect.
struct NesViewport {
  int width = 0, height = 0, x = 0, y = 0;
  void configure(int screenWidth, int screenHeight) {
    width = screenWidth;
    height = width * 3 / 4;
    if (height > screenHeight) { height = screenHeight; width = height * 4 / 3; }
    x = (screenWidth - width) / 2;
    y = (screenHeight - height) / 2;
  }
  int sourceX(int column) const { return column * 256 / width; }
  int sourceY(int row) const { return row * 240 / height; }
  int firstRow(int sourceRow) const { return (sourceRow * height + 239) / 240; }
};
}  // namespace flappy
