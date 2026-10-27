#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef ARDUINO
#include <Arduino.h>
#if __has_include(<esp32-hal-alloc-ble-mem.h>)
// Retain BLE memory before Arduino startup releases unused radio regions.
#include <esp32-hal-alloc-ble-mem.h>
#endif
#include <esp_heap_caps.h>
#include <NimBLEDevice.h>
#endif

namespace flappy {
namespace radio {

enum class Failure : uint8_t { None, Busy, Init, ClientStop, HostStop, ControllerStop, Objects };
struct Diagnostic {
  Failure failure = Failure::None;
  uint8_t before = 0, failedState = 0;
  uint32_t freeHeap = 0, largestBlock = 0;
  int cleanupError = 0;
};

// Controller states follow ESP-IDF: idle 0, initialized 1, enabled 2.
template <class Backend> int stopController(Backend& backend) {
  if (backend.state() == 2) {
    const int error = backend.disableController();
    if (error) return error;
  }
  if (backend.state() == 1) {
    const int error = backend.deinitController();
    if (error) return error;
  }
  return backend.state() == 0 ? 0 : -1;
}

template <class Backend> void capture(Backend& backend, Diagnostic& result) {
  result.failedState = uint8_t(backend.state());
  result.freeHeap = backend.freeHeap();
  result.largestBlock = backend.largestBlock();
}

template <class Backend> bool start(Backend& backend, const char* name, Diagnostic& result) {
  result = Diagnostic();
  result.before = uint8_t(backend.state());
  if (backend.hostInitialized() || result.before != 0 || backend.remainingClients()) {
    result.failure = Failure::Busy;
    capture(backend, result);
    return false;
  }
  if (backend.startHost(name)) return true;
  result.failure = Failure::Init;
  capture(backend, result);
  // NimBLE can fail after controller initialization, before its host flag is set.
  result.cleanupError = stopController(backend);
  return false;
}

template <class Backend> bool stop(Backend& backend, Diagnostic& result) {
  result = Diagnostic();
  result.before = uint8_t(backend.state());
  if (backend.hostInitialized()) {
    result.cleanupError = backend.quiesceClients(500);
    if (result.cleanupError) {
      result.failure = Failure::ClientStop;
      capture(backend, result);
      return false;
    }
  }
  // Keep callbacks and their storage alive if the host task could not stop.
  if (backend.hostInitialized() && !backend.stopHost()) {
    result.failure = Failure::HostStop;
    capture(backend, result);
    return false;
  }
  result.cleanupError = stopController(backend);
  if (result.cleanupError) {
    result.failure = Failure::ControllerStop;
    capture(backend, result);
    return false;
  }
  backend.deleteObjects();
  if (backend.remainingClients()) {
    result.failure = Failure::Objects;
    result.cleanupError = int(backend.remainingClients());
    capture(backend, result);
    return false;
  }
  return true;
}

inline const char* status(const Diagnostic& result) {
  switch (result.failure) {
    case Failure::Busy: return "Bluetooth is busy";
    case Failure::ClientStop: return "Controller disconnect failed";
    case Failure::HostStop: return "Bluetooth stop failed";
    case Failure::ControllerStop: return "Bluetooth reset failed";
    case Failure::Objects: return "Bluetooth cleanup failed";
    case Failure::Init: return "Bluetooth init failed";
    case Failure::None: return "Bluetooth off";
  }
  return "Bluetooth error";
}

inline void describe(const Diagnostic& result, char* output, size_t size) {
  const char* phase = result.failure == Failure::Busy ? "Busy" :
      result.failure == Failure::ClientStop ? "Disconnect" :
      result.failure == Failure::HostStop ? "Stop host" :
      result.failure == Failure::ControllerStop ? "Stop radio" :
      result.failure == Failure::Objects ? "Client cleanup" : "Init";
  snprintf(output, size, "%s B%u>%u H%u L%u", phase, unsigned(result.before),
           unsigned(result.failedState), unsigned(result.freeHeap), unsigned(result.largestBlock));
  if (result.cleanupError && output && size) {
    size_t used = 0;
    while (used < size && output[used]) ++used;
    if (used < size) snprintf(output + used, size - used, " C%d", result.cleanupError);
  }
}

#ifdef ARDUINO
struct EspBackend {
  int state() const { return int(esp_bt_controller_get_status()); }
  bool hostInitialized() const { return NimBLEDevice::isInitialized(); }
  bool startHost(const char* name) { return NimBLEDevice::init(name); }
  int quiesceClients(uint32_t timeoutMs) {
    const uint32_t began = millis();
    if (ble_gap_conn_active()) {
      const int error = ble_gap_conn_cancel();
      if (error && error != BLE_HS_EALREADY) return error;
    }
    for (;;) {
      // A connection may finish while its cancellation is being delivered.
      const auto clients = NimBLEDevice::getConnectedClients();
      for (NimBLEClient* client : clients) {
        if (!client->disconnect()) return client->getLastError();
      }
      if (clients.empty() && !ble_gap_conn_active()) return 0;
      if (uint32_t(millis() - began) >= timeoutMs) return BLE_HS_ETIMEOUT;
      delay(1);
    }
  }
  bool stopHost() { return NimBLEDevice::deinit(false); }
  void deleteObjects() { NimBLEDevice::deinit(true); }
  size_t remainingClients() const { return NimBLEDevice::getCreatedClientCount(); }
  int disableController() { return esp_bt_controller_disable(); }
  int deinitController() { return esp_bt_controller_deinit(); }
  uint32_t freeHeap() const { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); }
  uint32_t largestBlock() const { return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); }
};
#endif

}  // namespace radio
}  // namespace flappy
