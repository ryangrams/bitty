#define bit(b) (1UL << (b))
#include "../firmware/flappy/coop_protocol.h"
#undef bit

#include <assert.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using namespace flappy::coop;

static void near(float first, float second, float tolerance = 0.001f) {
  assert(std::fabs(first - second) <= tolerance);
}

static Packet encoded(const Message& message) {
  Packet packet;
  assert(encode(message, packet));
  return packet;
}

static Message snapshotMessage(uint32_t session, uint8_t round, uint32_t tick, float y) {
  Message message;
  message.type = MessageType::Snapshot;
  message.session = session;
  message.round = round;
  message.player.tick = tick;
  message.player.y = y;
  message.player.rotation = -45;
  message.player.frame = 2;
  message.player.life = Life::Alive;
  message.player.score = 234;
  return message;
}

static bool deliver(Session& recipient, const Message& message, uint32_t now) {
  const Packet packet = encoded(message);
  return recipient.receive(packet.bytes, packet.length, now);
}

struct Pair {
  Session host;
  Session guest;
  TetrisStats hostTetris;
  TetrisStats guestTetris;
  unsigned hostTetrisPackets = 0;
  unsigned guestTetrisPackets = 0;

  explicit Pair(uint32_t now = 0, GameProfile profile = GameProfile::Flappy,
                uint32_t seed = 123456, uint16_t hue = 73) {
    assert(host.setGameProfile(profile));
    assert(guest.setGameProfile(profile));
    host.beginHost(now, 0x12345678, seed, hue);
    guest.beginGuest(now);
    host.transportConnected(now);
    guest.transportConnected(now);
  }

  void pump(uint32_t now, Life hostLife = Life::Alive, Life guestLife = Life::Alive) {
    PlayerSnapshot local;
    local.tick = host.worldTick(now);
    local.life = hostLife;
    if (host.gameProfile() != GameProfile::Flappy) {
      hostTetris.toppedOut = hostLife == Life::GameOver;
      host.setLocalTetris(hostTetris);
    } else host.setLocal(local);
    local.tick = guest.worldTick(now);
    local.life = guestLife;
    if (guest.gameProfile() != GameProfile::Flappy) {
      guestTetris.toppedOut = guestLife == Life::GameOver;
      guest.setLocalTetris(guestTetris);
    } else guest.setLocal(local);
    Packet packet;
    if (host.nextPacket(now, packet)) {
      if (packet.bytes[3] == uint8_t(MessageType::TetrisStats)) ++hostTetrisPackets;
      guest.receive(packet.bytes, packet.length, now);
    }
    if (guest.nextPacket(now, packet)) {
      if (packet.bytes[3] == uint8_t(MessageType::TetrisStats)) ++guestTetrisPackets;
      host.receive(packet.bytes, packet.length, now);
    }
    host.advance(now);
    guest.advance(now);
  }

  uint32_t start(uint32_t now = 0) {
    for (uint32_t elapsed = 0; elapsed <= 300; elapsed += 10) pump(now + elapsed);
    assert(host.phase() == Phase::Lobby && guest.phase() == Phase::Lobby);
    host.requestReady(now + 310);
    guest.requestReady(now + 310);
    for (uint32_t elapsed = 310; elapsed <= 500; elapsed += 10) pump(now + elapsed);
    assert(host.phase() == Phase::Countdown && guest.phase() == Phase::Countdown);
    const uint32_t deadline = host.startDeadlineMs();
    for (uint32_t elapsed = 510; elapsed < 3300; elapsed += 10) pump(now + elapsed);
    assert(host.phase() == Phase::Playing && guest.phase() == Phase::Playing);
    return deadline;
  }
};

static void wireRoundTrips() {
  static_assert(uint8_t(MessageType::Bye) == 7, "Legacy message numbers changed");
  for (uint8_t rawType = uint8_t(MessageType::Hello); rawType <= uint8_t(MessageType::ZeroProgress); ++rawType) {
    Message message;
    message.type = MessageType(rawType);
    message.session = message.type == MessageType::Hello ? 0 : 0x90785634;
    message.round = 255;
    message.seed = 0x00abcdef;
    message.hue = 359;
    message.ready = true;
    message.remainingMs = CountdownMs;
    message.player = snapshotMessage(1, 1, UINT32_MAX, -63.9375f).player;
    message.player.score = UINT16_MAX;
    message.tetris.score = UINT32_MAX;
    message.tetris.lines = UINT16_MAX;
    message.tetris.level = UINT8_MAX;
    message.tetris.toppedOut = true;
    message.tetrisSequence = UINT16_MAX;
    message.zero.frame = UINT32_MAX - 2;
    message.zero.count = ZeroWindowPads;
    for (uint8_t i = 0; i < ZeroWindowPads; ++i) message.zero.pads[i] = uint8_t(85 * i);
    message.zeroProgress = UINT32_MAX;
    const Packet packet = encoded(message);
    assert(packet.length <= 20);
    Message decoded;
    assert(decode(packet.bytes, packet.length, decoded));
    assert(decoded.type == message.type && decoded.session == message.session);
    if (message.type != MessageType::Hello) assert(decoded.round == message.round);
    if (message.type == MessageType::Offer || message.type == MessageType::Start) {
      assert(decoded.seed == message.seed && decoded.hue == message.hue);
    }
    if (message.type == MessageType::Ready) assert(decoded.ready);
    if (message.type == MessageType::Start) assert(decoded.remainingMs == CountdownMs);
    if (message.type == MessageType::Snapshot) {
      near(decoded.player.y, message.player.y);
      assert(decoded.player.tick == UINT32_MAX);
      assert(decoded.player.rotation == -45 && decoded.player.frame == 2);
      assert(decoded.player.life == Life::Alive && decoded.player.score == UINT16_MAX);
    }
    if (message.type == MessageType::TetrisStats) {
      assert(packet.length == 19 && packet.length < 20);
      assert(decoded.tetris.score == UINT32_MAX && decoded.tetris.lines == UINT16_MAX);
      assert(decoded.tetris.level == UINT8_MAX && decoded.tetris.toppedOut);
      assert(decoded.tetrisSequence == UINT16_MAX);
    }
    if (message.type == MessageType::ZeroInputWindow) {
      assert(packet.length == 18);
      assert(decoded.zero.frame == message.zero.frame && decoded.zero.count == ZeroWindowPads);
      assert(!memcmp(decoded.zero.pads, message.zero.pads, ZeroWindowPads));
    }
    if (message.type == MessageType::ZeroProgress) {
      assert(packet.length == 13 && decoded.zeroProgress == UINT32_MAX);
    }
    for (size_t length = 0; length < packet.length; ++length) assert(!decode(packet.bytes, length, decoded));
    assert(!decode(packet.bytes, packet.length + 1, decoded));
  }
  Message message = snapshotMessage(1, 1, 9, 215.123f);
  Message decoded;
  const Packet packet = encoded(message);
  assert(decode(packet.bytes, packet.length, decoded));
  near(decoded.player.y, message.player.y, 1.0f / 32.0f);
}

static void malformedPackets() {
  Message message = snapshotMessage(1, 1, 0, 244);
  Packet packet = encoded(message);
  Message decoded;
  assert(!decode(nullptr, packet.length, decoded));
  for (uint8_t offset = 0; offset < 4; ++offset) {
    Packet changed = packet;
    changed.bytes[offset] = 0xff;
    assert(!decode(changed.bytes, changed.length, decoded));
  }
  Packet changed = packet;
  put32(changed.bytes + 4, 0);
  assert(!decode(changed.bytes, changed.length, decoded));
  changed = packet;
  changed.bytes[8] = 0;
  assert(!decode(changed.bytes, changed.length, decoded));
  changed = packet;
  changed.bytes[16] = 0x80;
  assert(!decode(changed.bytes, changed.length, decoded));
  changed.bytes[16] = 3;
  assert(!decode(changed.bytes, changed.length, decoded));
  changed = packet;
  changed.bytes[15] = uint8_t(-91);
  assert(!decode(changed.bytes, changed.length, decoded));
  changed = packet;
  put16(changed.bytes + 13, 9000);
  assert(!decode(changed.bytes, changed.length, decoded));
  message.player.y = std::numeric_limits<float>::quiet_NaN();
  assert(!encode(message, packet));
  message.player.y = std::numeric_limits<float>::infinity();
  assert(!encode(message, packet));
  message.type = MessageType::Offer;
  message.seed = 1;
  message.hue = 360;
  assert(!encode(message, packet));
  message.hue = 0;
  message.seed = 0;
  assert(!encode(message, packet));
  message.seed = 1;
  packet = encoded(message);
  put16(packet.bytes + 13, 360);
  assert(!decode(packet.bytes, packet.length, decoded));
  message.type = MessageType::Ready;
  packet = encoded(message);
  packet.bytes[9] = 2;
  assert(!decode(packet.bytes, packet.length, decoded));
  message.type = MessageType::Start;
  message.remainingMs = CountdownMs + 1;
  assert(!encode(message, packet));
  message.remainingMs = CountdownMs;
  packet = encoded(message);
  put16(packet.bytes + 13, CountdownMs + 1);
  assert(!decode(packet.bytes, packet.length, decoded));
}

static void readinessAndCourseAgreement() {
  Pair pair;
  for (uint32_t now = 0; now < 5000; now += 10) pair.pump(now);
  assert(pair.host.phase() == Phase::Lobby && pair.guest.phase() == Phase::Lobby);
  pair.host.requestReady(5000);
  for (uint32_t now = 5000; now < 5500; now += 10) pair.pump(now);
  assert(pair.host.phase() == Phase::Lobby);
  assert(pair.guest.remoteReady());
  pair.guest.requestReady(5500);
  for (uint32_t now = 5500; now < 5800; now += 10) pair.pump(now);
  assert(pair.host.phase() == Phase::Countdown && pair.guest.phase() == Phase::Countdown);
  StartInfo hostStart, guestStart;
  assert(pair.host.consumeStart(hostStart));
  assert(pair.guest.consumeStart(guestStart));
  assert(!pair.host.consumeStart(hostStart) && !pair.guest.consumeStart(guestStart));
  assert(hostStart.seed == guestStart.seed && hostStart.round == guestStart.round);
  assert(hostStart.baseHue == guestStart.baseHue);
  assert(hostStart.deadlineMs == guestStart.deadlineMs);
  for (uint32_t now = 5800; now < 9000; now += 10) pair.pump(now);
  assert(pair.host.phase() == Phase::Playing && pair.guest.phase() == Phase::Playing);
  assert(std::abs(tickDelta(pair.host.worldTick(9000), pair.guest.worldTick(9000))) <= 1);
  assert(pair.host.ghost(9000).visible && pair.guest.ghost(9000).visible);
}

static void invalidPeerCannotKeepSessionAlive() {
  Pair pair;
  pair.start();
  Message message = snapshotMessage(pair.host.sessionId() ^ 1, pair.host.round(), 40, 100);
  assert(!deliver(pair.host, message, 4000));
  message.session = pair.host.sessionId();
  message.round = nextRound(pair.host.round());
  assert(!deliver(pair.host, message, 4100));
  message.round = pair.host.round();
  message.player.tick = 1000000;
  assert(!deliver(pair.host, message, 4200));
  pair.host.advance(6300);
  assert(pair.host.phase() == Phase::Disconnected);
  assert(!pair.host.ghost(6300).visible);
}

static void interpolationAndStaleness() {
  Pair pair;
  const uint32_t deadline = pair.start();
  const uint32_t now = 4000;
  Message message = snapshotMessage(pair.host.sessionId(), pair.host.round(),
                                    uint32_t((uint64_t(now - deadline) * 60) / 1000), 100);
  assert(deliver(pair.guest, message, now));
  message.player.tick += 3;
  message.player.y = 200;
  message.player.rotation = 15;
  assert(deliver(pair.guest, message, now + 50));
  near(pair.guest.ghost(now + 75).y, 150);
  assert(pair.guest.ghost(now + 75).rotation == -15);
  near(pair.guest.ghost(now + 250).y, 200);
  assert(pair.guest.ghost(now + 549).visible);
  assert(!pair.guest.ghost(now + 550).visible);
  assert(!deliver(pair.guest, message, now + 551));
  message.player.tick += 30;
  message.player.y = 300;
  assert(deliver(pair.guest, message, now + 600));
  near(pair.guest.ghost(now + 610).y, 300);
  pair.guest.advance(now + 3599);
  assert(pair.guest.connected());
  pair.guest.advance(now + 3600);
  assert(pair.guest.phase() == Phase::Disconnected);
}

static void restartRequiresBothPlayers() {
  Pair pair;
  pair.start();
  const uint32_t oldSeed = pair.host.seed();
  const uint8_t oldRound = pair.host.round();
  for (uint32_t now = 3300; now < 3500; now += 10) pair.pump(now, Life::GameOver, Life::Alive);
  pair.host.requestReady(3500);
  pair.guest.requestReady(3500);
  assert(!pair.guest.localReady());
  for (uint32_t now = 3500; now < 4000; now += 10) pair.pump(now, Life::GameOver, Life::Alive);
  assert(pair.host.phase() == Phase::Playing);
  for (uint32_t now = 4000; now < 4200; now += 10) pair.pump(now, Life::GameOver, Life::GameOver);
  pair.guest.requestReady(4200);
  for (uint32_t now = 4200; now < 4500; now += 10) pair.pump(now, Life::GameOver, Life::GameOver);
  assert(pair.host.phase() == Phase::Countdown && pair.guest.phase() == Phase::Countdown);
  assert(pair.host.round() == nextRound(oldRound));
  assert(pair.host.seed() != oldSeed && pair.host.seed() == pair.guest.seed());
  assert(!pair.host.ghost(4500).visible && !pair.guest.ghost(4500).visible);
}

static void wrapAndPhysicalDisconnect() {
  assert(newerTick(0, UINT32_MAX));
  assert(newerTick(5, UINT32_MAX - 3));
  assert(!newerTick(UINT32_MAX, 0));
  assert(tickDelta(0, UINT32_MAX) == 1);
  assert(tickDelta(UINT32_MAX, 0) == -1);
  assert(tickDelta(0x80000000U, 0) == INT32_MIN);
  assert(nextRound(255) == 1);
  Pair pair(UINT32_MAX - 1500);
  pair.start(UINT32_MAX - 1500);
  assert(pair.host.phase() == Phase::Playing && pair.guest.phase() == Phase::Playing);
  pair.host.transportDisconnected();
  assert(pair.host.phase() == Phase::Disconnected);
  assert(!pair.host.ghost(1800).visible);
  pair.host.stop();
  assert(pair.host.phase() == Phase::Off && pair.host.role() == Role::Off);
}

static void countdownNeedsAcknowledgement() {
  Pair pair;
  for (uint32_t now = 0; now < 300; now += 10) pair.pump(now);
  pair.host.requestReady(300);
  Message ready;
  ready.type = MessageType::Ready;
  ready.session = pair.host.sessionId();
  ready.round = pair.host.round();
  ready.ready = true;
  assert(deliver(pair.host, ready, 300));
  assert(pair.host.phase() == Phase::Countdown);
  const uint32_t deadline = pair.host.startDeadlineMs();
  // Valid traffic keeps the transport alive, but a start acknowledgement is absent.
  assert(deliver(pair.host, ready, deadline - 50));
  pair.host.advance(deadline);
  assert(pair.host.phase() == Phase::Lobby);
  StartInfo start;
  assert(!pair.host.consumeStart(start));
}

static void delayedAndDroppedPacketsRecover() {
  struct InFlight {
    Packet packet;
    uint32_t arrival;
    bool toHost;
  };
  Pair pair;
  std::vector<InFlight> network;
  unsigned sent = 0;
  for (uint32_t now = 0; now < 8000; now += 10) {
    for (size_t i = 0; i < network.size();) {
      if (reached(now, network[i].arrival)) {
        Session& destination = network[i].toHost ? pair.host : pair.guest;
        destination.receive(network[i].packet.bytes, network[i].packet.length, now);
        network.erase(network.begin() + i);
      } else {
        ++i;
      }
    }
    if (now == 500) {
      pair.host.requestReady(now);
      pair.guest.requestReady(now);
    }
    PlayerSnapshot local;
    local.life = Life::Alive;
    local.tick = pair.host.worldTick(now);
    pair.host.setLocal(local);
    local.tick = pair.guest.worldTick(now);
    pair.guest.setLocal(local);
    Packet packet;
    if (pair.host.nextPacket(now, packet) && ++sent % 7) {
      InFlight pending;
      pending.packet = packet;
      pending.arrival = now + 20;
      pending.toHost = false;
      network.push_back(pending);
    }
    if (pair.guest.nextPacket(now, packet) && ++sent % 7) {
      InFlight pending;
      pending.packet = packet;
      pending.arrival = now + 30;
      pending.toHost = true;
      network.push_back(pending);
    }
    pair.host.advance(now);
    pair.guest.advance(now);
  }
  assert(pair.host.phase() == Phase::Playing && pair.guest.phase() == Phase::Playing);
  assert(pair.host.seed() == pair.guest.seed() && pair.host.round() == pair.guest.round());
  assert(pair.host.ghost(8000).visible && pair.guest.ghost(8000).visible);
  assert(std::abs(tickDelta(pair.host.worldTick(8000), pair.guest.worldTick(8000))) <= 3);
}

static void decoderFuzzStaysBounded() {
  uint32_t random = 75;
  uint8_t bytes[MaxPacketBytes + 8] = {};
  for (unsigned iteration = 0; iteration < 10000; ++iteration) {
    random = nextSeed(random);
    const size_t length = random % sizeof(bytes);
    for (size_t i = 0; i < sizeof(bytes); ++i) {
      random = nextSeed(random);
      bytes[i] = uint8_t(random);
    }
    // Periodically preserve the header so malformed payload branches are reached.
    if (iteration % 2 == 0) {
      bytes[0] = 'M';
      bytes[1] = 'F';
      bytes[2] = ProtocolVersion;
      bytes[3] = uint8_t(1 + iteration % 8);
    }
    Message message;
    if (decode(bytes, length, message)) {
      Packet packet;
      assert(encode(message, packet));
      assert(packet.length == length);
    }
  }
}

static Message tetrisMessage(const Session& session, uint16_t sequence, uint32_t score = 123456789) {
  Message message;
  message.type = MessageType::TetrisStats;
  message.session = session.sessionId();
  message.round = session.round();
  message.tetrisSequence = sequence;
  message.tetris.score = score;
  message.tetris.lines = 246;
  message.tetris.level = 28;
  return message;
}

static void gameProfilesAreIsolated() {
  assert(matchesGameService(GameProfile::Flappy, ServiceUuid));
  assert(matchesGameService(GameProfile::Tetris, TetrisServiceUuid));
  assert(!matchesGameService(GameProfile::Flappy, TetrisServiceUuid));
  assert(!matchesGameService(GameProfile::Tetris, ServiceUuid));
  assert(!matchesGameService(GameProfile::Tetris, nullptr));
  assert(strcmp(serviceUuid(GameProfile::Flappy), serviceUuid(GameProfile::Tetris)));
  assert(strcmp(hostTxUuid(GameProfile::Flappy), hostTxUuid(GameProfile::Tetris)));
  assert(strcmp(guestTxUuid(GameProfile::Flappy), guestTxUuid(GameProfile::Tetris)));
  for (GameProfile first : {GameProfile::Flappy, GameProfile::Tetris, GameProfile::TetrisZero}) {
    for (GameProfile second : {GameProfile::Flappy, GameProfile::Tetris, GameProfile::TetrisZero}) {
      assert(matchesGameService(first, serviceUuid(second)) == (first == second));
      if (first == second) continue;
      assert(strcmp(serviceUuid(first), serviceUuid(second)));
      assert(strcmp(hostTxUuid(first), hostTxUuid(second)));
      assert(strcmp(guestTxUuid(first), guestTxUuid(second)));
    }
  }
  Session session;
  assert(session.gameProfile() == GameProfile::Flappy);
  assert(!session.setGameProfile(GameProfile(255)));
  assert(session.setGameProfile(GameProfile::Tetris));
  session.beginHost(0, 1, 0x09001234, 1);
  assert(session.gameProfile() == GameProfile::Tetris);
  session.beginHost(0, 1, 0, 0);
  assert((session.seed() >> 24) == 0 && (session.seed() & 0xffffff) != 0);
  session.stop();
  assert(session.gameProfile() == GameProfile::Tetris);
  session.beginGuest(100);
  assert(session.gameProfile() == GameProfile::Tetris);
  session.transportConnected(100);
  assert(session.setGameProfile(GameProfile::Flappy));
  assert(session.role() == Role::Off && !session.connected());
}

static void tetrisStatsValidationAndFreshness() {
  Pair pair(0, GameProfile::Tetris, 0x09001234, 1);
  pair.hostTetris.score = 654321;
  pair.hostTetris.lines = 240;
  pair.hostTetris.level = 29;
  pair.guestTetris.score = 123456;
  pair.start();
  assert(pair.host.seed() == 0x09001234 && pair.host.baseHue() == 1);
  assert(pair.host.remoteTetris(3300).received && pair.guest.remoteTetris(3300).received);
  assert(!pair.host.ghost(3300).visible && !pair.guest.ghost(3300).visible);
  const unsigned sent = pair.hostTetrisPackets;
  for (uint32_t now = 3300; now < 4300; now += 10) pair.pump(now);
  assert(pair.hostTetrisPackets - sent >= 15 && pair.hostTetrisPackets - sent <= 21);
  RemoteTetris view = pair.guest.remoteTetris(4300);
  assert(view.received && view.fresh && view.stats.score == 654321);
  assert(view.stats.lines == 240 && view.stats.level == 29 && !view.stats.toppedOut);
  Message message = tetrisMessage(pair.guest, 500, UINT32_MAX);
  assert(deliver(pair.guest, message, 4400));
  view = pair.guest.remoteTetris(4400);
  assert(view.fresh && view.ageMs == 0 && view.stats.score == UINT32_MAX);
  assert(!deliver(pair.guest, message, 4401));
  message.tetrisSequence = 499;
  assert(!deliver(pair.guest, message, 4402));
  message.tetrisSequence = 501;
  message.tetris.score = 654322;
  message.tetris.toppedOut = true;
  assert(deliver(pair.guest, message, 4450));
  assert(pair.guest.remoteTetris(4450).stats.score == 654322);
  assert(pair.guest.remoteTetris(4450).stats.toppedOut);
  assert(pair.guest.remoteTetris(4949).fresh);
  assert(!pair.guest.remoteTetris(4950).fresh && pair.guest.remoteTetris(4950).received);
  Packet packet = encoded(message);
  for (uint8_t invalid = 2; invalid != 0; invalid = uint8_t(invalid << 1)) {
    packet.bytes[18] = invalid;
    Message decoded;
    assert(!decode(packet.bytes, packet.length, decoded));
  }
  Message bird = snapshotMessage(pair.guest.sessionId(), pair.guest.round(), 120, 200);
  assert(!deliver(pair.guest, bird, 5000));
  Pair flappy;
  flappy.start();
  message.session = flappy.host.sessionId();
  message.round = flappy.host.round();
  assert(!deliver(flappy.host, message, 5000));
  pair.guest.advance(7450);
  assert(pair.guest.phase() == Phase::Disconnected);
  assert(!pair.guest.remoteTetris(7450).received);
}

static void tetrisStatsSequenceWrapAndBadMetadata() {
  Session host;
  assert(host.setGameProfile(GameProfile::Tetris));
  host.beginHost(UINT32_MAX - 100, 42, 0x09001234, 0);
  host.transportConnected(UINT32_MAX - 100);
  host.requestReady(UINT32_MAX - 100);
  Message ready;
  ready.type = MessageType::Ready;
  ready.session = 42;
  ready.round = 1;
  ready.ready = true;
  assert(deliver(host, ready, UINT32_MAX - 100));
  assert(host.phase() == Phase::Countdown);
  Message message = tetrisMessage(host, 65534);
  assert(deliver(host, message, UINT32_MAX - 90));
  message.tetrisSequence = 65535;
  assert(deliver(host, message, UINT32_MAX - 80));
  message.tetrisSequence = 0;
  assert(deliver(host, message, 5));
  assert(host.remoteTetris(15).ageMs == 10);
  message.tetrisSequence = 65535;
  assert(!deliver(host, message, 20));
  message.tetrisSequence = 1;
  message.round = 2;
  assert(!deliver(host, message, 25));
  message.round = 1;
  message.session = 43;
  assert(!deliver(host, message, 30));
  host.transportDisconnected();
  assert(!host.remoteTetris(30).received);
  Session guest;
  guest.setGameProfile(GameProfile::Tetris);
  guest.beginGuest(0);
  guest.transportConnected(0);
  Message offer;
  offer.type = MessageType::Offer;
  offer.session = 42;
  offer.round = 1;
  offer.seed = 0x09001234;
  offer.hue = 2;
  assert(!deliver(guest, offer, 1));
}

static void tetrisRematchPreservesGameSettings() {
  Pair pair(0, GameProfile::Tetris, 0x09001234, 1);
  pair.start();
  const uint32_t seed = pair.host.seed();
  const uint8_t round = pair.host.round();
  for (uint32_t now = 3300; now < 3500; now += 10) pair.pump(now, Life::GameOver, Life::Alive);
  pair.host.requestReady(3500);
  pair.guest.requestReady(3500);
  assert(pair.host.localReady() && !pair.guest.localReady());
  for (uint32_t now = 3500; now < 4000; now += 10) pair.pump(now, Life::GameOver, Life::Alive);
  assert(pair.host.phase() == Phase::Playing);
  for (uint32_t now = 4000; now < 4200; now += 10) pair.pump(now, Life::GameOver, Life::GameOver);
  pair.guest.requestReady(4200);
  for (uint32_t now = 4200; now < 4500; now += 10) pair.pump(now, Life::GameOver, Life::GameOver);
  assert(pair.host.phase() == Phase::Countdown && pair.guest.phase() == Phase::Countdown);
  assert(pair.host.round() == nextRound(round));
  assert(pair.host.seed() != seed && pair.host.seed() == pair.guest.seed());
  assert((pair.host.seed() >> 24) == 9 && pair.host.baseHue() == 1 && pair.guest.baseHue() == 1);
  assert((pair.host.seed() & 0xffffff) != 0);
  assert(!pair.host.remoteTetris(4500).received && !pair.guest.remoteTetris(4500).received);
  StartInfo hostStart, guestStart;
  assert(pair.host.consumeStart(hostStart) && pair.guest.consumeStart(guestStart));
  assert(hostStart.seed == guestStart.seed && hostStart.baseHue == guestStart.baseHue);
}

static ZeroInputWindow inputWindow(uint32_t frame, uint8_t count, uint8_t firstPad = 0) {
  ZeroInputWindow window;
  window.frame = frame;
  window.count = count;
  for (uint8_t i = 0; i < count && i < ZeroWindowPads; ++i) window.pads[i] = uint8_t(firstPad + i);
  return window;
}

static Message zeroMessage(const Session& session, const ZeroInputWindow& window) {
  Message message;
  message.type = MessageType::ZeroInputWindow;
  message.session = session.sessionId();
  message.round = session.round();
  message.zero = window;
  return message;
}

static void zeroWireRejectsInvalidCountsAndUnusedPayload() {
  Session session;
  session.setGameProfile(GameProfile::TetrisZero);
  session.beginHost(0, 1, 1, 0);
  Message message = zeroMessage(session, inputWindow(UINT32_MAX, 1, 255));
  Packet packet = encoded(message);
  Message decoded;
  assert(decode(packet.bytes, packet.length, decoded));
  assert(decoded.zero.frame == UINT32_MAX && decoded.zero.pads[0] == 255);
  for (uint8_t count : {uint8_t(0), uint8_t(5), uint8_t(255)}) {
    packet.bytes[13] = count;
    assert(!decode(packet.bytes, packet.length, decoded));
    message.zero.count = count;
    assert(!encode(message, packet));
    packet = encoded(zeroMessage(session, inputWindow(1, 1)));
  }
  for (uint8_t index = 15; index < 18; ++index) {
    packet.bytes[index] = 1;
    assert(!decode(packet.bytes, packet.length, decoded));
    packet.bytes[index] = 0;
  }
}

static void zeroBufferIsBoundedImmutableAndWraps() {
  ZeroInputBuffer buffer;
  for (uint32_t first : {uint32_t(0), UINT32_MAX - 3}) {
    buffer.reset(first);
    // A faster peer can consume the preloaded neutral frames first.
    assert(buffer.acknowledge(first + ZeroInputDelay));
    for (uint32_t i = 0; i < 1000; ++i) {
      const uint32_t frame = first + i;
      const uint8_t captured = uint8_t(i * 31);
      assert(buffer.submit(frame + ZeroInputDelay, captured));
      assert(buffer.submit(frame + ZeroInputDelay, captured));
      assert(!buffer.submit(frame + ZeroInputDelay, uint8_t(captured ^ 1)));
      assert(!buffer.submit(frame + ZeroBufferFrames, captured));
      assert(buffer.receive(inputWindow(frame + ZeroInputDelay, 1, uint8_t(captured ^ 0x55))));
      assert(!buffer.receive(inputWindow(frame + ZeroInputDelay, 1, uint8_t(captured ^ 0x54))));
      assert(!buffer.receive(inputWindow(frame + ZeroBufferFrames, 1, 1)));
      assert(!buffer.acknowledge(frame + ZeroBufferFrames + 1));
      uint8_t local = 99, remote = 88;
      assert(!buffer.consume(frame + 1, local, remote));
      assert(local == 99 && remote == 88);
      assert(buffer.consume(frame, local, remote));
      assert(local == (i < ZeroInputDelay ? 0 : uint8_t((i - ZeroInputDelay) * 31)));
      assert(remote == (i < ZeroInputDelay ? 0 : uint8_t(uint8_t((i - ZeroInputDelay) * 31) ^ 0x55)));
      assert(!buffer.consume(frame, local, remote));
      assert(!buffer.receive(inputWindow(frame, 1, remote)));
      // The initial acknowledgement is already six frames ahead.
      if (i + 1 >= ZeroInputDelay) assert(buffer.acknowledge(frame + 1));
    }
  }
  buffer.reset();
  assert(buffer.receive(inputWindow(8, 1, 88)));
  assert(!buffer.receive(inputWindow(7, 2, 77)));
  for (uint32_t frame = 0; frame < ZeroInputDelay; ++frame) {
    uint8_t local, remote;
    assert(buffer.consume(frame, local, remote));
  }
  assert(buffer.submit(6, 1));
  uint8_t local = 1, remote = 2;
  assert(!buffer.consume(6, local, remote));
  assert(buffer.receive(inputWindow(6, 1, 6)));
  assert(buffer.consume(6, local, remote) && remote == 6);
  assert(buffer.submit(7, 1));
  assert(!buffer.consume(7, local, remote));
  assert(buffer.receive(inputWindow(7, 1, 7)));
  assert(buffer.consume(7, local, remote) && remote == 7);
  assert(buffer.submit(8, 1));
  assert(buffer.consume(8, local, remote) && remote == 88);
  buffer.clear();
  assert(!buffer.consume(0, local, remote));
}

static void zeroSessionValidationAndFlowTimeout() {
  Pair pair(0, GameProfile::TetrisZero, 0x12340002, 0);
  pair.start();
  uint8_t local = 77, remote = 66;
  assert(pair.host.zeroFrame() == 0);
  assert(pair.host.submitZeroInput(6, 5));
  Message window = zeroMessage(pair.host, inputWindow(6, 1, 9));
  Message invalid = window;
  ++invalid.session;
  assert(!deliver(pair.host, invalid, 3300));
  invalid = window;
  ++invalid.round;
  assert(!deliver(pair.host, invalid, 3301));
  assert(deliver(pair.host, window, 3302));
  for (uint32_t frame = 0; frame < ZeroInputDelay; ++frame) {
    assert(pair.host.zeroInputs(frame, local, remote));
    assert(local == 0 && remote == 0);
  }
  assert(pair.host.zeroInputs(6, local, remote) && local == 5 && remote == 9);
  assert(!pair.host.zeroInputs(6, local, remote));
  assert(!deliver(pair.host, window, 3303));
  Message progress;
  progress.type = MessageType::ZeroProgress;
  progress.session = pair.host.sessionId();
  progress.round = pair.host.round();
  progress.zeroProgress = 64;
  assert(!deliver(pair.host, progress, 3304));
  Pair flappy;
  flappy.start();
  window.session = flappy.host.sessionId();
  window.round = flappy.host.round();
  assert(!deliver(flappy.host, window, 3305));
  for (uint32_t now = 3310; now <= 6400; now += 10) pair.pump(now);
  assert(pair.host.phase() == Phase::Disconnected && pair.guest.phase() == Phase::Disconnected);
  assert(!pair.host.zeroInputs(7, local, remote));
  assert(!pair.host.submitZeroInput(7, 1));
}

struct ZeroPlayed {
  uint8_t p1, p2;
  bool operator==(const ZeroPlayed& other) const { return p1 == other.p1 && p2 == other.p2; }
};

static uint8_t zeroPad(bool host, uint32_t frame) { return uint8_t(frame * (host ? 31 : 53) + (host ? 7 : 19)); }

static void zeroDelayedLostAndReorderedInputsStayIdentical() {
  Pair pair(0, GameProfile::TetrisZero, 0x09010002, 0);
  pair.start();
  struct Transit { Packet packet; uint32_t due; bool toHost; };
  std::vector<Transit> transit;
  std::vector<ZeroPlayed> hostPlayed, guestPlayed;
  uint32_t hostFrame = 0, guestFrame = 0;
  bool hostSubmitted = false, guestSubmitted = false;
  unsigned serial = 0, dropped = 0, stalls = 0, inputPackets = 0;
  uint32_t completedAt = 0;
  constexpr uint32_t Frames = 1200;
  for (uint32_t elapsed = 0; elapsed < 22000; elapsed += 8) {
    const uint32_t now = 3300 + elapsed;
    for (size_t index = 0; index < transit.size();) {
      if (transit[index].due > now) { ++index; continue; }
      Transit item = transit[index];
      transit.erase(transit.begin() + index);
      Session& recipient = item.toHost ? pair.host : pair.guest;
      recipient.receive(item.packet.bytes, item.packet.length, now);
    }
    const uint32_t desired = uint32_t(uint64_t(elapsed) * 60 / 1000);
    for (bool host : {true, false}) {
      Session& session = host ? pair.host : pair.guest;
      uint32_t& frame = host ? hostFrame : guestFrame;
      bool& submitted = host ? hostSubmitted : guestSubmitted;
      if (frame < desired && frame < Frames) {
        if (!submitted) {
          assert(session.submitZeroInput(frame + ZeroInputDelay, zeroPad(host, frame)));
          submitted = true;
        }
        uint8_t local, remote;
        if (session.zeroInputs(frame, local, remote)) {
          (host ? hostPlayed : guestPlayed).push_back(host ? ZeroPlayed{local, remote} : ZeroPlayed{remote, local});
          ++frame;
          submitted = false;
        } else ++stalls;
      }
      Packet packet;
      if (session.nextPacket(now, packet)) {
        assert(packet.length < 20);
        ++serial;
        const bool input = packet.bytes[3] == uint8_t(MessageType::ZeroInputWindow);
        if (input) ++inputPackets;
        if (serial % 7 == 0 || (host && input && elapsed >= 1800 && elapsed < 2050)) {
          ++dropped;
          continue;
        }
        // Eight milliseconds of radio delay plus periodic extra delay reorders packets.
        transit.push_back(Transit{packet, now + 8 + (serial % 5 == 0 ? 32U : 0U), !host});
      }
    }
    pair.host.advance(now);
    pair.guest.advance(now);
    assert(pair.host.phase() == Phase::Playing && pair.guest.phase() == Phase::Playing);
    if (hostFrame == Frames && guestFrame == Frames && !completedAt) completedAt = elapsed;
  }
  assert(hostFrame == Frames && guestFrame == Frames);
  assert(completedAt <= 20500);
  assert(hostPlayed == guestPlayed && dropped > 0 && stalls > 0 && inputPackets > Frames / 2);
  for (uint32_t frame = 0; frame < Frames; ++frame) {
    assert(hostPlayed[frame].p1 == (frame < ZeroInputDelay ? 0 : zeroPad(true, frame - ZeroInputDelay)));
    assert(hostPlayed[frame].p2 == (frame < ZeroInputDelay ? 0 : zeroPad(false, frame - ZeroInputDelay)));
  }
  assert(pair.host.remoteTetris(25300).received && pair.guest.remoteTetris(25300).received);
}

static void zeroSlowRenderingCannotStarveInputPackets() {
  Pair pair(0, GameProfile::TetrisZero, 0x09010002, 0);
  pair.start();
  uint32_t hostFrame = 0, guestFrame = 0;
  bool hostSubmitted = false, guestSubmitted = false;
  std::vector<ZeroPlayed> hostPlayed, guestPlayed;
  for (uint32_t now = 3300; now < 23300 && (hostFrame < 120 || guestFrame < 120); now += 50) {
    pair.pump(now);
    for (bool host : {true, false}) {
      Session& session = host ? pair.host : pair.guest;
      uint32_t& frame = host ? hostFrame : guestFrame;
      bool& submitted = host ? hostSubmitted : guestSubmitted;
      assert(session.phase() == Phase::Playing);
      if (frame >= 120) continue;
      if (!submitted) {
        assert(session.submitZeroInput(frame + ZeroInputDelay, zeroPad(host, frame)));
        submitted = true;
      }
      uint8_t local, remote;
      if (session.zeroInputs(frame, local, remote)) {
        (host ? hostPlayed : guestPlayed).push_back(host ? ZeroPlayed{local, remote} : ZeroPlayed{remote, local});
        ++frame;
        submitted = false;
      }
    }
  }
  assert(hostFrame == 120 && guestFrame == 120 && hostPlayed == guestPlayed);
}

static void zeroRematchResetsInputsAndRetainsSettings() {
  Pair pair(0, GameProfile::TetrisZero, 0x09010002, 0);
  pair.start();
  const uint8_t oldRound = pair.host.round();
  Message oldInput = zeroMessage(pair.host, inputWindow(6, 1, 123));
  assert(pair.host.submitZeroInput(6, 99));
  assert(deliver(pair.host, oldInput, 3300));
  uint8_t local, remote;
  assert(pair.host.zeroInputs(0, local, remote));
  for (uint32_t now = 3310; now < 3700; now += 10) pair.pump(now, Life::GameOver, Life::GameOver);
  pair.host.requestReady(3700);
  pair.guest.requestReady(3700);
  for (uint32_t now = 3700; now < 4000; now += 10) pair.pump(now, Life::GameOver, Life::GameOver);
  assert(pair.host.phase() == Phase::Countdown && pair.guest.phase() == Phase::Countdown);
  assert(pair.host.round() == nextRound(oldRound) && pair.host.seed() == 0x09010002);
  assert(pair.guest.seed() == pair.host.seed());
  assert(pair.host.zeroFrame() == 0 && pair.guest.zeroFrame() == 0);
  assert(!deliver(pair.host, oldInput, 4000));
  assert(pair.host.submitZeroInput(6, 42));
  assert(!pair.host.zeroInputs(0, local, remote));
  pair.host.transportDisconnected();
  assert(pair.host.zeroFrame() == 0 && !pair.host.submitZeroInput(6, 1));
}

int main() {
  wireRoundTrips();
  malformedPackets();
  readinessAndCourseAgreement();
  invalidPeerCannotKeepSessionAlive();
  interpolationAndStaleness();
  restartRequiresBothPlayers();
  wrapAndPhysicalDisconnect();
  countdownNeedsAcknowledgement();
  delayedAndDroppedPacketsRecover();
  decoderFuzzStaysBounded();
  gameProfilesAreIsolated();
  tetrisStatsValidationAndFreshness();
  tetrisStatsSequenceWrapAndBadMetadata();
  tetrisRematchPreservesGameSettings();
  zeroWireRejectsInvalidCountsAndUnusedPayload();
  zeroBufferIsBoundedImmutableAndWraps();
  zeroSessionValidationAndFlowTimeout();
  zeroDelayedLostAndReorderedInputsStayIdentical();
  zeroSlowRenderingCannotStarveInputPackets();
  zeroRematchResetsInputsAndRetainsSettings();
  std::cout << "Co-op protocol checks passed.\n";
}
