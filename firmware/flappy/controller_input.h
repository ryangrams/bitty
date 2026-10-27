#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace flappy {
namespace controller {

enum Button : uint16_t {
  Up = 1u << 0, Down = 1u << 1, Left = 1u << 2, Right = 1u << 3,
  A = 1u << 4, B = 1u << 5, Start = 1u << 6, Select = 1u << 7,
  Menu = 1u << 8
};

struct Snapshot {
  uint16_t buttons = 0;
  uint16_t pressed = 0;
  uint32_t sequence = 0;
  bool connected = false;
};

inline void updateSnapshot(Snapshot& snapshot, uint16_t buttons, bool connected, bool clearPressed = false) {
  if (!connected || clearPressed) snapshot.pressed = 0;
  if (connected) snapshot.pressed |= uint16_t(buttons & ~snapshot.buttons);
  snapshot.buttons = connected ? buttons : 0;
  snapshot.connected = connected;
  ++snapshot.sequence;
}

inline Snapshot takeSnapshot(Snapshot& snapshot) {
  const Snapshot result = snapshot;
  snapshot.pressed = 0;
  return result;
}

// All limits apply before accepting a descriptor or extracting report bits.
// GATT supplies the report ID separately through the Report Reference descriptor.
class HidReportParser {
 public:
  static constexpr size_t MaxMapBytes = 4096;
  static constexpr size_t MaxReportBytes = 128;
  static constexpr uint8_t MaxReports = 8;
  static constexpr uint8_t MaxFields = 112;

  void clear() {
    fieldCount_ = reportCount_ = 0;
    valid_ = false;
    error_ = "No HID report map";
    for (uint8_t i = 0; i < MaxReports; ++i) reports_[i] = Report();
  }

  bool configure(const uint8_t* bytes, size_t length) {
    clear();
    if (!bytes || !length || length > MaxMapBytes) return fail("HID map size unsupported");
    Global global;
    Global stack[4];
    uint8_t stackSize = 0;
    Local local;
    uint8_t collectionDepth = 0;
    for (size_t cursor = 0; cursor < length;) {
      const uint8_t prefix = bytes[cursor++];
      if (prefix == 0xfe) {
        if (length - cursor < 2) return fail("Truncated HID item");
        const size_t count = bytes[cursor];
        cursor += 2;
        if (count > length - cursor) return fail("Truncated HID item");
        cursor += count;
        continue;
      }
      const size_t sizeCode = prefix & 3;
      const size_t size = sizeCode == 3 ? 4 : sizeCode;
      if (size > length - cursor) return fail("Truncated HID item");
      uint32_t value = 0;
      for (size_t i = 0; i < size; ++i) value |= uint32_t(bytes[cursor++]) << (8 * i);
      const uint8_t type = (prefix >> 2) & 3;
      const uint8_t tag = prefix >> 4;
      if (type == 1) {
        switch (tag) {
          case 0: if (value > 0xffff) return fail("HID usage page unsupported"); global.page = uint16_t(value); break;
          case 1: global.minimum = sign(value, uint8_t(size * 8)); break;
          case 2: global.maximum = global.minimum < 0 ? sign(value, uint8_t(size * 8)) : int64_t(value); break;
          case 7: if (value > 32) return fail("HID field size unsupported"); global.size = uint8_t(value); break;
          case 8: if (value == 0 || value > 255) return fail("Invalid HID report ID"); global.id = uint8_t(value); break;
          case 9: if (value > MaxReportBytes * 8) return fail("HID report too large"); global.count = uint16_t(value); break;
          case 10: if (stackSize == 4) return fail("HID global stack full"); stack[stackSize++] = global; break;
          case 11: if (!stackSize) return fail("Invalid HID global stack"); global = stack[--stackSize]; break;
          default: break;
        }
      } else if (type == 2) {
        switch (tag) {
          case 0:
            if (local.count == Local::MaxUsages) return fail("Too many HID usages");
            local.usages[local.count++] = usage(value, size, global.page);
            break;
          case 1: local.minimum = usage(value, size, global.page); local.haveMinimum = true; break;
          case 2: local.maximum = usage(value, size, global.page); local.haveMaximum = true; break;
          case 10: if (value) return fail("HID delimiter unsupported"); break;
          default: break;
        }
      } else if (type == 0) {
        if (tag == 8 && !addInput(global, local, value)) return false;
        if (tag == 10 && ++collectionDepth > 16) return fail("HID collection too deep");
        if (tag == 12) {
          if (!collectionDepth) return fail("Invalid HID collection");
          --collectionDepth;
        }
        local = Local();
      }
    }
    if (stackSize || collectionDepth) return fail("Unclosed HID collection");
    if (!fieldCount_) return fail("No supported HID inputs");
    // Mixing the implicit report zero with explicit IDs is ambiguous in HID.
    if (reportCount_ > 1) {
      for (uint8_t i = 0; i < reportCount_; ++i) {
        if (reports_[i].id == 0) return fail("Mixed HID report IDs");
      }
    }
    valid_ = true;
    error_ = "HID map ready";
    return true;
  }

  bool configureBootKeyboard() {
    static const uint8_t descriptor[] = {
      0x05,0x01,0x09,0x06,0xa1,0x01,0x05,0x07,0x19,0xe0,0x29,0xe7,
      0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x08,0x81,0x02,
      0x75,0x08,0x95,0x01,0x81,0x01,0x19,0x00,0x29,0xff,
      0x15,0x00,0x26,0xff,0x00,0x75,0x08,0x95,0x06,0x81,0x00,0xc0
    };
    return configure(descriptor, sizeof(descriptor));
  }

  bool decode(uint8_t reportId, const uint8_t* bytes, size_t length, uint16_t& buttons) const {
    buttons = 0;
    if (!valid_ || !bytes || length > MaxReportBytes) return false;
    const Report* report = findReport(reportId);
    if (!report || !report->supported || length < size_t((report->bits + 7) / 8)) return false;
    for (uint8_t i = 0; i < fieldCount_; ++i) {
      const Field& field = fields_[i];
      if (field.id != reportId) continue;
      for (uint16_t n = 0; n < field.count; ++n) {
        const uint32_t raw = extract(bytes, uint16_t(field.offset + n * field.size), field.size);
        const int64_t value = field.minimum < 0 ? sign(raw, field.size) : int64_t(raw);
        if (field.kind == Kind::KeyboardArray || field.kind == Kind::ButtonArray) {
          if (value < field.minimum || value > field.maximum) continue;
          const int64_t selected = int64_t(field.usage) + value - field.minimum;
          if (selected < field.usage || selected > field.usageMaximum) continue;
          if (field.kind == Kind::KeyboardArray) {
            if (selected >= 1 && selected <= 3) { buttons = 0; return false; }
            buttons |= keyboard(uint32_t(selected));
          } else buttons |= gamepadButton(uint16_t(selected));
        } else if (field.kind == Kind::Keyboard) {
          if (raw) buttons |= keyboard(field.usage);
        } else if (field.kind == Kind::Button) {
          if (raw) buttons |= gamepadButton(field.usage);
        } else if (field.kind == Kind::Direction) {
          if (raw) buttons |= direction(field.usage);
        } else if (field.kind == Kind::Hat) {
          if (value < field.minimum || value > field.maximum) continue;
          int64_t hat = value - field.minimum;
          if (field.maximum - field.minimum == 3) hat *= 2;
          if (hat > 7) continue;
          if (hat == 7 || hat == 0 || hat == 1) buttons |= Up;
          if (hat >= 1 && hat <= 3) buttons |= Right;
          if (hat >= 3 && hat <= 5) buttons |= Down;
          if (hat >= 5 && hat <= 7) buttons |= Left;
        } else {
          const int64_t span = field.maximum - field.minimum;
          if (span <= 0 || value < field.minimum || value > field.maximum) continue;
          const int64_t low = field.minimum + span / 4;
          const int64_t high = field.maximum - span / 4;
          if (value < low) buttons |= field.kind == Kind::AxisX ? Left : Up;
          if (value > high) buttons |= field.kind == Kind::AxisX ? Right : Down;
        }
      }
    }
    return true;
  }

  bool valid() const { return valid_; }
  const char* error() const { return error_; }
  uint8_t reportCount() const { return reportCount_; }
  uint8_t reportIdAt(uint8_t index) const { return index < reportCount_ ? reports_[index].id : 0; }
  bool supportsReport(uint8_t id) const { const Report* r = findReport(id); return valid_ && r && r->supported; }
  size_t expectedLength(uint8_t id) const { const Report* r = findReport(id); return r ? (r->bits + 7) / 8 : 0; }

 private:
  enum class Kind : uint8_t { Keyboard, KeyboardArray, Button, ButtonArray, Direction, Hat, AxisX, AxisY };
  struct Global {
    int64_t minimum = 0, maximum = 0;
    uint16_t page = 0, count = 0;
    uint8_t size = 0, id = 0;
  };
  struct Local {
    static constexpr uint8_t MaxUsages = 32;
    uint32_t usages[MaxUsages] = {};
    uint32_t minimum = 0, maximum = 0;
    uint8_t count = 0;
    bool haveMinimum = false, haveMaximum = false;
    uint32_t at(uint16_t index) const {
      if (index < count) return usages[index];
      const uint16_t rangeIndex = uint16_t(index - count);
      if (haveMinimum && haveMaximum && maximum >= minimum && uint64_t(minimum) + rangeIndex < uint64_t(maximum) + 1)
        return minimum + rangeIndex;
      return count ? usages[count - 1] : 0;
    }
  };
  struct Report { uint16_t bits = 0; uint8_t id = 0; bool supported = false; };
  struct Field {
    int64_t minimum = 0, maximum = 0;
    uint16_t offset = 0, count = 1, usage = 0, usageMaximum = 0;
    uint8_t size = 0, id = 0;
    Kind kind = Kind::Button;
  };
  Field fields_[MaxFields];
  Report reports_[MaxReports];
  uint8_t fieldCount_ = 0, reportCount_ = 0;
  bool valid_ = false;
  const char* error_ = "No HID report map";

  bool fail(const char* text) { valid_ = false; error_ = text; return false; }
  static int64_t sign(uint32_t value, uint8_t bits) {
    if (!bits) return 0;
    if (bits == 32) return int64_t(int32_t(value));
    return (value & (uint32_t(1) << (bits - 1))) ? int64_t(value) - (int64_t(1) << bits) : int64_t(value);
  }
  static uint32_t usage(uint32_t value, size_t size, uint16_t page) {
    return size == 4 ? value : (uint32_t(page) << 16) | value;
  }
  const Report* findReport(uint8_t id) const {
    for (uint8_t i = 0; i < reportCount_; ++i) if (reports_[i].id == id) return &reports_[i];
    return nullptr;
  }
  bool addInput(const Global& global, const Local& local, uint32_t flags) {
    if (!global.count || !global.size) return fail("Empty HID input field");
    Report* report = nullptr;
    for (uint8_t i = 0; i < reportCount_; ++i) if (reports_[i].id == global.id) report = &reports_[i];
    if (!report) {
      if (reportCount_ == MaxReports) return fail("Too many HID reports");
      report = &reports_[reportCount_++];
      report->id = global.id;
    }
    const uint32_t bits = uint32_t(global.size) * global.count;
    if (uint32_t(report->bits) + bits > MaxReportBytes * 8) return fail("HID report too large");
    const uint16_t offset = report->bits;
    report->bits = uint16_t(report->bits + bits);
    if (flags & 1) return true;
    if (!(flags & 2)) {
      const uint32_t firstUsage = local.at(0);
      const uint16_t page = uint16_t(firstUsage >> 16);
      if (page != 7 && page != 9) return true;
      Field field;
      field.id = global.id; field.offset = offset; field.size = global.size;
      field.count = global.count; field.kind = page == 7 ? Kind::KeyboardArray : Kind::ButtonArray;
      field.minimum = global.minimum; field.maximum = global.maximum;
      field.usage = uint16_t(firstUsage);
      if (local.haveMinimum && local.haveMaximum && !local.count) {
        if (local.maximum < local.minimum || (local.maximum >> 16) != page) return fail("Invalid HID usage range");
        field.usageMaximum = uint16_t(local.maximum);
      } else if (local.count) {
        for (uint8_t i = 0; i < local.count; ++i)
          if (local.usages[i] != firstUsage + i) return fail("Sparse HID array unsupported");
        if ((local.usages[local.count - 1] >> 16) != page) return fail("Invalid HID usage range");
        field.usageMaximum = uint16_t(local.usages[local.count - 1]);
      } else return fail("HID array has no usages");
      return append(field, *report);
    }
    for (uint16_t i = 0; i < global.count; ++i) {
      const uint32_t fullUsage = local.at(i);
      const uint16_t page = uint16_t(fullUsage >> 16);
      const uint16_t code = uint16_t(fullUsage);
      Field field;
      if (page == 7 && keyboard(code)) field.kind = Kind::Keyboard;
      else if (page == 9 && gamepadButton(code)) field.kind = Kind::Button;
      else if (page == 1 && direction(code)) field.kind = Kind::Direction;
      else if (page == 1 && code == 0x39) field.kind = Kind::Hat;
      else if (page == 1 && code == 0x30 && !(flags & 4)) field.kind = Kind::AxisX;
      else if (page == 1 && code == 0x31 && !(flags & 4)) field.kind = Kind::AxisY;
      else continue;
      field.id = global.id; field.offset = uint16_t(offset + i * global.size);
      field.size = global.size; field.usage = code;
      field.minimum = global.minimum; field.maximum = global.maximum;
      if (!append(field, *report)) return false;
    }
    return true;
  }
  bool append(const Field& field, Report& report) {
    if (fieldCount_ == MaxFields) return fail("Too many HID input fields");
    fields_[fieldCount_++] = field;
    report.supported = true;
    return true;
  }
  static uint32_t extract(const uint8_t* bytes, uint16_t offset, uint8_t count) {
    uint32_t value = 0;
    for (uint8_t bit = 0; bit < count; ++bit)
      value |= uint32_t((bytes[(offset + bit) / 8] >> ((offset + bit) & 7)) & 1) << bit;
    return value;
  }
  static uint16_t keyboard(uint32_t usage) {
    switch (usage) {
      case 0x04: return A;
      case 0x05: return B;
      case 0x28: return Start;
      case 0x29: return Menu;
      case 0x2a: return Select;
      case 0x4f: return Right;
      case 0x50: return Left;
      case 0x51: return Down;
      case 0x52: return Up;
      default: return 0;
    }
  }
  static uint16_t gamepadButton(uint16_t usage) {
    switch (usage) {
      case 1: return A;
      case 2: return B;
      case 9: return Select;
      case 10: return Start;
      default: return 0;
    }
  }
  static uint16_t direction(uint16_t usage) {
    switch (usage) {
      case 0x90: return Up;
      case 0x91: return Down;
      case 0x92: return Right;
      case 0x93: return Left;
      default: return 0;
    }
  }
};

}  // namespace controller
}  // namespace flappy
