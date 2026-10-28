#pragma once
#include <stdint.h>
namespace flappy {
inline bool libraryUnsigned(const char* text, uint32_t& value, unsigned base = 10) {
  if (!text || !*text || (base != 10 && base != 16)) return false;
  uint32_t result = 0;
  for (unsigned i = 0; text[i]; ++i) {
    if (i >= 10) return false;
    const char c = text[i];
    const unsigned digit = c >= '0' && c <= '9' ? unsigned(c - '0') :
        c >= 'a' && c <= 'f' ? unsigned(c - 'a' + 10) : c >= 'A' && c <= 'F' ? unsigned(c - 'A' + 10) : 16;
    if (digit >= base || result > (UINT32_MAX - digit) / base) return false;
    result = result * base + digit;
  }
  value = result; return true;
}
} // namespace flappy
