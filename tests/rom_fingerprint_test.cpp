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
  std::vector<uint8_t> gym(49168, 0), zero(81936, 0);
  assert(!tetris(gym.data(), gym.size(), false));
  assert(!tetris(zero.data(), zero.size(), true));
  assert(!tetris(gym.data(), gym.size(), true));
  assert(!tetris(zero.data(), zero.size(), false));
}
