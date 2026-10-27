#include "../firmware/flappy/nes_runtime.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using flappy::NesRuntime;
static unsigned owner;
static std::vector<uint32_t> clockEvents[2], readEvents[2];
#if defined(NES_TEST_MODE) && NES_TEST_MODE == 1
static std::vector<uint64_t> traceEvents[2];
extern "C" void nes_test_trace(nes_t*, uint16_t pc) { traceEvents[owner].push_back(pc); }
extern "C" void nes_test_rlog(nes_t*, uint16_t a, uint8_t value, uint16_t pc) {
  traceEvents[owner].push_back((uint64_t(1) << 56) | (uint64_t(pc) << 24) | (uint32_t(a) << 8) | value);
}
extern "C" void nes_test_wlog(nes_t*, uint16_t a, uint8_t value, uint16_t pc) {
  traceEvents[owner].push_back((uint64_t(2) << 56) | (uint64_t(pc) << 24) | (uint32_t(a) << 8) | value);
}
#endif

// The fixture is an original self-jump with small interrupt handlers.
static std::vector<uint8_t> fixture() {
  std::vector<uint8_t> rom(16 + 32768 + 8192, 0);
  rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1a;
  rom[4] = 2; rom[5] = 1;
  std::memset(rom.data() + 16, 0xea, 32768);
  const uint8_t loop[] = {0x4c, 0x00, 0x80};
  const uint8_t nmi[] = {0xe6, 0x21, 0x40};
  const uint8_t irq[] = {0xe6, 0x22, 0xad, 0x15, 0x40, 0xa9, 0x00, 0x8d, 0x15, 0x40, 0x40};
  std::memcpy(rom.data() + 16, loop, sizeof(loop));
  std::memcpy(rom.data() + 16 + 0x100, nmi, sizeof(nmi));
  std::memcpy(rom.data() + 16 + 0x200, irq, sizeof(irq));
  rom[16 + 0x7ffa] = 0x00; rom[16 + 0x7ffb] = 0x81;
  rom[16 + 0x7ffc] = 0x00; rom[16 + 0x7ffd] = 0x80;
  rom[16 + 0x7ffe] = 0x00; rom[16 + 0x7fff] = 0x82;
  return rom;
}

static nes_t* core(NesRuntime& runtime) { return const_cast<nes_t*>(runtime.core()); }
static void clockHook(nes_t* n, uint16_t cycles) {
  clockEvents[owner].push_back(cycles);
  if (clockEvents[owner].size() == 3) n->nes_cpu.irq_nmi = 1;
}
static uint8_t readHook(nes_t* n, uint16_t address) {
  const uint8_t value = n->nes_cpu.prg_banks[(address >> 13) - 4][address & 8191];
  readEvents[owner].push_back((uint32_t(address) << 8) | value);
  return value;
}

static void clearEvents() {
  for (unsigned i = 0; i < 2; ++i) {
    clockEvents[i].clear(); readEvents[i].clear();
#if defined(NES_TEST_MODE) && NES_TEST_MODE == 1
    traceEvents[i].clear();
#endif
  }
}
static void sameEvents() {
  assert(clockEvents[0] == clockEvents[1] && readEvents[0] == readEvents[1]);
#if defined(NES_TEST_MODE) && NES_TEST_MODE == 1
  assert(traceEvents[0] == traceEvents[1]);
#endif
}
static void sameCpuApu(const nes_t* a, const nes_t* b) {
  assert(std::memcmp(&a->nes_cpu, &b->nes_cpu, sizeof(a->nes_cpu)) == 0);
  assert(std::memcmp(&a->nes_apu, &b->nes_apu, sizeof(a->nes_apu)) == 0);
}

static void compareBudget(nes_t* a, nes_t* b, uint16_t pc, unsigned flags,
                          uint8_t delay, uint16_t budget) {
  a->nes_cpu = {};
  a->nes_cpu.PC = pc;
  a->nes_cpu.A = 0x97; a->nes_cpu.X = 0x23; a->nes_cpu.Y = 0xab;
  a->nes_cpu.P = uint8_t(0xe3 | ((flags & 1) ? 4 : 0));
  a->nes_cpu.SP = 0xfd;
  a->nes_cpu.cycles = budget % 8;
  a->nes_cpu.irq_nmi = (flags >> 1) & 1;
  a->nes_cpu.irq_nmi_delay = delay;
  a->nes_cpu.irq_pending = (flags >> 2) & 1;
  a->nes_apu.irq_line = (flags >> 3) & 1;
  a->nes_apu.dmc.irq_flag = (flags >> 4) & 1;
  for (unsigned i = 0; i < 4; ++i)
    a->host_prg_reads[i] = a->nes_cpu.prg_banks[i] = a->nes_rom.prg_rom + i * 8192;
  // Operands at the top of the address space wrap into internal RAM.
  if (pc == 0xfffe) a->nes_cpu.cpu_ram[0] = 0xff;
  if (pc == 0xffff) { a->nes_cpu.cpu_ram[0] = 0xff; a->nes_cpu.cpu_ram[1] = 0xff; }
  if (pc == 0x100) {
    a->nes_cpu.cpu_ram[0x100] = 0x4c;
    a->nes_cpu.cpu_ram[0x101] = 0x00;
    a->nes_cpu.cpu_ram[0x102] = 0x01;
  }
  b->nes_cpu = a->nes_cpu;
  b->nes_apu = a->nes_apu;
  for (unsigned i = 0; i < 4; ++i) b->host_prg_reads[i] = b->nes_cpu.prg_banks[i];
  clearEvents();
  owner = 0; nes_opcode(a, budget);
  owner = 1; nes_opcode(b, budget);
  sameCpuApu(a, b);
  sameEvents();
}

static void everyBudgetAndInterruptGuard() {
  auto rom = fixture();
  NesRuntime reference, candidate;
  assert(reference.load(rom.data(), rom.size()) && candidate.load(rom.data(), rom.size()));
  reference.setIdleJump(false); candidate.setIdleJump(true);
  nes_t* a = core(reference); nes_t* b = core(candidate);
  unsigned cases = 0;
  for (unsigned pc : {0x8000u, 0x80fdu, 0x9fffu, 0xbfffu, 0xfffdu}) {
    const unsigned offset = 16 + pc - 0x8000;
    rom[offset] = 0x4c; rom[offset + 1] = uint8_t(pc); rom[offset + 2] = uint8_t(pc >> 8);
    for (unsigned hooks = 0; hooks < 4; ++hooks) {
      a->nes_mapper.mapper_cpu_clock = b->nes_mapper.mapper_cpu_clock = hooks & 1 ? clockHook : nullptr;
      a->nes_mapper.mapper_read_prg = b->nes_mapper.mapper_read_prg = hooks & 2 ? readHook : nullptr;
      for (unsigned flags = 0; flags < 32; ++flags)
        for (uint8_t delay = 0; delay < 3; ++delay)
          for (unsigned budget = 0; budget <= 255; ++budget) {
            compareBudget(a, b, uint16_t(pc), flags, delay, uint16_t(budget)); ++cases;
          }
    }
  }
  a->nes_mapper.mapper_cpu_clock = b->nes_mapper.mapper_cpu_clock = nullptr;
  a->nes_mapper.mapper_read_prg = b->nes_mapper.mapper_read_prg = nullptr;
  for (unsigned pc : {0x100u, 0xfffeu, 0xffffu}) {
    if (pc >= 0x8000) {
      rom[16 + pc - 0x8000] = 0x4c;
      if (pc == 0xfffe) rom[16 + 0x7fff] = 0xfe;
    }
    for (unsigned budget : {1u, 2u, 3u, 4u, 28u, 85u, 106u, 113u, 255u}) {
      compareBudget(a, b, uint16_t(pc), 0, 0, uint16_t(budget)); ++cases;
    }
  }
  std::cout << "NROM idle-JMP CPU budget and interrupt cases passed: " << cases << '\n';
}

static void dmcAndNmiFramesRemainExact() {
  auto rom = fixture();
  NesRuntime reference, candidate;
  assert(reference.load(rom.data(), rom.size()) && candidate.load(rom.data(), rom.size()));
  reference.setIdleJump(false); candidate.setIdleJump(true);
  nes_t* a = core(reference); nes_t* b = core(candidate);
  nes_write_ppu_register(a, 0x2000, 0x80); nes_write_ppu_register(b, 0x2000, 0x80);
  a->nes_cpu.I = b->nes_cpu.I = 0;
  for (unsigned frame = 0; frame < 90; ++frame) {
    if (frame % 3 == 0) {
      for (nes_t* n : {a, b}) {
        nes_write_apu_register(n, 0x4010, 0x8f);
        nes_write_apu_register(n, 0x4012, 0xff);
        nes_write_apu_register(n, 0x4013, 4);
        nes_write_apu_register(n, 0x4015, 0x10);
      }
    }
    clearEvents(); owner = 0; assert(reference.step(0)); owner = 1; assert(candidate.step(0));
    sameCpuApu(a, b); sameEvents();
    assert(a->host_dot_remainder == b->host_dot_remainder);
    auto ap = a->nes_ppu; auto bp = b->nes_ppu;
    std::memset(ap.chr_banks, 0, sizeof(ap.chr_banks));
    std::memset(bp.chr_banks, 0, sizeof(bp.chr_banks));
    assert(std::memcmp(&ap, &bp, sizeof(ap)) == 0);
  }
  assert(a->nes_cpu.cpu_ram[0x21] > 0 && a->nes_cpu.cpu_ram[0x22] > 0);
}

int main() {
  everyBudgetAndInterruptGuard();
  dmcAndNmiFramesRemainExact();
  std::cout << "NROM idle-JMP DMC/NMI frames passed\n";
}
