#include "../firmware/flappy/menu_font.h"
#include <assert.h>
#include <string.h>
#include <vector>

int main() {
  using namespace flappy::MenuFont;
  assert(Width == 6 && Height == 12 && Advance == 6);
  uint32_t hash = 2166136261u;
  for (int c = 32; c <= 126; ++c) {
    const uint8_t* rows = glyph(char(c));
    unsigned ink = 0;
    for (int y = 0; y < Height; ++y) {
      assert(rows[y] < (1 << Width));
      hash = (hash ^ rows[y]) * 16777619u;
      ink |= rows[y];
    }
    assert((ink != 0) == (c != ' '));
  }
  // Hash of the unchanged native ASCII cells from the verified source BDF.
  assert(hash == 0xa8747a48u);
  assert(memcmp(glyph('a'), glyph('A'), Height) != 0);
  assert(memcmp(glyph(char(0xff)), glyph('?'), Height) == 0);
  for (char c : {'I', 'H'}) {
    for (int y : {3, 4, 6, 7, 8})
      assert(glyph(c)[y] == (c == 'I' ? 8 : 34));
  }
  assert(textWidth(nullptr, 2) == 0 && textWidth("", 2) == 0);
  assert(textWidth("Game Settings") == 78);
  assert(textWidth("Game Settings", 2) == 156);
  assert(textWidth("Game Settings", 1, 4) == 24);
  assert(textWidth("RIGHT NEXT  M5 SELECT") == 126);
  assert(textWidth("HOLD RIGHT TO EXIT") <= 128);
  assert(textWidth("123456789012345678901") == 126);
  assert(textWidth("1234567890123456789012") > 128);
  const char pairs[][2] = {{'M','N'}, {'0','O'}, {'V','W'}, {'B','8'}, {'I','1'}, {'l','I'}};
  for (const auto& pair : pairs) assert(memcmp(glyph(pair[0]), glyph(pair[1]), Height) != 0);

  for (int scale : {1, 2}) {
    const int width = 48, height = 32;
    std::vector<uint16_t> output(width * height + 8, 0xa55a);
    draw(output.data() + 4, width, height, "Ha", 3, 2, 0xffff, scale);
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
      const int localX = x - 3, localY = y - 2;
      bool filled = false;
      if (localX >= 0 && localX < 2 * Advance * scale && localY >= 0 && localY < Height * scale) {
        const int index = localX / (Advance * scale);
        const int col = localX / scale % Advance;
        filled = (glyph("Ha"[index])[localY / scale] & (1u << (Width - 1 - col))) != 0;
      }
      assert(output[4 + y * width + x] == (filled ? 0xffff : 0xa55a));
    }
    for (int i = 0; i < 4; ++i) assert(output[i] == 0xa55a && output[output.size() - i - 1] == 0xa55a);
    draw(output.data() + 4, width, height, "Clipped", -4, -4, 0xffff, scale);
    draw(output.data() + 4, width, height, "Clipped", 47, 31, 0xffff, scale);
    for (int i = 0; i < 4; ++i) assert(output[i] == 0xa55a && output[output.size() - i - 1] == 0xa55a);
  }
  uint16_t unchanged = 0x1234;
  draw(nullptr, 1, 1, "A", 0, 0, 0xffff);
  draw(&unchanged, 1, 1, "A", 0, 0, 0xffff, 0);
  draw(&unchanged, 1, 1, "A", 0, 0, 0xffff, 3);
  assert(unchanged == 0x1234);
}
