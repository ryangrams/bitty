#include "../firmware/flappy/controller_input.h"

#include <assert.h>
#include <iostream>
#include <vector>

using namespace flappy::controller;

static void accumulatedEdges() {
  Snapshot state;
  updateSnapshot(state, 0, true, true);
  updateSnapshot(state, A, true);
  updateSnapshot(state, 0, true);
  Snapshot read = takeSnapshot(state);
  assert(read.connected && read.buttons == 0 && read.pressed == A);
  assert(takeSnapshot(state).pressed == 0);
  updateSnapshot(state, A | Right, true);
  updateSnapshot(state, Right, true);
  updateSnapshot(state, B | Right, true);
  read = takeSnapshot(state);
  assert(read.buttons == (B | Right) && read.pressed == (A | B | Right));
  updateSnapshot(state, B | Right, true);
  assert(takeSnapshot(state).pressed == 0);
  updateSnapshot(state, A, true);
  updateSnapshot(state, 0, false);
  read = takeSnapshot(state);
  assert(!read.connected && read.buttons == 0 && read.pressed == 0);
  updateSnapshot(state, A, true);
  updateSnapshot(state, 0, true, true);
  read = takeSnapshot(state);
  assert(read.connected && read.buttons == 0 && read.pressed == 0);
}

static void keyboardReports() {
  HidReportParser parser;
  assert(parser.configureBootKeyboard());
  assert(parser.expectedLength(0) == 8);
  uint8_t report[8] = {0, 0, 4, 5, 0x52, 0x28, 0x29, 0x2a};
  uint16_t buttons = 0;
  assert(parser.decode(0, report, sizeof(report), buttons));
  assert(buttons == (A | B | Up | Start | Menu | Select));
  report[2] = 0x50;
  report[3] = 0x51;
  report[4] = 0x4f;
  report[5] = report[6] = report[7] = 0;
  assert(parser.decode(0, report, sizeof(report), buttons));
  assert(buttons == (Left | Down | Right));
  for (size_t length = 0; length < sizeof(report); ++length) {
    assert(!parser.decode(0, report, length, buttons));
    assert(buttons == 0);
  }
  assert(!parser.decode(1, report, sizeof(report), buttons));
  for (uint8_t rollover = 1; rollover <= 3; ++rollover) {
    report[7] = rollover;
    assert(!parser.decode(0, report, sizeof(report), buttons));
    assert(buttons == 0);
  }
  uint8_t oversized[HidReportParser::MaxReportBytes + 1] = {};
  assert(!parser.decode(0, oversized, sizeof(oversized), buttons));
  assert(!parser.decode(0, nullptr, 8, buttons));
}

static const uint8_t gamepadMap[] = {
  // Gamepad collection with report ID 3 and ten individual buttons.
  0x05,0x01,0x09,0x05,0xa1,0x01,0x85,0x03,
  0x05,0x09,0x19,0x01,0x29,0x0a,0x15,0x00,0x25,0x01,
  0x75,0x01,0x95,0x0a,0x81,0x02,
  // Six padding bits complete the first two bytes.
  0x75,0x06,0x95,0x01,0x81,0x01,
  // Eight-way hat and a padding nibble.
  0x05,0x01,0x09,0x39,0x15,0x00,0x25,0x07,
  0x75,0x04,0x95,0x01,0x81,0x42,
  0x75,0x04,0x95,0x01,0x81,0x01,
  // Signed X and Y axes range from -127 through 127.
  0x09,0x30,0x09,0x31,0x15,0x81,0x25,0x7f,
  0x75,0x08,0x95,0x02,0x81,0x02,0xc0
};

static void gamepadReports() {
  HidReportParser parser;
  assert(parser.configure(gamepadMap, sizeof(gamepadMap)));
  assert(parser.reportCount() == 1 && parser.reportIdAt(0) == 3);
  assert(parser.expectedLength(3) == 5);
  uint8_t report[] = {3,3,8,0,0};
  uint16_t buttons = 0;
  assert(parser.decode(3, report, sizeof(report), buttons));
  assert(buttons == (A | B | Start | Select));
  report[0] = report[1] = 0;
  const uint16_t hats[] = {Up, Up | Right, Right, Right | Down, Down, Down | Left, Left, Left | Up, 0};
  for (uint8_t hat = 0; hat < 9; ++hat) {
    report[2] = hat;
    assert(parser.decode(3, report, sizeof(report), buttons));
    assert(buttons == hats[hat]);
  }
  report[3] = uint8_t(-127);
  report[4] = 127;
  assert(parser.decode(3, report, sizeof(report), buttons));
  assert(buttons == (Left | Down));
  report[3] = 127;
  report[4] = uint8_t(-127);
  assert(parser.decode(3, report, sizeof(report), buttons));
  assert(buttons == (Right | Up));
  assert(!parser.decode(0, report, sizeof(report), buttons));
}

static void multipleReportsAndNkro() {
  const uint8_t map[] = {
    0x05,0x01,0x09,0x06,0xa1,0x01,0x85,0x01,
    0x05,0x07,0x19,0x00,0x29,0x5f,0x15,0x00,0x25,0x01,
    0x75,0x01,0x95,0x60,0x81,0x02,
    0x85,0x02,0x05,0x01,0x19,0x90,0x29,0x93,
    0x75,0x01,0x95,0x04,0x81,0x02,0xc0
  };
  HidReportParser parser;
  assert(parser.configure(map, sizeof(map)));
  assert(parser.reportCount() == 2);
  uint8_t keys[12] = {};
  keys[4 / 8] |= 1u << (4 % 8);
  keys[0x52 / 8] |= 1u << (0x52 % 8);
  uint16_t buttons = 0;
  assert(parser.decode(1, keys, sizeof(keys), buttons));
  assert(buttons == (A | Up));
  const uint8_t directions = 0x0a;
  assert(parser.decode(2, &directions, 1, buttons));
  assert(buttons == (Down | Left));
}

static void packedWideAxesAndPushPop() {
  const uint8_t map[] = {
    0x05,0x01,0x09,0x04,0xa1,0x01,
    0x05,0x09,0x09,0x01,0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x01,0x81,0x02,
    0xa4,0x05,0x01,0x09,0x30,0x09,0x31,0x15,0x00,0x26,0xff,0x0f,
    0x75,0x0c,0x95,0x02,0x81,0x02,0xb4,
    0x09,0x02,0x81,0x02,0xc0
  };
  HidReportParser parser;
  assert(parser.configure(map, sizeof(map)));
  uint8_t data[4] = {};
  const uint32_t packed = 1u | (4095u << 13) | (1u << 25);
  for (uint8_t i = 0; i < 4; ++i) data[i] = uint8_t(packed >> (8 * i));
  uint16_t buttons = 0;
  assert(parser.decode(0, data, sizeof(data), buttons));
  assert(buttons == (A | B | Left | Down));
}

static void buttonArraysAndOffsetUsages() {
  const uint8_t map[] = {
    0x05,9,0x19,0,0x29,10,0x15,0,0x25,10,0x75,4,0x95,2,0x81,0,
    0x85,2,0x05,7,0x19,4,0x29,5,0x15,0,0x25,1,0x75,8,0x95,1,0x81,0
  };
  // Use explicit IDs on both reports.
  std::vector<uint8_t> descriptor = {0x85,1};
  descriptor.insert(descriptor.end(), map, map + sizeof(map));
  HidReportParser parser;
  assert(parser.configure(descriptor.data(), descriptor.size()));
  uint16_t buttons = 0;
  uint8_t data = 0x21;
  assert(parser.decode(1, &data, 1, buttons) && buttons == (A | B));
  data = 0xa9;
  assert(parser.decode(1, &data, 1, buttons) && buttons == (Select | Start));
  data = 0;
  assert(parser.decode(2, &data, 1, buttons) && buttons == A);
  data = 1;
  assert(parser.decode(2, &data, 1, buttons) && buttons == B);
  data = 0xff;
  assert(parser.decode(2, &data, 1, buttons) && buttons == 0);
}

static void malformedAndBounded() {
  HidReportParser parser;
  assert(!parser.configure(nullptr, 1));
  assert(!parser.configure(gamepadMap, 0));
  for (size_t length = 1; length < sizeof(gamepadMap); ++length)
    assert(!parser.configure(gamepadMap, length));
  const uint8_t oversized[] = {0x75,0x20,0x96,0x00,0x04,0x81,0x01};
  assert(!parser.configure(oversized, sizeof(oversized)));
  const uint8_t invalidId[] = {0x85,0x00};
  assert(!parser.configure(invalidId, sizeof(invalidId)));
  const uint8_t pop[] = {0xb4};
  assert(!parser.configure(pop, sizeof(pop)));
  const uint8_t badLong[] = {0xfe,4,1,0};
  assert(!parser.configure(badLong, sizeof(badLong)));
  std::vector<uint8_t> tooMany = {0x05,9,0x15,0,0x25,1,0x75,1,0x95,1};
  for (uint8_t i = 0; i < 9; ++i) {
    const uint8_t input[] = {0x85,uint8_t(i + 1),0x09,1,0x81,2};
    tooMany.insert(tooMany.end(), input, input + sizeof(input));
  }
  assert(!parser.configure(tooMany.data(), tooMany.size()));
  const uint8_t mixedIds[] = {0x05,9,0x09,1,0x15,0,0x25,1,0x75,1,0x95,1,0x81,2,0x85,1,0x09,2,0x81,2};
  assert(!parser.configure(mixedIds, sizeof(mixedIds)));
  assert(!parser.valid());
  uint16_t buttons = 0xffff;
  const uint8_t value = 1;
  assert(!parser.decode(0, &value, 1, buttons) && buttons == 0);
}

static void boundedNoise() {
  HidReportParser parser;
  uint32_t state = 0x12345678;
  uint8_t bytes[160];
  uint16_t buttons = 0;
  for (unsigned iteration = 0; iteration < 5000; ++iteration) {
    for (size_t i = 0; i < sizeof(bytes); ++i) {
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      bytes[i] = uint8_t(state);
    }
    parser.configure(bytes, 1 + state % sizeof(bytes));
    for (uint8_t id = 0; id < 4; ++id) parser.decode(id, bytes, state % 129, buttons);
  }
}

int main() {
  accumulatedEdges();
  keyboardReports();
  gamepadReports();
  multipleReportsAndNkro();
  packedWideAxesAndPushPop();
  buttonArraysAndOffsetUsages();
  malformedAndBounded();
  boundedNoise();
  std::cout << "Controller HID tests passed\n";
}
