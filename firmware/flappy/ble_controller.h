#pragma once

#include "controller_input.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdio.h>
#include <string.h>

namespace flappy {
namespace controller {

// The application stops co-op before calling beginPairing and stops this host
// before starting co-op. Call update from the same task as the application UI.
class BleController {
 public:
  BleController() : scanCallbacks_(this), clientCallbacks_(this) {}
  BleController(const BleController&) = delete;
  BleController& operator=(const BleController&) = delete;

  bool beginPairing(uint32_t now) {
    stop();
    if (NimBLEDevice::isInitialized()) return fail("Bluetooth is busy");
    queue_ = xQueueCreateStatic(QueueDepth, sizeof(Event), queueBytes_, &queueStorage_);
    if (!queue_) return fail("Controller queue unavailable");
    lost_.store(false);
    overflow_.store(false);
    authentication_.store(0);
    if (!NimBLEDevice::init("M5 Game Controller")) return fail("Bluetooth could not start");
    initialized_ = true;
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    found_ = false;
    bestScore_ = -1000;
    name_[0] = '\0';
    state_ = State::Scanning;
    started_ = now;
    status_ = "Pair controller: searching";
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&scanCallbacks_, false);
    scan->setActiveScan(true);
    scan->setInterval(50);
    scan->setWindow(35);
    scan->setMaxResults(0);
    if (!scan->start(5000, false)) return fail("Controller scan failed");
    return true;
  }

  void stop() {
    releaseRadio();
    state_ = State::Off;
    status_ = "Controller off";
  }

  // Bond deletion is restricted to the last controller selected by this host.
  void forget() {
    stop();
    if (!havePeer_) return;
    if (NimBLEDevice::isInitialized()) { status_ = "Bluetooth is busy"; return; }
    if (!NimBLEDevice::init("M5 Game Controller")) { status_ = "Bluetooth could not start"; return; }
    const bool deleted = NimBLEDevice::deleteBond(peerAddress_);
    NimBLEDevice::deinit(true);
    if (deleted) havePeer_ = false;
    status_ = deleted ? "Controller forgotten" : "Controller pairing not found";
  }

  void update(uint32_t now) {
    if (!initialized_) return;
    const uint8_t auth = authentication_.load();
    if (auth == AuthPin) { fail("Pairing needs PIN confirmation"); return; }
    if (lost_.load()) { fail("Controller disconnected"); return; }
    if (overflow_.exchange(false)) {
      neutral();
      // Discard the batch as a dropped release report could leave a button held.
      xQueueReset(queue_);
      status_ = "Controller input overflow";
    }
    Event event;
    for (uint8_t i = 0; i < QueueDepth && xQueueReceive(queue_, &event, 0) == pdTRUE; ++i) {
      if (event.kind == EventKind::Candidate && state_ == State::Scanning) {
        if (!found_ || event.score > bestScore_) {
          bestAddress_ = NimBLEAddress(event.address, event.addressType);
          memcpy(name_, event.name, sizeof(name_));
          bestScore_ = event.score;
          found_ = true;
        }
      } else if (event.kind == EventKind::Report && state_ == State::Ready && event.slot < slotCount_) {
        Slot& slot = slots_[event.slot];
        uint16_t buttons = 0;
        const bool good = parser_.decode(slot.reportId, event.bytes, event.length, buttons);
        slot.buttons = good ? buttons : 0;
        uint16_t combined = 0;
        for (uint8_t n = 0; n < slotCount_; ++n) combined |= slots_[n].buttons;
        setSnapshot(combined, true);
        status_ = good ? "Controller ready" : "Invalid controller report";
      }
    }
    if (state_ == State::Scanning && !NimBLEDevice::getScan()->isScanning()) {
      if (!found_) { fail("No BLE controller found"); return; }
      connect();
      now = millis();
    }
    if (state_ == State::Securing) {
      if (authentication_.load() == AuthEncrypted) {
        if (!discover()) return;
      } else if (authentication_.load() == AuthFailed) {
        fail("Controller pairing failed");
      } else if (uint32_t(now - started_) > 20000) {
        fail("Controller pairing timed out");
      }
    }
  }

  Snapshot snapshot() const {
    portENTER_CRITICAL(&snapshotMux_);
    const Snapshot result = snapshot_;
    portEXIT_CRITICAL(&snapshotMux_);
    return result;
  }
  Snapshot consumeSnapshot() {
    portENTER_CRITICAL(&snapshotMux_);
    const Snapshot result = takeSnapshot(snapshot_);
    portEXIT_CRITICAL(&snapshotMux_);
    return result;
  }
  bool connected() const { return snapshot().connected; }
  const char* statusText() const { return status_; }
  const char* name() const { return name_; }

 private:
  enum class State : uint8_t { Off, Scanning, Connecting, Securing, Ready, Failed };
  enum class EventKind : uint8_t { Candidate, Report };
  static constexpr uint8_t AuthEncrypted = 1, AuthFailed = 2, AuthPin = 3;
  static constexpr uint8_t QueueDepth = 12;
  static constexpr uint8_t MaxSlots = 8;
  struct Event {
    EventKind kind = EventKind::Report;
    uint8_t slot = 0;
    uint16_t length = 0;
    uint8_t bytes[HidReportParser::MaxReportBytes] = {};
    uint8_t address[6] = {};
    uint8_t addressType = 0;
    int16_t score = -1000;
    char name[32] = {};
  };
  struct Slot {
    NimBLERemoteCharacteristic* characteristic = nullptr;
    uint16_t buttons = 0;
    uint8_t reportId = 0;
    bool boot = false;
  };
  class ScanCallbacks : public NimBLEScanCallbacks {
   public:
    explicit ScanCallbacks(BleController* owner) : owner_(owner) {}
    void onResult(const NimBLEAdvertisedDevice* device) override {
      const std::string name = device->getName();
      const bool known = name == "IINE-keyboard" || name == "Xbox Wireless Controller" || name == "Wireless Controller";
      if (!known && !device->isAdvertisingService(NimBLEUUID(uint16_t(0x1812)))) return;
      Event event;
      event.kind = EventKind::Candidate;
      const NimBLEAddress address = device->getAddress();
      memcpy(event.address, address.getVal(), sizeof(event.address));
      event.addressType = address.getType();
      // Prefer the manufacturer's keyboard profile when several modes are seen.
      event.score = int16_t(device->getRSSI() + (known ? 200 : 0) + (name == "IINE-keyboard" ? 100 : 0));
      snprintf(event.name, sizeof(event.name), "%s", name.empty() ? "BLE HID controller" : name.c_str());
      if (owner_->queue_) xQueueSend(owner_->queue_, &event, 0);
    }
   private:
    BleController* owner_;
  };
  class ClientCallbacks : public NimBLEClientCallbacks {
   public:
    explicit ClientCallbacks(BleController* owner) : owner_(owner) {}
    void onDisconnect(NimBLEClient*, int) override { owner_->lost_.store(true); }
    void onConnectFail(NimBLEClient*, int) override { owner_->lost_.store(true); }
    void onAuthenticationComplete(NimBLEConnInfo& info) override {
      // Preserve the actionable PIN message if a later failure callback follows.
      if (owner_->authentication_.load() != AuthPin)
        owner_->authentication_.store(info.isEncrypted() ? AuthEncrypted : AuthFailed);
    }
    void onPassKeyEntry(NimBLEConnInfo& info) override {
      owner_->authentication_.store(AuthPin);
      NimBLEClient* client = NimBLEDevice::getClientByHandle(info.getConnHandle());
      if (client) client->disconnect();
    }
    void onConfirmPasskey(NimBLEConnInfo& info, uint32_t) override {
      owner_->authentication_.store(AuthPin);
      NimBLEDevice::injectConfirmPasskey(info, false);
    }
   private:
    BleController* owner_;
  };

  HidReportParser parser_;
  Slot slots_[MaxSlots];
  uint8_t slotCount_ = 0;
  StaticQueue_t queueStorage_;
  alignas(4) uint8_t queueBytes_[QueueDepth * sizeof(Event)];
  QueueHandle_t queue_ = nullptr;
  ScanCallbacks scanCallbacks_;
  ClientCallbacks clientCallbacks_;
  NimBLEClient* client_ = nullptr;
  NimBLEAddress bestAddress_;
  NimBLEAddress peerAddress_;
  bool havePeer_ = false, initialized_ = false, found_ = false;
  std::atomic<bool> lost_{false};
  std::atomic<bool> overflow_{false};
  std::atomic<uint8_t> authentication_{0};
  mutable portMUX_TYPE snapshotMux_ = portMUX_INITIALIZER_UNLOCKED;
  Snapshot snapshot_;
  State state_ = State::Off;
  const char* status_ = "Controller off";
  char name_[32] = {};
  int16_t bestScore_ = -1000;
  uint32_t started_ = 0;

  void setSnapshot(uint16_t buttons, bool connected, bool clearPressed = false) {
    portENTER_CRITICAL(&snapshotMux_);
    updateSnapshot(snapshot_, buttons, connected, clearPressed);
    portEXIT_CRITICAL(&snapshotMux_);
  }
  void neutral() {
    for (uint8_t i = 0; i < MaxSlots; ++i) slots_[i].buttons = 0;
    setSnapshot(0, state_ == State::Ready, true);
  }
  void releaseRadio() {
    if (initialized_) NimBLEDevice::deinit(true);
    initialized_ = false;
    client_ = nullptr;
    if (queue_) vQueueDelete(queue_);
    queue_ = nullptr;
    slotCount_ = 0;
    parser_.clear();
    setSnapshot(0, false);
  }
  bool fail(const char* message) {
    releaseRadio();
    state_ = State::Failed;
    status_ = message;
    return false;
  }
  void connect() {
    state_ = State::Connecting;
    status_ = "Connecting controller";
    client_ = NimBLEDevice::createClient(bestAddress_);
    if (!client_) { fail("Controller client unavailable"); return; }
    client_->setClientCallbacks(&clientCallbacks_, false);
    client_->setConnectTimeout(4000);
    client_->setConnectionParams(12, 24, 0, 200);
    if (!client_->connect(bestAddress_)) { fail("Controller connection failed"); return; }
    peerAddress_ = bestAddress_;
    havePeer_ = true;
    state_ = State::Securing;
    status_ = "Pairing controller securely";
    started_ = millis();
    if (client_->getConnInfo().isEncrypted()) authentication_.store(AuthEncrypted);
    else if (!client_->secureConnection(true)) fail("Controller pairing failed");
  }
  bool addSlot(NimBLERemoteCharacteristic* characteristic, uint8_t id, bool boot) {
    if (slotCount_ == MaxSlots) return false;
    const uint8_t index = slotCount_++;
    Slot& slot = slots_[index];
    slot.characteristic = characteristic;
    slot.reportId = id;
    slot.buttons = 0;
    slot.boot = boot;
    if (!characteristic->subscribe(characteristic->canNotify(),
        [this, index](NimBLERemoteCharacteristic*, uint8_t* bytes, size_t length, bool) {
          if (!bytes || !length || length > HidReportParser::MaxReportBytes) {
            overflow_.store(true);
            return;
          }
          Event event;
          event.kind = EventKind::Report;
          event.slot = index;
          event.length = uint16_t(length);
          memcpy(event.bytes, bytes, length);
          if (!queue_ || xQueueSend(queue_, &event, 0) != pdTRUE) overflow_.store(true);
        }, true)) {
      --slotCount_;
      return false;
    }
    return true;
  }
  bool discover() {
    status_ = "Reading controller inputs";
    // Pairing may resolve an advertised private address to its bonded identity.
    peerAddress_ = client_->getConnInfo().getIdAddress();
    NimBLERemoteService* service = client_->getService(NimBLEUUID(uint16_t(0x1812)));
    if (!service) return fail("Controller has no BLE HID service");
    NimBLERemoteCharacteristic* reportMap = service->getCharacteristic(NimBLEUUID(uint16_t(0x2a4b)));
    bool mapped = false;
    if (reportMap && reportMap->canRead()) {
      const NimBLEAttValue map = reportMap->readValue();
      mapped = parser_.configure(map.data(), map.size());
    }
    NimBLERemoteCharacteristic* protocol = service->getCharacteristic(NimBLEUUID(uint16_t(0x2a4e)));
    if (mapped) {
      if (protocol && (protocol->canWrite() || protocol->canWriteNoResponse())) {
        const uint8_t mode = 1;
        if (!protocol->writeValue(&mode, 1, protocol->canWrite())) return fail("HID report mode failed");
      }
      for (NimBLERemoteCharacteristic* characteristic : service->getCharacteristics()) {
        if (characteristic->getUUID() != NimBLEUUID(uint16_t(0x2a4d)) ||
            (!characteristic->canNotify() && !characteristic->canIndicate())) continue;
        NimBLERemoteDescriptor* reference = characteristic->getDescriptor(NimBLEUUID(uint16_t(0x2908)));
        if (!reference) continue;
        const NimBLEAttValue value = reference->readValue();
        if (value.size() != 2 || value.data()[1] != 1 || !parser_.supportsReport(value.data()[0])) continue;
        if (slotCount_ == MaxSlots) return fail("Too many controller input reports");
        if (!addSlot(characteristic, value.data()[0], false)) return fail("Controller subscription failed");
      }
    }
    if (!slotCount_) {
      NimBLERemoteCharacteristic* boot = service->getCharacteristic(NimBLEUUID(uint16_t(0x2a22)));
      if (!boot || !protocol || (!boot->canNotify() && !boot->canIndicate()) ||
          (!protocol->canWrite() && !protocol->canWriteNoResponse()))
        return fail(mapped ? "No HID input notifications" : "HID map unsupported: try H mode");
      const uint8_t mode = 0;
      if (!protocol->writeValue(&mode, 1, protocol->canWrite()) || !parser_.configureBootKeyboard() || !addSlot(boot, 0, true))
        return fail("Keyboard boot mode failed");
    }
    state_ = State::Ready;
    status_ = "Controller ready";
    setSnapshot(0, true, true);
    return true;
  }
};

}  // namespace controller
}  // namespace flappy
#endif  // ARDUINO
