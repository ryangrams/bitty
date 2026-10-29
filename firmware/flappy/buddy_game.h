#pragma once

#include <stdint.h>
#include "arcade_support.h"

namespace flappy {

struct BuddyInput {
  int8_t lane = -1;
  int8_t step = 0;
  bool jump = false;
  bool roll = false;
  bool pressed = false;
};

class BuddyGame {
 public:
  enum { Width = 135, Height = 240, ObjectCount = 32, DepthUnit = 65536,
         LaneUnit = 256, LevelTicks = 36000, LevelCount = 5,
         JumpTicks = 54, RollTicks = 36, BoardTicks = 600,
         CelebrateTicks = 180, RetryTicks = 60 };
  enum class ObjectKind : uint8_t {
    Person, Straddle, LowObstacle, Coin, Hoverboard, Snake, Electronics, Shirt, Magnet, Multiplier, Mystery, Balloon
  };
  enum class Section : uint8_t { Welcome, Slalom, Hurdles, Underpass, Crowd, Rhythm, CoinRush, Breather };
  enum { MagnetTicks = 480, MultiplierTicks = 600, FlightTicks = 480, AirCoin = 0x100, RouteCoin = 0x200 };
  enum class Theme : uint8_t { Mall, School, Park, Volcano, Noodles };
  enum class Phase : uint8_t { Playing, Crashed, Celebrate, Complete };
  struct Object {
    ObjectKind kind = ObjectKind::Coin;
    int8_t lane = 1;
    float depth = 0;
    uint16_t variant = 0;
    bool active = false;
    int16_t pull = 0;
    uint8_t lift = 0;
    int32_t progress = 0;
  };

  BuddyGame() { reset(); }

  void setStartingLevel(uint8_t level) { startingLevel_ = level < LevelCount ? level : LevelCount - 1; }
  void setLook(uint8_t shirt, uint8_t mask) {
    selectedShirt_ = shirt_ = uint8_t(shirt % 4);
    selectedMask_ = mask_ = uint8_t(mask % 4);
  }
  void reset(uint32_t seed = 1, uint8_t difficulty = 1) {
    seed_ = seed ? seed : 1;
    random_ = seed_;
    difficulty_ = difficulty > 2 ? 2 : difficulty;
    frame_ = score_ = coins_ = parts_ = cleared_ = passed_ = 0;
    missionSets_ = goalCoins_ = goalJumps_ = goalRolls_ = tricks_ = 0;
    magnetTicks_ = multiplierTicks_ = rewardTicks_ = flightTicks_ = 0;
    lift_ = 0;
    combo_ = 0; bonusPicked_ = 0;
    distanceQ_ = 0;
    shirt_ = selectedShirt_;
    mask_ = selectedMask_;
    level_ = startingLevel_;
    cue_ = ArcadeCue::None;
    jumpWasDown_ = rollWasDown_ = false;
    jumpBuffer_ = rollBuffer_ = 0;
    startLevel();
  }

  void tick(const BuddyInput& input) {
    ++frame_;
    const bool jumpPressed = input.jump && !jumpWasDown_;
    const bool rollPressed = input.roll && !rollWasDown_;
    jumpWasDown_ = input.jump;
    rollWasDown_ = input.roll;
    if (phase_ != Phase::Playing) {
      if (phaseTicks_ < 65535) ++phaseTicks_;
      if (!input.pressed) return;
      if (phase_ == Phase::Crashed && phaseTicks_ >= RetryTicks) reset(seed_, difficulty_);
      else if (phase_ == Phase::Celebrate && phaseTicks_ >= CelebrateTicks) {
        if (level_ + 1 < LevelCount) { ++level_; startLevel(); }
        else { phase_ = Phase::Complete; phaseTicks_ = 0; }
      } else if (phase_ == Phase::Complete && phaseTicks_ >= RetryTicks) reset(seed_, difficulty_);
      return;
    }

    ++levelTick_;
    if (invulnerable_) --invulnerable_;
    if (boardTicks_ && !flightTicks_) --boardTicks_;
    if (flightTicks_) {
      --flightTicks_;
      if (lift_ < 100) lift_ = uint8_t(lift_ + 5);
      if (!flightTicks_) invulnerable_ = 90;
    } else if (lift_) lift_ = uint8_t(lift_ > 5 ? lift_ - 5 : 0);
    if (snakeTicks_) --snakeTicks_;
    if (coinFlash_) --coinFlash_;
    if (magnetTicks_) --magnetTicks_;
    if (multiplierTicks_) --multiplierTicks_;
    if (rewardTicks_) --rewardTicks_;
    if (jumpTick_) { if (++jumpTick_ > JumpTicks) jumpTick_ = 0; }
    if (rollTicks_) --rollTicks_;
    if (actionDelay_) --actionDelay_;
    movePlayer(input);
    if (jumpBuffer_) --jumpBuffer_;
    if (rollBuffer_) --rollBuffer_;
    if (jumpPressed && !flightTicks_ && !lift_) {
      jumpBuffer_ = 8;
      if (rollTicks_) { rollTicks_ = actionDelay_ = 0; }
    }
    if (rollPressed && !jumpPressed && !flightTicks_ && !lift_) {
      rollBuffer_ = 12;
      if (jumpTick_ && jumpTick_ < JumpTicks - 8) jumpTick_ = JumpTicks - 8;
    }
    if (!jumpTick_ && !rollTicks_ && !actionDelay_) {
      if (jumpBuffer_) { jumpTick_ = 1; jumpBuffer_ = rollBuffer_ = 0; signal(ArcadeCue::Fire); }
      else if (rollBuffer_) { rollTicks_ = RollTicks; rollBuffer_ = 0; signal(ArcadeCue::Fire); }
    }
    if (jumpTick_ == JumpTicks) actionDelay_ = rollBuffer_ ? 0 : 6;
    if (rollTicks_ == 1) actionDelay_ = 6;

    const int32_t step = speed();
    distanceQ_ += uint32_t(step);
    if (levelTick_ % 6 == 0) addScore(1);
    updateObjects(step);
    if (phase_ != Phase::Playing) return;
    if (levelTick_ >= LevelTicks) {
      phase_ = Phase::Celebrate;
      phaseTicks_ = 0;
      jumpTick_ = rollTicks_ = 0;
      score_ += 1000;
      signal(ArcadeCue::Win);
      return;
    }
    if (spawnDelay_) --spawnDelay_;
    else spawnDelay_ = spawnRow() ? uint16_t(rowSpacing() - 1) : 8;
  }

  Phase phase() const { return phase_; }
  Theme theme() const { return static_cast<Theme>(level_); }
  bool gameOver() const { return phase_ == Phase::Crashed || phase_ == Phase::Complete; }
  uint32_t ticks() const { return frame_; }
  uint32_t levelTicks() const { return levelTick_; }
  uint16_t phaseTicks() const { return phaseTicks_; }
  uint8_t level() const { return level_; }
  uint8_t difficulty() const { return difficulty_; }
  uint32_t score() const { return score_; }
  uint32_t coins() const { return coins_; }
  uint32_t parts() const { return parts_; }
  uint32_t tricks() const { return tricks_; }
  uint16_t magnetTicks() const { return magnetTicks_; }
  uint16_t multiplierTicks() const { return multiplierTicks_; }
  uint16_t rewardTicks() const { return rewardTicks_; }
  uint8_t combo() const { return combo_; }
  uint16_t flightTicks() const { return flightTicks_; }
  uint8_t flightHeight() const { return lift_; }
  uint8_t scoreMultiplier() const { return uint8_t((1 + (missionSets_ > 7 ? 7 : missionSets_)) * (multiplierTicks_ ? 2 : 1)); }
  uint16_t missionSets() const { return missionSets_; }
  uint16_t goal(unsigned i) const { return i == 0 ? goalCoins_ : i == 1 ? goalJumps_ : goalRolls_; }
  uint16_t goalTarget(unsigned i) const { return i == 0 ? uint16_t(25 + level_ * 5) : uint16_t(3 + (missionSets_ > 4 ? 4 : missionSets_)); }
  Section section() const { return section_; }
  uint8_t sectionBeat() const { return uint8_t(row_ ? (row_ - 1) % 6 : 0); }
  uint8_t sectionProgress() const {
    const unsigned spacing = rowSpacing();
    const unsigned elapsed = spawnDelay_ >= spacing ? 0 : spacing - spawnDelay_;
    return row_ ? uint8_t((sectionBeat() * spacing + elapsed) * 125 / (6 * spacing)) : 0;
  }
  uint32_t powerupsCollected() const { return bonusPicked_; }
  uint32_t clearedObstacles() const { return cleared_; }
  uint32_t passedObstacles() const { return passed_; }
  uint32_t distance() const { return distanceQ_ / 1024; }
  uint8_t shirt() const { return shirt_; }
  uint8_t mask() const { return mask_; }
  int lane() const { return lane_; }
  float lanePosition() const { return float(laneQ_) / LaneUnit; }
  float jumpHeight() const {
    return float(jumpTick_ * (JumpTicks - jumpTick_)) / float(JumpTicks * JumpTicks / 4);
  }
  uint8_t jumpTick() const { return jumpTick_; }
  bool rolling() const { return rollTicks_ != 0; }
  uint8_t rollTicks() const { return rollTicks_; }
  uint16_t boardTicks() const { return boardTicks_; }
  uint8_t snakeCharges() const { return snakeCharges_; }
  uint8_t snakeTicks() const { return snakeTicks_; }
  uint8_t coinFlash() const { return coinFlash_; }
  uint16_t invulnerableTicks() const { return invulnerable_; }
  const Object* objects() const { return objects_; }
  uint8_t objectCount() const {
    uint8_t count = 0;
    for (unsigned i = 0; i < ObjectCount; ++i) count += objects_[i].active;
    return count;
  }
  int32_t speed() const {
    const uint32_t ramp = levelTick_ > 10800 ? 10800 : levelTick_;
    const int approach = 190 - difficulty_ * 16 - level_ * 9 - int(ramp * 50 / 10800);
    return DepthUnit / (approach < 110 ? 110 : approach);
  }
  ArcadeCue takeCue() { const ArcadeCue result = cue_; cue_ = ArcadeCue::None; return result; }
  static bool obstacle(ObjectKind kind) {
    return kind == ObjectKind::Person || kind == ObjectKind::Straddle || kind == ObjectKind::LowObstacle;
  }

 private:
  friend struct BuddyGameTestAccess;
  void startLevel() {
    for (unsigned i = 0; i < ObjectCount; ++i) objects_[i] = Object();
    phase_ = Phase::Playing;
    phaseTicks_ = 0;
    levelTick_ = row_ = 0;
    spawnDelay_ = 90;
    lane_ = safeLane_ = 1;
    laneQ_ = LaneUnit;
    jumpTick_ = rollTicks_ = actionDelay_ = jumpBuffer_ = rollBuffer_ = 0;
    boardTicks_ = invulnerable_ = 0;
    snakeCharges_ = snakeTicks_ = coinFlash_ = 0;
    magnetTicks_ = multiplierTicks_ = flightTicks_ = 0;
    lift_ = 0;
    section_ = Section::Welcome;
  }
  uint32_t nextRandom() {
    random_ ^= random_ << 13;
    random_ ^= random_ >> 17;
    random_ ^= random_ << 5;
    return random_;
  }
  uint16_t rowSpacing() const {
    const uint32_t ramp = levelTick_ > 10800 ? 10800 : levelTick_;
    const int interval = 112 - difficulty_ * 12 - level_ * 6 - int(ramp * 24 / 10800);
    return uint16_t(interval < 54 ? 54 : interval);
  }
  void movePlayer(const BuddyInput& input) {
    if (input.lane >= 0 && input.lane < 3) lane_ = input.lane;
    else if (input.step < 0 && lane_ > 0) --lane_;
    else if (input.step > 0 && lane_ < 2) ++lane_;
    const int target = lane_ * LaneUnit;
    if (laneQ_ < target) { laneQ_ += 40; if (laneQ_ > target) laneQ_ = int16_t(target); }
    else if (laneQ_ > target) { laneQ_ -= 40; if (laneQ_ < target) laneQ_ = int16_t(target); }
  }
  bool addObject(ObjectKind kind, int lane, uint16_t variant = 0, int32_t progress = 0) {
    if (obstacle(kind) && snakeCharges_) { --snakeCharges_; ++cleared_; addScore(10); snakeTicks_ = 60; return true; }
    for (unsigned i = 0; i < ObjectCount; ++i) {
      if (objects_[i].active) continue;
      Object& object = objects_[i];
      object = Object();
      object.kind = kind;
      object.lane = int8_t(lane);
      object.variant = variant;
      object.active = true;
      object.progress = progress;
      object.depth = float(progress) / DepthUnit;
      return true;
    }
    return false;
  }
  bool spawnRow() {
    if (objectCount() > ObjectCount - 10) return false;
    // Sections share a reachable ground route. Extra coins invite jump and roll
    // tricks on the occupied lanes. No section needs a power-up to be solvable.
    const unsigned beat = row_ % 6;
    if (!beat && row_) {
      const unsigned chapter = row_ / 6;
      const Section previous = section_;
      if (chapter % 4 == 3) section_ = Section::CoinRush;
      else if (chapter % 4 == 0) section_ = Section::Breather;
      else {
        section_ = static_cast<Section>(1 + nextRandom() % 5);
        if (section_ == previous) section_ = static_cast<Section>(1 + unsigned(section_) % 5);
      }
    }
    if (section_ == Section::Slalom || !(row_ & 1)) {
      const int change = int(nextRandom() % 3) - 1;
      const int next = int(safeLane_) + change;
      safeLane_ = uint8_t(next < 0 ? 0 : next > 2 ? 2 : next);
    }
    const int other = (safeLane_ + 1 + int(nextRandom() % 2)) % 3;
    const ObjectKind mixed[6] = {ObjectKind::LowObstacle,ObjectKind::Straddle,ObjectKind::Person,
                                ObjectKind::Straddle,ObjectKind::LowObstacle,ObjectKind::Person};
    ObjectKind kind = section_ == Section::Hurdles ? ObjectKind::LowObstacle :
      section_ == Section::Underpass ? ObjectKind::Straddle :
      section_ == Section::Crowd || section_ == Section::Slalom ? ObjectKind::Person : mixed[beat];
    const bool quiet = section_ == Section::CoinRush || section_ == Section::Breather;
    // Reserve enough pool entries for the complete row before spawning it.
    if (!quiet) {
      addObject(kind,other,uint16_t(nextRandom() % 3));
      const unsigned density = 18 + difficulty_ * 10 + level_ * 8 + levelTick_ / 1800;
      if (row_ >= 6 && nextRandom() % 100 < (density > 75 ? 75 : density))
        addObject(mixed[(beat + 1) % 6],3 - safeLane_ - other,uint16_t(nextRandom() % 3));
      if (kind != ObjectKind::Person) {
        // Coins above a hurdle reward a jump; a low trail marks a roll gap.
        for (unsigned c = 0; c < 3; ++c)
          addObject(ObjectKind::Coin,other,uint16_t(c | (kind == ObjectKind::LowObstacle ? AirCoin | ((c == 1 ? 2 : 1) << 10) : 0)),
                    -int32_t(c * 9) * speed());
      }
    }
    const unsigned length = section_ == Section::CoinRush ? 7 : 4;
    for (unsigned c = 0; c < length; ++c)
      addObject(ObjectKind::Coin,safeLane_,uint16_t((c & 3) | RouteCoin),-int32_t(c * 9) * speed());
    // A guaranteed early shield teaches recovery before the power rotation.
    if (row_ % 24 == 2) addObject(ObjectKind::Hoverboard,safeLane_,0,-45 * speed());
    else if (row_ % 24 == 6) addObject(ObjectKind::Magnet,safeLane_,0,-45 * speed());
    else if (row_ % 24 == 10) addObject(ObjectKind::Snake,safeLane_,0,-45 * speed());
    else if (row_ % 24 == 14) addObject(ObjectKind::Multiplier,safeLane_,0,-45 * speed());
    else if (row_ % 24 == 18) addObject(ObjectKind::Balloon,safeLane_,0,-45 * speed());
    else if (row_ % 24 == 22) addObject(ObjectKind::Mystery,safeLane_,0,-45 * speed());
    else if (row_ % 8 == 3) addObject(ObjectKind::Electronics,safeLane_,0,-45 * speed());
    else if (row_ % 24 == 23) addObject(ObjectKind::Shirt,safeLane_,0,-45 * speed());
    ++row_;
    return true;
  }
  void addScore(uint32_t value) { score_ += value * scoreMultiplier(); }
  void checkGoals() {
    if (goalCoins_ < goalTarget(0) || goalJumps_ < goalTarget(1) || goalRolls_ < goalTarget(2)) return;
    if (missionSets_ < 65535) ++missionSets_;
    goalCoins_ = goalJumps_ = goalRolls_ = 0;
    coins_ += 10;
    addScore(500);
    rewardTicks_ = 210;
    boardTicks_ = BoardTicks;
    signal(ArcadeCue::Win);
  }
  void clearNextObstacles() {
    while (snakeCharges_) {
      int nearest = -1;
      for (unsigned i = 0; i < ObjectCount; ++i) {
        const Object& object = objects_[i];
        if (!object.active || !obstacle(object.kind)) continue;
        if (nearest < 0 || object.progress > objects_[nearest].progress) nearest = int(i);
      }
      if (nearest < 0) break;
      objects_[nearest].active = false;
      --snakeCharges_;
      ++cleared_;
      addScore(10);
    }
  }
  void collect(ObjectKind kind) {
    if (kind == ObjectKind::Coin) {
      ++coins_;
      if (goalCoins_ < 65535) ++goalCoins_;
      addScore(25);
      coinFlash_ = 18;
      signal(ArcadeCue::Hit);
      checkGoals();
    } else {
      addScore(50);
      ++bonusPicked_;
      if (kind == ObjectKind::Hoverboard) boardTicks_ = BoardTicks;
      else if (kind == ObjectKind::Snake) { snakeCharges_ = 10; snakeTicks_ = 90; clearNextObstacles(); }
      else if (kind == ObjectKind::Electronics) { ++parts_; if (parts_ % 3 == 0) mask_ = uint8_t((mask_ + 1) % 4); }
      else if (kind == ObjectKind::Shirt) shirt_ = uint8_t((shirt_ + 1) % 4);
      else if (kind == ObjectKind::Magnet) magnetTicks_ = MagnetTicks;
      else if (kind == ObjectKind::Multiplier) multiplierTicks_ = MultiplierTicks;
      else if (kind == ObjectKind::Balloon) {
        flightTicks_ = FlightTicks;
        jumpTick_ = rollTicks_ = jumpBuffer_ = rollBuffer_ = 0;
      }
      else if (kind == ObjectKind::Mystery) {
        // Original pocket prize: visible coins plus one of the useful powers.
        coins_ += 15; addScore(150); rewardTicks_ = 150;
        if (nextRandom() & 1) magnetTicks_ = MagnetTicks;
        else boardTicks_ = BoardTicks;
      }
      signal(ArcadeCue::Power);
    }
  }
  void hit() {
    if (invulnerable_) return;
    combo_ = 0;
    if (boardTicks_) {
      boardTicks_ = 0;
      invulnerable_ = 90;
      signal(ArcadeCue::Power);
      return;
    }
    phase_ = Phase::Crashed;
    phaseTicks_ = 0;
    signal(ArcadeCue::Death);
  }
  void updateObjects(int32_t step) {
    for (unsigned i = 0; i < ObjectCount; ++i) {
      Object& object = objects_[i];
      if (!object.active) continue;
      const int32_t previous = object.progress;
      object.progress += step;
      if (object.kind == ObjectKind::Coin) {
        if (lift_ > object.lift) object.lift = uint8_t(object.lift + (lift_ - object.lift > 5 ? 5 : lift_ - object.lift));
        else if (object.lift > lift_) object.lift = uint8_t(object.lift - (object.lift - lift_ > 5 ? 5 : object.lift - lift_));
      }
      if (object.kind == ObjectKind::Coin && magnetTicks_ && object.progress > DepthUnit / 3) {
        // Pull toward the runner over several frames, then collect on contact.
        object.pull += int16_t((laneQ_ - object.lane * LaneUnit - object.pull) / 4);
        object.progress += step * 2;
      }
      object.depth = float(object.progress) / DepthUnit;
      if (previous < DepthUnit && object.progress >= DepthUnit) {
        const int separation = laneQ_ - (object.lane * LaneUnit + object.pull);
        const bool sameLane = separation >= -104 && separation <= 104;
        object.active = false;
        if (obstacle(object.kind)) {
          const bool escaped = !sameLane || flightTicks_ || lift_ > 20 || (object.kind == ObjectKind::LowObstacle && jumpHeight() >= .30f) ||
                               (object.kind == ObjectKind::Straddle && rolling());
          if (escaped) {
            ++passed_; addScore(5);
            if (sameLane && !invulnerable_ && !flightTicks_ && !lift_) {
              ++tricks_; if (combo_ < 9) ++combo_;
              if (object.kind == ObjectKind::LowObstacle && goalJumps_ < 65535) ++goalJumps_;
              if (object.kind == ObjectKind::Straddle && goalRolls_ < 65535) ++goalRolls_;
              addScore(20 * combo_); coinFlash_ = 24; checkGoals();
            }
          }
          else hit();
        } else if (sameLane) {
          const bool reachable = object.kind != ObjectKind::Coin || !(object.variant & AirCoin) ||
              jumpHeight() >= .25f || magnetTicks_ || flightTicks_ || lift_;
          if (reachable) collect(object.kind);
        }
        if (phase_ != Phase::Playing) return;
      }
    }
  }
  void signal(ArcadeCue cue) {
    const unsigned priority = cue == ArcadeCue::Death ? 6 : static_cast<unsigned>(cue);
    const unsigned pending = cue_ == ArcadeCue::Death ? 6 : static_cast<unsigned>(cue_);
    if (priority >= pending) cue_ = cue;
  }

  Object objects_[ObjectCount];
  uint32_t seed_ = 1, random_ = 1, frame_ = 0, levelTick_ = 0, distanceQ_ = 0;
  uint32_t score_ = 0, coins_ = 0, parts_ = 0, cleared_ = 0, passed_ = 0;
  uint16_t phaseTicks_ = 0, spawnDelay_ = 0, row_ = 0, boardTicks_ = 0, invulnerable_ = 0;
  int16_t laneQ_ = LaneUnit;
  int8_t lane_ = 1;
  uint8_t level_ = 0, startingLevel_ = 0, difficulty_ = 1, safeLane_ = 1;
  uint8_t selectedShirt_ = 0, selectedMask_ = 1, shirt_ = 0, mask_ = 1;
  uint8_t jumpTick_ = 0, rollTicks_ = 0, actionDelay_ = 0;
  uint8_t snakeCharges_ = 0, snakeTicks_ = 0, coinFlash_ = 0;
  uint16_t magnetTicks_ = 0, multiplierTicks_ = 0, rewardTicks_ = 0, missionSets_ = 0, flightTicks_ = 0;
  uint16_t goalCoins_ = 0, goalJumps_ = 0, goalRolls_ = 0;
  uint32_t tricks_ = 0, bonusPicked_ = 0;
  uint8_t combo_ = 0, jumpBuffer_ = 0, rollBuffer_ = 0, lift_ = 0;
  Section section_ = Section::Welcome;
  bool jumpWasDown_ = false, rollWasDown_ = false;
  Phase phase_ = Phase::Playing;
  ArcadeCue cue_ = ArcadeCue::None;
};

}  // namespace flappy
