#pragma once

#include "controller_input.h"
#include "ble_radio.h"

#ifdef ARDUINO
#include <Arduino.h>
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
  enum class Phase : uint8_t { Off, Scanning, Connecting, Securing, Discovering, Ready, Failed };
  static constexpr uint32_t ScanDurationMs = 30000;
  BleController() : scanCallbacks_(this), clientCallbacks_(this) {}
  BleController(const BleController&) = delete;
  BleController& operator=(const BleController&) = delete;

  bool beginPairing(uint32_t now) {
    return beginScan(now, false);
  }

  // Light sleep retains peerAddress_ in RAM. Resume accepts that exact peer.
  bool resumePeer(uint32_t now) {
    if (!havePeer_) {
      stop();
      if (initialized_) return false;
      status_ = "No controller to resume";
      snprintf(detail_, sizeof(detail_), "Select Pair controller to connect");
      return false;
    }
    return beginScan(now, true);
  }

  void stop() {
    if (!releaseRadio()) {
      state_ = State::Failed;
      status_ = radio::status(radioFailure_);
      return;
    }
    state_ = State::Off;
    retainedOnly_ = false;
    status_ = "Controller off";
    detail_[0] = '\0';
    radioFailure_ = radio::Diagnostic();
  }

  // Bond deletion is restricted to the last controller selected by this host.
  void forget() {
    stop();
    if (initialized_) return;
    if (!havePeer_) {
      status_ = "No controller pairing selected";
      snprintf(detail_, sizeof(detail_), "Pair once to select the controller to forget");
      return;
    }
    if (!initializeRadio()) { fail(radio::status(radioFailure_)); return; }
    const bool deleted = NimBLEDevice::deleteBond(peerAddress_);
    if (deleted) {
      havePeer_ = false;
      peerName_[0] = '\0';
    }
    if (!releaseRadio()) {
      state_ = State::Failed;
      status_ = radio::status(radioFailure_);
      return;
    }
    status_ = deleted ? "Controller forgotten" : "Controller pairing not found";
    snprintf(detail_, sizeof(detail_), "%s", deleted ? "Select Pair controller to reconnect" : "Pair a controller before forgetting it");
  }

  void update(uint32_t now) {
    if (!initialized_ || state_ == State::Failed || state_ == State::Off) return;
    const uint8_t auth = authentication_.load();
    if (auth == AuthPin) {
      snprintf(detail_, sizeof(detail_), "This profile needs a PIN; try H keyboard");
      fail("Pairing needs PIN confirmation"); return;
    }
    if (lost_.load()) {
      lastError_ = callbackError_.load();
      snprintf(detail_, sizeof(detail_), "Link ended, reason %d. Pair again to retry", lastError_);
      fail(state_ == State::Connecting ? "Controller connection failed" : "Controller disconnected"); return;
    }
    if (overflow_.exchange(false)) {
      neutral();
      // Discard the batch as a dropped release report could leave a button held.
      xQueueReset(queue_);
      status_ = "Controller input overflow";
    }
    Event event;
    for (uint8_t i = 0; i < QueueDepth && xQueueReceive(queue_, &event, 0) == pdTRUE; ++i) {
      if (event.kind == EventKind::Candidate && state_ == State::Scanning) {
        if (!acceptsAddress(event.address)) continue;
        if (!found_ || event.score > bestScore_) {
          bestAddress_ = NimBLEAddress(event.address);
          memcpy(name_, event.name, sizeof(name_));
          bestScore_ = event.score;
          bestPreferred_ = event.preferred;
          if (event.preferred && !preferredAt_) preferredAt_ = now ? now : 1;
          found_ = true;
        }
      } else if (event.kind == EventKind::Report && state_ == State::Ready && event.slot < slotCount_) {
        Slot& slot = slots_[event.slot];
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
        if (traceIine_ && reportTrace_.accept(event.slot, event.bytes, event.length)) {
          Serial.printf("IINE RAW id=%u slot=%u len=%u data=", unsigned(slot.reportId), unsigned(event.slot), unsigned(event.length));
          const size_t visible = event.length < 32 ? event.length : 32;
          for (size_t index = 0; index < visible; ++index) Serial.printf("%02x", unsigned(event.bytes[index]));
          Serial.printf("%s\n", event.length > visible ? "..." : "");
        }
#endif
        if (slot.diagnosticOnly) continue;
        uint16_t buttons = 0;
        const bool good = parser_.decode(slot.reportId, event.bytes, event.length, buttons);
        slot.buttons = good ? buttons : 0;
        uint16_t combined = 0;
        for (uint8_t n = 0; n < slotCount_; ++n) combined |= slots_[n].buttons;
        setSnapshot(combined, true);
        status_ = good ? "Controller ready" : "Invalid controller report";
        if (!good) snprintf(detail_, sizeof(detail_), "Report %u: got %u bytes, need %u", unsigned(slot.reportId),
                            unsigned(event.length), unsigned(parser_.expectedLength(slot.reportId)));
      }
    }
    if (state_ == State::Scanning) {
      NimBLEScan* scan = NimBLEDevice::getScan();
      // Leave a short window for the keyboard profile's scan response to arrive.
      const bool preferredReady = found_ && bestPreferred_ && uint32_t(now - preferredAt_) >= 400;
      if (preferredReady && scan->isScanning() && !scan->stop()) {
        fail("Controller scan could not stop"); return;
      }
      if (!scan->isScanning()) {
        if (!found_) {
          lastError_ = callbackError_.load();
          if (retainedOnly_) {
            snprintf(detail_, sizeof(detail_), "Wake the previous controller, or select Pair controller");
            fail("Previous controller not found"); return;
          }
          snprintf(detail_, sizeof(detail_), "%lu BLE seen; use H keyboard and hold Pair 3s",
                   static_cast<unsigned long>(seen_.load()));
          fail("No BLE HID controller found"); return;
        }
        connect();
        return;
      }
      snprintf(scanStatus_, sizeof(scanStatus_), "%s: %lu seen", retainedOnly_ ? "Resuming controller" : "Searching BLE",
               static_cast<unsigned long>(seen_.load()));
      status_ = scanStatus_;
      if (retainedOnly_) snprintf(detail_, sizeof(detail_), "Wake the previous controller to reconnect");
      else snprintf(detail_, sizeof(detail_), "%lu matches; H keyboard, hold Pair 3s", static_cast<unsigned long>(matches_.load()));
    }
    if (state_ == State::Connecting) {
      if (linked_.load()) {
        const NimBLEAddress identity = client_->getConnInfo().getIdAddress();
        if (!acceptsAddress(*identity.getBase())) {
          fail("Previous controller identity changed"); return;
        }
        peerAddress_ = identity;
        havePeer_ = true;
        memcpy(peerName_, name_, sizeof(peerName_));
        state_ = State::Securing;
        status_ = "Pairing controller securely";
        snprintf(detail_, sizeof(detail_), "%s", name_);
        started_ = now;
        if (client_->getConnInfo().isEncrypted()) authentication_.store(AuthEncrypted);
        else if (!client_->secureConnection(true)) {
          lastError_ = client_->getLastError();
          fail("Controller pairing failed");
        }
        return;
      }
      if (uint32_t(now - started_) > 10000) {
        snprintf(detail_, sizeof(detail_), "Connection timed out; keep controller in pairing mode");
        fail("Controller connection timed out");
      }
    }
    if (state_ == State::Securing) {
      if (authentication_.load() == AuthEncrypted) {
        state_ = State::Discovering;
        status_ = "Reading controller inputs";
        snprintf(detail_, sizeof(detail_), "%s", name_);
        return;
      } else if (authentication_.load() == AuthFailed) {
        snprintf(detail_, sizeof(detail_), "Encryption failed; forget pairing and retry");
        fail("Controller pairing failed");
      } else if (uint32_t(now - started_) > 20000) {
        fail("Controller pairing timed out");
      }
    }
    if (state_ == State::Discovering) discover();
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
  bool canResumePeer() const { return havePeer_ && state_ != State::Off; }
  bool pairing() const { return state_ == State::Scanning || state_ == State::Connecting ||
                                state_ == State::Securing || state_ == State::Discovering; }
  bool scanning() const { return state_ == State::Scanning; }
  Phase phase() const { return state_; }
  const char* statusText() const { return status_; }
  const char* detailText() const { return detail_; }
  const char* name() const { return name_; }
  uint32_t devicesSeen() const { return seen_.load(); }
  uint32_t candidatesSeen() const { return matches_.load(); }
  uint32_t reportsReceived() const { return notifications_.load(); }
  int lastError() const { return lastError_; }

 private:
  using State = Phase;
  enum class EventKind : uint8_t { Candidate, Report };
  static constexpr uint8_t AuthEncrypted = 1, AuthFailed = 2, AuthPin = 3;
  static constexpr uint8_t QueueDepth = 12;
  static constexpr uint8_t MaxSlots = 8;
  struct Event {
    EventKind kind = EventKind::Report;
    uint8_t slot = 0;
    uint16_t length = 0;
    uint8_t bytes[HidReportParser::MaxReportBytes] = {};
    ble_addr_t address = {};
    int16_t score = -1000;
    bool preferred = false;
    char name[32] = {};
  };
  struct Slot {
    NimBLERemoteCharacteristic* characteristic = nullptr;
    uint16_t buttons = 0;
    uint8_t reportId = 0;
    bool boot = false;
    bool diagnosticOnly = false;
  };
  class ScanCallbacks : public NimBLEScanCallbacks {
   public:
    explicit ScanCallbacks(BleController* owner) : owner_(owner) {}
    void onDiscovered(const NimBLEAdvertisedDevice*) override { owner_->seen_.fetch_add(1); }
    void onResult(const NimBLEAdvertisedDevice* device) override {
      const NimBLEAddress address = device->getAddress();
      if (!owner_->acceptsAddress(*address.getBase())) return;
      const std::string name = device->getName();
      const AdvertisementMatch match = matchControllerAdvertisement(name.c_str(),
          device->isAdvertisingService(NimBLEUUID(uint16_t(0x1812))), device->isConnectable(), device->getRSSI());
      if (owner_->retainedOnly_ ? !device->isConnectable() : !match.eligible) return;
      owner_->matches_.fetch_add(1);
      Event event;
      event.kind = EventKind::Candidate;
      event.address = *address.getBase();
      // Prefer the manufacturer's keyboard profile when several modes are seen.
      event.score = owner_->retainedOnly_ ? 0 : match.score;
      event.preferred = owner_->retainedOnly_ || match.preferred;
      const char* advertisedName = name.empty() ?
          (owner_->retainedOnly_ && owner_->peerName_[0] ? owner_->peerName_ : "BLE HID controller") : name.c_str();
      snprintf(event.name, sizeof(event.name), "%s", advertisedName);
      if (owner_->queue_) xQueueSend(owner_->queue_, &event, 0);
    }
    void onScanEnd(const NimBLEScanResults&, int reason) override { owner_->callbackError_.store(reason); }
   private:
    BleController* owner_;
  };
  class ClientCallbacks : public NimBLEClientCallbacks {
   public:
    explicit ClientCallbacks(BleController* owner) : owner_(owner) {}
    void onConnect(NimBLEClient*) override { owner_->linked_.store(true); }
    void onDisconnect(NimBLEClient*, int reason) override {
      owner_->callbackError_.store(reason); owner_->lost_.store(true);
    }
    void onConnectFail(NimBLEClient*, int reason) override {
      owner_->callbackError_.store(reason); owner_->lost_.store(true);
    }
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
  bool retainedOnly_ = false;
  bool bestPreferred_ = false;
  std::atomic<bool> lost_{false};
  std::atomic<bool> linked_{false};
  std::atomic<bool> overflow_{false};
  std::atomic<uint8_t> authentication_{0};
  std::atomic<int> callbackError_{0};
  std::atomic<uint32_t> seen_{0}, matches_{0}, notifications_{0};
  mutable portMUX_TYPE snapshotMux_ = portMUX_INITIALIZER_UNLOCKED;
  Snapshot snapshot_;
  State state_ = State::Off;
  const char* status_ = "Controller off";
  char scanStatus_[48] = {};
  char detail_[112] = {};
  char name_[32] = {};
  char peerName_[32] = {};
  int16_t bestScore_ = -1000;
  uint32_t started_ = 0;
  uint32_t preferredAt_ = 0;
  int lastError_ = 0;
  radio::Diagnostic radioFailure_;
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
  bool traceIine_ = false;
  HidReportTrace reportTrace_;
#endif

  bool acceptsAddress(const ble_addr_t& candidate) const {
    if (!retainedOnly_) return true;
    if (!havePeer_) return false;
    return controllerPeerAllowed(true, true, peerAddress_.getVal(), peerAddress_.getType(),
                                 candidate.val, candidate.type);
  }

  bool beginScan(uint32_t now, bool retainedOnly) {
    stop();
    if (initialized_) return false;
    retainedOnly_ = retainedOnly;
    queue_ = xQueueCreateStatic(QueueDepth, sizeof(Event), queueBytes_, &queueStorage_);
    if (!queue_) return fail("Controller queue unavailable");
    lost_.store(false);
    linked_.store(false);
    overflow_.store(false);
    authentication_.store(0);
    callbackError_.store(0);
    seen_.store(0);
    matches_.store(0);
    notifications_.store(0);
    lastError_ = 0;
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
    traceIine_ = false;
    reportTrace_.reset();
#endif
    if (!initializeRadio()) return fail(radio::status(radioFailure_));
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    found_ = false;
    bestPreferred_ = false;
    preferredAt_ = 0;
    bestScore_ = -1000;
    if (retainedOnly_) memcpy(name_, peerName_, sizeof(name_));
    else name_[0] = '\0';
    state_ = State::Scanning;
    started_ = now;
    status_ = retainedOnly_ ? "Resuming controller" : "Searching BLE controllers";
    snprintf(detail_, sizeof(detail_), "%s", retainedOnly_ ? "Wake the previous controller to reconnect" :
             "IINE: H keyboard, hold Pair 3s");
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&scanCallbacks_, false);
    scan->setActiveScan(true);
    scan->setInterval(50);
    scan->setWindow(35);
    scan->setMaxResults(0);
    if (!scan->start(ScanDurationMs, false)) return fail("Controller scan failed");
    return true;
  }

  void setSnapshot(uint16_t buttons, bool connected, bool clearPressed = false) {
    portENTER_CRITICAL(&snapshotMux_);
    updateSnapshot(snapshot_, buttons, connected, clearPressed);
    portEXIT_CRITICAL(&snapshotMux_);
  }
  void neutral() {
    for (uint8_t i = 0; i < MaxSlots; ++i) slots_[i].buttons = 0;
    setSnapshot(0, state_ == State::Ready, true);
  }
  bool initializeRadio() {
    radio::EspBackend backend;
    if (!radio::start(backend, "M5 Game Controller", radioFailure_)) {
      initialized_ = radioFailure_.failure == radio::Failure::Init &&
                     (backend.hostInitialized() || backend.state() != 0);
      radio::describe(radioFailure_, detail_, sizeof(detail_));
      return false;
    }
    initialized_ = true;
    return true;
  }
  bool releaseRadio() {
    if (initialized_) {
      radio::EspBackend backend;
      radio::Diagnostic result;
      if (!radio::stop(backend, result)) {
        radioFailure_ = result;
        radio::describe(result, detail_, sizeof(detail_));
        setSnapshot(0, false);
        return false;
      }
    }
    initialized_ = false;
    client_ = nullptr;
    if (queue_) vQueueDelete(queue_);
    queue_ = nullptr;
    slotCount_ = 0;
    parser_.clear();
    setSnapshot(0, false);
    return true;
  }
  bool fail(const char* message) {
    if (!lastError_ && client_) lastError_ = client_->getLastError();
    if (lastError_) {
      char previous[sizeof(detail_)];
      memcpy(previous, detail_, sizeof(previous));
      previous[sizeof(previous) - 1] = '\0';
      snprintf(detail_, sizeof(detail_), "%.85s (error %d)", previous, lastError_);
    }
    const bool released = releaseRadio();
    state_ = State::Failed;
    status_ = released ? message : radio::status(radioFailure_);
    return false;
  }
  void connect() {
    state_ = State::Connecting;
    status_ = retainedOnly_ ? "Reconnecting controller" : "Connecting controller";
    snprintf(detail_, sizeof(detail_), "%s", name_);
    started_ = millis();
    client_ = NimBLEDevice::createClient(bestAddress_);
    if (!client_) { fail("Controller client unavailable"); return; }
    client_->setClientCallbacks(&clientCallbacks_, false);
    client_->setConnectTimeout(8000);
    client_->setConnectionParams(12, 24, 0, 200);
    if (!client_->connect(bestAddress_, true, true)) {
      lastError_ = client_->getLastError();
      fail("Controller connection failed");
    }
  }
  bool addSlot(NimBLERemoteCharacteristic* characteristic, uint8_t id, bool boot, bool diagnosticOnly = false) {
    if (slotCount_ == MaxSlots) return false;
    const uint8_t index = slotCount_++;
    Slot& slot = slots_[index];
    slot.characteristic = characteristic;
    slot.reportId = id;
    slot.buttons = 0;
    slot.boot = boot;
    slot.diagnosticOnly = diagnosticOnly;
    if (!characteristic->subscribe(characteristic->canNotify(),
        [this, index](NimBLERemoteCharacteristic*, uint8_t* bytes, size_t length, bool) {
          notifications_.fetch_add(1);
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
      characteristic->unsubscribe(false);
      --slotCount_;
      return false;
    }
    return true;
  }
  bool discover() {
    status_ = "Reading controller inputs";
    // Pairing may resolve an advertised private address to its bonded identity.
    const NimBLEAddress identity = client_->getConnInfo().getIdAddress();
    if (!acceptsAddress(*identity.getBase())) return fail("Previous controller identity changed");
    peerAddress_ = identity;
    NimBLERemoteService* service = client_->getService(NimBLEUUID(uint16_t(0x1812)));
    if (!service) return fail("Controller has no BLE HID service");
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
    traceIine_ = classifyControllerName(name_) == ControllerName::IineKeyboard;
#endif
    // UUID lookups only populate part of NimBLE's characteristic cache.
    // Discover everything before retaining pointers or iterating input reports.
    const auto& characteristics = service->getCharacteristics(true);
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
    Serial.printf("HID characteristics=%u\n", unsigned(characteristics.size()));
    for (const NimBLERemoteCharacteristic* characteristic : characteristics) {
      Serial.printf("HID char uuid=%s handle=%u read=%u write=%u write_nr=%u notify=%u indicate=%u\n",
          characteristic->getUUID().toString().c_str(), unsigned(characteristic->getHandle()),
          unsigned(characteristic->canRead()), unsigned(characteristic->canWrite()),
          unsigned(characteristic->canWriteNoResponse()), unsigned(characteristic->canNotify()),
          unsigned(characteristic->canIndicate()));
    }
#endif
    if (characteristics.empty()) return fail("HID characteristic discovery failed");
    NimBLERemoteCharacteristic* reportMap = service->getCharacteristic(NimBLEUUID(uint16_t(0x2a4b)));
    bool mapped = false;
    size_t mapBytes = 0;
    if (reportMap && reportMap->canRead()) {
      const NimBLEAttValue map = reportMap->readValue();
      mapBytes = map.size();
      mapped = parser_.configure(map.data(), map.size());
    }
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
    Serial.printf("HID map bytes=%u supported=%u reports=%u reason=%s\n", unsigned(mapBytes),
        unsigned(mapped), unsigned(parser_.reportCount()), parser_.error());
    for (uint8_t index = 0; index < parser_.reportCount(); ++index) {
      const uint8_t id = parser_.reportIdAt(index);
      Serial.printf("HID report id=%u length=%u supported=%u\n", unsigned(id),
          unsigned(parser_.expectedLength(id)), unsigned(parser_.supportsReport(id)));
    }
#endif
    // A retained suspend state must not prevent this host receiving controls.
    NimBLERemoteCharacteristic* control = service->getCharacteristic(NimBLEUUID(uint16_t(0x2a4c)));
    if (control && (control->canWriteNoResponse() || control->canWrite())) {
      const uint8_t exitSuspend = 1;
      control->writeValue(&exitSuspend, 1, !control->canWriteNoResponse());
    }
    NimBLERemoteCharacteristic* protocol = service->getCharacteristic(NimBLEUUID(uint16_t(0x2a4e)));
    if (mapped) {
      if (protocol && (protocol->canWrite() || protocol->canWriteNoResponse())) {
        const uint8_t mode = 1;
        if (!protocol->writeValue(&mode, 1, protocol->canWrite())) return fail("HID report mode failed");
      }
      for (NimBLERemoteCharacteristic* characteristic : characteristics) {
        if (characteristic->getUUID() != NimBLEUUID(uint16_t(0x2a4d)) ||
            (!characteristic->canNotify() && !characteristic->canIndicate())) continue;
        NimBLERemoteDescriptor* reference = characteristic->getDescriptor(NimBLEUUID(uint16_t(0x2908)));
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
        Serial.printf("HID input handle=%u reference=%u\n", unsigned(characteristic->getHandle()), unsigned(reference != nullptr));
#endif
        if (!reference) continue;
        const NimBLEAttValue value = reference->readValue();
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
        Serial.printf("HID reference bytes=%u id=%u type=%u\n", unsigned(value.size()),
            unsigned(value.size() > 0 ? value.data()[0] : 0), unsigned(value.size() > 1 ? value.data()[1] : 0));
#endif
        if (value.size() != 2 || value.data()[1] != 1 || !parser_.supportsReport(value.data()[0])) continue;
        if (slotCount_ == MaxSlots) return fail("Too many controller input reports");
        if (!addSlot(characteristic, value.data()[0], false)) return fail("Controller subscription failed");
      }
    }
    if (!slotCount_) {
      NimBLERemoteCharacteristic* boot = service->getCharacteristic(NimBLEUUID(uint16_t(0x2a22)));
      if (!boot || !protocol || (!boot->canNotify() && !boot->canIndicate()) ||
          (!protocol->canWrite() && !protocol->canWriteNoResponse())) {
        snprintf(detail_, sizeof(detail_), "%s", mapped ? "Input Report Reference or notifications missing" : parser_.error());
        return fail(mapped ? "No HID input notifications" : "HID map unsupported: try H mode");
      }
      const uint8_t mode = 0;
      if (!protocol->writeValue(&mode, 1, protocol->canWrite()) || !parser_.configureBootKeyboard() || !addSlot(boot, 0, true))
        return fail("Keyboard boot mode failed");
    }
#ifdef M5FLAP_STARTUP_DIAGNOSTICS
    if (traceIine_) {
      for (NimBLERemoteCharacteristic* characteristic : characteristics) {
        if (slotCount_ == MaxSlots) break;
        if (characteristic->getUUID() != NimBLEUUID(uint16_t(0x2a4d)) ||
            (!characteristic->canNotify() && !characteristic->canIndicate())) continue;
        bool subscribed = false;
        for (uint8_t slot = 0; slot < slotCount_; ++slot) subscribed |= slots_[slot].characteristic == characteristic;
        if (subscribed) continue;
        NimBLERemoteDescriptor* reference = characteristic->getDescriptor(NimBLEUUID(uint16_t(0x2908)));
        if (!reference) continue;
        const NimBLEAttValue value = reference->readValue();
        if (value.size() != 2 || value.data()[1] != 1) continue;
        const bool added = addSlot(characteristic, value.data()[0], false, true);
        Serial.printf("IINE trace subscribe id=%u handle=%u ok=%u\n", unsigned(value.data()[0]),
            unsigned(characteristic->getHandle()), unsigned(added));
      }
    }
#endif
    parser_.setKeyboardProfile(classifyControllerName(name_) == ControllerName::IineKeyboard ?
        KeyboardProfile::IineGamebrick : KeyboardProfile::Generic);
    state_ = State::Ready;
    status_ = "Controller ready";
    lastError_ = 0;
    snprintf(detail_, sizeof(detail_), "%s: %u input reports, %u map bytes", name_, unsigned(slotCount_), unsigned(mapBytes));
    setSnapshot(0, true, true);
    return true;
  }
};

}  // namespace controller
}  // namespace flappy
#endif  // ARDUINO
