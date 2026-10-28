#pragma once

#include <stdint.h>
#ifdef M5FLAP_JETPACK_IMPORTED
#include "jetpack_imported.h"
#endif
#include "arcade_support.h"
#include "jetpack_art.h"
#include "jetpack_progression.h"
#include "jetpack_profile.h"
#include "jetpack_rewards.h"
#include "jetpack_missions.h"
#include "jetpack_vehicle_physics.h"
#include "jetpack_spin.h"
#include "jetpack_time_step.h"
#include "jetpack_rocket_time.h"
#include "jetpack_rocket_course.h"
#include "jetpack_placement.h"
#ifdef M5FLAP_JETPACK_IMPORTED
static_assert(flappy::JetpackImported::MaxPatternPoints <= 512, "Imported coin group exceeds collection mask");
#endif

namespace flappy {

// Original artwork and a fixed 60 Hz simulation on the native landscape grid.
class JetpackGame {
 public:
  enum { Width = 240, Height = 135, Unit = 256, FlightUnit = 65536, PlayerX = 43,
         Ceiling = 22, Floor = 114, ZapCount = 6, CoinCount = 160, ScientistCount = 8,
         MissileCount = 24, AttractedCoinCount = 48, RocketChoiceCount = 6 };
  enum class Phase : uint8_t { Ready, Running, Crashing, Over, Spin };
  enum class Vehicle : uint8_t { None, Armor, Gravity, Hog, Mech, Bird, Teleporter, Dragon, SAM, Random };
  struct ImportedGroup { int32_t x = 0; int16_t pattern = -1, pitchQ8 = 0, topQ8 = 0; uint8_t step = 0, top = 0, collected[64] = {}; };
  enum class Power : uint8_t { None, Shield, Magnet, DoubleCoins, CashDash, Boost, RocketTime, Count };
  struct Bonus { int32_t x = 0; int16_t y = 0; Power kind = Power::None; bool active = false; };
  struct Token { int32_t x = 0; int16_t y = 0; uint8_t kind = 0; bool active = false; };
  struct Scientist { int32_t x = 0; int16_t y = 0, velocity = 0; uint8_t state = 0, age = 0; bool active = false; };
  struct Rotator { int32_t x = 0; int16_t y = 0; uint8_t angle = 0; bool active = false;
    uint16_t period = 112, clock = 0, lengthQ8 = 32 * Unit; bool clockwise = true; };
  struct Laser { int16_t y = 0; uint16_t ticks = 0; bool active = false; };
  struct Zap {
    int32_t x = 0;
    int16_t y = 0, dx = 0, dy = 0;
    bool active = false;
  };
  struct Coin { int32_t x = 0; int16_t y = 0; bool active = false; uint8_t value = 1; };
  struct AttractedCoin {
    int32_t x = 0;
    int16_t y = 0, vx = 0, vy = 0;
    uint8_t age = 0, value = 1;
    uint16_t lockQ8 = 0;
    bool falling = false, gem = false;
  };
  struct Missile {
    int32_t x = 0;
    int16_t y = 0;
    uint8_t warning = 0;
    bool active = false;
    uint32_t clockQ8 = 0;
    int16_t yQ8 = 0, vxQ8 = 0, vyQ8 = 0;
    uint16_t chaseQ8 = 0;
    uint16_t launchTicks = 0, warningStart = 0, trailQ8 = 0;
    uint8_t rocketFlags = 0;
  };
  struct Pickup {
    int32_t x = 0;
    int16_t y = 0;
    Vehicle kind = Vehicle::Armor;
    bool active = false;
  };
  using RunStats = JetpackMissions::Stats;

  JetpackGame() { reset(); }
  void reset(uint32_t seed = 1, uint8_t difficulty = 1) {
    settleRun();
    seed_ = seed ? seed : 1;
    random_ = seed_;
    difficulty_ = difficulty > 2 ? 2 : difficulty;
    nextDifficulty_ = difficulty_;
    phase_ = Phase::Ready;
    frame_ = 0;
    world_ = 0;
    coins_ = 0;
    y_ = Floor * FlightUnit;
    velocity_ = 0;
    progression_.reset(seed_ ^ 0x6a09e667u);
#ifdef M5FLAP_JETPACK_PLACEMENT
    progression_.setSourceSpacing(true);
#endif
    crashTicks_ = 0;
    immune_ = 0;
    vehicleHint_ = 0;
    vehicle_ = Vehicle::None;
    gravityUp_ = false;
    armed_ = false;
    thrust_ = false;
    pickup_ = Pickup{};
    for (Pickup& choice : rocketChoices_) choice = Pickup{};
    rocketTime_.cancel(); movementScale_ = FlightUnit; rocketNativeSpeed_ = 175 * FlightUnit;
    rocketWaves_ = 0; rocketShots_ = rocketDrops_ = 0;
    spinPresentation_.reset();
    for (Zap& zap : zaps_) zap = Zap{};
    for (Coin& coin : coinField_) coin = Coin{};
    for (AttractedCoin& coin : attractedCoins_) coin = AttractedCoin{};
    for (Missile& missile : missiles_) missile = Missile{};
    cue_ = ArcadeCue::None;
    bonus_ = Bonus{}; token_ = Token{}; samToken_ = Token{}; laser_ = Laser{};
    for (ImportedGroup& group : importedGroups_) group = ImportedGroup{};
    for (Rotator& rotator : rotators_) rotator = Rotator{};
    for (Scientist& scientist : scientists_) scientist = Scientist{};
    for (unsigned i = 0; i < unsigned(Power::Count); ++i) powerTicks_[i] = 0;
    spinTokens_ = samLetters_ = spinTicks_ = reward_ = samHits_ = 0;
    scientistInteractions_ = 0; crashBounces_ = 0; deathSpeed_ = 0;
    vehicleTicks_ = 0; vehiclePhysics_ = JetpackVehiclePhysics::State{};
    vehicleInputBlocked_ = false; blastUsed_ = revived_ = spinResult_ = false;
    rewardCoins_ = 0; fxAge_ = 32;
    powerEndCash_ = powerEndBoost_ = 0;
    cashStart_ = cashNextCoin_ = 0; cashPayload_ = Bonus{}; boostExtraVelocity_ = 0;
    cashShowerEmitted_ = 0; cashShowerRemaining_ = 0; cashShowerGemPercent_ = cashNextValue_ = 0;
    cashShowerX_ = PlayerX * Unit; cashShowerY_ = 70 * Unit;
    coinPattern_ = -1; patternCursor_ = 0; patternX_ = 0;
    if (finalSpinInitialized_) finalSpin_.resetWeights();
    lastAward_ = JetpackRewards::Award{JetpackRewards::Kind::Coins, 0, 0};
    lastSpinWon_ = false; lastSpinMatches_ = 0;
    stats_ = RunStats{}; footTravel_ = hogTravel_ = 0; shieldHits_ = 0;
    for (uint8_t& level : activeUpgrades_) level = 0;
    runStarted_ = runSettled_ = false;
  }

  void tick(const ArcadeInput& input) {
    ++frame_;
    if (fxAge_ < 32) ++fxAge_;
    if (!input.action && !input.pressed) armed_ = true;
    const bool press = input.pressed && armed_;
    if (press) armed_ = false;
    if (phase_ == Phase::Ready) {
      thrust_ = false;
      if (press) {
        phase_ = Phase::Running;
        thrust_ = input.action || input.pressed;
        velocity_ = 0;
        applyLoadout();
        signal(ArcadeCue::Fire);
      }
      return;
    }
    if (phase_ == Phase::Over) {
      thrust_ = false;
      if (press) {
        reset(seed_ + 0x9e3779b9u, nextDifficulty_);
        phase_ = Phase::Running;
        thrust_ = input.action || input.pressed;
        velocity_ = 0;
        applyLoadout();
        signal(ArcadeCue::Fire);
      }
      return;
    }
    if (phase_ == Phase::Spin) { tickSpin(press); observeRun(); return; }
    if (phase_ == Phase::Crashing) { tickCrash(press); observeRun(); return; }

    if (immune_) --immune_;
    if (vehicleHint_) --vehicleHint_;
    updatePowers();
    if (unsigned(vehicle_) >= unsigned(Vehicle::Gravity)) JetpackVehiclePhysics::advanceSpeed(vehiclePhysics_,uint8_t(vehicle_));
    thrust_ = input.action || input.pressed;
    moveFlight(thrust_, press);

    const int32_t advance = JetpackTimeStep::scale(speed(),movementScale_);
    frameAdvance_ = advance;
    if (vehicle_ == Vehicle::None && y_ == Floor * FlightUnit) footTravel_ += uint32_t(advance);
    if (vehicle_ == Vehicle::Hog) hogTravel_ += uint32_t(advance);
    if (world_ < 0x70000000u) world_ += uint32_t(advance);
    updateObjects(advance);
    updateExtras(advance);
    spawnProgression();
    collect();
    collectExtras();
    interactScientists();
    if (!immune_ && !safePower()) collide();
    observeRun();
  }

  void setNextLoadout(uint8_t ride, uint8_t gadget, bool headstart) {
    nextRide_ = ride <= unsigned(Vehicle::Random) ? ride : 0;
    nextGadget_ = gadget <= 6 ? gadget : 0; nextHeadstart_ = headstart;
    if (nextHeadstart_) { nextRide_ = 0; if (nextGadget_ >= 4) nextGadget_ = 0; }
    else if (nextGadget_ >= 4) nextRide_ = 0;
  }
  void attachProfile(JetpackProfile* profile) { profile_ = profile; if (profile) pendingRewards_ = 0; }
  void setProfileEnabled(bool enabled) { profileEnabled_ = enabled; }
  void settleRun() {
    if (!runStarted_ || runSettled_) return;
    if (spinTokens_) {
      stats_.cashInTokens += spinTokens_;
      addCoins(JetpackRewards::cashInValue(spinTokens_)); spinTokens_ = 0;
    }
    runSettled_ = true;
    if (profile_) {
      profile_->setSamLetters(samLetters_);
      profile_->setSamCompensation(progression_.samCompensationOnDeath(nativeDistance()));
      profile_->observeRunStats(runStats());
      profile_->settleRun(coins_, distance());
    }
  }
  bool rankedRun() const { return rankedRun_; }
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  void setDiagnosticProtection(bool enabled) { diagnosticProtection_ = enabled; }
  bool diagnosticPower(uint8_t id, uint8_t level = 0) {
    if (phase_ != Phase::Running || !id || id >= uint8_t(Power::Count)) return false;
    if (vehicle_ != Vehicle::None && (id == unsigned(Power::Boost) || id == unsigned(Power::CashDash) || id == unsigned(Power::RocketTime))) return false;
    const int upgrade = id == unsigned(Power::Shield) ? JetpackProfile::Shield : id == unsigned(Power::Boost) ? JetpackProfile::Boost : id == unsigned(Power::CashDash) ? JetpackProfile::CashDash : id == unsigned(Power::RocketTime) ? JetpackProfile::RocketTime : -1;
    if (upgrade >= 0) activeUpgrades_[upgrade] = level > JetpackProfile::maxLevel(uint8_t(upgrade)) ? JetpackProfile::maxLevel(uint8_t(upgrade)) : level;
    activatePower(Power(id)); return true;
  }
  bool diagnosticCoinBurst() {
    if (phase_ != Phase::Running) return false;
    startCashShower(300,5,PlayerX * Unit,int16_t(coinTargetY() * Unit)); return true;
  }
  bool diagnosticSpin(uint16_t tokens) {
    if (!tokens || (phase_ != Phase::Running && phase_ != Phase::Crashing)) return false;
    spinPresentation_.reset();
    phase_ = Phase::Spin; spinTokens_ = tokens; spinTicks_ = 0; spinResult_ = false; armed_ = false;
    clearHazards(); return true;
  }
#endif
  void observeRun() { if (profile_) profile_->observeRunStats(runStats()); }
  const JetpackProgression& progression() const { return progression_; }
  uint32_t eventSerial() const { return progression_.serial(); }
  bool event(uint32_t serial, JetpackProgression::Event& result) const { return progression_.event(serial, result); }
  RunStats runStats() const {
    RunStats stats = stats_; stats.meters = distance();
    stats.footMeters = JetpackProgression::nativeDistance(footTravel_) / 18;
    stats.hogMeters = JetpackProgression::nativeDistance(hogTravel_) / 18;
    return stats;
  }
  const JetpackRewards::Award& lastAward() const { return lastAward_; }
  const JetpackSpin& spinPresentation() const { return spinPresentation_; }
  const JetpackRocketTime& rocketTime() const { return rocketTime_; }
  uint32_t movementScale() const { return movementScale_; }
  uint16_t rocketShotCount() const { return rocketShots_; }
  uint16_t rocketDroppedShots() const { return rocketDrops_; }
  uint8_t rocketWaveCount() const { return rocketWaves_; }
  unsigned rocketChoiceCount() const { unsigned n = 0; for (const Pickup& p : rocketChoices_) n += p.active; return n; }
  bool lastSpinWon() const { return lastSpinWon_; }
  uint8_t lastSpinMatches() const { return lastSpinMatches_; }
  uint16_t cashInSpinTokens() {
    if (phase_ != Phase::Spin || spinResult_ || spinPresentation_.running()) return 0;
    const uint16_t tokens = spinTokens_; spinTokens_ = 0;
    stats_.cashInTokens += tokens;
    addCoins(JetpackRewards::cashInValue(tokens));
    phase_ = Phase::Over; armed_ = false; settleRun();
    return tokens;
  }
  uint16_t powerTicks(Power power) const { return unsigned(power) < unsigned(Power::Count) ? powerTicks_[unsigned(power)] : 0; }
  uint16_t spinTokens() const { return spinTokens_; }
  uint8_t shieldHits() const { return shieldHits_; }
  unsigned attractedCoinCount() const {
    unsigned count = 0; for (const AttractedCoin& coin : attractedCoins_) count += coin.age != 0; return count;
  }
  uint16_t cashShowerPending() const { return cashShowerRemaining_; }
  uint8_t samLetters() const { return samLetters_; }
  uint8_t vehicleHits() const { return samHits_; }
  uint16_t scientistInteractions() const { return scientistInteractions_; }
  uint8_t crashBounces() const { return crashBounces_; }
  int teleportTargetY() const { return int(JetpackVehiclePhysics::markerY(vehiclePhysics_) / FlightUnit); }
  const Laser& laser() const { return laser_; }
  const Scientist& scientist(unsigned index) const { return scientists_[index < ScientistCount ? index : 0]; }
  void setNextDifficulty(uint8_t difficulty) { nextDifficulty_ = difficulty > 2 ? 2 : difficulty; }
  uint32_t score() const { return distance(); }
  uint32_t distance() const { return uint32_t(uint64_t(world_) * 10u / (Unit * 69u)); }
  uint32_t nativeDistance() const { return JetpackProgression::nativeDistance(world_); }
  uint32_t coins() const { return coins_; }
  bool gameOver() const { return phase_ == Phase::Over; }
  Phase phase() const { return phase_; }
  Vehicle vehicle() const { return vehicle_; }
  bool gravityUp() const { return gravityUp_; }
  int playerY() const { return int(y_ / FlightUnit); }
  int32_t speed() const {
    const int difficultyPercent = difficulty_ == 0 ? 85 : difficulty_ == 2 ? 115 : 100;
    const int64_t native = unsigned(vehicle_) >= unsigned(Vehicle::Gravity) ?
        JetpackVehiclePhysics::fittedSpeed(vehiclePhysics_,difficultyPercent) :
        int64_t(normalNativeVelocity()) * difficultyPercent / 100 + boostExtraVelocity_;
    return divideNearest(native * 23 * Unit, int64_t(60) * 60 * FlightUnit);
  }
  const Zap& zap(unsigned index) const { return zaps_[index < ZapCount ? index : 0]; }
  const Missile& missile(unsigned index) const { return missiles_[index < MissileCount ? index : 0]; }
  const Pickup& pickup() const { return pickup_; }
  ArcadeCue takeCue() { const ArcadeCue result = cue_; cue_ = ArcadeCue::None; return result; }

  void draw(uint16_t* pixels, uint32_t best = 0) const {
    if (!pixels) return;
    Canvas c(pixels);
    drawLab(c);
    if (phase_ == Phase::Running && magnetRadius()) JetpackArt::drawMagnetField(c, PlayerX, coinTargetY(), frame_, magnetRadius());
    for (const Scientist& scientist : scientists_) if (scientist.active)
      JetpackArt::drawScientist(c, int(scientist.x / Unit), int(scientist.y / Unit), frame_, scientist.state > 2 ? 1 : scientist.state);
    drawExtras(c);
    for (const Coin& coin : coinField_) if (coin.active) {
      if (coin.value >= 5) JetpackArt::drawGem(c, int(coin.x / Unit), coin.y, frame_);
      else drawCoin(c, int(coin.x / Unit), coin.y);
    }
    for (const AttractedCoin& coin : attractedCoins_) if (coin.age)
      JetpackArt::drawAttractedCoin(c, int(coin.x / Unit), int(coin.y / Unit), frame_, coin.gem ? 5 : 1, coin.vx, coin.vy);
    if (pickup_.active) drawPickup(c);
    for (const Pickup& choice : rocketChoices_) if (choice.active) {
      JetpackArt::drawPickupIcon(c,int(choice.x / Unit),choice.y,frame_,0);
      if (choice.x < Width * Unit) {
        const char labels[] = {'G','H','M','B','T','D'};
        const char label[] = {labels[unsigned(choice.kind) - 2],0};
        c.text(label,int(choice.x / Unit) - 2,choice.y + 10,rgb(248,216,80));
      }
    }
    for (const Zap& zap : zaps_) if (zap.active) drawZap(c, zap);
    for (const Missile& missile : missiles_) if (missile.active) drawMissile(c, missile);
    drawPlayer(c);
    if (fxAge_ < 32) JetpackArt::drawExplosion(c, fxX_, fxY_, fxAge_, fxKind_);
    c.rect(0, 0, Width, 15, rgb(16, 24, 40));
    c.rect(0, 14, Width, 1, rgb(64, 88, 104));
    c.text("M", 4, 1, rgb(80, 184, 208));
    c.number(distance(), 16, 1, rgb(224, 240, 216));
    if (vehicleHint_ && phase_ == Phase::Running) {
      c.text(vehicleHint(), 91, 1, rgb(248, 216, 80));
    } else {
      drawCoin(c, 86, 7);
      c.number(coins_, 94, 1, rgb(248, 216, 80));
      c.text("BEST", 159, 1, rgb(80, 184, 208));
      c.number(best > score() ? best : score(), 189, 1, rgb(224, 240, 216));
    }
    if (phase_ == Phase::Ready) {
      panel(c, "JETPACK JOYRIDE", "HOLD TO FLY", "TAP TO START");
    } else if (phase_ == Phase::Spin) {
      drawSpin(c);
    } else if (phase_ == Phase::Over) {
      panel(c, "RUN COMPLETE", "RELEASE, THEN TAP", "TO TRY AGAIN");
      c.text("METERS", 71, 96, rgb(248, 216, 80));
      c.number(score(), 113, 96, rgb(224, 240, 216));
    }
    if (phase_ == Phase::Running) {
      if (powerTicks_[unsigned(Power::Shield)] && (frame_ & 8)) {
        c.line(PlayerX - 12, playerY() - 9, PlayerX + 10, playerY() - 9, rgb(112, 213, 205));
        c.line(PlayerX - 12, playerY() + 9, PlayerX + 10, playerY() + 9, rgb(112, 213, 205));
      }
      char status[20];
      snprintf(status, sizeof(status), "%s%s%s T%u %c%c%c", powerTicks_[unsigned(Power::Magnet)] ? "M " : "",
          powerTicks_[unsigned(Power::DoubleCoins)] ? "2X " : "", safePower() ? "GO " : "", unsigned(spinTokens_),
          samLetters_ & 1 ? 'S' : '.', samLetters_ & 2 ? 'A' : '.', samLetters_ & 4 ? 'M' : '.');
      c.text(status, 3, 123, rgb(255, 224, 113));
    }
  }

 private:
  friend struct JetpackTestAccess;
  class Canvas {
   public:
    explicit Canvas(uint16_t* pixels) : pixels_(pixels) {}
    void pixel(int x, int y, uint16_t color) {
      if (unsigned(x) < Width && unsigned(y) < Height) pixels_[y * Width + x] = color;
    }
    void rect(int x, int y, int width, int height, uint16_t color) {
      if (width <= 0 || height <= 0) return;
      const int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
      const int right = x + width > Width ? Width : x + width;
      const int bottom = y + height > Height ? Height : y + height;
      for (int row = top; row < bottom; ++row)
        for (int col = left; col < right; ++col) pixels_[row * Width + col] = color;
    }
    void line(int x0, int y0, int x1, int y1, uint16_t color) {
      const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
      const int dy = y1 > y0 ? y0 - y1 : y1 - y0;
      const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
      int error = dx + dy;
      for (;;) {
        pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        const int twice = error * 2;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
      }
    }
    void text(const char* text, int x, int y, uint16_t color) {
      MenuFont::draw(pixels_, Width, Height, text, x, y, color, 1, 40);
    }
    void number(uint32_t value, int x, int y, uint16_t color) {
      char text[12];
      snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(value));
      this->text(text, x, y, color);
    }
   private:
    uint16_t* pixels_;
  };

  static uint16_t rgb(unsigned r, unsigned g, unsigned b) {
    return uint16_t((r & 248u) << 8 | (g & 252u) << 3 | b >> 3);
  }
  static int32_t bounded(int32_t value, int32_t low, int32_t high) {
    return value < low ? low : value > high ? high : value;
  }
  static int32_t divideNearest(int64_t value, int64_t divisor) {
    return int32_t(value < 0 ? -((-value + divisor / 2) / divisor) : (value + divisor / 2) / divisor);
  }
  int32_t normalNativeVelocity() const {
    const uint32_t meters = distance() < 2250 ? distance() : 2250;
    return 175 * FlightUnit + int32_t(int64_t(305) * FlightUnit * meters / 2250);
  }
  int32_t flightFactor() const {
    const int32_t native = vehicle_ == Vehicle::SAM ? vehiclePhysics_.nativeSpeed : normalNativeVelocity();
    return FlightUnit + (native - 175 * FlightUnit) / 350;
  }
  uint32_t random() {
    random_ ^= random_ << 13; random_ ^= random_ >> 17; random_ ^= random_ << 5;
    return random_;
  }
  void signal(ArcadeCue cue) {
    if (unsigned(cue) > unsigned(cue_)) cue_ = cue;
  }
  bool missilePresent() const {
    for (const Missile& missile : missiles_) if (missile.active) return true;
    return false;
  }
  bool anyZap() const { for (const Zap& zap : zaps_) if (zap.active) return true; return false; }
  bool zapNearPlayer() const {
    for (const Zap& zap : zaps_)
      if (zap.active && zap.x > 20 * Unit && zap.x < 255 * Unit) return true;
    return false;
  }
  void updateObjects(int32_t advance) {
    for (Zap& zap : zaps_) if (zap.active) {
      zap.x -= advance;
      if (zap.x + int32_t(zap.dx) * Unit < -8 * Unit) zap.active = false;
    }
    for (Coin& coin : coinField_) if (coin.active) {
      coin.x -= advance;
      if (coin.x < -5 * Unit) coin.active = false;
    }
    for (Pickup& choice : rocketChoices_) if (choice.active) {
      choice.x -= advance; if (choice.x < -12 * Unit) choice.active = false;
    }
    if (pickup_.active) {
      pickup_.x -= advance;
      if (pickup_.x < -12 * Unit) pickup_.active = false;
    }
    for (Missile& missile : missiles_) if (missile.active) {
      if (missile.rocketFlags & 128) { updateRocketMissile(missile); continue; }
      if (missile.warning) {
        missile.clockQ8 += (movementScale_ + 128) / 256;
        if (missile.clockQ8 < Unit) continue;
        missile.clockQ8 -= Unit;
        if (missile.warning > 30 && vehicle_ != Vehicle::SAM) missile.y = int16_t(playerY());
        if (!--missile.warning) missile.x = 254 * Unit;
      } else {
        missile.x -= JetpackTimeStep::scale(1540,movementScale_) + advance / 4;
        if (vehicle_ == Vehicle::SAM) {
          const bool armOverlap = missile.x <= (PlayerX + 29) * Unit && missile.x >= (PlayerX + 5) * Unit &&
              missile.y >= playerY() - 9 && missile.y <= playerY() + 9;
          if (armOverlap) { addCoins(3); signal(ArcadeCue::Hit); missile.active = false; }
          else if (missile.x < (PlayerX + 6) * Unit) { if (!immune_) hit(); missile.active = false; }
        }
        if (missile.x < -16 * Unit) missile.active = false;
      }
    }
  }
  void endRocketTime() {
    rocketTime_.cancel(); movementScale_ = FlightUnit;
    powerTicks_[unsigned(Power::RocketTime)] = 0;
    for (Pickup& choice : rocketChoices_) choice.active = false;
    for (Missile& missile : missiles_) if (missile.rocketFlags & 128) missile = Missile{};
    progression_.expirePower(nativeDistance(),uint8_t(Power::RocketTime));
  }
  void spawnRocketChoices(uint8_t count) {
    if (count > RocketChoiceCount) count = RocketChoiceCount;
    uint8_t types[] = {2,3,4,5,6,7};
    const int rows = count > 3 ? (count + 1) / 2 : count;
    const int32_t anchor = PlayerX * Unit + JetpackTimeStep::scale(speed(),movementScale_) * 120;
    for (uint8_t i = 0; i < count; ++i) {
      const uint8_t selected = uint8_t(i + random() % (6 - i));
      const uint8_t temp = types[i]; types[i] = types[selected]; types[selected] = temp;
      Pickup& p = rocketChoices_[i]; p.active = true; p.kind = Vehicle(types[i]);
      p.x = anchor + (i / rows) * 32 * Unit;
      p.y = int16_t(rows == 1 ? 68 : 30 + (i % rows) * 76 / (rows - 1));
    }
  }
  void spawnRocketWave() {
    JetpackRocketCourse::Wave wave;
    JetpackRocketCourse::choose(random_,wave);
    ++rocketWaves_;
    for (unsigned i = 0; i < wave.count; ++i) {
      Missile* slot = nullptr;
      for (Missile& missile : missiles_) if (!missile.active) { slot = &missile; break; }
      if (!slot) { rocketDrops_ = uint16_t(rocketDrops_ + wave.count - i); break; }
      ++rocketShots_;
      const auto& shot = wave.shots[i];
      Missile& m = *slot; m = Missile{}; m.active = true;
      m.x = (shot.direction < 0 ? 254 : -14) * Unit;
      m.y = int16_t(JetpackRocketCourse::screenY(shot,Ceiling,Floor)); m.yQ8 = int16_t(m.y * Unit);
      m.launchTicks = shot.launchTicks; m.warningStart = shot.warningTicks; m.warning = 255;
      m.chaseQ8 = uint16_t(divideNearest(int64_t(wave.chaseSpeedQ8) * 23,60));
      const float angle = shot.angleQ8 * (0.0174532925199433f / Unit);
      const float native = wave.missileSpeedQ8 / float(Unit) *
          ((shot.flags & JetpackRocketCourse::Ricochet) ? .8f : shot.speedQ8 / float(Unit));
      m.vxQ8 = int16_t(bounded(int32_t(lroundf(shot.direction * native * cosf(angle) * 23 * Unit / 60)), -16 * Unit,16 * Unit));
      m.vyQ8 = int16_t(bounded(int32_t(lroundf(-native * sinf(angle) * 23 * Unit / 60)), -16 * Unit,16 * Unit));
      m.rocketFlags = uint8_t(128 | shot.flags);
      const auto config = JetpackRocketTime::config(rocketTime_.level());
      if (random() % 100 < config.trailChancePercent) m.rocketFlags |= 32;
    }
  }
  void updateRocketTime() {
    if (!rocketTime_.active()) { movementScale_ = FlightUnit; return; }
    const auto events = rocketTime_.advance();
    movementScale_ = rocketTime_.timeScaleQ16(rocketNativeSpeed_);
    for (unsigned wave = 0; wave < 10; ++wave) if (events.waveMask & (1u << wave)) spawnRocketWave();
    if (events.choiceCount) spawnRocketChoices(events.choiceCount);
    if (events.ended) endRocketTime();
  }
  void updateRocketMissile(Missile& m) {
    const uint32_t stepQ8 = (movementScale_ + 128) / 256;
    m.clockQ8 += stepQ8;
    const uint32_t launchQ8 = uint32_t(m.launchTicks) * Unit;
    if (!(m.rocketFlags & 64)) {
      if (m.clockQ8 < uint32_t(m.warningStart) * Unit) {
        const int32_t chase = JetpackTimeStep::scale(m.chaseQ8,movementScale_);
        const int32_t target = playerY() * Unit;
        m.yQ8 = int16_t(m.yQ8 + bounded(target - m.yQ8,-chase,chase));
        m.y = int16_t(m.yQ8 / Unit);
      }
      if (m.clockQ8 < launchQ8) {
        m.warning = uint8_t(bounded(int32_t((launchQ8 - m.clockQ8 + 255) / Unit),1,255)); return;
      }
      m.rocketFlags |= 64; m.warning = 0;
    }
    const uint32_t factor = uint32_t(flightFactor());
    m.x += JetpackTimeStep::scale(divideNearest(int64_t(m.vxQ8) * factor,FlightUnit),movementScale_);
    int32_t y = m.yQ8 + JetpackTimeStep::scale(divideNearest(int64_t(m.vyQ8) * factor,FlightUnit),movementScale_);
    const int center = (Ceiling + Floor) * Unit / 2;
    const int span = divideNearest(int64_t(136) * 23 * Unit,60);
    const int top = center - span, bottom = center + span;
    if (y < top || y > bottom) {
      if (m.rocketFlags & JetpackRocketCourse::Ricochet) {
        if (y < top) y = 2 * top - y;
        if (y > bottom) y = 2 * bottom - y;
        m.vyQ8 = int16_t(-m.vyQ8);
      } else { m.active = false; return; }
    }
    m.yQ8 = int16_t(bounded(y,top,bottom)); m.y = int16_t(m.yQ8 / Unit);
    if (m.rocketFlags & 32) {
      m.trailQ8 = uint16_t(m.trailQ8 + stepQ8);
      if (m.trailQ8 >= 6 * Unit) {
        for (Coin& coin : coinField_) if (!coin.active) {
          coin.active = true; coin.x = m.x; coin.y = m.y; coin.value = 5;
          m.trailQ8 = uint16_t(m.trailQ8 - 6 * Unit); break;
        }
      }
    }
    if (m.x < -24 * Unit || m.x > 264 * Unit) m.active = false;
  }
  void drawSpin(Canvas& c) const {
    c.rect(28,31,184,88,rgb(16,24,40));
    c.rect(28,31,184,1,rgb(128,176,184)); c.rect(28,118,184,1,rgb(128,176,184));
    c.text("FINAL SPIN",90,36,rgb(248,216,80));
    spinPresentation_.draw(c);
    const bool rolling = spinPresentation_.running();
    const char* caption = rolling ? "SPINNING" : !spinResult_ ? "ACTION: SPIN" :
        !lastSpinWon_ ? "NO MATCH" : reward_ == 0 ? "SECOND CHANCE" : reward_ == 2 ? "BLAST!" :
        reward_ == 3 ? "500M HEADSTART" : reward_ == 4 ? "DOUBLE COINS" : reward_ == 5 ? "3 MORE TOKENS" : "BONUS COINS";
    unsigned n = 0; while (caption[n]) ++n;
    c.text(caption,120 - int(n)*3,98,rgb(224,240,216));
    if (!rolling && !spinResult_) c.text("LEFT/DOWN: CASH",78,109,rgb(128,176,184));
    else if (!rolling) { c.text("TAP TO CONTINUE",78,109,rgb(128,176,184)); }
    c.number(spinTokens_,40,73,rgb(248,216,80));
  }

  void spawnProgression() {
    if (rocketTime_.active()) return;
    JetpackProgression::Context context;
    context.nativeDistance = nativeDistance(); context.speedQ8 = speed();
    context.vehicle = uint8_t(vehicle_); context.samLetters = samLetters_;
    if (profile_ && rankedRun_ && profile_->missions().rank() < 5) context.powerUnlockedMask = 7;
    context.safePower = safePower();
    context.hazardsClear = !missilePresent() && !laser_.active && !anyZap();
    for (const Rotator& rotator : rotators_) if (rotator.active) context.hazardsClear = false;
    unsigned freeCoins = 0;
    for (const Coin& coin : coinField_) freeCoins += !coin.active;
    context.coinSlot = freeCoins >= 40;
#ifdef M5FLAP_JETPACK_IMPORTED
    context.coinSlot = false;
#endif
    for (const ImportedGroup& group : importedGroups_) if (group.pattern < 0) context.coinSlot = true;
    context.zapSlot = !missilePresent() && !laser_.active;
    bool freeZap = false; for (const Zap& zap : zaps_) freeZap |= !zap.active;
    context.zapSlot &= freeZap;
    context.missileSlot = !missilePresent(); context.laserSlot = !laser_.active;
    context.vehicleSlot = context.powerSlot = !pickup_.active && !bonus_.active;
    context.spinSlot = !token_.active; context.samSlot = !samToken_.active;
    context.scientistSlot = false;
    for (const Scientist& scientist : scientists_) if (!scientist.active) context.scientistSlot = true;
    if (powerTicks_[unsigned(Power::Shield)]) context.powerMask |= 1;
    if (powerTicks_[unsigned(Power::Boost)]) context.powerMask |= 2;
    if (powerTicks_[unsigned(Power::CashDash)]) context.powerMask |= 4;
    if (powerTicks_[unsigned(Power::RocketTime)]) context.powerMask |= 8;
    const JetpackProgression::Batch batch = progression_.step(context);
    for (unsigned i = 0; i < batch.count; ++i) {
      const JetpackProgression::Event& event = batch.events[i];
      using Kind = JetpackProgression::Kind;
      uint32_t span = 0;
      switch (event.kind) {
        case Kind::Coins: spawnCoins(); span = coinSpanNative(); break;
        case Kind::Zap: spawnZap(); span = zapSpawnSpan_; break;
        case Kind::RotatingZap:
          for (Rotator& rotator : rotators_) if (!rotator.active) {
            rotator.active = true; rotator.x = 265 * Unit;
            rotator.y = int16_t(48 + random() % 42); rotator.clock = uint16_t(random() % rotator.period);
            rotator.angle = uint8_t(unsigned(rotator.clock) * 256 / rotator.period); break;
          }
          span = 90; break;
        case Kind::Missile:
          for (Missile& missile : missiles_) if (!missile.active) {
            missile = Missile{}; missile.active = true; missile.warning = vehicle_ == Vehicle::SAM ? 50 : 80;
            missile.x = 254 * Unit;
            missile.y = vehicle_ == Vehicle::SAM ? int16_t(30 + random() % 77) : int16_t(playerY()); break;
          }
          span = 128; break;
        case Kind::Laser:
          laser_.active = true; laser_.ticks = 140; laser_.y = int16_t(playerY()); span = 360; break;
        case Kind::Vehicle:
          pickup_.active = true;
          pickup_.x = PlayerX * Unit + int32_t(JetpackProgression::screenQ8(event.encounterDistance - nativeDistance()));
          pickup_.y = int16_t(43 + random() % 53);
          pickup_.kind = Vehicle(event.detail); span = 48; break;
        case Kind::Power: {
          static const Power powers[] = {Power::None, Power::Shield, Power::Boost, Power::CashDash, Power::RocketTime};
          bonus_.active = true;
          bonus_.x = PlayerX * Unit + int32_t(JetpackProgression::screenQ8(event.encounterDistance - nativeDistance()));
          bonus_.y = int16_t(38 + random() % 60);
          bonus_.kind = powers[event.detail <= 4 ? event.detail : 1]; span = 48; break;
        }
        case Kind::SpinToken:
          token_.active = true; token_.x = 265 * Unit; token_.y = int16_t(35 + random() % 65); token_.kind = 0; break;
        case Kind::SamLetter:
          samToken_.active = true; samToken_.x = 265 * Unit; samToken_.y = int16_t(35 + random() % 65);
          samToken_.kind = event.detail;
          if (profile_) profile_->setSamCompensation(0);
          break;
        case Kind::Scientist:
          for (Scientist& scientist : scientists_) if (!scientist.active) {
            scientist.active = true; scientist.x = 255 * Unit; scientist.y = 114 * Unit;
            scientist.state = scientist.age = 0; scientist.velocity = 0; break;
          }
          break;
        default: break;
      }
      if (event.kind >= Kind::Coins && event.kind <= Kind::Power)
        progression_.completeContent(nativeDistance(), span, speed(), safePower());
    }
  }
  uint32_t coinSpanNative() const {
    return coinSpawnSpan_;
  }
#ifdef M5FLAP_JETPACK_OBSTACLES
  unsigned obstacleWeight(unsigned index, uint8_t profile, uint32_t meters, bool rotatingSlot) const {
    const auto& record = JetpackImported::Obstacles[index];
    if (record.profile != profile || meters < record.start || (record.period && !rotatingSlot)) return 0;
    for (unsigned i = 0; i < JetpackImported::ObstacleCount; ++i) {
      const auto& newer = JetpackImported::Obstacles[i];
      if (newer.profile == profile && newer.templateId == record.templateId &&
          newer.start > record.start && newer.start <= meters) return 0;
    }
    return record.weight;
  }
#endif
  void spawnZap() {
    zapSpawnSpan_ = 84;
#ifdef M5FLAP_JETPACK_OBSTACLES
    const uint8_t profile = vehicle_ == Vehicle::Gravity ? 2 : vehicle_ == Vehicle::Hog ? 3 :
        vehicle_ == Vehicle::Mech ? 4 : vehicle_ == Vehicle::Dragon ? 7 : 0;
    const uint32_t meters = progression_.streakMeters(nativeDistance());
    unsigned total = 0;
    bool rotatingSlot = false;
    for (const Rotator& r : rotators_) rotatingSlot |= !r.active;
    for (unsigned i = 0; i < JetpackImported::ObstacleCount; ++i) {
      total += obstacleWeight(i, profile, meters, rotatingSlot);
    }
    if (total) {
      unsigned choice = random() % total;
      for (unsigned i = 0; i < JetpackImported::ObstacleCount; ++i) {
        const auto& record = JetpackImported::Obstacles[i];
        const unsigned weight = obstacleWeight(i, profile, meters, rotatingSlot);
        if (choice >= weight) { choice -= weight; continue; }
        const unsigned scaleQ8 = record.minimum * Unit + random() % ((record.maximum - record.minimum) * Unit + 1u);
        const int lengthQ8 = JetpackPlacement::screenQ8(scaleQ8);
        const unsigned phase = unsigned(record.angle) * 256 / 360;
        const int sine = JetpackPlacement::sine(phase), cosine = JetpackPlacement::sine(phase + 64);
        const unsigned extent = record.period ? record.maximum : unsigned(abs(sine) * record.maximum / 1024);
        const JetpackPlacement::Weights weights = {record.top, record.bottom, record.topHalf, record.bottomHalf};
        const int centerY = 68 - JetpackPlacement::screenQ8(JetpackPlacement::verticalOffset(extent, weights, random(), random())) / Unit;
        zapSpawnSpan_ = scaleQ8 / Unit;
        if (record.period) {
          for (Rotator& rotator : rotators_) if (!rotator.active) {
            rotator = Rotator{}; rotator.active = true;
            rotator.x = 258 * Unit + lengthQ8 / 2; rotator.y = int16_t(centerY);
            rotator.angle = uint8_t(phase); rotator.period = record.period;
            rotator.clock = uint16_t(phase * record.period / 256);
            rotator.lengthQ8 = uint16_t(lengthQ8); rotator.clockwise = (record.flags & 1) != 0;
            return;
          }
        } else for (Zap& zap : zaps_) if (!zap.active) {
          const int dx = cosine * lengthQ8 / (1024 * Unit), dy = -sine * lengthQ8 / (1024 * Unit);
          zap.active = true; zap.x = 258 * Unit; zap.y = int16_t(centerY - dy / 2);
          zap.dx = int16_t(dx); zap.dy = int16_t(dy);
          return;
        }
      }
    }
#endif
    for (Zap& zap : zaps_) if (!zap.active) {
      zap.active = true;
      zap.x = 258 * Unit;
      const unsigned kind = random() % 8;
      if (kind <= 2) {
        zap.y = kind == 0 ? Ceiling : kind == 1 ? Floor : int16_t(47 + random() % 42);
        zap.dx = 32; zap.dy = 0;
      } else if (kind <= 4) {
        zap.dy = int16_t(36 + random() % 12);
        zap.y = kind == 3 ? Ceiling : int16_t(Floor - zap.dy);
        zap.dx = 0;
      } else if (kind <= 6) {
        zap.y = kind == 5 ? Ceiling : Floor;
        zap.dx = 32; zap.dy = kind == 5 ? 39 : -39;
      } else {
        zap.y = int16_t(49 + random() % 19);
        zap.dx = 32; zap.dy = (random() & 1) ? 27 : -27;
      }
      return;
    }
  }
  bool putCoin(int x, int y, uint8_t value = 1) {
    for (Coin& coin : coinField_) if (!coin.active) {
      if (x > coinSpawnRight_) coinSpawnRight_ = x;
      coin.x = x * Unit; coin.y = int16_t(bounded(y, 27, 110)); coin.active = true; coin.value = value; return true;
    }
    return false;
  }
  void spawnCoins() {
    if (startImported()) return;
    coinSpawnRight_ = 250;
    const int cy = int(43 + random() % 42);
    const unsigned pattern = random() % 8;
    if (pattern == 0) {
      for (int i = 0; i < 16; ++i) putCoin(250 + i * 10, cy);
    } else if (pattern == 1 || pattern == 2) {
      for (int i = 0; i < 17; ++i) putCoin(250 + i * 9, cy + (i < 9 ? i : 16 - i) * (pattern == 1 ? 4 : -4));
    } else if (pattern == 3) {
      for (int row = 0; row < 3; ++row) for (int col = 0; col < 10; ++col) putCoin(250 + col * 10, 40 + row * 14);
    } else if (pattern == 4) {
      static const int8_t wave[16] = {0,6,11,15,16,15,11,6,0,-6,-11,-15,-16,-15,-11,-6};
      for (int i = 0; i < 20; ++i) putCoin(250 + i * 9, cy + wave[i & 15]);
    } else if (pattern == 5) {
      for (int i = 0; i < 5; ++i) for (int side = -1; side <= 1; side += 2) {
        putCoin(250 + i * 10, cy + side * i * 5); putCoin(300 + i * 10, cy + side * (5 - i) * 5);
      }
    } else if (pattern == 6) {
      for (int i = 0; i < 18; ++i) { putCoin(250 + i * 9, 35 + i / 3 * 10); putCoin(250 + i * 9, 46 + i / 3 * 10); }
    } else {
      for (int i = 0; i < 6; ++i) { putCoin(250 + i * 10, cy); putCoin(290 + i * 8, cy - i * 5); putCoin(290 + i * 8, cy + i * 5); }
    }
    coinSpawnSpan_ = JetpackProgression::nativeDistance(uint32_t(coinSpawnRight_ - 250 + 10) * Unit);
  }
  void collect() {
    for (Coin& coin : coinField_) if (coin.active) {
      if (sweptCoinContact(int(coin.x / Unit), coin.y)) {
        coin.active = false;
        addCoins(coin.value, true);
        signal(ArcadeCue::Power);
      }
    }
    if (pickup_.active) {
      const int dx = int(pickup_.x / Unit) - PlayerX, dy = pickup_.y - playerY();
      if (dx * dx + dy * dy < 14 * 14) {
        const Vehicle kind = pickup_.kind;
        pickup_.active = false;
        equip(kind);
      }
    }
  }
  static bool nearSegment(int px, int py, int x, int y, int dx, int dy, int radius) {
    const int64_t ox = px - x, oy = py - y;
    const int64_t length = int64_t(dx) * dx + int64_t(dy) * dy;
    const int64_t projection = ox * dx + oy * dy;
    const int64_t rr = int64_t(radius) * radius;
    if (projection <= 0) return ox * ox + oy * oy <= rr;
    if (projection >= length) {
      const int64_t ex = ox - dx, ey = oy - dy;
      return ex * ex + ey * ey <= rr;
    }
    const int64_t cross = ox * dy - oy * dx;
    return cross * cross <= rr * length;
  }
  bool hitsBodySegment(int x, int y, int dx, int dy) const {
    const int py = playerY();
    switch (vehicle_) {
      case Vehicle::Hog: return nearSegment(PlayerX - 5, py + 1, x, y, dx, dy, 5) || nearSegment(PlayerX + 5, py + 1, x, y, dx, dy, 5);
      case Vehicle::Mech: return nearSegment(PlayerX, py - 5, x, y, dx, dy, 5) || nearSegment(PlayerX, py + 1, x, y, dx, dy, 5);
      case Vehicle::Bird: return nearSegment(PlayerX, py - 2, x, y, dx, dy, 7);
      case Vehicle::Teleporter: return nearSegment(PlayerX, py - 2, x, y, dx, dy, vehiclePhysics_.teleportStage == 1 ? 3 : 7);
      case Vehicle::Dragon: return nearSegment(PlayerX + 9, py - 3, x, y, dx, dy, 7) || nearSegment(PlayerX, py, x, y, dx, dy, 6);
      default: return nearSegment(PlayerX, py, x, y, dx, dy, 6);
    }
  }
  void collide() {
    if (vehicle_ == Vehicle::SAM) return;
    if (laser_.active && laser_.ticks <= 40 && hitsBodySegment(0, laser_.y, Width - 1, 0)) { hit(); return; }
    for (Rotator& rotator : rotators_) if (rotator.active) {
      int x, y, dx, dy; rotatorEnds(rotator, x, y, dx, dy);
      if (hitsBodySegment(x, y, dx, dy)) { rotator.active = false; hit(); return; }
    }
    for (Zap& zap : zaps_) if (zap.active &&
        hitsBodySegment(int(zap.x / Unit), zap.y, zap.dx, zap.dy)) {
      zap.active = false;
      hit(); return;
    }
    for (Missile& missile : missiles_) if (missile.active && !missile.warning &&
        hitsBodySegment(int(missile.x / Unit) - 7, missile.y, 13, 0)) {
      missile.active = false;
      hit(); return;
    }
  }
  void hit() {
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
    if (diagnosticProtection_) return;
#endif
    if (powerTicks_[unsigned(Power::Shield)]) {
      if (shieldHits_) --shieldHits_;
      if (!shieldHits_) powerTicks_[unsigned(Power::Shield)] = 0;
      immune_ = 90; burst(1); signal(ArcadeCue::Power); return;
    }
    if (vehicle_ == Vehicle::SAM && samHits_ > 1) { --samHits_; immune_ = 30; signal(ArcadeCue::Hit); return; }
    if (vehicle_ != Vehicle::None) {
      burst(2);
      progression_.ejectVehicle(nativeDistance(), uint8_t(vehicle_));
      vehicle_ = Vehicle::None;
      vehicleHint_ = 0;
      gravityUp_ = false;
      velocity_ = -200 * 256;
      immune_ = 120;
      signal(ArcadeCue::Power);
    } else {
      if (rocketTime_.active()) endRocketTime();
      phase_ = Phase::Crashing;
      crashTicks_ = 150;
      deathSpeed_ = speed();
      crashBounces_ = 0;
      velocity_ = -450 * 256;
      armed_ = false;
      cue_ = ArcadeCue::Death;
    }
  }

  const char* vehicleHint() const {
    switch (vehicle_) {
      case Vehicle::Gravity: return "GRAVITY: TAP TO FLIP";
      case Vehicle::Hog: return "HOG: TAP + HOLD JUMP";
      case Vehicle::Mech: return "MECH: HOLD TO BOOST";
      case Vehicle::Bird: return "BIRD: TAP TO FLAP";
      case Vehicle::Teleporter: return "TELEPORTER: TAP TO GO";
      case Vehicle::Dragon: return "DRAGON: HOLD TO DIVE";
      case Vehicle::SAM: return "SAM: HOLD ARM UP";
      default: return "ARMOR: HOLD TO FLY";
    }
  }
  static int32_t scaledNative(int32_t hundredths) {
    return divideNearest(int64_t(hundredths) * 23 * FlightUnit, 6000);
  }
  bool safePower() const { return powerTicks_[unsigned(Power::CashDash)] || powerTicks_[unsigned(Power::Boost)]; }
  void clearHazards() {
    for (Zap& zap : zaps_) zap.active = false;
    for (Missile& missile : missiles_) missile.active = false;
    for (Rotator& rotator : rotators_) rotator.active = false;
    laser_.active = false;
  }
  void activatePower(Power power) {
    const unsigned index = unsigned(power);
    if (vehicle_ != Vehicle::None && (power == Power::Boost || power == Power::CashDash || power == Power::RocketTime)) return;
    if ((power == Power::Boost || power == Power::CashDash) && rocketTime_.active()) endRocketTime();
    if (!index || index >= unsigned(Power::Count)) return;
    const uint16_t duration = power == Power::RocketTime ? JetpackProfile::rocketTicks(activeUpgrades_[JetpackProfile::RocketTime]) : power == Power::Shield ? UINT16_MAX : 1200;
    if (powerTicks_[index] != UINT16_MAX) powerTicks_[index] = duration;
    if (power == Power::Shield) shieldHits_ = JetpackProfile::shieldHits(activeUpgrades_[JetpackProfile::Shield]);
    progression_.acquirePower(nativeDistance(), uint8_t(power));
    if (power == Power::RocketTime) {
      powerTicks_[unsigned(Power::Boost)] = powerTicks_[unsigned(Power::CashDash)] = 0;
      boostExtraVelocity_ = 0;
      rocketShots_ = rocketDrops_ = 0;
      rocketNativeSpeed_ = uint32_t(normalNativeVelocity());
      rocketTime_.start(activeUpgrades_[JetpackProfile::RocketTime]);
      powerTicks_[index] = UINT16_MAX; rocketWaves_ = 0;
      clearHazards(); pickup_.active = false;
      for (Pickup& choice : rocketChoices_) choice = Pickup{};
    }
    if (power == Power::CashDash) {
      powerTicks_[index] = UINT16_MAX;
      powerEndCash_ = distance() + JetpackProfile::cashMeters(activeUpgrades_[JetpackProfile::CashDash]); clearHazards();
      cashStart_ = distance(); cashNextCoin_ = cashStart_; cashPayload_ = Bonus{};
      cashNextValue_ = 0;
    }
    if (power == Power::Boost) {
      powerTicks_[index] = UINT16_MAX;
      powerEndBoost_ = distance() + JetpackProfile::boostMeters(activeUpgrades_[JetpackProfile::Boost]); clearHazards();
    }
    signal(ArcadeCue::Win);
  }
  void burst(uint8_t kind) { fxAge_ = 0; fxKind_ = kind; fxX_ = PlayerX; fxY_ = int16_t(playerY()); }
  void equip(Vehicle vehicle) {
    if (rocketTime_.active()) endRocketTime();
    burst(2);
    const int32_t incomingNative = unsigned(vehicle_) >= unsigned(Vehicle::Gravity) ? vehiclePhysics_.nativeSpeed : normalNativeVelocity();
    vehicle_ = vehicle;
    ++stats_.vehiclesCollected;
    if (vehicle == Vehicle::Mech) ++stats_.mechsCollected;
    velocity_ = 0; gravityUp_ = false;
    vehicleTicks_ = 0; vehicleInputBlocked_ = true;
    vehiclePhysics_ = JetpackVehiclePhysics::State{};
    JetpackVehiclePhysics::beginSpeed(vehiclePhysics_,uint8_t(vehicle),incomingNative);
    if (vehicle == Vehicle::Hog || vehicle == Vehicle::Mech) y_ = Floor * FlightUnit;
    if (vehicle == Vehicle::SAM) { samHits_ = 3; vehicleTicks_ = 0; clearHazards(); }
    clearHazards();
    progression_.acquireVehicle(nativeDistance(), uint8_t(vehicle));
    vehicleHint_ = 180; immune_ = 90; armed_ = false;
    signal(ArcadeCue::Win);
  }
  void applyLoadout() {
    if (!finalSpinInitialized_) { finalSpin_.reset(seed_ ^ 0xa54ff53au); finalSpinInitialized_ = true; }
    runStarted_ = true; runSettled_ = false; rankedRun_ = profileEnabled_;
    if (profile_) {
      profile_->beginRun(rankedRun_);
      samLetters_ = profile_->samLetters();
      progression_.setSamCompensation(profile_->samCompensation());
      for (uint8_t i = 0; i < JetpackProfile::UpgradeCount; ++i) activeUpgrades_[i] = profile_->upgradeLevel(i);
    }
    if (nextRide_) {
      Vehicle ride = Vehicle(nextRide_);
      if (ride == Vehicle::Random) ride = Vehicle(2 + random() % 7);
      equip(ride);
    }
    if (nextGadget_ == 1) powerTicks_[unsigned(Power::Magnet)] = UINT16_MAX;
    if (nextGadget_ == 2) powerTicks_[unsigned(Power::DoubleCoins)] = UINT16_MAX;
    if (nextGadget_ >= 3) {
      const Power bonuses[] = {Power::Shield,Power::Boost,Power::CashDash,Power::RocketTime};
      activatePower(bonuses[nextGadget_ - 3]);
    }
    if (pendingRewards_ & 2) powerTicks_[unsigned(Power::DoubleCoins)] = UINT16_MAX;
    if (profile_ && profile_->takeDoubleCoins()) powerTicks_[unsigned(Power::DoubleCoins)] = UINT16_MAX;
    const bool earnedHeadstart = profile_ && profile_->takeHeadstart();
    if (nextHeadstart_ || (pendingRewards_ & 1) || earnedHeadstart) { activatePower(Power::Boost); powerEndBoost_ = distance() + 500; }
    pendingRewards_ = 0;
  }
  void updatePowers() {
    updateRocketTime();
    for (unsigned i = 1; i < unsigned(Power::Count); ++i)
      if (i != unsigned(Power::RocketTime) && powerTicks_[i] && powerTicks_[i] != UINT16_MAX) --powerTicks_[i];
    if (powerTicks_[unsigned(Power::CashDash)] && distance() >= powerEndCash_) {
      powerTicks_[unsigned(Power::CashDash)] = 0;
      cashPayload_.active = true; cashPayload_.x = (PlayerX + 92) * Unit;
      cashPayload_.y = int16_t(cashPathY(powerEndCash_ - cashStart_)); cashPayload_.kind = Power::CashDash;
      progression_.expirePower(nativeDistance(), uint8_t(Power::CashDash));
    }
    if (powerTicks_[unsigned(Power::Boost)] && distance() + JetpackProfile::boostEndThreshold(activeUpgrades_[JetpackProfile::Boost]) >= powerEndBoost_) {
      powerTicks_[unsigned(Power::Boost)] = 0;
      progression_.expirePower(nativeDistance(), uint8_t(Power::Boost));
    }
    if (powerTicks_[unsigned(Power::Boost)]) {
      const int32_t target = int32_t(JetpackProfile::boostNativeSpeed(activeUpgrades_[JetpackProfile::Boost])) * FlightUnit;
      boostExtraVelocity_ += 35 * FlightUnit;
      if (boostExtraVelocity_ > target) boostExtraVelocity_ = target;
    } else {
      boostExtraVelocity_ = divideNearest(int64_t(boostExtraVelocity_) * 9, 10);
      if (boostExtraVelocity_ <= FlightUnit) boostExtraVelocity_ = 0;
    }
    if (safePower()) clearHazards();
  }
  static int cashPathY(uint32_t meters) {
    static const int8_t wave[] = {0,11,20,27,29,27,20,11,0,-11,-20,-27,-29,-27,-20,-11};
    return 69 + wave[(meters / 3) & 15];
  }
  void moveFlight(bool held, bool press) {
    if (vehicleInputBlocked_) {
      if (!held && !press) vehicleInputBlocked_ = false;
      held = press = false;
    }
    if (vehicle_ == Vehicle::Gravity) {
      if (press) { gravityUp_ = !gravityUp_; ++stats_.gravityFlips; signal(ArcadeCue::Fire); }
      const int32_t acceleration = divideNearest(int64_t(FlightUnit) * (gravityUp_ ? -299 : 299), 3000);
      velocity_ = divideNearest(int64_t(velocity_ + acceleration) * 98, 100);
    } else if (vehicle_ == Vehicle::Hog || vehicle_ == Vehicle::Mech) {
      const bool grounded = y_ >= Floor * FlightUnit;
      if (JetpackVehiclePhysics::hop(vehiclePhysics_,vehicle_ == Vehicle::Mech,held,press,grounded,velocity_)) signal(ArcadeCue::Fire);
    } else if (vehicle_ == Vehicle::Bird) {
      if (JetpackVehiclePhysics::bird(vehiclePhysics_,held,press,velocity_)) signal(ArcadeCue::Fire);
    } else if (vehicle_ == Vehicle::Teleporter) {
      velocity_ = 0;
      const JetpackVehiclePhysics::Teleport event = JetpackVehiclePhysics::teleporter(vehiclePhysics_,press);
      if (event.started) signal(ArcadeCue::Fire);
      if (event.arrived) { y_ = JetpackVehiclePhysics::markerY(vehiclePhysics_); burst(2); signal(ArcadeCue::Power); }
    } else if (vehicle_ == Vehicle::Dragon) {
      velocity_ += scaledNative(held ? 20 : -20);
      velocity_ = bounded(velocity_, -scaledNative(550), scaledNative(550));
    } else {
      if (press) signal(ArcadeCue::Fire);
      // Net thrust and gravity retain the 240-unit center travel in 92 pixels.
      const int32_t acceleration = divideNearest(int64_t(flightFactor()) * (held ? -161 : 115), 2000);
      velocity_ = JetpackTimeStep::accelerate(velocity_,acceleration,movementScale_);
    }
    y_ += JetpackTimeStep::scale(velocity_,movementScale_);
    const int top = vehicle_ == Vehicle::SAM ? 27 : vehicle_ == Vehicle::Teleporter ? Ceiling :
        unsigned(vehicle_) >= unsigned(Vehicle::Hog) ? 28 : Ceiling;
    if (y_ < top * FlightUnit) { y_ = top * FlightUnit; velocity_ = 0; }
    if (y_ > Floor * FlightUnit) { y_ = Floor * FlightUnit; velocity_ = 0; }
  }
  void addCoins(uint32_t count, bool gameplay = false) {
    if (gameplay && powerTicks_[unsigned(Power::DoubleCoins)]) count *= 2;
    if (gameplay) stats_.coinsCollected += count;
    coins_ = coins_ + count > 999999 ? 999999 : coins_ + count;
  }
  int magnetRadius() const { return powerTicks_[unsigned(Power::Magnet)] ? 65 : 0; }
  int coinTargetY() const { return vehicle_ == Vehicle::SAM ? 105 : playerY(); }
  bool coinContact(int x, int y) const {
    if (vehicle_ == Vehicle::SAM) return x >= PlayerX - 18 && x <= PlayerX + 18 && y >= 90 && y <= 123;
    const int dx = x - PlayerX, dy = y - playerY();
    return dx * dx + dy * dy <= 10 * 10;
  }
  bool sweptCoinContact(int x, int y) const {
    if (coinContact(x,y)) return true;
    const int span = (frameAdvance_ + Unit - 1) / Unit;
    if (vehicle_ == Vehicle::SAM) return x <= PlayerX + 18 && x + span >= PlayerX - 18 && y >= 90 && y <= 123;
    return nearSegment(PlayerX,playerY(),x,y,span,0,10);
  }
  bool attractCoin(int32_t x, int y, uint8_t value, int16_t vx = 0, int16_t vy = 0) {
    for (AttractedCoin& coin : attractedCoins_) if (!coin.age) {
      coin.x = x; coin.y = int16_t(bounded(y * Unit, 16 * Unit, 122 * Unit));
      coin.vx = vx; coin.vy = vy; coin.age = 1; coin.value = value;
      coin.falling = false; coin.lockQ8 = 0; coin.gem = value >= 5;
      return true;
    }
    return false;
  }
  void updateAttractedCoins() {
    const float dt = movementScale_ / float(FlightUnit);
    const int32_t drag99 = movementScale_ == FlightUnit ? 64881 : int32_t(lroundf(powf(.99f,dt) * FlightUnit));
    const float bounceDrag = movementScale_ == FlightUnit ? .95f : powf(.95f,dt);
    const int32_t targetY = coinTargetY() * Unit;
    for (AttractedCoin& coin : attractedCoins_) if (coin.age) {
      if (coin.falling) {
        const uint16_t step = uint16_t((movementScale_ + 128) / 256);
        coin.lockQ8 = coin.lockQ8 > step ? uint16_t(coin.lockQ8 - step) : 0;
        const int dx = int(coin.x / Unit) - PlayerX, dy = int(coin.y / Unit) - coinTargetY();
        if (!coin.lockQ8 && coinContact(int(coin.x / Unit),int(coin.y / Unit))) {
          addCoins(coin.value,true); coin.age = 0; signal(ArcadeCue::Power); continue;
        }
        if (!coin.lockQ8 && magnetRadius() && dx * dx + dy * dy <= magnetRadius() * magnetRadius()) coin.falling = false;
        else {
          coin.x += JetpackTimeStep::scale(coin.vx,movementScale_) - frameAdvance_;
          int32_t y = coin.y + JetpackTimeStep::scale(coin.vy,movementScale_);
          coin.vx = int16_t(divideNearest(int64_t(coin.vx) * drag99,FlightUnit));
          coin.vy = int16_t(bounded(divideNearest(int64_t(coin.vy + JetpackTimeStep::scale(16,movementScale_)) * drag99,FlightUnit),-10 * Unit,10 * Unit));
          const int top = 68 * Unit - divideNearest(int64_t(132) * 23 * Unit,60);
          const int floor = 68 * Unit + divideNearest(int64_t(132) * 23 * Unit,60);
          if (y < top || y > floor) {
            const bool bottom = y > floor; y = bottom ? floor : top;
            if (bottom && abs(coin.vy) < 98) coin.vy = 0;
            else {
              const int32_t bounce = int32_t(lroundf((movementScale_ == FlightUnit ? (750 + random() % 101) / 1000.f : powf((750 + random() % 101) / 1000.f,dt)) * FlightUnit));
              coin.vy = int16_t(-divideNearest(int64_t(coin.vy) * bounce,FlightUnit));
            }
            coin.vx = int16_t(lroundf(coin.vx * bounceDrag));
          }
          coin.y = int16_t(y);
          if (coin.x < -25 * Unit || coin.x > (Width + 100) * Unit) coin.age = 0;
          else if (coin.age < 255) ++coin.age;
          continue;
        }
      }
      const int32_t dx = PlayerX * Unit - coin.x, dy = targetY - coin.y;
      if (coin.age >= 3 && coinContact(int(coin.x / Unit), int(coin.y / Unit))) {
        addCoins(coin.value, true); coin.age = 0; signal(ArcadeCue::Power); continue;
      }
      const int32_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
      const int32_t length = ax > ay ? ax : ay;
      if (length) {
        coin.vx = int16_t(bounded(int32_t(coin.vx) * 7 / 8 + dx * 96 / length, -6 * Unit, 6 * Unit));
        coin.vy = int16_t(bounded(int32_t(coin.vy) * 7 / 8 + dy * 96 / length, -6 * Unit, 6 * Unit));
      }
      coin.x += coin.vx;
      coin.y = int16_t(bounded(int32_t(coin.y) + coin.vy, 16 * Unit, 122 * Unit));
      if (coin.age < 255) ++coin.age;
    }
  }
  void emitCashShower() {
    for (unsigned i = 0; i < 6 && cashShowerRemaining_; ++i) {
      if (attractedCoinCount() == AttractedCoinCount) return;
      const unsigned free = AttractedCoinCount - attractedCoinCount();
      unsigned batch = (cashShowerRemaining_ + free - 1) / free;
      if (batch > 32) batch = 32;
      uint8_t value = 0; bool gem = false;
      for (unsigned k = 0; k < batch; ++k) {
        const bool isGem = random() % 100 < cashShowerGemPercent_;
        gem |= isGem; value = uint8_t(value + (isGem ? 5 : 1));
      }
      const int x = int(cashShowerX_ / Unit) + int(random() % 31) - 15;
      const int y = int(cashShowerY_ / Unit) + int(random() % 17) - 8;
      const bool first = cashShowerEmitted_ < 5;
      const float angle = first ? (random() % 6284) / 1000.f : cashSprayAngle_ + (int(random() % 901) - 450) / 1000.f;
      const float magnitude = first ? (200 + random() % 601) * (23.f * Unit / 6000) : cashSpraySpeed_ * (1200 + random() % 801) / 1000.f;
      const int16_t vx = int16_t(bounded(int32_t(lroundf(cosf(angle) * magnitude)) + (first ? speed() : 0),-10 * Unit,10 * Unit));
      const int16_t vy = int16_t(bounded(int32_t(lroundf(sinf(angle) * magnitude)),-10 * Unit,10 * Unit));
      for (AttractedCoin& coin : attractedCoins_) if (!coin.age) {
        coin.x = x * Unit; coin.y = int16_t(bounded(y * Unit,17 * Unit,119 * Unit));
        coin.vx = vx; coin.vy = vy; coin.age = 1; coin.value = value;
        coin.falling = true; coin.gem = gem; coin.lockQ8 = 18 * Unit;
        cashShowerRemaining_ = uint16_t(cashShowerRemaining_ - batch); ++cashShowerEmitted_; break;
      }
    }
  }
  void startCashShower(uint16_t count, uint8_t gems, int32_t x, int16_t y) {
    cashShowerRemaining_ = uint16_t(cashShowerRemaining_ > UINT16_MAX - count ? UINT16_MAX : cashShowerRemaining_ + count);
    cashShowerGemPercent_ = gems; cashShowerX_ = x; cashShowerY_ = y; cashShowerEmitted_ = 0;
    const float vx = float(speed()), vy = velocity_ / float(FlightUnit / Unit);
    cashSprayAngle_ = atan2f(vy,vx); cashSpraySpeed_ = sqrtf(vx * vx + vy * vy);
  }
  void updateExtras(int32_t advance) {
    if (bonus_.active) { bonus_.x -= advance; if (bonus_.x < -12 * Unit) bonus_.active = false; }
    if (cashPayload_.active) { cashPayload_.x -= advance; if (cashPayload_.x < -12 * Unit) cashPayload_.active = false; }
    if (token_.active) { token_.x -= advance; if (token_.x < -12 * Unit) token_.active = false; }
    if (samToken_.active) { samToken_.x -= advance; if (samToken_.x < -12 * Unit) samToken_.active = false; }
    for (Rotator& rotator : rotators_) if (rotator.active) {
      rotator.x -= advance;
      rotator.clock = uint16_t((rotator.clock + 1) % rotator.period);
      const unsigned phase = unsigned(rotator.clock) * 256 / rotator.period;
      rotator.angle = uint8_t(rotator.clockwise ? phase : 256u - phase);
      if (rotator.x < -int32_t(rotator.lengthQ8)) rotator.active = false;
    }
    if (laser_.active) {
      if (laser_.ticks > 80) laser_.y = int16_t(playerY());
      if (laser_.ticks && !--laser_.ticks) laser_.active = false;
    }
    for (Scientist& scientist : scientists_) if (scientist.active) {
      scientist.x -= advance;
      if (scientist.age) --scientist.age;
      if (scientist.state == 2) {
        scientist.velocity = int16_t(bounded(scientist.velocity + 20, -1200, 1200));
        scientist.y = int16_t(scientist.y + scientist.velocity);
        if (scientist.y > 114 * Unit) {
          scientist.y = 114 * Unit;
          scientist.velocity = int16_t(-scientist.velocity * 7 / 10);
          if (scientist.velocity > -50) { scientist.velocity = 0; scientist.state = 1; }
        }
      } else {
        scientist.x -= scientist.state == 1 ? 100 : 28;
        if (scientist.x < 110 * Unit && scientist.state == 0) scientist.state = 1;
      }
      if (scientist.x < -15 * Unit) scientist.active = false;
    }
    updateAttractedCoins();
    emitCashShower();
    if (magnetRadius()) {
      for (Coin& coin : coinField_) if (coin.active) {
        const int dx = int(coin.x / Unit) - PlayerX, dy = coin.y - coinTargetY();
        if (!coinContact(int(coin.x / Unit), coin.y) && dx * dx + dy * dy <= magnetRadius() * magnetRadius() &&
            attractCoin(coin.x, coin.y, coin.value)) coin.active = false;
      }
    }
    updateImported(advance);
    while (cashNextCoin_ <= distance() && cashNextCoin_ < powerEndCash_) {
      if (!cashNextValue_) cashNextValue_ = random() % 100 < JetpackProfile::cashGemPercent(activeUpgrades_[JetpackProfile::CashDash]) ? 5 : 1;
      if (!putCoin(PlayerX + 92, cashPathY(cashNextCoin_ - cashStart_), cashNextValue_)) break;
      ++cashNextCoin_; cashNextValue_ = 0;
    }
  }
  void collectExtras() {
    for (Pickup& choice : rocketChoices_) if (choice.active &&
        abs(int(choice.x / Unit) - PlayerX) <= 10 && abs(int(choice.y) - playerY()) <= 12) {
      const Vehicle selected = choice.kind; endRocketTime(); equip(selected); break;
    }
    if (cashPayload_.active) {
      const int dx = int(cashPayload_.x / Unit) - PlayerX, dy = cashPayload_.y - playerY();
      if (dx * dx + dy * dy < 14 * 14) {
        const uint16_t payload = JetpackProfile::cashPayload(activeUpgrades_[JetpackProfile::CashDash]);
        const uint8_t gems = JetpackProfile::cashGemPercent(activeUpgrades_[JetpackProfile::CashDash]);
        startCashShower(payload,gems,cashPayload_.x,int16_t(cashPayload_.y * Unit));
        cashPayload_.active = false; burst(0); signal(ArcadeCue::Win);
      }
    }
    if (bonus_.active && vehicle_ == Vehicle::None) {
      const int dx = int(bonus_.x / Unit) - PlayerX, dy = bonus_.y - playerY();
      if (dx * dx + dy * dy < 14 * 14) { activatePower(bonus_.kind); bonus_.active = false; }
    }
    Token* tokens[] = {&token_, &samToken_};
    for (Token* token : tokens) if (token->active) {
      const int dx = int(token->x / Unit) - PlayerX, dy = token->y - playerY();
      if (dx * dx + dy * dy < 12 * 12) {
        if (!token->kind) { if (spinTokens_ < UINT16_MAX) ++spinTokens_; ++stats_.tokensCollected; }
        else samLetters_ |= uint8_t(1u << (token->kind - 1));
        token->active = false; signal(ArcadeCue::Win);
        if (samLetters_ == 7) { samLetters_ = 0; equip(Vehicle::SAM); }
        if (profile_) profile_->setSamLetters(samLetters_);
      }
    }
    collectImported();
  }
  void interactScientists() {
    for (Scientist& scientist : scientists_) if (scientist.active && !scientist.age && scientist.state != 2) {
      const int dx = int(scientist.x / Unit) - PlayerX;
      if (dx > -11 && dx < 12 && playerY() > 101) {
        if (scientistInteractions_ != UINT16_MAX) ++scientistInteractions_;
        scientist.age = 120;
        if (vehicle_ != Vehicle::None && vehicle_ != Vehicle::Armor && vehicle_ != Vehicle::Gravity) {
          scientist.state = 2; scientist.velocity = -800; addCoins(2);
        } else { scientist.state = 3; ++stats_.highFives; addCoins(1); }
        signal(ArcadeCue::Hit);
      } else if (thrust_ && dx > -15 && dx < 3 && playerY() > 78 && playerY() < 105) {
        scientist.state = 1; scientist.age = 30;
      }
    }
  }
  void rotatorEnds(const Rotator& rotator, int& x, int& y, int& dx, int& dy) const {
    const int rx = JetpackPlacement::sine(unsigned(rotator.angle) + 64) * rotator.lengthQ8 / (2048 * Unit);
    const int ry = JetpackPlacement::sine(rotator.angle) * rotator.lengthQ8 / (2048 * Unit);
    x = int(rotator.x / Unit) - rx; y = rotator.y - ry; dx = rx * 2; dy = ry * 2;
  }
  void tickCrash(bool press) {
    thrust_ = false;
    if (press && !blastUsed_) {
      blastUsed_ = true; burst(0); velocity_ = -4 * FlightUnit;
      deathSpeed_ += 2 * Unit; crashTicks_ = uint16_t(crashTicks_ + 45);
      signal(ArcadeCue::Win);
    }
    velocity_ = divideNearest(int64_t(velocity_ + scaledNative(24)) * 98, 100);
    y_ += velocity_;
    if (y_ < Ceiling * FlightUnit) { y_ = Ceiling * FlightUnit; velocity_ = 0; }
    if (y_ >= Floor * FlightUnit) {
      y_ = Floor * FlightUnit;
      if (velocity_ > scaledNative(180) && crashBounces_ < 5) {
        velocity_ = -divideNearest(int64_t(velocity_) * 65, 100); ++crashBounces_;
      } else velocity_ = 0;
      deathSpeed_ = deathSpeed_ * 94 / 100;
    } else deathSpeed_ = deathSpeed_ * 995 / 1000;
    if (deathSpeed_ < 3) deathSpeed_ = 0;
    if (world_ < 0x70000000u) world_ += uint32_t(deathSpeed_);
    frameAdvance_ = deathSpeed_;
    updateObjects(deathSpeed_); updateExtras(deathSpeed_);
    if (crashTicks_) --crashTicks_;
    if (!crashTicks_ && y_ == Floor * FlightUnit && velocity_ == 0 && deathSpeed_ == 0) {
      if (spinTokens_) { phase_ = Phase::Spin; spinTicks_ = 90; spinResult_ = false; }
      else { phase_ = Phase::Over; settleRun(); }
      armed_ = false;
    }
  }
  void tickSpin(bool press) {
    if (spinResult_ && spinPresentation_.running()) { spinPresentation_.tick(); return; }
    if (spinTicks_) --spinTicks_;
    if (spinResult_) {
      if (press) spinTicks_ = 0;
      if (spinTicks_) return;
      if (lastAward_.kind == JetpackRewards::Kind::Revive) {
        phase_ = Phase::Running; vehicle_ = Vehicle::None;
        y_ = 70 * FlightUnit; velocity_ = 0; immune_ = 180; clearHazards();
        progression_.recover(nativeDistance(), 444); armed_ = false;
      } else if (lastAward_.kind == JetpackRewards::Kind::Blast) {
        phase_ = Phase::Crashing; spinResult_ = false; crashTicks_ = 90; burst(0);
        const int multiplier = int(lastAward_.tier) + 1;
        velocity_ = -scaledNative(600 * multiplier);
        deathSpeed_ = divideNearest(int64_t(500 * multiplier) * 23 * Unit, 3600);
        blastUsed_ = true;
      } else if (spinTokens_) { spinResult_ = false; spinTicks_ = 0; armed_ = false; }
      else { phase_ = Phase::Over; armed_ = false; settleRun(); }
      return;
    }
    if (!press) return;
    if (spinTokens_) --spinTokens_;
    const uint32_t prizeWord = random(), stopWord = random(), reseedWord = random();
    const JetpackRewards::Outcome outcome = finalSpin_.spin(prizeWord, stopWord, reseedWord);
    lastAward_ = outcome.award; lastSpinWon_ = outcome.hasPrize; lastSpinMatches_ = outcome.matches;
    if (outcome.hasPrize) ++stats_.prizesWon;
    spinPresentation_.begin(outcome,stopWord ^ reseedWord);
    spinResult_ = true; spinTicks_ = outcome.hasPrize ? JetpackSpin::WinHold : JetpackSpin::LossHold; rewardCoins_ = lastAward_.amount;
    if (!outcome.hasPrize) { reward_ = 6; signal(ArcadeCue::Hit); return; }
    using Kind = JetpackRewards::Kind;
    switch (lastAward_.kind) {
      case Kind::Revive: revived_ = true; reward_ = 0; break;
      case Kind::Blast: reward_ = 2; break;
      case Kind::Headstart:
        reward_ = 3;
        if (profile_ && profile_->active()) profile_->grantHeadstart(); else if (!profile_) pendingRewards_ |= 1;
        break;
      case Kind::DoubleCoins:
        reward_ = 4;
        if (profile_ && profile_->active()) profile_->grantDoubleCoins(); else if (!profile_) pendingRewards_ |= 2;
        break;
      case Kind::Tokens:
        reward_ = 5;
        spinTokens_ = uint16_t(spinTokens_ > UINT16_MAX - lastAward_.amount ? UINT16_MAX : spinTokens_ + lastAward_.amount);
        break;
      case Kind::Coins: reward_ = 1; addCoins(lastAward_.amount); break;
    }
    signal(lastAward_.kind == Kind::Coins ? ArcadeCue::Power : ArcadeCue::Win);
  }
  void drawExtras(Canvas& c) const {
    for (const Rotator& rotator : rotators_) if (rotator.active) {
      int x, y, dx, dy; rotatorEnds(rotator, x, y, dx, dy);
      JetpackArt::drawZap(c, x, y, x + dx, y + dy, frame_);
    }
    if (laser_.active) {
      const uint16_t color = laser_.ticks > 40 ? rgb(214, 78, 72) : rgb(255, 237, 139);
      if (laser_.ticks <= 40 || (frame_ & 8)) {
        for (int x = 3; x < Width; x += laser_.ticks > 40 ? 8 : 1) c.rect(x, laser_.y - 1, laser_.ticks > 40 ? 3 : 1, 3, color);
      }
      c.text(laser_.ticks > 40 ? "LASER LOCK" : "FIRING", 91, 17, color);
    }
    if (bonus_.active) JetpackArt::drawPickupIcon(c, int(bonus_.x / Unit), bonus_.y, frame_, powerIcon(bonus_.kind));
    if (cashPayload_.active) JetpackArt::drawPickupIcon(c, int(cashPayload_.x / Unit), cashPayload_.y, frame_, 5);
    const Token* tokens[] = {&token_, &samToken_};
    for (const Token* token : tokens) if (token->active) {
      JetpackArt::drawPickupIcon(c, int(token->x / Unit), token->y, frame_, 4);
      if (token->kind) {
        const char letter[] = {token->kind == 1 ? 'S' : token->kind == 2 ? 'A' : 'M', 0};
        c.text(letter, int(token->x / Unit) - 3, token->y - 6, rgb(255, 249, 218));
      }
    }
    if (vehicle_ == Vehicle::Teleporter) {
      c.line(PlayerX + 5, teleportTargetY() - 5, PlayerX + 12, teleportTargetY() - 5, rgb(211, 180, 239));
      c.line(PlayerX + 5, teleportTargetY() + 5, PlayerX + 12, teleportTargetY() + 5, rgb(211, 180, 239));
    }
    drawImported(c);
  }
  static uint8_t powerIcon(Power power) {
    switch (power) {
      case Power::Shield: return 1; case Power::Magnet: return 2; case Power::DoubleCoins: return 3;
      case Power::CashDash: return 5; case Power::Boost: return 6; case Power::RocketTime: return 7;
      default: return 0;
    }
  }
  void updateImported(int32_t advance) {
#ifdef M5FLAP_JETPACK_IMPORTED
    for (ImportedGroup& group : importedGroups_) if (group.pattern >= 0) {
      group.x -= advance;
      const auto& pattern = JetpackImported::CoinPatterns[unsigned(group.pattern)];
      if (group.x + int32_t(pattern.width) * (group.pitchQ8 ? group.pitchQ8 : group.step * Unit) < -8 * Unit) group.pattern = -1;
    }
#else
    (void)advance;
#endif
  }
#ifdef M5FLAP_JETPACK_IMPORTED
  bool validImported(unsigned index) const {
    const auto& pattern = JetpackImported::CoinPatterns[index];
#ifndef M5FLAP_JETPACK_PROFILE_WEIGHTS
    if (!pattern.count) return false;
#endif
    return pattern.count <= 512 && pattern.height && pattern.height <= 40 && pattern.width && pattern.width <= 256 &&
        unsigned(pattern.offset) + pattern.count <= JetpackImported::CoinPointCount;
  }
  unsigned importedWeight(unsigned index) const {
    const auto& pattern = JetpackImported::CoinPatterns[index];
    if (powerTicks_[unsigned(Power::CashDash)]) return pattern.flags & 2 ? (pattern.weight ? pattern.weight : 1) : 0;
#ifdef M5FLAP_JETPACK_PROFILE_WEIGHTS
    const uint8_t profile = vehicle_ == Vehicle::SAM ? JetpackImported::ProfileSAM :
        vehicle_ == Vehicle::Gravity ? JetpackImported::ProfileGravity :
        vehicle_ == Vehicle::Mech ? JetpackImported::ProfileMech :
        vehicle_ == Vehicle::Teleporter ? JetpackImported::ProfileTeleporter :
        (vehicle_ == Vehicle::Hog || progression_.streak() == JetpackProgression::Streak::Coins) ? JetpackImported::ProfileCoinStreak : JetpackImported::ProfileDefault;
    const uint32_t meters = progression_.streakMeters(nativeDistance());
    unsigned weight = 0;
    for (unsigned i = 0; i < JetpackImported::ProfileWeightCount; ++i) {
      const auto& record = JetpackImported::ProfileWeights[i];
      if (record.profile != profile || record.pattern != index) continue;
      if (record.startMeters <= meters) weight = record.weight;
    }
    return weight;
#else
    return pattern.flags & 1 ? (pattern.weight ? pattern.weight : 1) : 0;
#endif
  }
#endif
  bool startImported() {
#ifdef M5FLAP_JETPACK_IMPORTED
    ImportedGroup* slot = nullptr;
    for (ImportedGroup& group : importedGroups_) if (group.pattern < 0) { slot = &group; break; }
    if (!slot) return true;
    unsigned total = 0;
    for (unsigned i = 0; i < JetpackImported::PatternCount; ++i)
      if (validImported(i)) total += importedWeight(i);
    if (!total) { coinSpawnSpan_ = JetpackPlacement::coinSpan(0, speed()); return true; }
    unsigned choice = random() % total;
    for (unsigned i = 0; i < JetpackImported::PatternCount; ++i) {
      if (!validImported(i)) continue;
      const unsigned weight = importedWeight(i);
      if (choice >= weight) { choice -= weight; continue; }
      coinPattern_ = int16_t(i);
      coinSpawnSpan_ = JetpackPlacement::coinSpan(JetpackImported::CoinPatterns[i].width, speed());
      if (!JetpackImported::CoinPatterns[i].count) return true;
      const auto& pattern = JetpackImported::CoinPatterns[i];
      *slot = ImportedGroup{}; slot->pattern = int16_t(i); slot->x = 252 * Unit;
      slot->pitchQ8 = int16_t(JetpackPlacement::screenQ8(16 * Unit));
      JetpackPlacement::Weights placement = {0,0,0,0};
#ifdef M5FLAP_JETPACK_PLACEMENT
      const uint8_t profile = vehicle_ == Vehicle::SAM ? 2 : vehicle_ == Vehicle::Gravity ? 4 :
          vehicle_ == Vehicle::Mech ? 5 : vehicle_ == Vehicle::Teleporter ? 1 :
          (vehicle_ == Vehicle::Hog || progression_.streak() == JetpackProgression::Streak::Coins) ? 3 : 0;
      for (unsigned p = 0; p < JetpackImported::CoinPlacementCount; ++p) {
        const auto& record = JetpackImported::CoinPlacements[p];
        if (record.profile == profile) placement = {record.top,record.bottom,record.topHalf,record.bottomHalf};
      }
#endif
      const uint32_t placementChoice = random(), placementPosition = random();
      slot->topQ8 = int16_t(JetpackPlacement::coinTopQ8(pattern.height, placement, placementChoice, placementPosition));
      return true;
    }
#endif
    return false;
  }
  static int importedX(const ImportedGroup& group, uint16_t point) {
    return int((group.x + int32_t(point & 255) * (group.pitchQ8 ? group.pitchQ8 : group.step * Unit)) / Unit);
  }
  static int importedY(const ImportedGroup& group, uint16_t point) {
    return group.pitchQ8 ? (group.topQ8 + int32_t(point >> 8) * group.pitchQ8) / Unit : group.top + int(point >> 8) * group.step;
  }
  void collectImported() {
#ifdef M5FLAP_JETPACK_IMPORTED
    const int minimum = vehicle_ == Vehicle::SAM ? 18 : 10;
    const int radius = magnetRadius() > minimum ? magnetRadius() : minimum;
    for (ImportedGroup& group : importedGroups_) if (group.pattern >= 0) {
      const auto& pattern = JetpackImported::CoinPatterns[unsigned(group.pattern)];
      for (unsigned i = 0; i < pattern.count; ++i) {
        if (group.collected[i / 8] & (1u << (i & 7))) continue;
        const uint16_t point = JetpackImported::CoinPoints[pattern.offset + i];
        const int x = importedX(group, point);
        if (x > PlayerX + radius || x < PlayerX - radius) continue;
        const int y = importedY(group, point);
        const int dx = x - PlayerX, dy = y - coinTargetY();
        const int squared = dx * dx + dy * dy;
        if (sweptCoinContact(x,y)) {
          group.collected[i / 8] |= uint8_t(1u << (i & 7)); addCoins(1, true); signal(ArcadeCue::Power);
        } else if (magnetRadius() && squared <= radius * radius && attractCoin(x * Unit, y, 1)) {
          group.collected[i / 8] |= uint8_t(1u << (i & 7));
        }
      }
    }
#endif
  }
  void drawImported(Canvas& c) const {
#ifdef M5FLAP_JETPACK_IMPORTED
    for (const ImportedGroup& group : importedGroups_) if (group.pattern >= 0) {
      const auto& pattern = JetpackImported::CoinPatterns[unsigned(group.pattern)];
      for (unsigned i = 0; i < pattern.count; ++i) {
        if (group.collected[i / 8] & (1u << (i & 7))) continue;
        const uint16_t point = JetpackImported::CoinPoints[pattern.offset + i];
        const int x = importedX(group, point);
        if (x < -3 || x > Width + 2) continue;
        const int y = importedY(group, point);
        drawCoin(c, x, y);
      }
    }
#else
    (void)c;
#endif
  }


  void drawLab(Canvas& c) const {
    JetpackArt::drawLab(c, world_, frame_);
  }
  void drawCoin(Canvas& c, int x, int y) const {
    JetpackArt::drawCoin(c, x, y, frame_);
  }
  void drawZap(Canvas& c, const Zap& zap) const {
    const int x = int(zap.x / Unit);
    JetpackArt::drawZap(c, x, zap.y, x + zap.dx, zap.y + zap.dy, frame_);
  }
  void drawMissile(Canvas& c, const Missile& missile) const {
    if ((missile.rocketFlags & 128) && missile.clockQ8 < uint32_t(missile.warningStart) * Unit) return;
    const int y = missile.y;
    if (missile.warning) {
      const uint16_t red = rgb(232, 128, 48);
      if ((frame_ / 6 & 1) || missile.warning < 30) {
        c.rect(225, y - 6, 13, 13, rgb(16, 24, 40));
        c.text("!", 229, y - 6, red);
        c.line(220, y, 224, y - 4, red);
        c.line(220, y, 224, y + 4, red);
      }
      if (missile.warning <= 30) c.pixel(216, y, rgb(232, 176, 112));
      return;
    }
    JetpackArt::drawMissile(c, int(missile.x / Unit), y, frame_);
  }
  void drawPickup(Canvas& c) const {
    JetpackArt::drawPickupIcon(c, int(pickup_.x / Unit), pickup_.y, frame_, 0);
  }
  void drawPlayer(Canvas& c) const {
    if (immune_ && (frame_ & 4)) return;
    if (vehicle_ == Vehicle::Teleporter &&
        ((vehiclePhysics_.teleportStage == 1 && vehiclePhysics_.teleportTicks <= 6) ||
         (vehiclePhysics_.teleportStage == 2 && vehiclePhysics_.teleportTicks > 6))) {
      c.rect(PlayerX - 1,playerY() - 7,3,12,rgb(239,226,255));
      c.line(PlayerX - 4,playerY() - 4,PlayerX + 4,playerY() - 4,rgb(166,114,224));
      return;
    }
    if (phase_ == Phase::Crashing || phase_ == Phase::Over || phase_ == Phase::Spin) {
      JetpackArt::drawCrash(c, PlayerX, playerY(), frame_, uint8_t(phase_ == Phase::Crashing ? frame_ / 5 & 3 : 0), playerY() == Floor && !velocity_);
    } else if (unsigned(vehicle_) >= unsigned(Vehicle::Hog)) {
      JetpackArt::drawVehicle(c, PlayerX, playerY(), frame_, uint8_t(vehicle_), thrust_, playerY() == Floor);
    } else JetpackArt::drawRunner(c, PlayerX, playerY(), frame_, thrust_,
        !thrust_ && playerY() == Floor, false, uint8_t(vehicle_), gravityUp_);
  }
  void panel(Canvas& c, const char* title, const char* line1, const char* line2) const {
    c.rect(28, 39, 184, 54, rgb(16, 24, 40));
    c.rect(28, 39, 184, 1, rgb(128, 176, 184));
    c.rect(28, 92, 184, 1, rgb(128, 176, 184));
    unsigned length = 0; while (title[length]) ++length;
    c.text(title, 120 - int(length) * 3, 43, rgb(248, 216, 80));
    length = 0; while (line1[length]) ++length;
    c.text(line1, 120 - int(length) * 3, 60, rgb(224, 240, 216));
    length = 0; while (line2[length]) ++length;
    c.text(line2, 120 - int(length) * 3, 75, rgb(128, 176, 184));
  }

  ImportedGroup importedGroups_[2];
  uint32_t coinSpawnSpan_ = 0, zapSpawnSpan_ = 84;
  int coinSpawnRight_ = 250;
  JetpackProgression progression_;
  JetpackVehiclePhysics::State vehiclePhysics_;
  JetpackSpin spinPresentation_;
  JetpackRocketTime rocketTime_;
  uint32_t rocketNativeSpeed_ = 175 * FlightUnit;
  uint32_t movementScale_ = FlightUnit;
  uint8_t rocketWaves_ = 0;
  uint16_t rocketShots_ = 0, rocketDrops_ = 0;
  Pickup rocketChoices_[RocketChoiceCount];
  JetpackRewards::FinalSpin finalSpin_;
  JetpackRewards::Award lastAward_{JetpackRewards::Kind::Coins, 0, 0};
  bool lastSpinWon_ = false;
  bool finalSpinInitialized_ = false;
  uint8_t lastSpinMatches_ = 0;
  JetpackProfile* profile_ = nullptr;
  RunStats stats_;
  Bonus bonus_;
  Bonus cashPayload_;
  Token token_;
  Token samToken_;
  Scientist scientists_[ScientistCount];
  AttractedCoin attractedCoins_[AttractedCoinCount];
  Rotator rotators_[2];
  Laser laser_;
  uint16_t powerTicks_[unsigned(Power::Count)] = {};
  uint32_t powerEndCash_ = 0, powerEndBoost_ = 0;
  uint32_t cashStart_ = 0, cashNextCoin_ = 0, footTravel_ = 0, hogTravel_ = 0;
  int32_t cashShowerX_ = PlayerX * Unit;
  int16_t cashShowerY_ = 70 * Unit;
  uint16_t cashShowerRemaining_ = 0, cashShowerEmitted_ = 0;
  float cashSprayAngle_ = 0, cashSpraySpeed_ = 0;
  int32_t frameAdvance_ = 0;
  uint8_t cashShowerGemPercent_ = 0, cashNextValue_ = 0;
  int32_t boostExtraVelocity_ = 0;
  int32_t deathSpeed_ = 0, patternX_ = 0;
  int16_t coinPattern_ = -1;
  uint16_t patternCursor_ = 0, vehicleTicks_ = 0, scientistInteractions_ = 0, rewardCoins_ = 0;
  uint16_t spinTokens_ = 0;
  uint8_t samLetters_ = 0, spinTicks_ = 0, reward_ = 0;
  uint8_t activeUpgrades_[JetpackProfile::UpgradeCount] = {}, shieldHits_ = 0;
  int16_t fxX_ = PlayerX, fxY_ = Floor;
  uint8_t fxAge_ = 32, fxKind_ = 0;
  uint8_t samHits_ = 0, crashBounces_ = 0, nextRide_ = 0, nextGadget_ = 0, pendingRewards_ = 0;
  bool nextHeadstart_ = false, vehicleInputBlocked_ = false, blastUsed_ = false, revived_ = false, spinResult_ = false;
  bool profileEnabled_ = true, rankedRun_ = true, runStarted_ = false, runSettled_ = false;
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  bool diagnosticProtection_ = false;
#endif
  Zap zaps_[ZapCount];
  Coin coinField_[CoinCount];
  Missile missiles_[MissileCount];
  Pickup pickup_;
  uint32_t seed_ = 1, random_ = 1, frame_ = 0, world_ = 0, coins_ = 0;
  int32_t y_ = Floor * FlightUnit, velocity_ = 0;
  uint16_t immune_ = 0, vehicleHint_ = 0;
  uint8_t difficulty_ = 1, nextDifficulty_ = 1;
  uint16_t crashTicks_ = 0;
  Phase phase_ = Phase::Ready;
  Vehicle vehicle_ = Vehicle::None;
  ArcadeCue cue_ = ArcadeCue::None;
  bool gravityUp_ = false, armed_ = false, thrust_ = false;
};

}  // namespace flappy
