#pragma once

#include "coop_protocol.h"
#include "ble_radio.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdio.h>
#include <string.h>

namespace flappy {
namespace coop {

// Call beginHost/beginJoin only after the user selects a co-op role.
// The application owns this single instance for the lifetime of the sketch.
class BleLink {
 public:
  BleLink() : serverCallbacks_(this), characteristicCallbacks_(this),
              clientCallbacks_(this), scanCallbacks_(this) {}
  BleLink(const BleLink&) = delete;
  BleLink& operator=(const BleLink&) = delete;

  bool setGameProfile(GameProfile profile) {
    if (!validGameProfile(profile)) return false;
    if (profile != gameProfile()) stop();
    if (initialized_ && fault_) return false;
    return session_.setGameProfile(profile);
  }
  GameProfile gameProfile() const { return session_.gameProfile(); }

  bool beginHost(uint32_t now, uint32_t session, uint32_t seed, uint16_t hue) {
    stop();
    if (initialized_) return false;
    if (!initialize()) return false;
    session_.beginHost(now, session, seed, hue);
    server_ = NimBLEDevice::createServer();
    if (!server_) return fail();
    server_->setCallbacks(&serverCallbacks_, false);
    server_->advertiseOnDisconnect(false);
    NimBLEService* service = server_->createService(serviceUuid(gameProfile()));
    if (!service) return fail();
    hostTx_ = service->createCharacteristic(hostTxUuid(gameProfile()), NIMBLE_PROPERTY::NOTIFY, MaxPacketBytes);
    NimBLECharacteristic* guestTx = service->createCharacteristic(
        guestTxUuid(gameProfile()), NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR, MaxPacketBytes);
    if (!hostTx_ || !guestTx) return fail();
    guestTx->setCallbacks(&characteristicCallbacks_);
    if (!service->start()) return fail();
    server_->start();
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->enableScanResponse(true);
    if (!advertising->addServiceUUID(serviceUuid(gameProfile())) || !advertising->setName(deviceName_) ||
        !advertising->start()) return fail();
    return true;
  }

  // The host chooses the shared hue. The optional hue argument keeps the UI's
  // role-selection call symmetric without creating different colors per screen.
  bool beginJoin(uint32_t now, uint16_t = 0) {
    stop();
    if (initialized_) return false;
    if (!initialize()) return false;
    session_.beginGuest(now);
    scanning_ = true;
    bestRssi_ = -128;
    foundHost_ = multipleHosts_ = false;
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&scanCallbacks_, false);
    scan->setActiveScan(true);
    scan->setInterval(45);
    scan->setWindow(30);
    // Callback-only scanning releases each advertisement after inspection.
    scan->setMaxResults(0);
    if (!scan->start(3500, false)) return fail();
    return true;
  }

  void stop() {
    if (initialized_ && session_.connected()) {
      Message bye;
      bye.type = MessageType::Bye;
      bye.session = session_.sessionId();
      bye.round = session_.round();
      Packet packet;
      if (encode(bye, packet)) send(packet);
    }
    const bool released = releaseRadio();
    session_.stop();
    if (!released) { fault_ = true; return; }
    fault_ = false;
    peerName_[0] = '\0';
    detail_[0] = '\0';
  }

  void setLocal(const PlayerSnapshot& snapshot) { session_.setLocal(snapshot); }
  void setLocalTetris(const TetrisStats& stats) { session_.setLocalTetris(stats); }
  void requestReady(uint32_t now) { session_.requestReady(now); }
  bool consumeStart(StartInfo& start) { return session_.consumeStart(start); }
  uint32_t worldTick(uint32_t now) const { return session_.worldTick(now); }
  RemoteView ghost(uint32_t now) const { return session_.ghost(now); }
  RemoteTetris remoteTetris(uint32_t now) const { return session_.remoteTetris(now); }
  bool submitZeroInput(uint32_t frame, uint8_t pad) { return session_.submitZeroInput(frame, pad); }
  bool zeroInputs(uint32_t frame, uint8_t& local, uint8_t& remote) { return session_.zeroInputs(frame, local, remote); }
  uint32_t zeroFrame() const { return session_.zeroFrame(); }
  const Session& session() const { return session_; }
  hockey::Channel& hockeyChannel() { return session_.hockeyChannel(); }
  const char* deviceName() const { return deviceName_; }
  const char* peerName() const { return peerName_; }
  bool multipleHostsFound() const { return multipleHosts_; }
  bool scanning() const { return scanning_; }
  bool fault() const { return fault_; }
  const char* detailText() const { return detail_; }

  const char* statusText() const {
    if (fault_) return "Bluetooth error";
    if (scanning_) return "Searching for host";
    if (connecting_) return "Connecting";
    switch (session_.phase()) {
      case Phase::Off: return "Bluetooth off";
      case Phase::Waiting: return session_.role() == Role::Host ? "Waiting for player" : "No host found";
      case Phase::Lobby: return session_.localReady() ? "Waiting for other player" : "Action: ready";
      case Phase::Countdown: return "Get ready";
      case Phase::Playing: return gameProfile() == GameProfile::Hockey ? "Hockey Queen linked" : gameProfile() == GameProfile::TetrisZero ? "Tetris Zero linked" :
          gameProfile() == GameProfile::Tetris ? "Tetris competition" : "Same course race";
      case Phase::Disconnected: return "Connection lost";
    }
    return "Bluetooth error";
  }

  // Call every main loop. A join connection and GATT discovery can briefly block
  // in the lobby. Gameplay packets and scan callbacks never run game logic.
  void update(uint32_t now, uint32_t authoritativeHostWorldTick = 0) {
    (void)authoritativeHostWorldTick;
    if (!initialized_ || fault_) return;
    Event event;
    for (uint8_t count = 0; count < QueueDepth && xQueueReceive(queue_, &event, 0) == pdTRUE; ++count) {
      now = millis();
      switch (event.kind) {
        case EventKind::Connected: session_.transportConnected(now); break;
        case EventKind::Disconnected: session_.transportDisconnected(); break;
        case EventKind::Data: session_.receive(event.bytes, event.length, now); break;
        case EventKind::ScanResult: {
          const NimBLEAddress address(event.address);
          if (foundHost_ && address != bestAddress_) multipleHosts_ = true;
          if (!foundHost_ || event.rssi > bestRssi_) {
            foundHost_ = true;
            bestRssi_ = event.rssi;
            bestAddress_ = address;
            memcpy(peerName_, event.name, sizeof(peerName_));
          }
          break;
        }
        case EventKind::ScanDone:
          scanning_ = false;
          if (foundHost_) connectToHost();
          else session_.transportDisconnected();
          break;
      }
    }
    // A full receive queue can drop the scan-end event. The scan state still
    // guarantees a bounded search and a usable retry path.
    if (scanning_ && !NimBLEDevice::getScan()->isScanning()) {
      scanning_ = false;
      if (foundHost_) connectToHost();
      else session_.transportDisconnected();
    }
    // Discovery may take time, so use the current clock after event handling.
    now = millis();
    session_.advance(now);
    if (session_.phase() == Phase::Disconnected) {
      releaseRadio();
      return;
    }
    Packet packet;
    if (session_.nextPacket(now, packet)) send(packet);
  }

 private:
  enum class EventKind : uint8_t { Connected, Disconnected, Data, ScanResult, ScanDone };
  struct Event {
    EventKind kind = EventKind::Data;
    uint8_t length = 0;
    uint8_t bytes[MaxPacketBytes] = {};
    ble_addr_t address = {};
    int8_t rssi = -128;
    char name[32] = {};
  };
  static constexpr uint8_t QueueDepth = 12;

  class ServerCallbacks : public NimBLEServerCallbacks {
   public:
    explicit ServerCallbacks(BleLink* owner) : owner_(owner) {}
    void onConnect(NimBLEServer* server, NimBLEConnInfo& info) override {
      if (owner_->connectionHandle_ != BLE_HS_CONN_HANDLE_NONE) {
        server->disconnect(info.getConnHandle());
        return;
      }
      owner_->connectionHandle_ = info.getConnHandle();
      server->updateConnParams(info.getConnHandle(), 12, 24, 0, 200);
      owner_->enqueue(EventKind::Connected);
    }
    void onDisconnect(NimBLEServer*, NimBLEConnInfo& info, int) override {
      if (info.getConnHandle() != owner_->connectionHandle_) return;
      owner_->connectionHandle_ = BLE_HS_CONN_HANDLE_NONE;
      owner_->enqueue(EventKind::Disconnected);
    }
   private:
    BleLink* owner_;
  };

  class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
   public:
    explicit CharacteristicCallbacks(BleLink* owner) : owner_(owner) {}
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) override {
      if (info.getConnHandle() != owner_->connectionHandle_) return;
      const NimBLEAttValue value = characteristic->getValue();
      owner_->enqueueData(value.data(), value.size());
    }
   private:
    BleLink* owner_;
  };

  class ClientCallbacks : public NimBLEClientCallbacks {
   public:
    explicit ClientCallbacks(BleLink* owner) : owner_(owner) {}
    void onConnect(NimBLEClient*) override { owner_->enqueue(EventKind::Connected); }
    void onDisconnect(NimBLEClient*, int) override { owner_->enqueue(EventKind::Disconnected); }
    void onConnectFail(NimBLEClient*, int) override { owner_->enqueue(EventKind::Disconnected); }
   private:
    BleLink* owner_;
  };

  class ScanCallbacks : public NimBLEScanCallbacks {
   public:
    explicit ScanCallbacks(BleLink* owner) : owner_(owner) {}
    void onResult(const NimBLEAdvertisedDevice* device) override {
      if (!device->isAdvertisingService(NimBLEUUID(serviceUuid(owner_->gameProfile())))) return;
      const NimBLEAddress address = device->getAddress();
      Event event;
      event.kind = EventKind::ScanResult;
      event.address = *address.getBase();
      event.rssi = int8_t(device->getRSSI());
      const std::string name = device->getName();
      snprintf(event.name, sizeof(event.name), "%s", name.empty() ?
          (owner_->gameProfile() == GameProfile::TetrisZero ? "M5Zero host" :
           owner_->gameProfile() == GameProfile::Tetris ? "M5Tetris host" : "M5Flap host") : name.c_str());
      if (owner_->queue_) xQueueSend(owner_->queue_, &event, 0);
    }
    void onScanEnd(const NimBLEScanResults&, int) override { owner_->enqueue(EventKind::ScanDone); }
   private:
    BleLink* owner_;
  };

  Session session_;
  QueueHandle_t queue_ = nullptr;
  NimBLEServer* server_ = nullptr;
  NimBLEClient* client_ = nullptr;
  NimBLECharacteristic* hostTx_ = nullptr;
  NimBLERemoteCharacteristic* guestTx_ = nullptr;
  ServerCallbacks serverCallbacks_;
  CharacteristicCallbacks characteristicCallbacks_;
  ClientCallbacks clientCallbacks_;
  ScanCallbacks scanCallbacks_;
  uint16_t connectionHandle_ = BLE_HS_CONN_HANDLE_NONE;
  bool initialized_ = false;
  bool scanning_ = false;
  bool connecting_ = false;
  bool fault_ = false;
  bool foundHost_ = false;
  bool multipleHosts_ = false;
  int bestRssi_ = -128;
  NimBLEAddress bestAddress_;
  char deviceName_[20] = {};
  char peerName_[32] = {};
  char detail_[112] = {};

  static BleLink*& activeInstance() {
    static BleLink* active = nullptr;
    return active;
  }

  bool initialize() {
    if (NimBLEDevice::isInitialized()) {
      snprintf(detail_, sizeof(detail_), "Bluetooth is busy");
      fault_ = true; return false;
    }
    queue_ = xQueueCreate(QueueDepth, sizeof(Event));
    if (!queue_) {
      snprintf(detail_, sizeof(detail_), "Link queue unavailable");
      fault_ = true; return false;
    }
    snprintf(deviceName_, sizeof(deviceName_), gameProfile() == GameProfile::Hockey ? "M5Hockey-%06lX" : gameProfile() == GameProfile::TetrisZero ? "M5Zero-%06lX" :
             gameProfile() == GameProfile::Tetris ? "M5Tetris-%06lX" : "M5Flap-%06lX",
             static_cast<unsigned long>((ESP.getEfuseMac() >> 24) & 0xffffff));
    radio::EspBackend backend;
    radio::Diagnostic result;
    if (!radio::start(backend, deviceName_, result)) {
      initialized_ = result.failure == radio::Failure::Init &&
                     (backend.hostInitialized() || backend.state() != 0);
      radio::describe(result, detail_, sizeof(detail_));
      vQueueDelete(queue_);
      queue_ = nullptr;
      fault_ = true;
      return false;
    }
    initialized_ = true;
    activeInstance() = this;
    return true;
  }

  bool fail() {
    releaseRadio();
    session_.transportDisconnected();
    fault_ = true;
    return false;
  }

  bool releaseRadio() {
    if (initialized_) {
      radio::EspBackend backend;
      radio::Diagnostic result;
      if (!radio::stop(backend, result)) {
        radio::describe(result, detail_, sizeof(detail_));
        fault_ = true;
        return false;
      }
    }
    if (activeInstance() == this) activeInstance() = nullptr;
    initialized_ = false;
    scanning_ = connecting_ = false;
    server_ = nullptr;
    client_ = nullptr;
    hostTx_ = nullptr;
    guestTx_ = nullptr;
    connectionHandle_ = BLE_HS_CONN_HANDLE_NONE;
    if (queue_) vQueueDelete(queue_);
    queue_ = nullptr;
    return true;
  }

  void enqueue(EventKind kind) {
    if (!queue_) return;
    Event event;
    event.kind = kind;
    xQueueSend(queue_, &event, 0);
  }

  void enqueueData(const uint8_t* bytes, size_t length) {
    if (!queue_ || !bytes || length > MaxPacketBytes || length < 8) return;
    Event event;
    event.kind = EventKind::Data;
    event.length = uint8_t(length);
    memcpy(event.bytes, bytes, length);
    xQueueSend(queue_, &event, 0);
  }

  void connectToHost() {
    connecting_ = true;
    client_ = NimBLEDevice::createClient(bestAddress_);
    if (!client_) {
      connecting_ = false;
      session_.transportDisconnected();
      return;
    }
    client_->setClientCallbacks(&clientCallbacks_, false);
    client_->setConnectTimeout(2500);
    client_->setConnectionParams(12, 24, 0, 200);
    bool good = client_->connect(bestAddress_);
    if (good) {
      NimBLERemoteService* service = client_->getService(serviceUuid(gameProfile()));
      NimBLERemoteCharacteristic* hostTx = service ? service->getCharacteristic(hostTxUuid(gameProfile())) : nullptr;
      guestTx_ = service ? service->getCharacteristic(guestTxUuid(gameProfile())) : nullptr;
      good = hostTx && hostTx->canNotify() && guestTx_ && guestTx_->canWriteNoResponse();
      if (good) {
        good = hostTx->subscribe(true, [](NimBLERemoteCharacteristic*, uint8_t* data, size_t length, bool) {
          BleLink* active = activeInstance();
          if (active) active->enqueueData(data, length);
        });
      }
    }
    connecting_ = false;
    if (!good) {
      if (client_->isConnected()) client_->disconnect();
      // Process this after a queued connection callback, if one was emitted.
      enqueue(EventKind::Disconnected);
    }
  }

  bool send(const Packet& packet) {
    if (session_.role() == Role::Host && hostTx_ && connectionHandle_ != BLE_HS_CONN_HANDLE_NONE) {
      return hostTx_->notify(packet.bytes, packet.length, connectionHandle_);
    }
    if (session_.role() == Role::Guest && guestTx_ && client_ && client_->isConnected()) {
      return guestTx_->writeValue(packet.bytes, packet.length, false);
    }
    return false;
  }
};

}  // namespace coop
}  // namespace flappy
#endif  // ARDUINO
