#include "../firmware/flappy/coop_protocol.h"

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

  explicit Pair(uint32_t now = 0) {
    host.beginHost(now, 0x12345678, 123456, 73);
    guest.beginGuest(now);
    host.transportConnected(now);
    guest.transportConnected(now);
  }

  void pump(uint32_t now, Life hostLife = Life::Alive, Life guestLife = Life::Alive) {
    PlayerSnapshot local;
    local.tick = host.worldTick(now);
    local.life = hostLife;
    host.setLocal(local);
    local.tick = guest.worldTick(now);
    local.life = guestLife;
    guest.setLocal(local);
    Packet packet;
    if (host.nextPacket(now, packet)) guest.receive(packet.bytes, packet.length, now);
    if (guest.nextPacket(now, packet)) host.receive(packet.bytes, packet.length, now);
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
  for (uint8_t rawType = uint8_t(MessageType::Hello); rawType <= uint8_t(MessageType::Bye); ++rawType) {
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
      bytes[3] = uint8_t(1 + iteration % 7);
    }
    Message message;
    if (decode(bytes, length, message)) {
      Packet packet;
      assert(encode(message, packet));
      assert(packet.length == length);
    }
  }
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
  std::cout << "Co-op protocol checks passed.\n";
}
