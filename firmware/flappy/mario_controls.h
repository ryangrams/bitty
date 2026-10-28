#pragma once
#include "mario_viewport.h"

namespace flappy {

// Title selection pauses emulation. Launch uses native Select and Start inputs.
class MarioFrontend {
 public:
  struct Input { uint8_t buttons = 0; bool runFrame = true, gameplay = false, failed = false; };
  void reset(bool enabled = true) {
    enabled_ = enabled; phase_ = Boot; frames_ = 0;
    players_ = 1; previous_ = blocked_ = wait_ = stable_ = 0;
    confirm_ = continuing_ = queuedContinue_ = false;
  }
  bool selecting() const { return enabled_ && phase_ == Choice; }
  bool launching() const { return enabled_ && phase_ != Running && phase_ != Failed; }
  uint8_t players() const { return players_; }

  Input step(MarioCartridge::Screen screen, uint8_t pad, bool twoPlayers) {
    Input out;
    if (!enabled_) return out;
    uint8_t pressed = uint8_t(pad & ~previous_); previous_ = pad;
    blocked_ &= pad; pressed &= uint8_t(~blocked_);
    const bool chord = (pad & 3) == 3 && (pressed & 3);
    if (phase_ == Failed) { out.failed = true; out.runFrame = false; return out; }
    if (phase_ == Running) {
      if (screen == MarioCartridge::Screen::GameOver && chord) queuedContinue_ = true;
      if (screen != MarioCartridge::Screen::Title) {
        out.gameplay = screen == MarioCartridge::Screen::Playing;
        return out;
      }
      players_ = twoPlayers ? 2 : 1;
      enterChoice(pad);
      if (queuedContinue_) { confirm_ = continuing_ = true; wait_ = 1; }
      queuedContinue_ = false;
      pressed = 0;
    }
    if (phase_ == Boot) {
      if (screen == MarioCartridge::Screen::Title) {
        if (++stable_ >= 2) enterChoice(pad);
      } else stable_ = 0;
      if (phase_ == Boot) {
        out.buttons = pressed & 8;
        if (++frames_ >= 900) { phase_ = Failed; out.failed = true; }
        return out;
      }
      pressed = 0;
    }
    if (phase_ == Choice) {
      out.runFrame = false;
      if (!confirm_ && (pressed & 0xf4)) players_ = uint8_t(3 - players_);
      if (chord) { confirm_ = continuing_ = true; wait_ = 6; }
      else if (!confirm_ && (pressed & 11)) { confirm_ = true; continuing_ = false; wait_ = 6; }
      if (!confirm_ || (wait_ && --wait_)) return out;
      phase_ = Sync; frames_ = wait_ = stable_ = 0;
    }
    out.runFrame = true;
    if (++frames_ >= 900) { phase_ = Failed; out.failed = true; return out; }
    if (phase_ == Sync) {
      if (screen != MarioCartridge::Screen::Title) return out;
      if (wait_) { --wait_; return out; }
      if (twoPlayers != (players_ == 2)) { out.buttons = 4; wait_ = 20; return out; }
      phase_ = continuing_ ? HoldA : Start;
      return out;
    }
    if (phase_ == HoldA) { out.buttons = 1; phase_ = Start; return out; }
    if (phase_ == Start) { out.buttons = continuing_ ? 9 : 8; phase_ = Waiting; return out; }
    if (phase_ == Waiting) {
      if (screen == MarioCartridge::Screen::Playing) {
        if (++stable_ >= 8) { phase_ = Running; out.gameplay = true; }
      } else stable_ = 0;
    }
    return out;
  }

 private:
  enum Phase : uint8_t { Boot, Choice, Sync, HoldA, Start, Waiting, Running, Failed };
  uint16_t frames_ = 0;
  uint8_t players_ = 1, previous_ = 0, blocked_ = 0, wait_ = 0, stable_ = 0;
  Phase phase_ = Boot;
  bool enabled_ = false, confirm_ = false, continuing_ = false, queuedContinue_ = false;
  void enterChoice(uint8_t pad) {
    phase_ = Choice; blocked_ = pad; confirm_ = continuing_ = false; wait_ = 0;
  }
};

// Poll physical keys freely; action pulse lengths count actual NES frames.
class MarioButtons {
 public:
  enum class Context : uint8_t { Off, Title, Play };
  static constexpr uint32_t MenuHoldMs = 700, TapMs = 180, DoubleTapMs = 250;
  static constexpr uint8_t JumpFrames = 12;
  void reset() { *this = MarioButtons(); }
  void block() {
    context_ = Context::Off; armed_ = false;
    jump_ = fire_ = fireRelease_ = menu_ = 0; tap_ = paired_ = false;
  }
  bool facingLeft() const { return left_; }
  bool playing() const { return context_ == Context::Play; }
  void poll(bool front, bool right, bool leftClick, Context context, uint32_t now) {
    if (context != context_) {
      context_ = context; armed_ = false;
      jump_ = fire_ = fireRelease_ = menu_ = 0;
      tap_ = paired_ = false;
    }
    if (context == Context::Off) { armed_ = false; return; }
    if (!armed_) {
      front_ = front; right_ = right;
      if (!front && !right && !leftClick) armed_ = true;
      return;
    }
    if (front && !front_) {
      frontAt_ = now;
      if (context == Context::Title) menu_ |= 8;
      else {
        paired_ = tap_ && uint32_t(now - tapAt_) <= DoubleTapMs;
        if (paired_) left_ = !left_;
        tap_ = false;
      }
    }
    if (!front && front_ && context == Context::Play) {
      tap_ = !paired_ && uint32_t(now - frontAt_) <= TapMs;
      if (tap_) tapAt_ = now;
      paired_ = false;
    }
    if (right && !right_) rightAt_ = now;
    const bool rightClick = !right && right_ && uint32_t(now - rightAt_) < MenuHoldMs;
    if (context == Context::Title) {
      if (rightClick || leftClick) menu_ |= 4;
    } else {
      if (rightClick) jump_ = JumpFrames;
      if (leftClick && fire_ < 3) ++fire_;
    }
    front_ = front; right_ = right;
  }
  uint8_t frame() {
    if (!armed_ || context_ == Context::Off) return 0;
    if (context_ == Context::Title) { const uint8_t out = menu_; menu_ = 0; return out; }
    uint8_t out = uint8_t(2 | (front_ ? (left_ ? 64 : 128) : 0));
    if (jump_) { out |= 1; --jump_; }
    if (fireRelease_) fireRelease_ = 0;
    else if (fire_) { --fire_; fireRelease_ = 1; out &= uint8_t(~2); }
    return out;
  }
 private:
  uint32_t frontAt_ = 0, tapAt_ = 0, rightAt_ = 0;
  uint8_t jump_ = 0, fire_ = 0, fireRelease_ = 0, menu_ = 0;
  Context context_ = Context::Off;
  bool armed_ = false, front_ = false, right_ = false, tap_ = false, paired_ = false, left_ = false;
};

// A shared controller can be passed between Mario and Luigi's alternating turns.
inline uint8_t marioSecondPort(uint8_t buttons, uint8_t players) { return players == 2 ? buttons : 0; }
}  // namespace flappy
