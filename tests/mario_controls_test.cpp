#include "../firmware/flappy/mario_controls.h"
#include <assert.h>
#include <iostream>

using flappy::MarioButtons;
using flappy::MarioCartridge;
using flappy::MarioFrontend;
using Screen = MarioCartridge::Screen;
using Context = MarioButtons::Context;

static void title(MarioFrontend& menu, uint8_t held = 0) {
  menu.reset();
  for (unsigned i = 0; i < 20; ++i) assert(!menu.step(Screen::Other,held,false).buttons);
  menu.step(Screen::Title,held,false);
  const auto out = menu.step(Screen::Title,held,false);
  assert(menu.selecting() && !out.runFrame && !out.buttons);
}

static void selectionAndNativeLaunch() {
  MarioFrontend menu;
  title(menu,1);
  for (unsigned frame = 0; frame < 72000; ++frame) {
    const auto out = menu.step(Screen::Title,1,false);
    assert(menu.selecting() && !out.runFrame && !out.buttons && !out.failed);
  }
  menu.step(Screen::Title,0,false);
  for (const uint8_t navigation : {uint8_t(4),uint8_t(16),uint8_t(32),uint8_t(64),uint8_t(128)}) {
    menu.step(Screen::Title,navigation,false);
    assert(menu.players() == 2);
    menu.step(Screen::Title,0,false);
    menu.step(Screen::Title,navigation,false);
    assert(menu.players() == 1);
    menu.step(Screen::Title,0,false);
  }
  for (unsigned players = 1; players <= 2; ++players) {
    title(menu);
    if (players == 2) { menu.step(Screen::Title,4,false); menu.step(Screen::Title,0,false); }
    menu.step(Screen::Title,8,false);
    unsigned selects = 0, starts = 0;
    bool nativeTwo = false;
    for (unsigned frame = 0; frame < 90; ++frame) {
      const auto out = menu.step(Screen::Title,0,nativeTwo);
      if (out.buttons == 4) { nativeTwo = !nativeTwo; ++selects; }
      starts += out.buttons == 8;
      assert(!out.failed);
      if (starts) break;
    }
    assert(selects == players - 1 && starts == 1 && nativeTwo == (players == 2));
    for (unsigned frame = 0; frame < 7; ++frame) assert(!menu.step(Screen::Playing,0,nativeTwo).gameplay);
    assert(menu.step(Screen::Playing,0,nativeTwo).gameplay && !menu.launching());
    for (unsigned pad = 0; pad < 256; ++pad) {
      assert(menu.step(Screen::Playing,uint8_t(pad),nativeTwo).gameplay);
      assert(flappy::marioSecondPort(uint8_t(pad),uint8_t(players)) == (players == 2 ? pad : 0));
    }
    menu.step(Screen::GameOver,0,nativeTwo);
    assert(menu.step(Screen::Title,8,nativeTwo).runFrame == false);
    for (unsigned frame = 0; frame < 30; ++frame) assert(!menu.step(Screen::Title,8,nativeTwo).runFrame);
    menu.step(Screen::Title,0,nativeTwo);
    assert(menu.selecting() && menu.players() == players);
  }
}

static void continueAndFailure() {
  MarioFrontend menu; title(menu);
  menu.step(Screen::Title,2,false);
  menu.step(Screen::Title,3,false);
  bool heldA = false, continued = false;
  for (unsigned frame = 0; frame < 50; ++frame) {
    const auto out = menu.step(Screen::Title,0,false);
    if (out.buttons == 1) heldA = true;
    if (out.buttons == 9) { assert(heldA); continued = true; }
    assert(out.buttons != 8);
  }
  assert(continued);
  for (unsigned i = 0; i < 8; ++i) menu.step(Screen::Playing,0,false);
  menu.step(Screen::GameOver,3,false);
  for (unsigned i = 0; i < 100; ++i) menu.step(Screen::Other,0,false);
  heldA = continued = false;
  for (unsigned i = 0; i < 50; ++i) {
    const auto out = menu.step(Screen::Title,0,false);
    heldA |= out.buttons == 1;
    if (out.buttons == 9) { assert(heldA); continued = true; }
  }
  assert(continued);
  menu.reset(false);
  for (unsigned i = 0; i < 1000; ++i) assert(!menu.step(Screen::Other,0,false).failed);
  menu.reset();
  for (unsigned i = 0; i < 899; ++i) assert(!menu.step(Screen::Other,0,false).failed);
  assert(menu.step(Screen::Other,0,false).failed);
}

static void arm(MarioButtons& buttons, Context context, uint32_t now = 0) {
  buttons.poll(false,false,false,context,now);
  assert(buttons.frame() == (context == Context::Play ? 2 : 0));
}

static void movementAndDoubleTap() {
  MarioButtons buttons;
  arm(buttons,Context::Play);
  buttons.poll(true,false,false,Context::Play,10);
  assert(buttons.frame() == 130 && !buttons.facingLeft());
  buttons.poll(true,false,false,Context::Play,10000);
  assert(buttons.frame() == 130);
  buttons.poll(false,false,false,Context::Play,10001);
  assert(buttons.frame() == 2);
  buttons.poll(true,false,false,Context::Play,10050);
  assert(buttons.frame() == 130);
  buttons.poll(false,false,false,Context::Play,10100);
  buttons.poll(true,false,false,Context::Play,10350);
  assert(buttons.frame() == 66 && buttons.facingLeft());
  buttons.poll(false,false,false,Context::Play,10360);
  buttons.poll(true,false,false,Context::Play,10370);
  assert(buttons.frame() == 66);
  buttons.poll(false,false,false,Context::Play,10380);
  buttons.poll(true,false,false,Context::Play,10631);
  assert(buttons.frame() == 66);
  buttons.poll(false,false,false,Context::Play,10640);
  buttons.poll(true,false,false,Context::Play,10680);
  assert(buttons.frame() == 130 && !buttons.facingLeft());
  buttons.reset();
  const uint32_t at = UINT32_MAX - 40;
  arm(buttons,Context::Play,at - 1);
  buttons.poll(true,false,false,Context::Play,at);
  buttons.poll(false,false,false,Context::Play,at + 30);
  buttons.poll(true,false,false,Context::Play,at + 100);
  assert(buttons.frame() == 66 && buttons.facingLeft());
}

static void jumpFireAndMenuHold() {
  MarioButtons buttons; arm(buttons,Context::Play);
  buttons.poll(true,true,false,Context::Play,10);
  for (unsigned i = 0; i < 20; ++i) assert(buttons.frame() == 130);
  buttons.poll(true,false,false,Context::Play,100);
  for (unsigned i = 0; i < MarioButtons::JumpFrames; ++i) assert(buttons.frame() == 131);
  assert(buttons.frame() == 130);
  buttons.poll(true,false,true,Context::Play,110);
  assert(buttons.frame() == 128);
  assert(buttons.frame() == 130);
  buttons.poll(true,false,true,Context::Play,120);
  assert(buttons.frame() == 128 && buttons.frame() == 130);
  buttons.poll(true,true,false,Context::Play,200);
  buttons.poll(true,false,false,Context::Play,900);
  assert(buttons.frame() == 130);
  buttons.poll(true,true,false,Context::Play,1000);
  buttons.poll(true,false,false,Context::Play,1050);
  buttons.poll(true,false,true,Context::Play,1051);
  assert(buttons.frame() == 129 && buttons.frame() == 131);

  buttons.poll(true,true,false,Context::Off,1100);
  assert(buttons.frame() == 0);
  buttons.poll(true,true,false,Context::Play,2000);
  assert(buttons.frame() == 0);
  buttons.poll(false,false,false,Context::Play,2100);
  assert(buttons.frame() == 2);
  buttons.poll(false,false,false,Context::Title,2200);
  buttons.poll(false,true,false,Context::Title,2300);
  buttons.poll(false,false,false,Context::Title,2350);
  assert(buttons.frame() == 4 && buttons.frame() == 0);
  buttons.poll(true,false,false,Context::Title,2400);
  assert(buttons.frame() == 8);
  buttons.poll(true,false,false,Context::Title,5000);
  assert(buttons.frame() == 0);
  buttons.poll(false,true,false,Context::Title,5100);
  buttons.poll(false,false,false,Context::Title,5800);
  assert(buttons.frame() == 0);
}

int main() {
  selectionAndNativeLaunch(); continueAndFailure(); movementAndDoubleTap(); jumpFireAndMenuHold();
  std::cout << "Mario controls: deliberate player choice, native 2P/continue, held-entry blocking, movement, double taps, jump pulses, fire edges and menu holds passed; state "
            << sizeof(MarioFrontend) + sizeof(MarioButtons) << " bytes\n";
}
