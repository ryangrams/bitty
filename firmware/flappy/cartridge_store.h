#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace flappy {

inline uint32_t cartridgeCrc(const uint8_t* bytes, size_t length, uint32_t state = 0xffffffffu) {
  while (length--) {
    state ^= *bytes++;
    for (unsigned bit = 0; bit < 8; ++bit)
      state = (state >> 1) ^ (0xedb88320u & (0u - (state & 1u)));
  }
  return state;
}

// Flash implements size(), read(offset,bytes,length), write() and erase().
// Writes/erases return false on failure. Payloads become visible at commit only.
template<class Flash>
class CartridgeStore {
 public:
  enum { Page = 4096, MaxRecords = 12, DirectoryBytes = 1024, Chunk = 256 };
  struct Record {
    uint8_t id = 0, kind = 0; // 1 NES, 2 Tetris GYM, 3 Tetris Zero, 4 Geometry.
    char title[33] = {};
    uint32_t offset = 0, length = 0, crc = 0;
  };
  explicit CartridgeStore(Flash& flash) : flash_(flash) {}
  bool mount() {
    ready_ = pending_ = false;
    Record a[MaxRecords] = {}, b[MaxRecords] = {};
    uint32_t ga = 0, gb = 0;
    const bool va = loadDirectory(0, a, ga), vb = loadDirectory(1, b, gb);
    if (!va && !vb) {
      uint8_t bytes[128];
      if (flash_.size() < Page * 3u) return false;
      // Never automatically format a partition that contains an unknown format.
      for (unsigned pos = 0; pos < Page * 2u; pos += sizeof(bytes)) {
        if (!flash_.read(pos, bytes, sizeof(bytes))) return false;
        for (uint8_t value : bytes) if (value != 255) return false;
      }
      for (Record& record : records_) record = Record{};
      generation_ = 0; directory_ = 1;
      ready_ = true;
      if (!saveDirectory(records_)) { ready_ = false; return false; }
      return true;
    }
    directory_ = vb && (!va || int32_t(gb - ga) > 0) ? 1 : 0;
    generation_ = directory_ ? gb : ga;
    memcpy(records_, directory_ ? b : a, sizeof(records_));
    ready_ = true;
    return true;
  }
  bool ready() const { return ready_; }
  bool pending() const { return pending_; }
  const Record* incoming() const { return pending_ ? &incoming_ : nullptr; }
  uint32_t received() const { return received_; }
  uint32_t capacity() const { return ready_ ? (flash_.size() / Page - 2) * Page : 0; }
  const Record* find(uint8_t id) const {
    for (const Record& record : records_) if (record.id == id && id) return &record;
    return nullptr;
  }
  const Record& at(unsigned index) const { return records_[index < MaxRecords ? index : 0]; }
  uint32_t used() const {
    uint32_t bytes = 0;
    for (const Record& record : records_) if (record.id) bytes += pages(record.length) * Page;
    return bytes;
  }
  uint32_t largestFree() const {
    uint32_t run = 0, best = 0;
    for (uint32_t page = 2; page < flash_.size() / Page; ++page) {
      if (occupied(page)) run = 0;
      else { run += Page; if (run > best) best = run; }
    }
    return best;
  }
  bool begin(uint8_t id, uint8_t kind, const char* title, uint32_t length, uint32_t crc) {
    if (!ready_ || pending_ || !id || kind < 1 || kind > 4 || length < 16 || length > capacity() || !title) return false;
    unsigned titleLength = 0;
    while (titleLength <= 32 && title[titleLength]) {
      const uint8_t c = title[titleLength++];
      if (c < 32 || c > 126) return false;
    }
    if (!titleLength || titleLength > 32) return false;
    unsigned slot = MaxRecords;
    for (unsigned i = 0; i < MaxRecords; ++i) {
      if (records_[i].id == id) { slot = i; break; }
      if (!records_[i].id) slot = i;
    }
    if (slot == MaxRecords) return false;
    uint32_t run = 0, first = 0;
    for (uint32_t page = 2; page < flash_.size() / Page; ++page) {
      if (occupied(page)) run = 0;
      else if (++run >= pages(length)) { first = page + 1 - run; break; }
    }
    if (!first) return false;
    Record next;
    next.id = id; next.kind = kind; next.length = length; next.crc = crc; next.offset = first * Page;
    memcpy(next.title, title, titleLength);
    // Existing records stay allocated throughout replacement.
    if (!flash_.erase(next.offset, pages(length) * Page)) return false;
    incoming_ = next; slot_ = slot; received_ = 0; checksum_ = 0xffffffffu;
    pending_ = true;
    return true;
  }
  bool chunk(uint32_t offset, const uint8_t* data, size_t length, uint32_t crc) {
    if (!pending_ || !data || !length || length > Chunk || offset > incoming_.length ||
        length > incoming_.length - offset || (cartridgeCrc(data, length) ^ 0xffffffffu) != crc) return false;
    if (offset < received_) {
      if (length > received_ - offset) return false;
      uint8_t old[Chunk];
      return flash_.read(incoming_.offset + offset, old, length) && !memcmp(old, data, length);
    }
    if (offset != received_ || (offset & 3u) || (length != Chunk && offset + length != incoming_.length)) return false;
    uint8_t aligned[Chunk];
    memset(aligned, 255, sizeof(aligned)); memcpy(aligned, data, length);
    if (!flash_.write(incoming_.offset + offset, aligned, (length + 3u) & ~3u)) return false;
    checksum_ = cartridgeCrc(data, length, checksum_);
    received_ += length;
    return true;
  }
  bool commit() {
    if (!pending_ || received_ != incoming_.length || (checksum_ ^ 0xffffffffu) != incoming_.crc || !verify(incoming_)) return false;
    Record next[MaxRecords]; memcpy(next, records_, sizeof(next)); next[slot_] = incoming_;
    if (!saveDirectory(next)) return false;
    memcpy(records_, next, sizeof(records_)); pending_ = false;
    return true;
  }
  void abort() { pending_ = false; received_ = 0; }
  bool remove(uint8_t id) {
    if (!ready_ || pending_ || !find(id)) return false;
    Record next[MaxRecords]; memcpy(next, records_, sizeof(next));
    for (Record& record : next) if (record.id == id) record = Record{};
    if (!saveDirectory(next)) return false;
    memcpy(records_, next, sizeof(records_));
    return true;
  }
  bool verify(const Record& record) const {
    uint32_t crc = 0xffffffffu;
    uint8_t bytes[Chunk];
    for (uint32_t offset = 0; offset < record.length;) {
      const uint32_t count = record.length - offset < Chunk ? record.length - offset : Chunk;
      if (!flash_.read(record.offset + offset, bytes, count)) return false;
      crc = cartridgeCrc(bytes, count, crc); offset += count;
    }
    return (crc ^ 0xffffffffu) == record.crc;
  }

 private:
  static uint32_t pages(uint32_t bytes) { return (bytes + Page - 1) / Page; }
  static uint32_t get(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
  static void put(uint8_t* p, uint32_t v) { for (unsigned i = 0; i < 4; ++i) p[i] = uint8_t(v >> (i * 8)); }
  bool occupied(uint32_t page) const {
    for (const Record& r : records_)
      if (r.id && page >= r.offset / Page && page < r.offset / Page + pages(r.length)) return true;
    return false;
  }
  bool loadDirectory(unsigned bank, Record* records, uint32_t& generation) const {
    uint8_t bytes[DirectoryBytes];
    if (flash_.size() < Page * 3u || !flash_.read(bank * Page, bytes, sizeof(bytes)) ||
        get(bytes) != 0x31595054u || get(bytes + 4) != 1 || get(bytes + 12) != flash_.size() ||
        get(bytes + 1020) != 0x54494d43u ||
        (cartridgeCrc(bytes, 1016) ^ 0xffffffffu) != get(bytes + 1016)) return false;
    generation = get(bytes + 8);
    for (unsigned i = 0; i < MaxRecords; ++i) {
      const uint8_t* raw = bytes + 32 + i * 64;
      Record& r = records[i]; r = Record{};
      r.id = raw[0]; if (!r.id) continue;
      r.kind = raw[1]; r.offset = get(raw + 4); r.length = get(raw + 8); r.crc = get(raw + 12);
      memcpy(r.title, raw + 16, 32);
      if (r.kind < 1 || r.kind > 4 || r.offset < Page * 2u || r.offset % Page || r.length < 16 ||
          r.offset > flash_.size() || r.length > flash_.size() - r.offset ||
          pages(r.length) * Page > flash_.size() - r.offset || !r.title[0]) return false;
      for (char c : r.title) if (c && (uint8_t(c) < 32 || uint8_t(c) > 126)) return false;
      for (unsigned j = 0; j < i; ++j) if (records[j].id) {
        const Record& old = records[j];
        if (old.id == r.id || (r.offset < old.offset + pages(old.length) * Page && old.offset < r.offset + pages(r.length) * Page)) return false;
      }
    }
    return true;
  }
  bool saveDirectory(const Record* records) {
    uint8_t bytes[DirectoryBytes] = {};
    put(bytes, 0x31595054u); put(bytes + 4, 1); put(bytes + 8, generation_ + 1); put(bytes + 12, flash_.size());
    for (unsigned i = 0; i < MaxRecords; ++i) {
      const Record& r = records[i]; uint8_t* raw = bytes + 32 + i * 64;
      raw[0] = r.id; raw[1] = r.kind; put(raw + 4, r.offset); put(raw + 8, r.length); put(raw + 12, r.crc);
      memcpy(raw + 16, r.title, 32);
    }
    put(bytes + 1016, cartridgeCrc(bytes, 1016) ^ 0xffffffffu);
    put(bytes + 1020, 0x54494d43u);
    const unsigned target = directory_ ^ 1u;
    if (!flash_.erase(target * Page, Page) || !flash_.write(target * Page, bytes, 1020) ||
        !flash_.write(target * Page + 1020, bytes + 1020, 4)) { ready_ = pending_ = false; return false; }
    Record check[MaxRecords] = {}; uint32_t generation = 0;
    if (!loadDirectory(target, check, generation) || generation != generation_ + 1) { ready_ = pending_ = false; return false; }
    directory_ = target; generation_ = generation;
    return true;
  }
  Flash& flash_;
  Record records_[MaxRecords] = {}, incoming_;
  uint32_t generation_ = 0, received_ = 0, checksum_ = 0xffffffffu;
  unsigned directory_ = 0, slot_ = 0;
  bool ready_ = false, pending_ = false;
};
} // namespace flappy
