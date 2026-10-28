#include "../firmware/flappy/library_protocol.h"
#include <assert.h>
#include <iostream>
int main() {
  uint32_t number = 123;
  assert(flappy::libraryUnsigned("4294967295", number) && number == UINT32_MAX);
  assert(flappy::libraryUnsigned("ffffffff", number, 16) && number == UINT32_MAX);
  assert(flappy::libraryUnsigned("0000000001", number) && number == 1);
  for (const char* bad : {"", "-1", "+1", " 1", "1x", "4294967296", "9999999999", "00000000000"})
    assert(!flappy::libraryUnsigned(bad, number));
  for (const char* bad : {"100000000", "0xffffffff", "fffffffff", "1g", ""})
    assert(!flappy::libraryUnsigned(bad, number, 16));
  assert(!flappy::libraryUnsigned(nullptr, number));
  assert(!flappy::libraryUnsigned("1", number, 8));
  std::cout << "USB numeric fields reject overflow independently of host integer width\n";
}
