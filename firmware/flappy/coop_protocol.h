#pragma once

#include <stddef.h>
#include <stdint.h>

namespace flappy {
namespace coop {

constexpr uint8_t ProtocolVersion = 1;
constexpr size_t MaxPacketBytes = 20;
constexpr uint32_t SnapshotIntervalMs = 50;
constexpr uint32_t InterpolationMs = 50;
constexpr uint32_t MaxInterpolationMs = 100;
constexpr uint32_t GhostStaleMs = 500;
constexpr uint32_t DisconnectMs = 3000;
constexpr uint16_t CountdownMs = 2500;

enum class Role : uint8_t { Off, Host, Guest };
enum class Phase : uint8_t { Off, Waiting, Lobby, Countdown, Playing, Disconnected };
enum class Life : uint8_t { Ready, Alive, Dying, GameOver };
enum class MessageType : uint8_t { Hello = 1, Offer, Ready, Start, StartAck, Snapshot, Bye };

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
};

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
    case MessageType::Hello:
    case MessageType::StartAck:
    case MessageType::Bye: break;
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
    case MessageType::Hello:
    case MessageType::StartAck:
    case MessageType::Bye: break;
  }
  message = decoded;
  return true;
}

// All state changes happen on the game task. BLE callbacks only queue bytes.
class Session {
 public:
  void beginHost(uint32_t now, uint32_t session, uint32_t seed, uint16_t hue) {
    *this = Session();
    role_ = Role::Host;
    phase_ = Phase::Waiting;
    session_ = session ? session : 1;
    seed_ = seed ? seed : 0x8f6e37a1U;
    hue_ = hue % 360;
    round_ = 1;
    resetTimers(now);
  }

  void beginGuest(uint32_t now) {
    *this = Session();
    role_ = Role::Guest;
    phase_ = Phase::Waiting;
    resetTimers(now);
  }

  void stop() { *this = Session(); }

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
    localReady_ = remoteReady_ = false;
  }

  void setLocal(const PlayerSnapshot& player) {
    if (validPlayer(player)) local_ = player;
  }

  void requestReady(uint32_t now) {
    if (!connected_ || !session_ || !round_) return;
    if (phase_ == Phase::Lobby || (phase_ == Phase::Playing && local_.life == Life::GameOver)) {
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
    const bool finished = phase_ == Phase::Playing && local_.life == Life::GameOver &&
                          remoteCount_ && current_.player.life == Life::GameOver;
    if (role_ == Role::Host && localReady_ && remoteReady_ && (phase_ == Phase::Lobby || finished)) {
      if (finished) {
        round_ = nextRound(round_);
        seed_ = nextSeed(seed_);
      }
      startMs_ = now + CountdownMs;
      phase_ = Phase::Countdown;
      localReady_ = remoteReady_ = false;
      pendingStart_ = true;
      startAck_ = false;
      anchorValid_ = false;
      remoteCount_ = 0;
      controlSentMs_ = now - 50;
    }
    if (phase_ == Phase::Countdown && reached(now, startMs_)) {
      if (role_ == Role::Host && !startAck_) {
        phase_ = Phase::Lobby;
        pendingStart_ = false;
        controlSentMs_ = now - 250;
      } else {
        phase_ = Phase::Playing;
        snapshotSentMs_ = now - SnapshotIntervalMs;
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
      const bool initial = phase_ == Phase::Lobby && message.round == round_ && localReady_;
      const bool restart = phase_ == Phase::Playing && message.round == nextRound(round_) &&
                           localReady_ && local_.life == Life::GameOver;
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
      case MessageType::Bye:
        transportDisconnected();
        return true;
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
    } else if (phase_ == Phase::Playing && uint32_t(now - snapshotSentMs_) >= SnapshotIntervalMs) {
      message.type = MessageType::Snapshot;
      message.player = local_;
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
    if (!connected_ || phase_ != Phase::Playing || !remoteCount_ ||
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

  Role role() const { return role_; }
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

  void resetTimers(uint32_t now) {
    lastReceivedMs_ = now;
    controlSentMs_ = now - 250;
    readySentMs_ = now - 200;
    snapshotSentMs_ = now - SnapshotIntervalMs;
  }
};

}  // namespace coop
}  // namespace flappy
