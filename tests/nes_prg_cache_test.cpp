#include "../firmware/flappy/nes_runtime.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

static bool failNextAllocation = false;
static unsigned failAllocationCountdown = 0;
static size_t liveCacheBytes = 0, maximumCacheBytes = 0;
static std::vector<void*> cacheAllocations;
extern "C" void* nes_malloc(int bytes) {
  if (failNextAllocation || (failAllocationCountdown && --failAllocationCountdown == 0)) {
    failNextAllocation = false;
    return nullptr;
  }
  void* allocation = std::malloc(size_t(bytes));
  if (bytes == 8192 && allocation) {
    cacheAllocations.push_back(allocation);
    liveCacheBytes += 8192;
    if (liveCacheBytes > maximumCacheBytes) maximumCacheBytes = liveCacheBytes;
  }
  return allocation;
}
extern "C" void nes_free(void* allocation) {
  for (auto i = cacheAllocations.begin(); i != cacheAllocations.end(); ++i) {
    if (*i == allocation) {
      liveCacheBytes -= 8192;
      cacheAllocations.erase(i);
      break;
    }
  }
  std::free(allocation);
}

// Each bank contains a small program written for this test and a bank marker.
static std::vector<uint8_t> cartridge(unsigned mapper) {
  const unsigned prg = mapper == 0 ? 32768 : 65536;
  std::vector<uint8_t> rom(16 + prg + 8192, 0);
  rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1a;
  rom[4] = uint8_t(prg / 16384); rom[5] = 1;
  rom[6] = uint8_t((mapper & 15u) << 4);
  rom[7] = uint8_t(mapper & 0xf0u);
  for (unsigned bank = 0; bank < prg / 8192; ++bank) {
    uint8_t* start = rom.data() + 16 + bank * 8192;
    for (unsigned i = 0; i < 8192; ++i) start[i] = uint8_t(i * 37u + bank * 13u);
    const uint8_t program[] = {0xe6, 0x00, 0xa9, uint8_t(bank), 0x85, 0x01, 0x4c, 0x00, 0x80};
    std::memcpy(start, program, sizeof(program));
  }
  for (unsigned i = prg - 6; i < prg; i += 2) {
    rom[16 + i] = 0; rom[16 + i + 1] = 0x80;
  }
  return rom;
}

static nes_t* core(flappy::NesRuntime& runtime) {
  return const_cast<nes_t*>(runtime.core());
}

static void sameMachine(const flappy::NesRuntime& reference, const flappy::NesRuntime& cached) {
  const nes_t* a = reference.core();
  const nes_t* b = cached.core();
  assert(std::memcmp(&a->nes_cpu, &b->nes_cpu, sizeof(a->nes_cpu)) == 0);
  assert(std::memcmp(&a->nes_apu, &b->nes_apu, sizeof(a->nes_apu)) == 0);
  assert(a->host_dot_remainder == b->host_dot_remainder);
  assert(a->nes_frame_skip_count == b->nes_frame_skip_count);
  auto ap = a->nes_ppu;
  auto bp = b->nes_ppu;
  std::memset(ap.chr_banks, 0, sizeof(ap.chr_banks));
  std::memset(bp.chr_banks, 0, sizeof(bp.chr_banks));
  assert(std::memcmp(&ap, &bp, sizeof(ap)) == 0);
  assert(a->nes_mapper.mapper_state_size == b->nes_mapper.mapper_state_size);
  if (a->nes_mapper.mapper_state_size) {
    assert(std::memcmp(a->nes_mapper.mapper_register, b->nes_mapper.mapper_register,
                       a->nes_mapper.mapper_state_size) == 0);
  }
  if (a->nes_rom.sram) assert(std::memcmp(a->nes_rom.sram, b->nes_rom.sram, 8192) == 0);
}

static void fixedWindowsPreserveStateAndBankIdentity() {
  for (unsigned mapper : {0u, 1u, 4u, 118u}) {
    auto rom = cartridge(mapper);
    flappy::NesRuntime reference, cached;
    assert(reference.load(rom.data(), rom.size()));
    assert(cached.load(rom.data(), rom.size()));
    nes_t* a = core(reference);
    nes_t* b = core(cached);
    assert(b->host_prg_cache == nullptr && b->host_prg_cache_mask == 0);
    assert(nes_host_prg_cache(b, 3) == NES_OK);
    for (unsigned round = 0; round < 24; ++round) {
      const uint8_t masks[] = {0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 12};
      const uint8_t mask = masks[round % sizeof(masks)];
      assert(nes_host_prg_cache(b, mask) == NES_OK);
      for (unsigned slot = 0; slot < 4; ++slot) {
        const uint16_t bank = uint16_t(round * 3 + slot);
        assert(nes_load_prgrom_8k(a, uint8_t(slot), bank) == NES_OK);
        assert(nes_load_prgrom_8k(b, uint8_t(slot), bank) == NES_OK);
        assert(a->nes_cpu.prg_banks[slot] == b->nes_cpu.prg_banks[slot]);
        assert(b->host_prg_reads[slot] != nullptr);
        assert(std::memcmp(b->host_prg_reads[slot], b->nes_cpu.prg_banks[slot], 8192) == 0);
        if (!(mask & (1u << slot))) assert(b->host_prg_reads[slot] == b->nes_cpu.prg_banks[slot]);
        for (unsigned offset = 0; offset < 8192; offset += 17) {
          const uint16_t address = uint16_t(0x8000 + slot * 8192 + offset);
          assert(nes_cpu_dma_read(a, address) == nes_cpu_dma_read(b, address));
        }
      }
      for (unsigned frame = 0; frame < 3; ++frame) {
        assert(reference.step(uint8_t(frame)) && cached.step(uint8_t(frame)));
        sameMachine(reference, cached);
      }
    }
  }
}

static unsigned overrideReads = 0;
static uint8_t mappedRead(nes_t*, uint16_t address) {
  ++overrideReads;
  return uint8_t(address ^ 0xa5u);
}

static void mapperCallbacksRetainReadOwnership() {
  auto rom = cartridge(4);
  flappy::NesRuntime runtime;
  assert(runtime.load(rom.data(), rom.size()));
  nes_t* machine = core(runtime);
  assert(nes_host_prg_cache(machine, 3) == NES_OK);
  machine->nes_mapper.mapper_read_prg = mappedRead;
  for (unsigned address = 0x8000; address <= 0xffff; address += 31) {
    assert(nes_cpu_dma_read(machine, uint16_t(address)) == uint8_t(address ^ 0xa5u));
  }
  assert(overrideReads > 1000);
  machine->nes_mapper.mapper_read_prg = nullptr;
}

static void allocationFailureAndInvalidMasksPreserveLiveCache() {
  assert(nes_host_prg_cache(nullptr, 3) == NES_ERROR);
  nes_t* empty = nes_init();
  assert(empty && nes_host_prg_cache(empty, 3) == NES_ERROR);
  nes_deinit(empty);
  auto rom = cartridge(4);
  flappy::NesRuntime runtime;
  assert(runtime.load(rom.data(), rom.size()));
  nes_t* machine = core(runtime);
  const size_t baseBytes = liveCacheBytes;
  maximumCacheBytes = baseBytes;
  failNextAllocation = true;
  assert(nes_host_prg_cache(machine, 3) == NES_ERROR);
  assert(machine->host_prg_cache == nullptr && machine->host_prg_cache_mask == 0);
  failAllocationCountdown = 2;
  assert(nes_host_prg_cache(machine, 3) == NES_ERROR);
  assert(machine->host_prg_cache == nullptr && machine->host_prg_cache_extra == nullptr);
  assert(liveCacheBytes == baseBytes);
  assert(nes_host_prg_cache(machine, 2) == NES_OK);
  uint8_t* saved = machine->host_prg_cache;
  uint8_t* reads[4];
  std::memcpy(reads, machine->host_prg_reads, sizeof(reads));
  for (uint8_t mask : {uint8_t(7), uint8_t(15), uint8_t(0x80)}) {
    assert(nes_host_prg_cache(machine, mask) == NES_ERROR);
    assert(machine->host_prg_cache == saved && machine->host_prg_cache_mask == 2);
  }
  failNextAllocation = true;
  assert(nes_host_prg_cache(machine, 3) == NES_ERROR);
  assert(machine->host_prg_cache == saved && machine->host_prg_cache_mask == 2);
  assert(std::memcmp(reads, machine->host_prg_reads, sizeof(reads)) == 0);
  assert(nes_host_prg_cache(machine, 0) == NES_OK);
  for (unsigned slot = 0; slot < 4; ++slot) {
    assert(machine->host_prg_reads[slot] == machine->nes_cpu.prg_banks[slot]);
  }
  assert(machine->host_prg_cache == nullptr && machine->host_prg_cache_mask == 0);
  assert(liveCacheBytes == baseBytes);
  for (uint8_t mask : {uint8_t(3), uint8_t(9), uint8_t(4), uint8_t(0)}) {
    assert(nes_host_prg_cache(machine, mask) == NES_OK);
    assert(liveCacheBytes <= baseBytes + 16384);
  }
  assert(maximumCacheBytes <= baseBytes + 16384);
}

int main() {
  fixedWindowsPreserveStateAndBankIdentity();
  mapperCallbacksRetainReadOwnership();
  allocationFailureAndInvalidMasksPreserveLiveCache();
  assert(liveCacheBytes == 0 && cacheAllocations.empty());
  std::cout << "NES fixed-window PRG cache checks passed\n";
}
