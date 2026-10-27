#include "../firmware/flappy/rom_fingerprint.h"
#include <assert.h>
#include <vector>

int main() {
  using namespace flappy::RomFingerprint;
  const uint8_t check[] = {'1','2','3','4','5','6','7','8','9'};
  assert(crc32(check, sizeof(check)) == 0xcbf43926u);
  assert(crc32(nullptr, 0) == 0);
  assert(!tetris(nullptr, 49168, false));
  assert(!tetris(check, sizeof(check), false));
  assert(!tetris(check, sizeof(check), true));
  assert(!famidash(nullptr, 786448));
  assert(!famidash(check, sizeof(check)));
  std::vector<uint8_t> geometry(786448, 0);
  assert(!famidash(geometry.data(), geometry.size()));
  std::vector<uint8_t> gym(49168, 0), zero(81936, 0);
  assert(!tetris(gym.data(), gym.size(), false));
  assert(!tetris(zero.data(), zero.size(), true));
  assert(!tetris(gym.data(), gym.size(), true));
  assert(!tetris(zero.data(), zero.size(), false));
}
