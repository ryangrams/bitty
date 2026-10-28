#include "../firmware/flappy/arcade_support.h"

#include <assert.h>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

using namespace flappy;

static void tiltDeadzoneSensitivityAndPolarity() {
  ArcadeTilt tilt;
  assert(tilt.neutral() == 0);
  for (float sample : {0.f, .035f, -.035f, .01f, -.01f}) assert(tilt.update(sample, true, 1) == 0);
  const int16_t positive = tilt.update(.2f, true, 1);
  tilt.reset();
  const int16_t negative = tilt.update(-.2f, true, 1);
  assert(positive > 0 && positive < 1024 && negative == -positive);
  ArcadeTilt fine, normal, fast, fallback;
  const int16_t fineMove = fine.update(.2f, true, 0);
  const int16_t normalMove = normal.update(.2f, true, 1);
  const int16_t fastMove = fast.update(.2f, true, 2);
  assert(0 < fineMove && fineMove < normalMove && normalMove < fastMove && fastMove < 1024);
  assert(fallback.update(.2f, true, UINT8_MAX) == normalMove);
  int16_t previous = 0;
  tilt.reset();
  for (unsigned i = 0; i < 16; ++i) {
    const int16_t move = tilt.update(.3f, true, 1);
    assert(move >= previous && move <= 1024);
    previous = move;
  }
  assert(previous > normalMove);
  for (unsigned i = 0; i < 16; ++i) assert(tilt.update(10.f, true, 2) <= 1024);
  assert(tilt.update(10.f, true, 2) == 1024);
  for (unsigned i = 0; i < 16; ++i) assert(tilt.update(-10.f, true, 2) >= -1024);
  assert(tilt.update(-10.f, true, 2) == -1024);
  for (unsigned i = 0; i < 100; ++i) tilt.update(0, true, 1);
  assert(tilt.update(0, true, 1) == 0);
}

static void tiltFreshnessAndCalibration() {
  ArcadeTilt tilt, fresh;
  assert(tilt.calibrate(.3f) && tilt.neutral() == .3f);
  assert(tilt.update(.3f, true, 1) == 0);
  assert(tilt.update(.5f, true, 1) > 0);
  assert(tilt.update(.5f, false, 1) == 0);
  assert(fresh.calibrate(.3f));
  assert(tilt.update(.5f, true, 1) == fresh.update(.5f, true, 1));
  assert(tilt.update(std::numeric_limits<float>::quiet_NaN(), true, 1) == 0);
  fresh.reset();
  assert(tilt.update(.5f, true, 1) == fresh.update(.5f, true, 1));
  assert(tilt.update(std::numeric_limits<float>::infinity(), true, 1) == 0);
  assert(tilt.update(-std::numeric_limits<float>::infinity(), true, 1) == 0);
  for (float rejected : {1.5001f, -1.5001f, std::numeric_limits<float>::quiet_NaN(),
                         std::numeric_limits<float>::infinity()}) {
    assert(!tilt.calibrate(rejected) && tilt.neutral() == .3f);
  }
  assert(tilt.calibrate(1.5f) && tilt.neutral() == 1.5f);
  assert(tilt.update(1.5f, true, 2) == 0);
  assert(tilt.calibrate(-1.5f) && tilt.neutral() == -1.5f);
  assert(tilt.update(-1.5f, true, 0) == 0);
  assert(tilt.calibrate(-.2f));
  assert(tilt.update(-.4f, true, 1) < 0);
  tilt.reset();
  assert(tilt.neutral() == -.2f && tilt.update(-.2f, true, 1) == 0);
}

static void rightButtonPausesOnShortRelease() {
  ArcadeControls controls;
  auto events = controls.update(false, false, true, 0);
  assert(!events.pause && !events.zero);
  events = controls.update(true, false, true, 10);
  assert(!events.pause);
  events = controls.update(true, false, true, 300);
  assert(!events.pause);
  events = controls.update(false, false, true, 709);
  assert(events.pause && !events.zero);
  assert(!controls.update(false, false, true, 710).pause);
  assert(!controls.update(true, false, true, 1000).pause);
  assert(!controls.update(false, false, true, 1700).pause);
  assert(!controls.update(true, false, true, 2000).pause);
  assert(!controls.update(true, false, true, 2700).pause);
  assert(!controls.update(false, false, true, 4000).pause);
  assert(!controls.update(true, false, true, UINT32_MAX - 50u).pause);
  assert(controls.update(false, false, true, 50).pause);
}

static void menuTransitionsCancelRightButton() {
  ArcadeControls controls;
  assert(!controls.update(true, false, false, 10).pause);
  assert(!controls.update(false, false, true, 20).pause);
  assert(!controls.update(true, false, true, 30).pause);
  assert(!controls.update(true, false, false, 40).pause);
  assert(!controls.update(false, false, true, 50).pause);
  assert(!controls.update(true, false, true, 60).pause);
  assert(!controls.update(false, false, false, 70).pause);
  assert(!controls.update(false, false, true, 80).pause);
  assert(!controls.update(true, false, true, 90).pause);
  controls.reset();
  assert(!controls.update(true, false, true, 100).pause);
  assert(!controls.update(false, false, true, 110).pause);
  assert(!controls.update(true, false, true, 120).pause);
  assert(controls.update(false, false, true, 130).pause);
  controls.reset();
  assert(!controls.update(false, false, true, 140).pause);
  assert(!controls.update(true, false, true, 150).pause);
  assert(controls.update(false, false, true, 160).pause);
  auto events = controls.update(false, true, true, 170);
  assert(events.zero && !events.pause);
  events = controls.update(false, true, false, 180);
  assert(!events.zero && !events.pause);
  assert(!controls.update(false, false, true, 190).zero);
}

static void actionRequiresACompleteRelease() {
  ArcadeControls controls;
  assert(!controls.armed());
  ArcadeInput input = controls.input(2000, true, true);
  assert(input.move == 1024 && !input.action && !input.pressed && !controls.armed());
  input = controls.input(-2000, false, true);
  assert(input.move == -1024 && !input.action && !input.pressed && !controls.armed());
  input = controls.input(13, true, false);
  assert(input.move == 13 && !input.action && !input.pressed && !controls.armed());
  input = controls.input(0, false, false);
  assert(!input.action && !input.pressed && controls.armed());
  input = controls.input(-1024, true, true);
  assert(input.move == -1024 && input.action && input.pressed);
  input = controls.input(1024, true, false);
  assert(input.move == 1024 && input.action && !input.pressed);
  input = controls.input(0, false, false);
  assert(!input.action && !input.pressed && controls.armed());
  controls.blockAction();
  assert(!controls.armed());
  input = controls.input(std::numeric_limits<int>::max(), true, true);
  assert(input.move == 1024 && !input.action && !input.pressed);
  controls.reset();
  input = controls.input(std::numeric_limits<int>::min(), true, false);
  assert(input.move == -1024 && !input.action && !input.pressed && !controls.armed());
  controls.input(0, false, true);
  assert(!controls.armed());
  controls.input(0, false, false);
  assert(controls.armed());
  input = controls.input(321, false, true);
  assert(input.move == 321 && !input.action && input.pressed);
}

struct GuardedCanvas {
  static constexpr uint16_t Guard = 0xa55a;
  enum { Padding = 8, Count = ArcadeCanvas::Width * ArcadeCanvas::Height };
  std::vector<uint16_t> storage;
  ArcadeCanvas canvas;
  GuardedCanvas() : storage(Count + Padding * 2, Guard), canvas(storage.data() + Padding) {}
  uint16_t at(int x, int y) const { return storage[Padding + y * ArcadeCanvas::Width + x]; }
  void guards() const {
    for (int i = 0; i < Padding; ++i) {
      assert(storage[i] == Guard);
      assert(storage[storage.size() - 1 - i] == Guard);
    }
  }
};

constexpr uint16_t GuardedCanvas::Guard;

static void canvasPixelsRectsAndLinesClip() {
  assert(ArcadeCanvas::Width == 135 && ArcadeCanvas::Height == 240);
  assert(ArcadeCanvas::rgb(0, 0, 0) == 0 && ArcadeCanvas::rgb(255, 255, 255) == 0xffff);
  assert(ArcadeCanvas::rgb(255, 0, 0) == 0xf800 && ArcadeCanvas::rgb(0, 255, 0) == 0x07e0);
  assert(ArcadeCanvas::rgb(0, 0, 255) == 0x001f);
  GuardedCanvas c;
  c.canvas.clear(0x1234);
  for (int y = 0; y < ArcadeCanvas::Height; ++y)
    for (int x = 0; x < ArcadeCanvas::Width; ++x) assert(c.at(x, y) == 0x1234);
  c.canvas.pixel(-1, 0, 1); c.canvas.pixel(0, -1, 1);
  c.canvas.pixel(135, 0, 1); c.canvas.pixel(0, 240, 1);
  c.canvas.pixel(134, 239, 2);
  assert(c.at(0, 0) == 0x1234 && c.at(134, 239) == 2);
  c.canvas.rect(-2, -3, 5, 6, 3);
  c.canvas.rect(133, 238, 5, 5, 4);
  for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x)
    assert(c.at(x, y) == (x < 3 && y < 3 ? 3 : 0x1234));
  assert(c.at(133, 238) == 4 && c.at(134, 239) == 4 && c.at(132, 238) == 0x1234);
  const auto before = c.storage;
  c.canvas.rect(0, 0, 0, 5, 9); c.canvas.rect(0, 0, 5, -1, 9);
  c.canvas.rect(-10, -10, 2, 2, 9); c.canvas.rect(140, 245, 2, 2, 9);
  assert(c.storage == before);
  c.canvas.clear(0);
  c.canvas.line(-3, 2, 137, 2, 5);
  c.canvas.line(3, 242, 3, -2, 6);
  for (int x = 0; x < 135; ++x) assert(c.at(x, 2) == (x == 3 ? 6 : 5));
  for (int y = 0; y < 240; ++y) assert(c.at(3, y) == 6);
  c.canvas.line(10, 10, 14, 14, 7);
  c.canvas.line(14, 20, 10, 24, 8);
  for (int i = 0; i < 5; ++i) {
    assert(c.at(10 + i, 10 + i) == 7);
    assert(c.at(14 - i, 20 + i) == 8);
  }
  c.canvas.line(134, 239, 134, 239, 9);
  assert(c.at(134, 239) == 9);
  c.guards();
}

static void canvasTextAndNumbersRespectBounds() {
  GuardedCanvas c;
  c.canvas.clear(0);
  c.canvas.text("A", 3, 4, 0xffff);
  unsigned ink = 0;
  for (int y = 0; y < ArcadeCanvas::Height; ++y) for (int x = 0; x < ArcadeCanvas::Width; ++x) {
    const bool inGlyph = x >= 3 && x < 3 + MenuFont::Width && y >= 4 && y < 4 + MenuFont::Height;
    const bool filled = inGlyph && (MenuFont::glyph('A')[y - 4] & (1u << (MenuFont::Width - 1 - (x - 3))));
    assert(c.at(x, y) == (filled ? 0xffff : 0));
    if (filled) ++ink;
  }
  assert(ink > 0);
  c.canvas.text("Clipped", -5, -4, 1);
  c.canvas.text("Clipped", 132, 237, 2);
  c.canvas.text(nullptr, 0, 0, 3);
  c.guards();
  GuardedCanvas number, text;
  number.canvas.clear(0); text.canvas.clear(0);
  number.canvas.number(UINT32_MAX, 4, 8, 0x1234);
  text.canvas.text("4294967295", 4, 8, 0x1234);
  assert(number.storage == text.storage);
  number.canvas.clear(0); text.canvas.clear(0);
  number.canvas.number(0, 132, 237, 0xffff);
  text.canvas.text("0", 132, 237, 0xffff);
  assert(number.storage == text.storage);
  number.guards(); text.guards();
}

int main() {
  tiltDeadzoneSensitivityAndPolarity();
  tiltFreshnessAndCalibration();
  rightButtonPausesOnShortRelease();
  menuTransitionsCancelRightButton();
  actionRequiresACompleteRelease();
  canvasPixelsRectsAndLinesClip();
  canvasTextAndNumbersRespectBounds();
}
