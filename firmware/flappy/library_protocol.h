#pragma once
#include <stdint.h>
#include "clock_calendar.h"
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
struct LibraryClock {
  ClockDate date;
  uint8_t hour = 0, minute = 0, second = 0;
};
inline bool libraryClock(char* const* fields, unsigned count, LibraryClock& clock) {
  if (count != 6) return false;
  uint32_t values[6];
  for (unsigned i = 0; i < 6; ++i) if (!libraryUnsigned(fields[i], values[i])) return false;
  if (values[0] < 2000 || values[0] > 2099 || values[1] > 12 || values[2] > 31 ||
      values[3] >= 24 || values[4] >= 60 || values[5] >= 60) return false;
  LibraryClock parsed;
  parsed.date.year = uint16_t(values[0]); parsed.date.month = uint8_t(values[1]); parsed.date.day = uint8_t(values[2]);
  if (!ClockCalendar::valid(parsed.date)) return false;
  parsed.hour = uint8_t(values[3]); parsed.minute = uint8_t(values[4]); parsed.second = uint8_t(values[5]);
  clock = parsed;
  return true;
}
} // namespace flappy
