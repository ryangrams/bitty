#include "../firmware/flappy/cartridge_store.h"
#include <assert.h>
#include <vector>
#include <iostream>
#include <algorithm>

struct Flash {
  std::vector<uint8_t> data;
  int budget = -1;
  unsigned erases = 0;
  explicit Flash(unsigned bytes = 16 * 4096) : data(bytes, 255) {}
  uint32_t size() const { return uint32_t(data.size()); }
  bool read(uint32_t offset, void* out, size_t bytes) const {
    if (offset > size() || bytes > size() - offset) return false;
    memcpy(out, data.data() + offset, bytes); return true;
  }
  bool write(uint32_t offset, const void* source, size_t bytes) {
    if (offset % 4 || bytes % 4 || offset > size() || bytes > size() - offset) return false;
    const auto* p = static_cast<const uint8_t*>(source);
    for (unsigned i = 0; i < bytes; ++i) {
      if (budget == 0) return false;
      if (budget > 0) --budget;
      assert((data[offset + i] & p[i]) == p[i]);
      data[offset + i] &= p[i];
    }
    return true;
  }
  bool erase(uint32_t offset, size_t bytes) {
    if (offset % 4096 || bytes % 4096 || offset > size() || bytes > size() - offset) return false;
    ++erases;
    for (unsigned i = 0; i < bytes; ++i) {
      if (budget == 0) return false;
      if (budget > 0) --budget;
      data[offset + i] = 255;
    }
    return true;
  }
};
using Store = flappy::CartridgeStore<Flash>;
static uint32_t crc(const std::vector<uint8_t>& bytes) {
  return flappy::cartridgeCrc(bytes.data(), bytes.size()) ^ 0xffffffffu;
}
static bool upload(Store& store, uint8_t id, const std::vector<uint8_t>& bytes) {
  if (!store.begin(id, 1, "Synthetic cartridge", bytes.size(), crc(bytes))) return false;
  for (unsigned offset = 0; offset < bytes.size(); offset += Store::Chunk) {
    const unsigned count = std::min(unsigned(Store::Chunk), unsigned(bytes.size() - offset));
    const auto sum = flappy::cartridgeCrc(bytes.data() + offset, count) ^ 0xffffffffu;
    if (!store.chunk(offset, bytes.data() + offset, count, sum)) return false;
    assert(store.chunk(offset, bytes.data() + offset, count, sum)); // Lost acknowledgement.
  }
  return store.commit();
}
int main() {
  const std::vector<uint8_t> old(5001, 0x39), next(7001, 0xa5);
  Flash original; Store initial(original);
  assert(initial.mount() && initial.capacity() == 14 * 4096);
  assert(upload(initial, 1, old));
  assert(initial.find(1)->length == old.size() && initial.verify(*initial.find(1)));
  assert(initial.used() == 8192 && initial.largestFree() == 12 * 4096);
  // Cut power at every byte through erase, payload, directory and commit marker.
  const int mutations = 8192 + 7004 + 4096 + 1024;
  for (int cut = 0; cut <= mutations + 1; ++cut) {
    Flash flash = original; flash.budget = cut;
    Store writer(flash); assert(writer.mount());
    upload(writer, 1, next);
    flash.budget = -1;
    Store reboot(flash); assert(reboot.mount());
    const auto* record = reboot.find(1); assert(record && reboot.verify(*record));
    assert(record->crc == crc(old) || record->crc == crc(next));
    assert(record->length == (record->crc == crc(old) ? old.size() : next.size()));
  }
  Flash flash = original; Store store(flash); assert(store.mount());
  assert(store.begin(1, 1, "Replacement", next.size(), crc(next)));
  assert(store.find(1)->crc == crc(old));
  assert(!store.chunk(4, next.data(), 256, flappy::cartridgeCrc(next.data(), 256) ^ 0xffffffffu));
  assert(!store.chunk(0, next.data(), 256, 0));
  assert(!store.commit()); store.abort();
  assert(store.find(1)->crc == crc(old));
  assert(upload(store, 2, next));
  assert(store.remove(1) && !store.find(1));
  assert(store.used() == 8192);
  const auto before = flash.data;
  assert(!store.begin(3, 1, "Too large", store.capacity() + 1, 0));
  assert(flash.data == before);
  assert(!store.begin(3, 0, "Bad kind", 16, 0));
  assert(!store.begin(3, 1, "\nControl", 16, 0));
  assert(!store.begin(3, 1, "12345678901234567890123456789012345", 16, 0));
  Flash full(4 * 4096); Store small(full); assert(small.mount());
  assert(upload(small, 1, old));
  const auto intact = full.data; const unsigned erases = full.erases;
  assert(!small.begin(1, 1, "No room to replace", old.size(), crc(old)));
  assert(full.data == intact && full.erases == erases);
  Flash unknown; unknown.data[50] = 0;
  Store blocked(unknown); assert(!blocked.mount() && unknown.erases == 0);
  std::cout << "Cartridge atomic replacement, bytewise power cuts, checksums, retries, removal and allocation passed\n";
}
