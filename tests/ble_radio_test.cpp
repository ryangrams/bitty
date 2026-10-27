#include "../firmware/flappy/ble_radio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>

using namespace flappy::radio;

struct FakeRadio {
  int controller = 0;
  bool initialized = false, startResult = true, stopResult = true;
  bool stopLeavesController = false;
  int failedStartState = 0, disableError = 0, deinitError = 0;
  int quiesceError = 0;
  unsigned clients = 0;
  bool retainClients = false;
  unsigned deleted = 0;
  std::vector<char> calls;
  int state() const { return controller; }
  bool hostInitialized() const { return initialized; }
  bool startHost(const char*) {
    calls.push_back('S');
    controller = startResult ? 2 : failedStartState;
    initialized = startResult;
    return startResult;
  }
  bool stopHost() {
    calls.push_back('H');
    if (!stopResult) return false;
    initialized = false;
    if (!stopLeavesController) controller = 0;
    return true;
  }
  int quiesceClients(uint32_t timeoutMs) {
    assert(timeoutMs == 500);
    calls.push_back('Q');
    return quiesceError;
  }
  void deleteObjects() {
    calls.push_back('X');
    ++deleted;
    if (!retainClients) clients = 0;
  }
  size_t remainingClients() const { return clients; }
  int disableController() {
    calls.push_back('D');
    if (!disableError) controller = 1;
    return disableError;
  }
  int deinitController() {
    calls.push_back('C');
    if (!deinitError) controller = 0;
    return deinitError;
  }
  uint32_t freeHeap() const { return controller == 0 ? 100000 : 63000; }
  uint32_t largestBlock() const { return controller == 0 ? 50000 : 22000; }
};

static void partialStartupCanRetry() {
  for (int failedState = 0; failedState <= 2; ++failedState) {
    FakeRadio radio;
    radio.startResult = false;
    radio.failedStartState = failedState;
    Diagnostic failure;
    assert(!start(radio, "controller", failure));
    assert(failure.failure == Failure::Init);
    assert(failure.before == 0 && failure.failedState == failedState);
    assert(failure.freeHeap == (failedState ? 63000u : 100000u));
    assert(failure.largestBlock == (failedState ? 22000u : 50000u));
    assert(failure.cleanupError == 0 && radio.controller == 0);
    const std::vector<char> expected = failedState == 0 ? std::vector<char>{'S'} :
        failedState == 1 ? std::vector<char>{'S', 'C'} : std::vector<char>{'S', 'D', 'C'};
    assert(radio.calls == expected);
    radio.startResult = true;
    assert(start(radio, "controller", failure));
    assert(failure.failure == Failure::None && radio.initialized);
    assert(stop(radio, failure));
    assert(!radio.initialized && radio.controller == 0 && radio.deleted == 1);
  }
}

static void cleanupFailuresRetainOwnership() {
  FakeRadio radio;
  radio.startResult = false;
  radio.failedStartState = 2;
  radio.disableError = 0x101;
  Diagnostic failure;
  assert(!start(radio, "controller", failure));
  assert(failure.failure == Failure::Init && failure.cleanupError == 0x101);
  assert(radio.controller == 2 && radio.deleted == 0);
  const size_t previousCalls = radio.calls.size();
  assert(!start(radio, "other owner", failure));
  assert(failure.failure == Failure::Busy && radio.calls.size() == previousCalls);
  assert(!stop(radio, failure));
  assert(failure.failure == Failure::ControllerStop && failure.cleanupError == 0x101);
  assert(radio.controller == 2 && radio.deleted == 0);
  radio.disableError = 0;
  radio.deinitError = 0x103;
  assert(!stop(radio, failure));
  assert(failure.cleanupError == 0x103 && radio.controller == 1 && radio.deleted == 0);
  radio.deinitError = 0;
  assert(stop(radio, failure));
  assert(radio.controller == 0 && radio.deleted == 1);
}

static void failedHostStopKeepsCallbacksAlive() {
  FakeRadio radio;
  Diagnostic failure;
  assert(start(radio, "controller", failure));
  radio.stopResult = false;
  radio.calls.clear();
  assert(!stop(radio, failure));
  assert(failure.failure == Failure::HostStop);
  assert((radio.calls == std::vector<char>{'Q', 'H'}));
  assert(radio.initialized && radio.controller == 2 && radio.deleted == 0);
  assert(!start(radio, "other owner", failure));
  assert(failure.failure == Failure::Busy);
  radio.stopResult = true;
  radio.stopLeavesController = true;
  radio.calls.clear();
  assert(stop(radio, failure));
  assert((radio.calls == std::vector<char>{'Q', 'H', 'D', 'C', 'X'}));
  assert(!radio.initialized && radio.controller == 0 && radio.deleted == 1);
}

static void disconnectTimeoutPreservesLiveClient() {
  FakeRadio radio;
  Diagnostic failure;
  assert(start(radio, "controller", failure));
  radio.clients = 1;
  radio.quiesceError = 13;
  radio.calls.clear();
  assert(!stop(radio, failure));
  assert(failure.failure == Failure::ClientStop && failure.cleanupError == 13);
  assert((radio.calls == std::vector<char>{'Q'}));
  assert(radio.initialized && radio.controller == 2 && radio.clients == 1 && radio.deleted == 0);
  radio.quiesceError = 0;
  assert(stop(radio, failure));
  assert(!radio.initialized && radio.controller == 0 && radio.clients == 0 && radio.deleted == 1);
}

static void retainedClientIsReportedAfterObjectClear() {
  FakeRadio radio;
  Diagnostic failure;
  assert(start(radio, "controller", failure));
  radio.clients = 1;
  radio.retainClients = true;
  assert(!stop(radio, failure));
  assert(failure.failure == Failure::Objects && failure.cleanupError == 1);
  assert(radio.controller == 0 && !radio.initialized && radio.clients == 1);
  assert(!strcmp(status(failure), "Bluetooth cleanup failed"));
  const size_t previousCalls = radio.calls.size();
  assert(!start(radio, "other owner", failure));
  assert(failure.failure == Failure::Busy && radio.calls.size() == previousCalls);
  radio.retainClients = false;
  assert(stop(radio, failure));
  assert(radio.clients == 0);
}

static void busyRadioIsNeverTakenOver() {
  for (int state = 1; state <= 2; ++state) {
    FakeRadio radio;
    radio.controller = state;
    radio.initialized = state == 2;
    Diagnostic failure;
    assert(!start(radio, "controller", failure));
    assert(failure.failure == Failure::Busy);
    assert(radio.calls.empty() && radio.controller == state && radio.deleted == 0);
  }
}

static void diagnosticsShowPhaseBeforeHeap() {
  Diagnostic result;
  result.failure = Failure::Init;
  result.failedState = 1;
  result.freeHeap = 63000;
  result.largestBlock = 22000;
  result.cleanupError = 259;
  char text[112];
  describe(result, text, sizeof(text));
  assert(!strcmp(text, "Init B0>1 H63000 L22000 C259"));
  assert(strlen(text) <= 33);
  assert(!strcmp(status(result), "Bluetooth init failed"));
  struct { char text[8]; char guard; } small = {{}, 'G'};
  describe(result, small.text, sizeof(small.text));
  assert(small.text[7] == '\0' && small.guard == 'G');
  describe(result, nullptr, 0);
  result.failure = Failure::HostStop;
  assert(!strcmp(status(result), "Bluetooth stop failed"));
  result.failure = Failure::ControllerStop;
  assert(!strcmp(status(result), "Bluetooth reset failed"));
}

int main() {
  partialStartupCanRetry();
  cleanupFailuresRetainOwnership();
  failedHostStopKeepsCallbacksAlive();
  disconnectTimeoutPreservesLiveClient();
  retainedClientIsReportedAfterObjectClear();
  busyRadioIsNeverTakenOver();
  diagnosticsShowPhaseBeforeHeap();
  puts("BLE lifecycle tests passed");
}
