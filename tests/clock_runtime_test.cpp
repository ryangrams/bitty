#include "../firmware/flappy/clock_runtime.h"

#include <array>
#include <assert.h>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <string>
#include <type_traits>

namespace AllocationAudit {
struct alignas(std::max_align_t) alignas(16) Header { size_t size; };
static size_t live = 0, bytes = 0, requests = 0, largest = 0;
static bool deny = false;
}

void* operator new(size_t length) {
  using namespace AllocationAudit;
  ++requests;
  if (deny) throw std::bad_alloc();
  if (length > largest) largest = length;
  auto* header = static_cast<Header*>(std::malloc(sizeof(Header) + length));
  if (!header) throw std::bad_alloc();
  header->size = length; ++live; bytes += length;
  return header + 1;
}
void operator delete(void* pointer) noexcept {
  if (!pointer) return;
  auto* header = static_cast<AllocationAudit::Header*>(pointer) - 1;
  assert(AllocationAudit::live > 0 && AllocationAudit::bytes >= header->size);
  --AllocationAudit::live; AllocationAudit::bytes -= header->size;
  std::free(header);
}
void* operator new[](size_t length) { return ::operator new(length); }
void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
void operator delete(void* pointer,size_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer,size_t) noexcept { ::operator delete(pointer); }
void* operator new(size_t length,const std::nothrow_t&) noexcept {
  try { return ::operator new(length); } catch (...) { return nullptr; }
}
void* operator new[](size_t length,const std::nothrow_t&) noexcept {
  try { return ::operator new[](length); } catch (...) { return nullptr; }
}
void operator delete(void* pointer,const std::nothrow_t&) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer,const std::nothrow_t&) noexcept { ::operator delete[](pointer); }

namespace {
using flappy::ClockRuntime;
using flappy::ClockTime;
using flappy::GameTitle;
using flappy::Skins;
namespace Scene = flappy::ClockScene;

struct Canvas {
  static constexpr uint16_t Guard = 0xa55a;
  std::array<uint16_t,Scene::Width * Scene::Height + 128> storage;
  Canvas() { storage.fill(Guard); }
  uint16_t* data() { return storage.data() + 64; }
  const uint16_t* data() const { return storage.data() + 64; }
  void check() const {
    for (unsigned i = 0; i < 64; ++i)
      assert(storage[i] == Guard && storage[Scene::Width * Scene::Height + 64 + i] == Guard);
  }
  uint64_t hash() const {
    uint64_t value = 1469598103934665603ULL;
    for (unsigned i = 0; i < Scene::Width * Scene::Height; ++i)
      value = (value ^ data()[i]) * 1099511628211ULL;
    return value;
  }
  bool allBlack() const {
    for (unsigned i = 0; i < Scene::Width * Scene::Height; ++i) if (data()[i]) return false;
    return true;
  }
};
constexpr uint16_t Canvas::Guard;

static ClockTime timeAt(unsigned frame) {
  ClockTime time;
  time.hour = uint8_t((23u + frame / 3600u) % 24u);
  time.minute = uint8_t((59u + frame / 60u) % 60u);
  time.second = uint8_t(frame % 60u);
  time.ms = UINT32_MAX - 500u + frame * 17u;
  time.twentyFourHour = (frame / 400u) % 2u == 0;
  return time;
}

static void allTitlesAndDeterminism() {
  Skins skins; skins.select(0);
  Canvas first,second;
  for (unsigned title = 0; title < flappy::GameTitleCount; ++title) {
    const size_t before = AllocationAudit::live, beforeBytes = AllocationAudit::bytes;
    {
      ClockRuntime a,b;
      assert(a.reset(GameTitle(title),19) && b.reset(GameTitle(title),19));
      assert(a.ready() && a.stateBytes() > 0 && a.stateBytes() < 3072);
      assert(a.fallback() == (title == unsigned(GameTitle::Nes)));
      assert(AllocationAudit::live == before + 2);
      assert(AllocationAudit::bytes == beforeBytes + a.stateBytes() + b.stateBytes());
      a.draw(nullptr,timeAt(0),skins);
      const size_t steadyRequests = AllocationAudit::requests;
      unsigned different = 0;
      uint64_t previous = 0;
      for (unsigned frame = 0; frame < 1800; ++frame) {
        const ClockTime time = timeAt(frame);
        a.step(time); b.step(time);
        if (frame % 37u == 0) {
          const ClockRuntime& immutable = a;
          immutable.draw(first.data(),time,skins); b.draw(second.data(),time,skins);
          assert(first.hash() == second.hash());
          const uint64_t hash = first.hash(); different += hash != previous; previous = hash;
          immutable.draw(first.data(),time,skins); assert(first.hash() == hash);
          first.check(); second.check();
          assert(!first.allBlack());
        }
      }
      assert(different > 30);
      assert(AllocationAudit::requests == steadyRequests);
      assert(AllocationAudit::live == before + 2);
    }
    assert(AllocationAudit::live == before && AllocationAudit::bytes == beforeBytes);
  }
}

static void tetrisZeroUsesTheSameClock() {
  ClockRuntime original,zero;
  assert(original.reset(GameTitle::Tetris,97) && zero.reset(GameTitle::TetrisZero,97));
  assert(original.stateBytes() == zero.stateBytes());
  Skins skins; skins.select(2);
  Canvas first,second;
  for (unsigned frame = 0; frame < 2400; ++frame) {
    const ClockTime time = timeAt(frame);
    original.step(time); zero.step(time);
    if (frame % 89u == 0) {
      original.draw(first.data(),time,skins); zero.draw(second.data(),time,skins);
      assert(first.hash() == second.hash()); first.check(); second.check();
    }
  }
}

static void invalidRomsAndNoRomFallback() {
  ClockRuntime runtime;
  Skins skins; skins.select(0);
  Canvas canvas;
  const ClockTime time = timeAt(0);
  assert(!runtime.ready() && runtime.stateBytes() == 0);
  runtime.step(time); runtime.draw(canvas.data(),time,skins); canvas.check(); assert(canvas.allBlack());
  const uint8_t shortRom[] = {'N','E','S',0x1a,2,1,1,0};
  std::array<uint8_t,40976> synthetic;
  synthetic.fill(0);
  for (unsigned i = 0; i < sizeof(shortRom); ++i) synthetic[i] = shortRom[i];
  for (unsigned scenario = 0; scenario < 5; ++scenario) {
    const uint8_t* bytes = scenario < 2 ? nullptr : scenario == 2 ? shortRom : synthetic.data();
    const size_t length = scenario == 0 ? 0 : scenario == 1 ? synthetic.size() :
        scenario == 2 ? sizeof(shortRom) : scenario == 3 ? synthetic.size() - 1 : synthetic.size();
    assert(runtime.reset(GameTitle::Nes,71,bytes,length));
    assert(runtime.ready() && runtime.fallback() && runtime.stateBytes() == sizeof(flappy::NesShelfClock));
    for (unsigned frame = 0; frame < 70; ++frame) runtime.step(timeAt(frame));
    runtime.draw(canvas.data(),time,skins); canvas.check(); assert(!canvas.allBlack());
    runtime.release(); runtime.release();
    assert(!runtime.ready() && runtime.stateBytes() == 0);
  }
}

static void repeatedCyclingAndAllocationFailure() {
  ClockRuntime runtime;
  Skins skins; skins.select(4);
  Canvas canvas;
  const size_t before = AllocationAudit::live, beforeBytes = AllocationAudit::bytes;
  for (unsigned cycle = 0; cycle < 1200; ++cycle) {
    const GameTitle title = GameTitle(cycle % flappy::GameTitleCount);
    assert(runtime.reset(title,cycle));
    assert(AllocationAudit::live == before + 1);
    assert(AllocationAudit::bytes == beforeBytes + runtime.stateBytes());
    for (unsigned frame = 0; frame < 5; ++frame) runtime.step(timeAt(cycle + frame));
    runtime.draw(canvas.data(),timeAt(cycle),skins); canvas.check();
    if (cycle % 17u == 0) {
      runtime.release();
      assert(AllocationAudit::live == before && AllocationAudit::bytes == beforeBytes);
    }
  }
  runtime.release();
  assert(AllocationAudit::live == before && AllocationAudit::bytes == beforeBytes);
  for (unsigned title = 0; title < flappy::GameTitleCount; ++title) {
    assert(runtime.reset(GameTitle::FlappyBird,4));
    AllocationAudit::deny = true;
    const bool allocated = runtime.reset(GameTitle(title),5);
    AllocationAudit::deny = false;
    assert(!allocated && !runtime.ready() && runtime.stateBytes() == 0);
    assert(AllocationAudit::live == before && AllocationAudit::bytes == beforeBytes);
    runtime.step(timeAt(0)); runtime.draw(canvas.data(),timeAt(0),skins);
    assert(canvas.allBlack()); canvas.check();
  }
  assert(!runtime.reset(GameTitle(255),4));
  assert(!runtime.ready() && runtime.stateBytes() == 0);
  runtime.release();
}

static void writePpm(const Canvas& canvas,const std::string& path) {
  std::ofstream output(path,std::ios::binary); assert(output);
  output << "P6\n135 240\n255\n";
  for (unsigned i = 0; i < Scene::Width * Scene::Height; ++i) {
    const uint16_t p = canvas.data()[i];
    const char rgb[] = {char((p >> 11) * 255 / 31),char((p >> 5 & 63) * 255 / 63),char((p & 31) * 255 / 31)};
    output.write(rgb,3);
  }
}
}

int main(int argc,char** argv) {
  static_assert(sizeof(ClockRuntime) <= 32,"Clock runtime must keep its canvas outside scene state");
  static_assert(!std::is_copy_constructible<ClockRuntime>::value,"ClockRuntime owns a unique scene");
  static_assert(!std::is_copy_assignable<ClockRuntime>::value,"ClockRuntime owns a unique scene");
  allTitlesAndDeterminism();
  tetrisZeroUsesTheSameClock();
  invalidRomsAndNoRomFallback();
  repeatedCyclingAndAllocationFailure();
  if (argc > 1) {
    Skins skins; skins.select(0);
    Canvas canvas;
    for (unsigned title = 0; title < flappy::GameTitleCount; ++title) {
      ClockRuntime runtime; assert(runtime.reset(GameTitle(title),33));
      ClockTime time; time.hour = 12; time.minute = 34;
      for (unsigned frame = 0; frame < 280; ++frame) { time.ms = frame * 1000u / 60u; runtime.step(time); }
      runtime.draw(canvas.data(),time,skins);
      writePpm(canvas,std::string(argv[1]) + "-" + std::to_string(title) + ".ppm");
    }
  }
  std::cout << "Clock runtime: all game identifiers, NES fallback, deterministic rollover, 1200 switches, single-state allocation and exhaustion recovery passed\n";
}
