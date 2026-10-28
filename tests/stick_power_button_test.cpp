#include "../firmware/flappy/stick_power_button.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

struct Bus {
  uint8_t registers[256] = {};
  unsigned writes = 0;
  int failRead = -1, failWrite = -1;
  Bus() { registers[0]=0x50; registers[1]=0x20; registers[2]=5; registers[3]=6; registers[0x49]=0x2a; }
  bool read(uint8_t address, uint8_t* data, size_t length) {
    if (address == failRead) return false;
    memcpy(data,registers+address,length);
    if (address == 0x48) registers[address] &= 1u;
    return true;
  }
  bool write(uint8_t address, uint8_t value) {
    ++writes;
    if (address == failWrite) return false;
    registers[address]=value;
    return true;
  }
};
int main() {
  Bus bus; flappy::StickPowerButton key;
  assert(key.begin(bus) && key.ready());
  assert(bus.registers[0x49] == 0x2b && bus.registers[0x4a] == 1);
  assert(!(bus.registers[0x49] & 0x80)); // Emergency download remains enabled.
  bus.registers[0x48] = 0x81; assert(key.poll(bus,0));
  assert(!key.poll(bus,4)); assert(!key.poll(bus,8)); // Held key never repeats.
  bus.registers[0x48] = 0; assert(!key.poll(bus,16));
  bus.registers[0x48] = 0x81; assert(key.poll(bus,24)); // A second fast tap is independent.
  bus.registers[0x48] = 0; assert(!key.poll(bus,32));
  bus.registers[0x48] = 0x80; assert(key.poll(bus,40)); // A short pulse survives between polls.
  assert(!key.poll(bus,48));
  bus.failRead=0x48; assert(!key.poll(bus,56)); bus.failRead=-1;
  bus.registers[0x48]=0; assert(!key.poll(bus,UINT32_MAX-4));
  bus.registers[0x48]=0x81; assert(key.poll(bus,4)); // Millisecond wrap.
  Bus old; old.registers[3]=5; flappy::StickPowerButton oldKey;
  assert(!oldKey.begin(old) && old.writes == 0); // Old register maps are never guessed.
  Bus failed; failed.failWrite=0x4a; flappy::StickPowerButton failedKey;
  assert(!failedKey.begin(failed)); assert(failed.registers[0x49] == 0x2a);
  Bus held; held.registers[0x48]=0x81; flappy::StickPowerButton heldKey;
  assert(heldKey.begin(held)); assert(!heldKey.poll(held,0));
  held.registers[0x48]=0; assert(!heldKey.poll(held,8));
  held.registers[0x48]=0x81; assert(heldKey.poll(held,16));
  Bus newer; newer.registers[3]=0x53; flappy::StickPowerButton newerKey; assert(newerKey.begin(newer));
  puts("S3 key: verified register map, preserved download, rollback, independent taps, holds and wrap passed");
}
