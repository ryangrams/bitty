#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "hockey_protocol.h"
#include <memory>
#include <new>
#include <utility>

namespace flappy {
namespace coop {

constexpr uint8_t ProtocolVersion = 2;
constexpr size_t MaxPacketBytes = 20;
constexpr uint32_t SnapshotIntervalMs = 50;
constexpr uint32_t InterpolationMs = 50;
constexpr uint32_t MaxInterpolationMs = 100;
constexpr uint32_t GhostStaleMs = 500;
constexpr uint32_t DisconnectMs = 3000;
constexpr uint16_t CountdownMs = 2500;
constexpr uint8_t ZeroInputDelay = 6;
constexpr uint8_t ZeroWindowPads = 4;
constexpr uint8_t ZeroBufferFrames = 64;
constexpr uint32_t ZeroInputIntervalMs = 16;
constexpr uint32_t ZeroProgressIntervalMs = 32;
constexpr uint32_t ZeroStatsIntervalMs = 200;

enum class GameProfile : uint8_t { Flappy, Tetris, TetrisZero, Hockey };
constexpr const char* HockeyServiceUuid = "a8ed0500-3fec-4f60-a066-83ae580bbed0";
constexpr const char* HockeyHostTxUuid = "a8ed0501-3fec-4f60-a066-83ae580bbed0";
constexpr const char* HockeyGuestTxUuid = "a8ed0502-3fec-4f60-a066-83ae580bbed0";
constexpr const char* ServiceUuid = "a8ed0200-3fec-4f60-a066-83ae580bbed0";
constexpr const char* HostTxUuid = "a8ed0201-3fec-4f60-a066-83ae580bbed0";
constexpr const char* GuestTxUuid = "a8ed0202-3fec-4f60-a066-83ae580bbed0";
constexpr const char* TetrisServiceUuid = "a8ed0300-3fec-4f60-a066-83ae580bbed0";
constexpr const char* TetrisHostTxUuid = "a8ed0301-3fec-4f60-a066-83ae580bbed0";
constexpr const char* TetrisGuestTxUuid = "a8ed0302-3fec-4f60-a066-83ae580bbed0";
constexpr const char* ZeroServiceUuid = "a8ed0400-3fec-4f60-a066-83ae580bbed0";
constexpr const char* ZeroHostTxUuid = "a8ed0401-3fec-4f60-a066-83ae580bbed0";
constexpr const char* ZeroGuestTxUuid = "a8ed0402-3fec-4f60-a066-83ae580bbed0";

inline bool validGameProfile(GameProfile profile) {
  return profile == GameProfile::Flappy || profile == GameProfile::Tetris || profile == GameProfile::TetrisZero || profile == GameProfile::Hockey;
}
inline const char* serviceUuid(GameProfile profile) {
  if (profile == GameProfile::Hockey) return HockeyServiceUuid;
  return profile == GameProfile::TetrisZero ? ZeroServiceUuid : profile == GameProfile::Tetris ? TetrisServiceUuid : ServiceUuid;
}
inline const char* hostTxUuid(GameProfile profile) {
  if (profile == GameProfile::Hockey) return HockeyHostTxUuid;
  return profile == GameProfile::TetrisZero ? ZeroHostTxUuid : profile == GameProfile::Tetris ? TetrisHostTxUuid : HostTxUuid;
}
inline const char* guestTxUuid(GameProfile profile) {
  if (profile == GameProfile::Hockey) return HockeyGuestTxUuid;
  return profile == GameProfile::TetrisZero ? ZeroGuestTxUuid : profile == GameProfile::Tetris ? TetrisGuestTxUuid : GuestTxUuid;
}

inline bool matchesGameService(GameProfile profile, const char* uuid) {
  return validGameProfile(profile) && uuid && !strcmp(serviceUuid(profile), uuid);
}

enum class Role : uint8_t { Off, Host, Guest };
enum class Phase : uint8_t { Off, Waiting, Lobby, Countdown, Playing, Disconnected };
enum class Life : uint8_t { Ready, Alive, Dying, GameOver };
enum class MessageType : uint8_t { Hello = 1, Offer, Ready, Start, StartAck, Snapshot, Bye, TetrisStats, ZeroInputWindow, ZeroProgress, HockeyInput, HockeyMotion, HockeyBoard };

struct ZeroInputWindow {
  uint32_t frame = 0;
  uint8_t count = 0;
  uint8_t pads[ZeroWindowPads] = {};
};

struct TetrisStats {
  uint32_t score = 0;
  uint16_t lines = 0;
  uint8_t level = 0;
  bool toppedOut = false;
};

struct RemoteTetris {
  TetrisStats stats;
  bool received = false;
  bool fresh = false;
  uint32_t ageMs = 0;
};

struct PlayerSnapshot {
  uint32_t tick = 0;
  float y = 244.0f;
  int8_t rotation = 0;
  uint8_t frame = 0;
  Life life = Life::Ready;
  uint16_t score = 0;
};

struct RemoteView : PlayerSnapshot {
  bool visible = false;
};

struct StartInfo {
  uint32_t seed = 0;
  uint8_t round = 0;
  uint32_t deadlineMs = 0;
  uint16_t baseHue = 0;
};

struct Packet {
  uint8_t bytes[MaxPacketBytes] = {};
  uint8_t length = 0;
};

struct Message {
  MessageType type = MessageType::Hello;
  uint32_t session = 0;
  uint8_t round = 0;
  uint32_t seed = 0;
  uint16_t hue = 0;
  uint16_t remainingMs = 0;
  bool ready = false;
  PlayerSnapshot player;
  TetrisStats tetris;
  uint16_t tetrisSequence = 0;
  ZeroInputWindow zero;
  uint32_t zeroProgress = 0;
  hockey::Frame hockeyFrame;
  hockey::Input hockeyInput;
};

inline hockey::Part hockeyPart(MessageType type) {
  return type == MessageType::HockeyInput ? hockey::Part::Input : type == MessageType::HockeyMotion ? hockey::Part::Motion : hockey::Part::Board;
}

// Unsigned subtraction keeps ordering valid across tick and millis() wrap.
// Sessions never compare timestamps more than 2^31 ticks apart.
inline int32_t tickDelta(uint32_t a, uint32_t b) {
  const uint32_t difference = a - b;
  if (difference <= INT32_MAX) return int32_t(difference);
  return -int32_t(UINT32_MAX - difference) - 1;
}

inline bool newerTick(uint32_t a, uint32_t b) { return tickDelta(a, b) > 0; }
inline bool reached(uint32_t now, uint32_t deadline) { return tickDelta(now, deadline) >= 0; }

inline uint8_t nextRound(uint8_t round) { return round == UINT8_MAX ? 1 : uint8_t(round + 1); }

inline uint32_t nextSeed(uint32_t seed) {
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return seed ? seed : 0x8f6e37a1U;
}

inline uint32_t nextGameSeed(GameProfile profile, uint32_t seed) {
  if (profile == GameProfile::TetrisZero) return seed;
  if (profile != GameProfile::Tetris) return nextSeed(seed);
  uint32_t pieces = nextSeed(seed & 0x00ffffffU) & 0x00ffffffU;
  if (!pieces) pieces = 1;
  return (seed & 0xff000000U) | pieces;
}

inline void put16(uint8_t* bytes, uint16_t value) {
  bytes[0] = uint8_t(value);
  bytes[1] = uint8_t(value >> 8);
}

inline void put32(uint8_t* bytes, uint32_t value) {
  for (uint8_t i = 0; i < 4; ++i) bytes[i] = uint8_t(value >> (i * 8));
}

inline uint16_t get16(const uint8_t* bytes) {
  return uint16_t(bytes[0]) | uint16_t(uint16_t(bytes[1]) << 8);
}

inline uint32_t get32(const uint8_t* bytes) {
  uint32_t value = 0;
  for (uint8_t i = 0; i < 4; ++i) value |= uint32_t(bytes[i]) << (i * 8);
  return value;
}

inline bool validPlayer(const PlayerSnapshot& player) {
  // This comparison rejects NaN as well as values outside the reference canvas.
  return player.y >= -64.0f && player.y <= 512.0f &&
         player.rotation >= -90 && player.rotation <= 45 && player.frame < 3 &&
         uint8_t(player.life) <= uint8_t(Life::GameOver);
}

inline uint8_t wireLength(MessageType type) {
  switch (type) {
    case MessageType::Hello: return 8;
    case MessageType::Offer: return 15;
    case MessageType::Ready: return 10;
    case MessageType::Start: return 17;
    case MessageType::StartAck: return 9;
    case MessageType::Snapshot: return 19;
    case MessageType::Bye: return 9;
    case MessageType::TetrisStats: return 19;
    case MessageType::ZeroInputWindow: return 18;
    case MessageType::ZeroProgress: return 13;
    case MessageType::HockeyInput: return 14;
    case MessageType::HockeyMotion:
    case MessageType::HockeyBoard: return 20;
  }
  return 0;
}

inline bool encode(const Message& message, Packet& packet) {
  packet = Packet();
  const uint8_t length = wireLength(message.type);
  if (!length || length > MaxPacketBytes) return false;
  if (message.type == MessageType::Hello) {
    if (message.session != 0) return false;
  } else if (!message.session || !message.round) {
    return false;
  }
  if ((message.type == MessageType::Offer || message.type == MessageType::Start) &&
      (!message.seed || message.hue >= 360)) return false;
  if (message.type == MessageType::Start && message.remainingMs > CountdownMs) return false;
  if (message.type == MessageType::Snapshot && !validPlayer(message.player)) return false;
  if (message.type == MessageType::ZeroInputWindow && (!message.zero.count || message.zero.count > ZeroWindowPads)) return false;
  packet.length = length;
  packet.bytes[0] = 'M';
  packet.bytes[1] = 'F';
  packet.bytes[2] = ProtocolVersion;
  packet.bytes[3] = uint8_t(message.type);
  put32(packet.bytes + 4, message.session);
  if (length > 8) packet.bytes[8] = message.round;
  switch (message.type) {
    case MessageType::Offer:
      put32(packet.bytes + 9, message.seed);
      put16(packet.bytes + 13, message.hue);
      break;
    case MessageType::Ready:
      packet.bytes[9] = message.ready ? 1 : 0;
      break;
    case MessageType::Start:
      put32(packet.bytes + 9, message.seed);
      put16(packet.bytes + 13, message.remainingMs);
      put16(packet.bytes + 15, message.hue);
      break;
    case MessageType::Snapshot: {
      put32(packet.bytes + 9, message.player.tick);
      const float scaled = message.player.y * 16.0f;
      const int16_t y = int16_t(scaled + (scaled < 0 ? -0.5f : 0.5f));
      put16(packet.bytes + 13, uint16_t(y));
      packet.bytes[15] = uint8_t(message.player.rotation);
      packet.bytes[16] = uint8_t(message.player.frame | (uint8_t(message.player.life) << 2));
      put16(packet.bytes + 17, message.player.score);
      break;
    }
    case MessageType::TetrisStats:
      put16(packet.bytes + 9, message.tetrisSequence);
      put32(packet.bytes + 11, message.tetris.score);
      put16(packet.bytes + 15, message.tetris.lines);
      packet.bytes[17] = message.tetris.level;
      packet.bytes[18] = message.tetris.toppedOut ? 1 : 0;
      break;
    case MessageType::ZeroInputWindow:
      put32(packet.bytes + 9, message.zero.frame);
      packet.bytes[13] = message.zero.count;
      memcpy(packet.bytes + 14, message.zero.pads, message.zero.count);
      break;
    case MessageType::ZeroProgress:
      put32(packet.bytes + 9, message.zeroProgress);
      break;
    case MessageType::Hello:
    case MessageType::StartAck:
    case MessageType::Bye: break;
    case MessageType::HockeyInput:
    case MessageType::HockeyMotion:
    case MessageType::HockeyBoard:
      if (!hockey::encodePayload(hockeyPart(message.type),message.hockeyFrame,message.hockeyInput,packet.bytes+9)) return false;
      break;
  }
  return true;
}

inline bool decode(const uint8_t* bytes, size_t length, Message& message) {
  if (!bytes || length < 8 || length > MaxPacketBytes || bytes[0] != 'M' || bytes[1] != 'F' ||
      bytes[2] != ProtocolVersion) return false;
  Message decoded;
  decoded.type = MessageType(bytes[3]);
  if (wireLength(decoded.type) != length) return false;
  decoded.session = get32(bytes + 4);
  if (decoded.type == MessageType::Hello) {
    if (decoded.session != 0) return false;
  } else {
    decoded.round = bytes[8];
    if (!decoded.session || !decoded.round) return false;
  }
  switch (decoded.type) {
    case MessageType::Offer:
      decoded.seed = get32(bytes + 9);
      decoded.hue = get16(bytes + 13);
      if (!decoded.seed || decoded.hue >= 360) return false;
      break;
    case MessageType::Ready:
      if (bytes[9] > 1) return false;
      decoded.ready = bytes[9] != 0;
      break;
    case MessageType::Start:
      decoded.seed = get32(bytes + 9);
      decoded.remainingMs = get16(bytes + 13);
      decoded.hue = get16(bytes + 15);
      if (!decoded.seed || decoded.remainingMs > CountdownMs || decoded.hue >= 360) return false;
      break;
    case MessageType::Snapshot: {
      decoded.player.tick = get32(bytes + 9);
      const uint16_t encodedY = get16(bytes + 13);
      const int32_t signedY = encodedY <= INT16_MAX ? int32_t(encodedY) : int32_t(encodedY) - 65536;
      decoded.player.y = float(signedY) / 16.0f;
      decoded.player.rotation = bytes[15] <= INT8_MAX ? int8_t(bytes[15]) : int8_t(int(bytes[15]) - 256);
      if (bytes[16] & 0xf0) return false;
      decoded.player.frame = bytes[16] & 3;
      decoded.player.life = Life((bytes[16] >> 2) & 3);
      decoded.player.score = get16(bytes + 17);
      if (!validPlayer(decoded.player)) return false;
      break;
    }
    case MessageType::TetrisStats:
      decoded.tetrisSequence = get16(bytes + 9);
      decoded.tetris.score = get32(bytes + 11);
      decoded.tetris.lines = get16(bytes + 15);
      decoded.tetris.level = bytes[17];
      if (bytes[18] > 1) return false;
      decoded.tetris.toppedOut = bytes[18] != 0;
      break;
    case MessageType::ZeroInputWindow:
      decoded.zero.frame = get32(bytes + 9);
      decoded.zero.count = bytes[13];
      if (!decoded.zero.count || decoded.zero.count > ZeroWindowPads) return false;
      memcpy(decoded.zero.pads, bytes + 14, decoded.zero.count);
      for (uint8_t i = decoded.zero.count; i < ZeroWindowPads; ++i) if (bytes[14 + i]) return false;
      break;
    case MessageType::ZeroProgress:
      decoded.zeroProgress = get32(bytes + 9);
      break;
    case MessageType::Hello:
    case MessageType::StartAck:
    case MessageType::Bye: break;
    case MessageType::HockeyInput:
    case MessageType::HockeyMotion:
    case MessageType::HockeyBoard:
      if (!hockey::decodePayload(hockeyPart(decoded.type),bytes+9,length-9,decoded.hockeyFrame,decoded.hockeyInput)) return false;
      break;
  }
  message = decoded;
  return true;
}

// Frames remain immutable until both players have consumed them. Progress
// acknowledgements keep lost inputs eligible for retransmission after a stall.
class ZeroInputBuffer {
 public:
  void clear() { *this = ZeroInputBuffer(); }
  void reset(uint32_t firstFrame = 0) {
    clear();
    next_ = peerNext_ = retained_ = firstFrame;
    for (uint8_t i = 0; i < ZeroInputDelay; ++i) {
      const uint64_t mask = frameBit(firstFrame + i);
      localValid_ |= mask;
      remoteValid_ |= mask;
    }
  }
  uint32_t nextFrame() const { return next_; }

  bool submit(uint32_t frame, uint8_t pad) {
    if (tickDelta(frame, next_) < 0 || !within(frame, retained_)) return false;
    const uint64_t mask = frameBit(frame);
    if (localValid_ & mask) return local_[frame % ZeroBufferFrames] == pad;
    local_[frame % ZeroBufferFrames] = pad;
    localValid_ |= mask;
    return true;
  }

  bool receive(const ZeroInputWindow& window) {
    if (!window.count || window.count > ZeroWindowPads) return false;
    bool useful = false;
    for (uint8_t i = 0; i < window.count; ++i) {
      const uint32_t frame = window.frame + i;
      if (tickDelta(frame, next_) < 0) continue;
      if (!within(frame, next_)) return false;
      useful = true;
      if ((remoteValid_ & frameBit(frame)) && remote_[frame % ZeroBufferFrames] != window.pads[i]) return false;
    }
    if (!useful) return false;
    for (uint8_t i = 0; i < window.count; ++i) {
      const uint32_t frame = window.frame + i;
      if (tickDelta(frame, next_) < 0) continue;
      remote_[frame % ZeroBufferFrames] = window.pads[i];
      remoteValid_ |= frameBit(frame);
    }
    return true;
  }

  bool acknowledge(uint32_t peerNext) {
    const int32_t distance = tickDelta(peerNext, peerNext_);
    if (distance < 0 || distance > ZeroBufferFrames) return false;
    for (uint32_t frame = peerNext_; frame != peerNext; ++frame) if (!knownLocal(frame)) return false;
    peerNext_ = peerNext;
    releaseConsumed();
    return true;
  }

  bool window(ZeroInputWindow& result) const {
    result = ZeroInputWindow();
    result.frame = peerNext_;
    while (result.count < ZeroWindowPads && knownLocal(result.frame + result.count)) {
      result.pads[result.count] = local_[(result.frame + result.count) % ZeroBufferFrames];
      ++result.count;
    }
    return result.count != 0;
  }

  bool consume(uint32_t frame, uint8_t& local, uint8_t& remote) {
    if (frame != next_ || !knownLocal(frame) || !(remoteValid_ & frameBit(frame))) return false;
    local = local_[frame % ZeroBufferFrames];
    remote = remote_[frame % ZeroBufferFrames];
    remoteValid_ &= ~frameBit(frame);
    ++next_;
    releaseConsumed();
    return true;
  }

 private:
  uint8_t local_[ZeroBufferFrames] = {};
  uint8_t remote_[ZeroBufferFrames] = {};
  uint64_t localValid_ = 0, remoteValid_ = 0;
  uint32_t next_ = 0, peerNext_ = 0, retained_ = 0;
  static uint64_t frameBit(uint32_t frame) { return uint64_t(1) << (frame % ZeroBufferFrames); }
  static bool within(uint32_t frame, uint32_t start) {
    const int32_t distance = tickDelta(frame, start);
    return distance >= 0 && distance < ZeroBufferFrames;
  }
  bool knownLocal(uint32_t frame) const { return within(frame, retained_) && (localValid_ & frameBit(frame)); }
  void releaseConsumed() {
    while (retained_ != next_ && tickDelta(peerNext_, retained_) > 0) {
      localValid_ &= ~frameBit(retained_);
      ++retained_;
    }
  }
};

// All state changes happen on the game task. BLE callbacks only queue bytes.
class Session {
 public:
  Session() = default;
  Session(Session&&) = default;
  Session& operator=(Session&&) = default;
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  bool setGameProfile(GameProfile profile) {
    if (!validGameProfile(profile)) return false;
    if (profile == GameProfile::Hockey && !hockey_) {
      hockey_.reset(new(std::nothrow) hockey::Channel);
      if (!hockey_) return false;
    }
    if (profile_ != profile) { stop(); profile_ = profile; }
    if (profile != GameProfile::Hockey) hockey_.reset();
    return true;
  }

  void beginHost(uint32_t now, uint32_t session, uint32_t seed, uint16_t hue) {
    resetState();
    role_ = Role::Host;
    phase_ = Phase::Waiting;
    session_ = session ? session : 1;
    seed_ = seed ? seed : (profile_ == GameProfile::Tetris ? 0x006e37a1U : 0x8f6e37a1U);
    hue_ = profile_ == GameProfile::Tetris ? uint16_t(hue != 0) : hue % 360;
    round_ = 1;
    resetTimers(now);
  }

  void beginGuest(uint32_t now) {
    resetState();
    role_ = Role::Guest;
    phase_ = Phase::Waiting;
    resetTimers(now);
  }

  void stop() { resetState(); }

  void transportConnected(uint32_t now) {
    if (role_ == Role::Off) return;
    connected_ = true;
    phase_ = role_ == Role::Host ? Phase::Lobby : Phase::Waiting;
    localReady_ = remoteReady_ = false;
    resetTimers(now);
  }

  void transportDisconnected() {
    if (role_ == Role::Off) return;
    connected_ = false;
    phase_ = Phase::Disconnected;
    pendingStart_ = false;
    remoteCount_ = 0;
    clearRemoteTetris();
    zero_.clear();
    localReady_ = remoteReady_ = false;
  }

  void setLocal(const PlayerSnapshot& player) {
    if (validPlayer(player)) local_ = player;
  }

  void setLocalTetris(const TetrisStats& stats) { localTetris_ = stats; }
  hockey::Channel& hockeyChannel() { return *hockey_; }
  const hockey::Channel& hockeyChannel() const { return *hockey_; }

  bool submitZeroInput(uint32_t frame, uint8_t pad) {
    return profile_ == GameProfile::TetrisZero && connected_ &&
           (phase_ == Phase::Playing || phase_ == Phase::Countdown) && zero_.submit(frame, pad);
  }

  // A successful call consumes exactly this frame. Step the NES once using
  // host-local as player 1 and guest-local as player 2; a false result must wait.
  bool zeroInputs(uint32_t frame, uint8_t& local, uint8_t& remote) {
    return profile_ == GameProfile::TetrisZero && connected_ && phase_ == Phase::Playing &&
           zero_.consume(frame, local, remote);
  }
  uint32_t zeroFrame() const { return zero_.nextFrame(); }

  void requestReady(uint32_t now) {
    if (!connected_ || !session_ || !round_) return;
    if (phase_ == Phase::Lobby || (phase_ == Phase::Playing && localFinished())) {
      localReady_ = true;
      readySentMs_ = now - 200;
      advance(now);
    }
  }

  void advance(uint32_t now) {
    if (!connected_) return;
    if (uint32_t(now - lastReceivedMs_) >= DisconnectMs) {
      transportDisconnected();
      return;
    }
    const bool finished = phase_ == Phase::Playing && localFinished() && remoteFinished();
    if (role_ == Role::Host && localReady_ && remoteReady_ && (phase_ == Phase::Lobby || finished)) {
      if (finished) {
        round_ = nextRound(round_);
        seed_ = nextGameSeed(profile_, seed_);
      }
      startMs_ = now + CountdownMs;
      phase_ = Phase::Countdown;
      localReady_ = remoteReady_ = false;
      pendingStart_ = true;
      startAck_ = false;
      anchorValid_ = false;
      remoteCount_ = 0;
      clearRemoteTetris();
      localTetris_ = TetrisStats();
      tetrisSequence_ = 0;
      zero_.reset();
      if (hockey_) hockey_->reset();
      controlSentMs_ = now - 50;
    }
    if (phase_ == Phase::Countdown && reached(now, startMs_)) {
      if (role_ == Role::Host && !startAck_) {
        phase_ = Phase::Lobby;
        pendingStart_ = false;
        zero_.clear();
        controlSentMs_ = now - 250;
      } else {
        phase_ = Phase::Playing;
        snapshotSentMs_ = now - SnapshotIntervalMs;
        zeroInputSentMs_ = now - ZeroInputIntervalMs;
        zeroProgressSentMs_ = now - ZeroProgressIntervalMs;
        zeroPacketTurn_ = 0;
        zeroProgressMs_ = now;
        observedZeroFrame_ = zero_.nextFrame();
      }
    }
    if (profile_ == GameProfile::TetrisZero && phase_ == Phase::Playing && !localFinished()) {
      if (zero_.nextFrame() != observedZeroFrame_) {
        observedZeroFrame_ = zero_.nextFrame();
        zeroProgressMs_ = now;
      } else if (uint32_t(now - zeroProgressMs_) >= DisconnectMs) {
        transportDisconnected();
      }
    }
  }

  bool receive(const uint8_t* bytes, size_t length, uint32_t now) {
    if (!connected_) return false;
    Message message;
    if (!decode(bytes, length, message)) return false;
    if (message.type == MessageType::Hello) {
      if (role_ != Role::Host || phase_ != Phase::Lobby) return false;
      lastReceivedMs_ = now;
      controlSentMs_ = now - 250;
      return true;
    }
    if (message.type == MessageType::Offer) {
      if (profile_ == GameProfile::Tetris && message.hue > 1) return false;
      hockey::Options options;
      if (profile_ == GameProfile::Hockey && !hockey::Options::decode(message.hue,options)) return false;
      if (role_ != Role::Guest || (phase_ != Phase::Waiting && phase_ != Phase::Lobby &&
          !(phase_ == Phase::Countdown && message.round == round_))) return false;
      if (session_ && message.session != session_) return false;
      if (round_ && message.round != round_) return false;
      session_ = message.session;
      seed_ = message.seed;
      hue_ = message.hue;
      round_ = message.round;
      if (phase_ != Phase::Lobby) {
        phase_ = Phase::Lobby;
        pendingStart_ = false;
        localReady_ = remoteReady_ = false;
      }
      lastReceivedMs_ = now;
      return true;
    }
    if (message.session != session_ || !session_) return false;
    if (message.type == MessageType::Start) {
      if (role_ != Role::Guest) return false;
      if (profile_ == GameProfile::Tetris && message.hue > 1) return false;
      hockey::Options options;
      if (profile_ == GameProfile::Hockey && !hockey::Options::decode(message.hue,options)) return false;
      const bool initial = phase_ == Phase::Lobby && message.round == round_ && localReady_;
      const bool restart = phase_ == Phase::Playing && message.round == nextRound(round_) &&
                           localReady_ && localFinished();
      const bool repeat = (phase_ == Phase::Countdown || phase_ == Phase::Playing) && message.round == round_;
      if (!initial && !restart && !repeat) return false;
      if (repeat && (message.seed != seed_ || message.hue != hue_)) return false;
      if (initial || restart) {
        seed_ = message.seed;
        hue_ = message.hue;
        round_ = message.round;
        startMs_ = now + message.remainingMs;
        phase_ = Phase::Countdown;
        localReady_ = remoteReady_ = false;
        pendingStart_ = true;
        anchorValid_ = false;
        remoteCount_ = 0;
        clearRemoteTetris();
        localTetris_ = TetrisStats();
        tetrisSequence_ = 0;
        zero_.reset();
        if (hockey_) hockey_->reset();
      } else if (phase_ == Phase::Countdown) {
        const uint32_t corrected = now + message.remainingMs;
        // Choose the earliest observed deadline so late packets cannot delay play.
        if (tickDelta(corrected, startMs_) < 0) startMs_ = corrected;
      }
      ackPending_ = true;
      lastReceivedMs_ = now;
      advance(now);
      return true;
    }
    if (message.round != round_) return false;
    switch (message.type) {
      case MessageType::Ready:
        if (phase_ == Phase::Lobby || phase_ == Phase::Playing) remoteReady_ = message.ready;
        break;
      case MessageType::StartAck:
        if (role_ != Role::Host || phase_ != Phase::Countdown) return false;
        startAck_ = true;
        break;
      case MessageType::Snapshot:
        if (profile_ != GameProfile::Flappy) return false;
        if (phase_ != Phase::Playing && phase_ != Phase::Countdown) return false;
        if (remoteCount_ && !newerTick(message.player.tick, current_.player.tick)) return false;
        if (remoteCount_ && tickDelta(message.player.tick, current_.player.tick) > 600) return false;
        if (phase_ == Phase::Playing) {
          const int32_t offset = tickDelta(message.player.tick, worldTick(now));
          if (offset < -180 || offset > 180) return false;
        }
        if (remoteCount_) previous_ = current_;
        current_.player = message.player;
        current_.receivedMs = now;
        if (remoteCount_ < 2) ++remoteCount_;
        if (role_ == Role::Guest) {
          anchorTick_ = message.player.tick;
          anchorMs_ = now;
          anchorValid_ = true;
        }
        break;
      case MessageType::TetrisStats: {
        if ((profile_ != GameProfile::Tetris && profile_ != GameProfile::TetrisZero) || (phase_ != Phase::Playing && phase_ != Phase::Countdown)) return false;
        const uint16_t difference = uint16_t(message.tetrisSequence - remoteTetrisSequence_);
        if (remoteTetrisReceived_ && (!difference || difference >= 0x8000)) return false;
        remoteTetrisStats_ = message.tetris;
        remoteTetrisSequence_ = message.tetrisSequence;
        remoteTetrisReceivedMs_ = now;
        remoteTetrisReceived_ = true;
        break;
      }
      case MessageType::ZeroInputWindow:
        if (profile_ != GameProfile::TetrisZero || (phase_ != Phase::Playing && phase_ != Phase::Countdown) ||
            !zero_.receive(message.zero)) return false;
        break;
      case MessageType::ZeroProgress:
        if (profile_ != GameProfile::TetrisZero || (phase_ != Phase::Playing && phase_ != Phase::Countdown) ||
            !zero_.acknowledge(message.zeroProgress)) return false;
        break;
      case MessageType::Bye:
        transportDisconnected();
        return true;
      case MessageType::HockeyInput:
        if (profile_ != GameProfile::Hockey || role_ != Role::Host || phase_ != Phase::Playing || !hockey_->receiveInput(message.hockeyInput,now)) return false;
        break;
      case MessageType::HockeyMotion:
      case MessageType::HockeyBoard:
        if (profile_ != GameProfile::Hockey || role_ != Role::Guest || phase_ != Phase::Playing ||
            !hockey_->receiveFrame(hockeyPart(message.type),message.hockeyFrame,now)) return false;
        break;
      default: return false;
    }
    lastReceivedMs_ = now;
    advance(now);
    return true;
  }

  bool nextPacket(uint32_t now, Packet& packet) {
    advance(now);
    if (!connected_) return false;
    Message message;
    message.session = session_;
    message.round = round_;
    if (role_ == Role::Guest && phase_ == Phase::Waiting && uint32_t(now - controlSentMs_) >= 250) {
      message.type = MessageType::Hello;
      message.session = 0;
      controlSentMs_ = now;
    } else if (role_ == Role::Host && phase_ == Phase::Lobby && uint32_t(now - controlSentMs_) >= 250) {
      message.type = MessageType::Offer;
      message.seed = seed_;
      message.hue = hue_;
      controlSentMs_ = now;
    } else if (role_ == Role::Host &&
               (phase_ == Phase::Countdown || (phase_ == Phase::Playing && uint32_t(now - startMs_) < 500)) &&
               uint32_t(now - controlSentMs_) >= 50) {
      message.type = MessageType::Start;
      message.seed = seed_;
      message.hue = hue_;
      message.remainingMs = reached(now, startMs_) ? 0 : uint16_t(startMs_ - now);
      controlSentMs_ = now;
    } else if (role_ == Role::Guest && ackPending_) {
      message.type = MessageType::StartAck;
      ackPending_ = false;
    } else if ((phase_ == Phase::Lobby || phase_ == Phase::Playing) &&
               uint32_t(now - readySentMs_) >= 200) {
      message.type = MessageType::Ready;
      message.ready = localReady_;
      readySentMs_ = now;
    } else if (profile_ == GameProfile::TetrisZero && phase_ == Phase::Playing) {
      if (!nextZeroPacket(now, message)) return false;
    } else if (profile_ == GameProfile::Hockey && phase_ == Phase::Playing) {
      hockey::Part part;
      if (!hockey_->next(role_==Role::Host,now,part,message.hockeyFrame,message.hockeyInput)) return false;
      message.type = part==hockey::Part::Input ? MessageType::HockeyInput : part==hockey::Part::Motion ? MessageType::HockeyMotion : MessageType::HockeyBoard;
    } else if (profile_ != GameProfile::TetrisZero && phase_ == Phase::Playing &&
               uint32_t(now - snapshotSentMs_) >= SnapshotIntervalMs) {
      if (profile_ != GameProfile::Flappy) {
        message.type = MessageType::TetrisStats;
        message.tetris = localTetris_;
        message.tetrisSequence = ++tetrisSequence_;
      } else {
        message.type = MessageType::Snapshot;
        message.player = local_;
      }
      snapshotSentMs_ = now;
    } else {
      return false;
    }
    return encode(message, packet);
  }

  bool consumeStart(StartInfo& start) {
    if (!pendingStart_) return false;
    start.seed = seed_;
    start.round = round_;
    start.deadlineMs = startMs_;
    start.baseHue = hue_;
    pendingStart_ = false;
    return true;
  }

  uint32_t worldTick(uint32_t now) const {
    if (phase_ != Phase::Playing) return 0;
    if (role_ == Role::Guest && anchorValid_) {
      const uint32_t elapsed = uint32_t(now - anchorMs_);
      const uint32_t bounded = elapsed < GhostStaleMs ? elapsed : GhostStaleMs;
      return anchorTick_ + uint32_t((uint64_t(bounded) * 60) / 1000);
    }
    return uint32_t((uint64_t(uint32_t(now - startMs_)) * 60) / 1000);
  }

  RemoteView ghost(uint32_t now) const {
    RemoteView view;
    if (profile_ != GameProfile::Flappy || !connected_ || phase_ != Phase::Playing || !remoteCount_ ||
        uint32_t(now - current_.receivedMs) >= GhostStaleMs) return view;
    static_cast<PlayerSnapshot&>(view) = current_.player;
    view.visible = true;
    if (remoteCount_ < 2) return view;
    const uint32_t span = uint32_t(current_.receivedMs - previous_.receivedMs);
    if (!span || span > MaxInterpolationMs) return view;
    const int32_t age = tickDelta(now - InterpolationMs, previous_.receivedMs);
    if (age <= 0) {
      static_cast<PlayerSnapshot&>(view) = previous_.player;
    } else if (uint32_t(age) < span) {
      const float amount = float(age) / float(span);
      view.y = previous_.player.y + (current_.player.y - previous_.player.y) * amount;
      view.rotation = int8_t(float(previous_.player.rotation) +
                            float(current_.player.rotation - previous_.player.rotation) * amount);
    }
    return view;
  }

  RemoteTetris remoteTetris(uint32_t now) const {
    RemoteTetris view;
    if (profile_ == GameProfile::Flappy || !connected_ || !remoteTetrisReceived_) return view;
    view.stats = remoteTetrisStats_;
    view.received = true;
    view.ageMs = uint32_t(now - remoteTetrisReceivedMs_);
    view.fresh = phase_ == Phase::Playing && view.ageMs < GhostStaleMs;
    return view;
  }

  Role role() const { return role_; }
  GameProfile gameProfile() const { return profile_; }
  Phase phase() const { return phase_; }
  bool connected() const { return connected_; }
  bool localReady() const { return localReady_; }
  bool remoteReady() const { return remoteReady_; }
  uint32_t sessionId() const { return session_; }
  uint32_t seed() const { return seed_; }
  uint8_t round() const { return round_; }
  uint16_t baseHue() const { return hue_; }
  uint32_t startDeadlineMs() const { return startMs_; }

 private:
  struct Sample { PlayerSnapshot player; uint32_t receivedMs = 0; };
  GameProfile profile_ = GameProfile::Flappy;
  Role role_ = Role::Off;
  Phase phase_ = Phase::Off;
  uint32_t session_ = 0;
  uint32_t seed_ = 0;
  uint16_t hue_ = 0;
  uint8_t round_ = 0;
  bool connected_ = false;
  bool localReady_ = false;
  bool remoteReady_ = false;
  bool pendingStart_ = false;
  bool ackPending_ = false;
  bool startAck_ = false;
  uint32_t startMs_ = 0;
  uint32_t lastReceivedMs_ = 0;
  uint32_t controlSentMs_ = 0;
  uint32_t readySentMs_ = 0;
  uint32_t snapshotSentMs_ = 0;
  uint32_t anchorTick_ = 0;
  uint32_t anchorMs_ = 0;
  bool anchorValid_ = false;
  uint8_t remoteCount_ = 0;
  PlayerSnapshot local_;
  Sample previous_;
  Sample current_;
  TetrisStats localTetris_;
  TetrisStats remoteTetrisStats_;
  uint32_t remoteTetrisReceivedMs_ = 0;
  uint16_t tetrisSequence_ = 0;
  uint16_t remoteTetrisSequence_ = 0;
  bool remoteTetrisReceived_ = false;
  ZeroInputBuffer zero_;
  std::unique_ptr<hockey::Channel> hockey_;
  uint32_t zeroInputSentMs_ = 0, zeroProgressSentMs_ = 0;
  uint32_t zeroProgressMs_ = 0, observedZeroFrame_ = 0;
  uint8_t zeroPacketTurn_ = 0;

  bool nextZeroPacket(uint32_t now, Message& message) {
    // A slow rendering loop must still alternate inputs and acknowledgements.
    // Strict timer priority would let overdue acknowledgements starve inputs.
    for (uint8_t offset = 0; offset < 3; ++offset) {
      const uint8_t turn = uint8_t((zeroPacketTurn_ + offset) % 3);
      if (turn == 0 && uint32_t(now - zeroInputSentMs_) >= ZeroInputIntervalMs && zero_.window(message.zero)) {
        message.type = MessageType::ZeroInputWindow;
        zeroInputSentMs_ = now;
      } else if (turn == 1 && uint32_t(now - zeroProgressSentMs_) >= ZeroProgressIntervalMs) {
        message.type = MessageType::ZeroProgress;
        message.zeroProgress = zero_.nextFrame();
        zeroProgressSentMs_ = now;
      } else if (turn == 2 && uint32_t(now - snapshotSentMs_) >= ZeroStatsIntervalMs) {
        message.type = MessageType::TetrisStats;
        message.tetris = localTetris_;
        message.tetrisSequence = ++tetrisSequence_;
        snapshotSentMs_ = now;
      } else continue;
      zeroPacketTurn_ = uint8_t((turn + 1) % 3);
      return true;
    }
    return false;
  }

  bool localFinished() const { return profile_ == GameProfile::Hockey ? hockey_->finished(role_==Role::Host) : profile_ != GameProfile::Flappy ? localTetris_.toppedOut : local_.life == Life::GameOver; }
  bool remoteFinished() const {
    if (profile_ == GameProfile::Hockey) return hockey_->peerFinished(role_==Role::Host);
    return profile_ != GameProfile::Flappy ? remoteTetrisReceived_ && remoteTetrisStats_.toppedOut :
           remoteCount_ && current_.player.life == Life::GameOver;
  }
  void clearRemoteTetris() {
    remoteTetrisStats_ = TetrisStats();
    remoteTetrisReceived_ = false;
    remoteTetrisSequence_ = 0;
    remoteTetrisReceivedMs_ = 0;
  }
  void resetState() {
    const GameProfile profile = profile_;
    auto channel = std::move(hockey_);
    *this = Session();
    profile_ = profile;
    hockey_ = std::move(channel);
    if (hockey_) hockey_->reset();
  }

  void resetTimers(uint32_t now) {
    lastReceivedMs_ = now;
    controlSentMs_ = now - 250;
    readySentMs_ = now - 200;
    snapshotSentMs_ = now - SnapshotIntervalMs;
  }
};

}  // namespace coop
}  // namespace flappy
