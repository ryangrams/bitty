#pragma once
#include <stdint.h>
#include "inactivity.h"

namespace flappy {

struct BatteryStatus {
  int8_t percent = -1;
  uint16_t millivolts = 0;
  PowerSource source = PowerSource::Unknown;
  bool charging = false;
};

// Voltage estimates follow the pinned M5Unified range, rounded to five percent.
// A failed read stays unknown, including while USB power is present.
inline BatteryStatus batteryStatus(bool valid, int millivolts, PowerSource source,
                                   bool charging = false) {
  BatteryStatus result;
  result.source = source;
  if (!valid || millivolts < 2500 || millivolts > 4500) return result;
  result.millivolts = uint16_t(millivolts);
  int percent = (millivolts - 3300) * 100 / 800;
  percent = percent < 0 ? 0 : percent > 100 ? 100 : percent;
  result.percent = int8_t(((percent + 2) / 5) * 5);
  result.charging = charging && source == PowerSource::External;
  return result;
}

}  // namespace flappy
