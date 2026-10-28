#pragma once
#include <stdint.h>
#include <stdio.h>

namespace flappy {

enum class ClockDatePosition : uint8_t { None, Top, Bottom };

struct ClockDate {
  uint16_t year = 2000;
  uint8_t month = 1, day = 1;
  bool operator==(const ClockDate& other) const {
    return year == other.year && month == other.month && day == other.day;
  }
  bool operator!=(const ClockDate& other) const { return !(*this == other); }
};

namespace ClockCalendar {
inline bool leapYear(unsigned year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }
inline uint8_t monthDays(unsigned year, unsigned month) {
  static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  return month >= 1 && month <= 12 ? uint8_t(days[month - 1] + (month == 2 && leapYear(year))) : 0;
}
inline bool valid(const ClockDate& date) {
  return date.year >= 2000 && date.year <= 9999 && date.month >= 1 && date.month <= 12 &&
         date.day >= 1 && date.day <= monthDays(date.year,date.month);
}
// Encode the device's local civil time without consulting a host timezone.
inline bool clockSeconds(const ClockDate& date, unsigned hour, unsigned minute, unsigned second, uint32_t& value) {
  if (!valid(date) || date.year > 2099 || hour >= 24 || minute >= 60 || second >= 60) return false;
  uint32_t days = 0;
  for (unsigned year = 1970; year < date.year; ++year) days += leapYear(year) ? 366u : 365u;
  for (unsigned month = 1; month < date.month; ++month) days += monthDays(date.year,month);
  days += date.day - 1;
  value = days * 86400u + hour * 3600u + minute * 60u + second;
  return true;
}
inline ClockDate addDays(ClockDate date, unsigned days) {
  while (days) {
    const unsigned remaining = unsigned(monthDays(date.year,date.month)) - date.day;
    if (days <= remaining) { date.day = uint8_t(date.day + days); break; }
    days -= remaining + 1;
    date.day = 1;
    if (++date.month > 12) { date.month = 1; ++date.year; }
  }
  return date;
}
inline uint8_t weekDay(const ClockDate& date) {
  static const uint8_t offsets[] = {0,3,2,5,0,3,5,1,4,6,2,4};
  if (!valid(date)) return 0;
  const unsigned year = date.year - (date.month < 3);
  return uint8_t((year + year / 4 - year / 100 + year / 400 + offsets[date.month - 1] + date.day) % 7);
}
inline const char* dayName(const ClockDate& date) {
  static const char* const names[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
  return names[weekDay(date)];
}
inline const char* monthName(unsigned month) {
  static const char* const names[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
  return month >= 1 && month <= 12 ? names[month - 1] : "";
}
inline void format(char* output, unsigned capacity, const ClockDate& date, bool compact = false) {
  if (!output || !capacity) return;
  if (!valid(date)) { output[0] = 0; return; }
  if (compact) snprintf(output,capacity,"%s %02u/%02u",dayName(date),unsigned(date.month),unsigned(date.day));
  else snprintf(output,capacity,"%s %s %02u %04u",dayName(date),monthName(date.month),unsigned(date.day),unsigned(date.year));
}
}  // namespace ClockCalendar
}  // namespace flappy
