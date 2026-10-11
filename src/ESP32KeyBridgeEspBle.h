#pragma once

#include <ESP32KeyBridge.h>
#include <ESP32KeyBridgeBluetooth.h>
#include <ESP32KeyBridgeBluetoothSettings.h>
#include <EspBle.h>

// EspBle's connection callbacks report the over-the-air (possibly rotating
// private) address. A bonded host must be identified by NimBLE's resolved
// peer_id_addr instead; this is the same identity used by EspBle::bond().
#include <host/ble_gap.h>

#include <stdio.h>
#include <string.h>

// BLE adapters backed by the EspBle library: a HID over GATT output
// (peripheral, seen by a PC/phone as a Bluetooth keyboard + consumer + mouse)
// and HID over GATT inputs (central, reading BLE keyboards / mice / gamepads).
//
// Ownership model: the sketch owns the EspBle stack and starts it with the
// library's own configuration (device name, security, advertising). The HID
// service composition is fixed at ble.begin(), so the output adapter declares
// its profiles at construction time — construct it before ble.begin(). The
// input adapters subscribe lazily on their first update(). Neither has a
// begin().
//
// Threading: unlike EspUsbHost, EspBle dispatches user callbacks from the
// context that calls EspBle::update(), not from the stack task, so these
// adapters need no critical section. The sketch must therefore call
// ble.update() from the same context as bridge.update() (the usual loop):
//
//   void loop() { ble.update(); bridge.update(); }
//
// Subscription uses the multi-listener API, so a sketch can keep using every
// on*() hook next to an adapter. The one exception is the keyboard LED
// output report of the output adapter, which is single-slot by design in all
// three libraries — read ble.hidKeyboard().ledState() instead of installing
// onOutputReport() when that adapter is in use.

namespace esp32keybridge
{

// ---------------------------------------------------------------------------
// Output: BLE HID peripheral
// ---------------------------------------------------------------------------

// Composite HID over GATT output: keyboard + consumer + mouse in one HID
// service. The full composition is always declared — a profile that never
// sends a report costs one report descriptor entry and nothing else, which is
// the same trade-off the USB Device adapter makes.
//
// The report formats match EspUsbDeviceHidOutputAdapter (6KRO keyboard,
// one 16-bit consumer usage, 8 buttons + X/Y/Wheel mouse), so a sketch can
// swap or run both outputs with identical behaviour.
class EspBleHidOutputAdapter : public OutputAdapter
{
public:
  // The configs carry HID device identity (manufacturer, VID/PID, layout,
  // boot protocol). Defaults are fine for a plain keyboard; pass your own to
  // control how the host names and treats the device.
  explicit EspBleHidOutputAdapter(
      EspBle &ble,
      const EspBleHidKeyboardConfig &keyboardConfig = EspBleHidKeyboardConfig(),
      const EspBleHidMouseConfig &mouseConfig = EspBleHidMouseConfig(),
      const EspBleHidConsumerControlConfig &consumerConfig = EspBleHidConsumerControlConfig())
      : ble_(ble)
  {
    ble_.hidKeyboard().configure(keyboardConfig);
    ble_.hidConsumerControl().configure(consumerConfig);
    ble_.hidMouse().configure(mouseConfig);

    // The LED value itself is polled from ledState(), which the library keeps
    // updated whether or not a callback is installed. This hook only records
    // that a host has spoken at least once, which ledState() cannot express
    // (no report yet and "all LEDs off" look the same).
    ble_.hidKeyboard().onOutputReport(
        [this](const EspBleHidKeyboardOutputReport &report) {
          if (report.connectionId == peripheralConnectionId_) ledSeen_ = true;
        });
    connectedListener_ = ble_.addConnectedListener(
        [this](const EspBleConnection &connection) {
          if (connection.localRole != EspBleRole::Peripheral) return;
          peripheralConnectionId_ = connection.id;
          resetSession();
        });
    disconnectedListener_ = ble_.addDisconnectedListener(
        [this](const EspBleConnection &connection) {
          if (connection.id != peripheralConnectionId_) return;
          peripheralConnectionId_ = 0;
          resetSession();
        });
  }

  ~EspBleHidOutputAdapter() override
  {
    ble_.removeConnectionListener(connectedListener_);
    ble_.removeConnectionListener(disconnectedListener_);
    ble_.hidKeyboard().onOutputReport({});
  }

  // Packs the key set into keyboard, consumer, and mouse-button reports.
  // Each report is sent when its state changed (or when the previous send
  // failed, so a dropped notification is retried on the next update).
  void write(const KeySet &keys) override
  {
    writeKeyboard(keys);
    writeConsumer(keys);
    writeMouse(keys);
  }

  // True while a host is connected, encrypted (when security is enabled), and
  // subscribed to the keyboard input report. Reports cannot go out before
  // that, so this is what gates the bridge's writes.
  bool connected() const override { return ble_.hidKeyboard().ready(); }

  // LED output reports from the host provide the lock state. False until the
  // first report arrives; the bridge keeps its shadow meanwhile.
  bool reportsLockState() const override { return true; }
  bool getLockState(LockState &out) const override
  {
    if (!ledSeen_)
    {
      return false;
    }
    const EspBleHidKeyboardOutputReport report = ble_.hidKeyboard().ledState();
    out.numLock = report.numLock;
    out.capsLock = report.capsLock;
    out.scrollLock = report.scrollLock;
    out.kana = report.kana;
    return true;
  }

  // Relative axis totals accumulate here; the bridge delivers them before
  // write() in the same update, so they ride the mouse report of the same
  // cycle (int8 saturation with the remainder carried to the next update).
  // The HID mouse report has no pan field, so Pan is dropped.
  void writeAxisDelta(Axis axis, int32_t delta) override
  {
    if (axis == Axis::Pan)
    {
      return;
    }
    pendingAxis_[static_cast<size_t>(axis)] += delta;
  }

private:
  void writeKeyboard(const KeySet &keys)
  {
    const HidKeyboardReport report = buildHidKeyboardReport(keys);
    EspBleHidKeyboardReport bleReport;
    bleReport.modifiers = report.modifiers;
    for (size_t i = 0; i < HidKeyboardReport::MaxKeys; ++i)
    {
      bleReport.keys[i] = i < report.keyCount ? report.keys[i] : 0;
    }

    if (!keyboardPending_ && memcmp(&bleReport, &lastKeyboardReport_, sizeof(bleReport)) == 0)
    {
      return;
    }
    lastKeyboardReport_ = bleReport;
    keyboardPending_ = !ble_.hidKeyboard().sendReport(bleReport);
  }

  void writeConsumer(const KeySet &keys)
  {
    const HidConsumerReport report = buildHidConsumerReport(keys);
    if (!consumerPending_ && report.usage == lastConsumerUsage_)
    {
      return;
    }
    lastConsumerUsage_ = report.usage;
    consumerPending_ = !ble_.hidConsumerControl().sendUsage(report.usage);
  }

  void writeMouse(const KeySet &keys)
  {
    HidMouseReport report = buildHidMouseReport(keys);
    for (size_t axis = 0; axis < kAxisCount; ++axis)
    {
      pendingAxis_[axis] = report.applyAxisDelta(static_cast<Axis>(axis), pendingAxis_[axis]);
    }
    const bool moved = report.x != 0 || report.y != 0 || report.wheel != 0;
    // lastMouseButtons_ tracks the last successful send, so a failed button
    // change keeps mismatching and retries on the next update.
    if (!mousePending_ && !moved && report.buttons == lastMouseButtons_)
    {
      return;
    }

    EspBleHidMouseReport bleReport;
    bleReport.buttons = report.buttons;
    bleReport.x = report.x;
    bleReport.y = report.y;
    bleReport.wheel = report.wheel;
    if (ble_.hidMouse().sendReport(bleReport))
    {
      lastMouseButtons_ = report.buttons;
      mousePending_ = false;
    }
    else
    {
      // Not sent: put the movement back so nothing is lost.
      pendingAxis_[static_cast<size_t>(Axis::X)] += report.x;
      pendingAxis_[static_cast<size_t>(Axis::Y)] += report.y;
      pendingAxis_[static_cast<size_t>(Axis::Wheel)] += report.wheel;
    }
  }

  void resetSession()
  {
    // Each HID host starts with an independent report/LED state. The bridge
    // can already hold keys when a new host subscribes, so force a complete
    // first snapshot even if it equals the previous host's last report.
    lastKeyboardReport_ = EspBleHidKeyboardReport();
    keyboardPending_ = true;
    lastConsumerUsage_ = 0;
    consumerPending_ = true;
    lastMouseButtons_ = 0;
    mousePending_ = true;
    ledSeen_ = false;
    for (size_t i = 0; i < kAxisCount; ++i) pendingAxis_[i] = 0;
  }

  EspBle &ble_;
  EspBleConnectionId peripheralConnectionId_ = 0;
  EspBleListenerId connectedListener_ = EspBleInvalidListenerId;
  EspBleListenerId disconnectedListener_ = EspBleInvalidListenerId;
  EspBleHidKeyboardReport lastKeyboardReport_;
  bool keyboardPending_ = false;
  uint16_t lastConsumerUsage_ = 0;
  bool consumerPending_ = false;
  uint8_t lastMouseButtons_ = 0;
  bool mousePending_ = true;
  int32_t pendingAxis_[kAxisCount] = {};
  bool ledSeen_ = false;
};

// Selects which bonded BLE Host may reconnect to the HID peripheral.  EspBle's
// bundled NimBLE stores at most three bonds, so those bonds are exposed as
// slots 1..3.  Selecting a populated slot disconnects the current peripheral
// link and starts directed advertising to that bond.  Pairing removes the bond
// currently occupying the selected slot, then starts ordinary discoverable
// advertising so a replacement Host can pair there.
//
// Construct this next to EspBleHidOutputAdapter, then wrap the physical keyboard
// input in BluetoothHotkeyInputAdapter.  This object observes connection events
// through EspBle's multi-listener API and does not take over the sketch's on*()
// callbacks.
class EspBleHidTargetController : public BluetoothTargetController
{
public:
  static constexpr size_t MaxSlots = 3;

  explicit EspBleHidTargetController(EspBle &ble, const char *baseDeviceName = nullptr)
      : ble_(ble), baseDeviceName_(baseDeviceName == nullptr ? "" : baseDeviceName)
  {
    connectedListener_ = ble_.addConnectedListener(
        [this](const EspBleConnection &connection) { handleConnected(connection); });
    disconnectedListener_ = ble_.addDisconnectedListener(
        [this](const EspBleConnection &connection) { handleDisconnected(connection); });
    securityListener_ = ble_.addSecurityChangedListener(
        [this](const EspBleSecurityChanged &event) { handleSecurity(event); });
    characteristicDiscoveredListener_ = ble_.addCharacteristicDiscoveredListener(
        [this](const EspBleGattResult &result) { handleNameDiscovered(result); });
    characteristicReadListener_ = ble_.addCharacteristicReadListener(
        [this](const EspBleGattResult &result) { handleNameRead(result); });
  }

  EspBleHidTargetController(EspBle &ble, BluetoothProfileSettingsStore &store,
                            const char *baseDeviceName = nullptr)
      : EspBleHidTargetController(ble, baseDeviceName)
  {
    profileStore_ = &store;
  }

  ~EspBleHidTargetController() override
  {
    ble_.removeConnectionListener(connectedListener_);
    ble_.removeConnectionListener(disconnectedListener_);
    ble_.removeConnectionListener(securityListener_);
    ble_.removeGattListener(characteristicDiscoveredListener_);
    ble_.removeGattListener(characteristicReadListener_);
  }

  size_t slotCount() const override { return MaxSlots; }
  size_t selectedSlot() const override { return selectedSlot_; }
  BluetoothTargetStatus status() const override { return status_; }
  uint32_t nowMilliseconds() const override { return millis(); }

  bool loadProfileSettings()
  {
    if (profilesLoaded_) return true;
    if (profileStore_ != nullptr)
    {
      const BluetoothProfileSettingsLoadStatus result =
          profileStore_->loadOrDefault(registry_);
      if (result == BluetoothProfileSettingsLoadStatus::DefaultsStorageError)
      {
        setError("profile_storage_read_failed");
        return false;
      }
    }
    profilesLoaded_ = true;
    selectedSlot_ = registry_.selectedSlot();
    slotsLoaded_ = false;
    loadSlots();
    return status_ != BluetoothTargetStatus::Failed;
  }

  // Resume the saved bonded target on boot, never ordinary discoverable
  // advertising. An empty selected slot remains idle until explicit Pair.
  bool resumeSelectedSlot()
  {
    if (!loadProfileSettings()) return false;
    loadSlots();
    ble_.advertising().stop();
    if (!slotUsed_[selectedSlot_ - 1])
    {
      ble_.advertising().clearDirectedTarget();
      operation_ = BluetoothProfileOperation::None;
      status_ = BluetoothTargetStatus::Idle;
      return true;
    }
    targetBond_ = slots_[selectedSlot_ - 1];
    status_ = BluetoothTargetStatus::Switching;
    operation_ = BluetoothProfileOperation::Connecting;
    pending_ = Pending::DirectedAdvertising;
    reconnectDeadline_.start(millis(), ReconnectTimeoutMs);
    return startPending();
  }

  void update() override
  {
    loadProfileSettings();
    if (reconnectDeadline_.expired(millis()))
    {
      reconnectDeadline_.cancel();
      ble_.advertising().stop();
      pending_ = Pending::None;
      operation_ = BluetoothProfileOperation::None;
      status_ = BluetoothTargetStatus::Idle;
      clearError();
    }
    if (status_ == BluetoothTargetStatus::Pairing &&
        static_cast<uint32_t>(millis() - pairingStartedMs_) >= PairingTimeoutMs)
    {
      ble_.advertising().stop();
      pending_ = Pending::None;
      if (pairingConnectionId_ != 0)
      {
        cancellingDisconnect_ = true;
        operation_ = BluetoothProfileOperation::Cancelling;
        if (!ble_.disconnect(pairingConnectionId_)) fail("disconnect_failed");
      }
      else
      {
        operation_ = BluetoothProfileOperation::None;
        status_ = BluetoothTargetStatus::Idle;
      }
    }
  }

  // Applies the selected slot suffix to EspBle's advertising name. Call once
  // after ble.begin() and before resumeSelectedSlot();
  // reconnect and pairing paths call it automatically afterwards.
  bool applySelectedSlotName()
  {
    if (baseDeviceName_.isEmpty())
    {
      return true;
    }
    char name[32] = {};
    if (!formatBluetoothSlotName(baseDeviceName_.c_str(), selectedSlot_, name,
                                 sizeof(name)))
    {
      return false;
    }
    ble_.advertising().setName(name);
    return true;
  }

  bool selectSlot(size_t slot) override
  {
    if (busy()) return reject("bluetooth_busy");
    suppressReconnect_ = false;
    if (slot == 0 || slot > MaxSlots)
    {
      status_ = BluetoothTargetStatus::Failed;
      return false;
    }

    clearError();
    loadProfileSettings();
    loadSlots();
    const size_t previousSlot = selectedSlot_;
    const BluetoothProfileRegistry previousRegistry = registry_;
    selectedSlot_ = slot;
    registry_.select(slot);
    if (!saveRegistry())
    {
      selectedSlot_ = previousSlot;
      registry_ = previousRegistry;
      return fail("profile_storage_write_failed");
    }
    if (!slotUsed_[slot - 1])
    {
      operation_ = BluetoothProfileOperation::None;
      status_ = peripheralConnectionId_ != 0 ? BluetoothTargetStatus::Connected
                                              : BluetoothTargetStatus::Idle;
      return applySelectedSlotName();
    }
    targetBond_ = slots_[slot - 1];
    status_ = BluetoothTargetStatus::Switching;
    operation_ = BluetoothProfileOperation::Connecting;
    pending_ = Pending::DirectedAdvertising;
    ble_.advertising().stop();

    if (peripheralConnectionId_ != 0)
    {
      if (peripheralAddress_.equalsIgnoreCase(targetBond_.peerAddress))
      {
        pending_ = Pending::None;
        status_ = BluetoothTargetStatus::Connected;
        operation_ = BluetoothProfileOperation::None;
        return true;
      }
      if (!ble_.disconnect(peripheralConnectionId_))
      {
        pending_ = Pending::None;
        return fail("disconnect_failed");
      }
      return true;
    }
    return startPending();
  }

  bool pairSlot(size_t slot) override
  {
    if (busy()) return reject("bluetooth_busy");
    suppressReconnect_ = false;
    if (slot == 0 || slot > MaxSlots)
    {
      status_ = BluetoothTargetStatus::Failed;
      return false;
    }

    clearError();
    loadProfileSettings();
    loadSlots();
    const size_t previousSlot = selectedSlot_;
    const BluetoothProfileRegistry previousRegistry = registry_;
    selectedSlot_ = slot;
    registry_.select(slot);
    if (!saveRegistry())
    {
      selectedSlot_ = previousSlot;
      registry_ = previousRegistry;
      return fail("profile_storage_write_failed");
    }
    status_ = BluetoothTargetStatus::Pairing;
    operation_ = BluetoothProfileOperation::Pairing;
    pairingStartedMs_ = millis();
    pairingConnectionId_ = 0;
    pending_ = Pending::PairingAdvertising;
    ble_.advertising().stop();
    if (peripheralConnectionId_ != 0)
    {
      if (!ble_.disconnect(peripheralConnectionId_))
      {
        pending_ = Pending::None;
        return fail("disconnect_failed");
      }
      return true;
    }
    return startPending();
  }

  bool cancelPairing() override
  {
    if (status_ != BluetoothTargetStatus::Pairing) return reject("not_pairing");
    ble_.advertising().stop();
    pending_ = Pending::None;
    if (pairingConnectionId_ != 0)
    {
      cancellingDisconnect_ = true;
      operation_ = BluetoothProfileOperation::Cancelling;
      if (!ble_.disconnect(pairingConnectionId_)) return fail("disconnect_failed");
    }
    else
    {
      operation_ = BluetoothProfileOperation::None;
      status_ = BluetoothTargetStatus::Idle;
    }
    return true;
  }

  bool deleteSlot(size_t slot) override
  {
    if (busy()) return reject("bluetooth_busy");
    if (slot == 0 || slot > MaxSlots) return fail("invalid_slot");
    clearError();
    loadProfileSettings();
    loadSlots();
    if (!slotUsed_[slot - 1]) return fail("empty_slot");
    deleteSlot_ = slot;
    reconnectSlot_ = 0;
    if (peripheralConnectionId_ != 0 && slotUsed_[slot - 1] &&
        !peripheralAddress_.equalsIgnoreCase(slots_[slot - 1].peerAddress))
    {
      for (size_t i = 0; i < MaxSlots; ++i)
        if (slotUsed_[i] && peripheralAddress_.equalsIgnoreCase(slots_[i].peerAddress))
          reconnectSlot_ = i + 1;
    }
    status_ = BluetoothTargetStatus::Deleting;
    operation_ = BluetoothProfileOperation::Deleting;
    pending_ = Pending::DeleteBond;
    ble_.advertising().stop();
    if (peripheralConnectionId_ != 0)
    {
      if (!ble_.disconnect(peripheralConnectionId_))
      {
        pending_ = Pending::None;
        return fail("disconnect_failed");
      }
      return true;
    }
    return startPending();
  }

  bool setSlotLabel(size_t slot, const char *label) override
  {
    loadProfileSettings();
    BluetoothProfileRegistry before = registry_;
    if (!registry_.setLabel(slot, label)) return fail("invalid_label");
    if (!saveRegistry())
    {
      registry_ = before;
      return fail("profile_storage_write_failed");
    }
    clearError();
    return true;
  }

  bool forceDisconnect() override
  {
    ble_.advertising().stop();
    reconnectDeadline_.cancel();
    pending_ = Pending::None;
    deleteSlot_ = 0;
    reconnectSlot_ = 0;
    pairingConnectionId_ = 0;
    suppressReconnect_ = true;
    clearError();
    if (forceStopDisconnect_) return true;
    if (peripheralConnectionId_ != 0)
    {
      forceStopDisconnect_ = true;
      operation_ = BluetoothProfileOperation::Cancelling;
      status_ = BluetoothTargetStatus::Switching;
      if (!ble_.disconnect(peripheralConnectionId_))
      {
        forceStopDisconnect_ = false;
        return fail("disconnect_failed");
      }
      return true;
    }
    forceStopDisconnect_ = false;
    operation_ = BluetoothProfileOperation::None;
    status_ = BluetoothTargetStatus::Idle;
    return true;
  }

  bool resetBluetooth() override
  {
    ble_.advertising().stop();
    reconnectDeadline_.cancel();
    pending_ = Pending::None;
    suppressReconnect_ = true;
    clearError();
    if (peripheralConnectionId_ != 0)
    {
      resetAfterDisconnect_ = true;
      forceStopDisconnect_ = true;
      operation_ = BluetoothProfileOperation::Cancelling;
      status_ = BluetoothTargetStatus::Switching;
      if (!ble_.disconnect(peripheralConnectionId_))
      {
        resetAfterDisconnect_ = false;
        forceStopDisconnect_ = false;
        return fail("disconnect_failed");
      }
      return true;
    }
    return completeBluetoothReset();
  }

  bool completeBluetoothReset()
  {
    const bool bondsCleared = ble_.deleteAllBonds();
    const bool profilesCleared = profileStore_ == nullptr || profileStore_->clear();
    registry_.reset();
    selectedSlot_ = 1;
    slotsLoaded_ = true;
    for (size_t i = 0; i < MaxSlots; ++i)
    {
      slotUsed_[i] = false;
      slots_[i] = EspBleBond();
    }
    if (!bondsCleared) return fail("bond_delete_failed");
    if (!profilesCleared) return fail("profile_storage_write_failed");
    operation_ = BluetoothProfileOperation::None;
    status_ = BluetoothTargetStatus::Idle;
    return true;
  }

  bool busy() const override
  {
    return operation_ != BluetoothProfileOperation::None || pending_ != Pending::None ||
           status_ == BluetoothTargetStatus::Switching ||
           status_ == BluetoothTargetStatus::Pairing ||
           status_ == BluetoothTargetStatus::Deleting;
  }

  bool snapshot(BluetoothTargetSnapshot &out) const override
  {
    out = BluetoothTargetSnapshot();
    out.profileCount = MaxSlots;
    out.selectedSlot = selectedSlot_;
    out.connectedCount = peripheralConnectionId_ == 0 ? 0 : 1;
    out.status = status_;
    out.operation = operation_;
    if (status_ == BluetoothTargetStatus::Pairing)
    {
      const uint32_t elapsed = static_cast<uint32_t>(millis() - pairingStartedMs_);
      out.pairingSecondsRemaining = elapsed >= PairingTimeoutMs
          ? 0 : (PairingTimeoutMs - elapsed + 999) / 1000;
    }
    strncpy(out.error, error_, sizeof(out.error) - 1);
    for (size_t i = 0; i < MaxSlots; ++i)
    {
      BluetoothProfileSnapshot &p = out.profiles[i];
      const BluetoothProfileRecord &saved = registry_.profile(i + 1);
      p.slot = i + 1;
      p.occupied = saved.occupied;
      p.selected = selectedSlot_ == i + 1;
      p.connected = saved.occupied && peripheralConnectionId_ != 0 &&
          peripheralAddress_.equalsIgnoreCase(saved.address);
      strncpy(p.label, saved.label, sizeof(p.label) - 1);
      strncpy(p.hostName, saved.hostName, sizeof(p.hostName) - 1);
      if (saved.occupied) strncpy(p.address, saved.address, sizeof(p.address) - 1);
      p.state = p.connected ? BluetoothProfileDisplayState::Connected
          : (operation_ == BluetoothProfileOperation::Pairing && p.selected)
              ? BluetoothProfileDisplayState::Pairing
          : (operation_ == BluetoothProfileOperation::Deleting && deleteSlot_ == i + 1)
              ? BluetoothProfileDisplayState::Deleting
          : (operation_ == BluetoothProfileOperation::Connecting && p.selected)
              ? BluetoothProfileDisplayState::Connecting
          : p.selected ? BluetoothProfileDisplayState::Selected
          : p.occupied ? BluetoothProfileDisplayState::Remembered
                       : BluetoothProfileDisplayState::Empty;
    }
    return profilesLoaded_;
  }

private:
  enum class Pending : uint8_t
  {
    None = 0,
    DirectedAdvertising,
    PairingAdvertising,
    DeleteBond,
  };

  bool startPending()
  {
    const Pending operation = pending_;
    pending_ = Pending::None;
    if (operation == Pending::DirectedAdvertising)
    {
      if (!applySelectedSlotName() ||
          !ble_.advertising().setDirectedTarget(
              targetBond_.peerAddress.c_str(), targetBond_.peerAddressType) ||
          !ble_.advertising().start())
      {
        return fail("directed_advertising_failed");
      }
      reconnectDeadline_.start(millis(), ReconnectTimeoutMs);
      return true;
    }

    if (operation == Pending::PairingAdvertising)
    {
      const size_t index = selectedSlot_ - 1;
      if (slotUsed_[index] && !ble_.deleteBond(slots_[index]))
      {
        return fail("bond_delete_failed");
      }
      slotUsed_[index] = false;
      registry_.clear(selectedSlot_);
      if (!saveRegistry()) return fail("profile_storage_write_failed");
      ble_.advertising().clearDirectedTarget();
      if (!applySelectedSlotName() || !ble_.advertising().start())
      {
        return fail("pairing_advertising_failed");
      }
      return true;
    }
    if (operation == Pending::DeleteBond)
    {
      const size_t index = deleteSlot_ - 1;
      if (!ble_.deleteBond(slots_[index])) return fail("bond_delete_failed");
      slotUsed_[index] = false;
      registry_.clear(deleteSlot_);
      if (!saveRegistry()) return fail("profile_storage_write_failed");
      deleteSlot_ = 0;
      operation_ = BluetoothProfileOperation::None;
      status_ = BluetoothTargetStatus::Idle;
      if (reconnectSlot_ != 0)
      {
        const size_t reconnect = reconnectSlot_;
        reconnectSlot_ = 0;
        targetBond_ = slots_[reconnect - 1];
        status_ = BluetoothTargetStatus::Switching;
        operation_ = BluetoothProfileOperation::Connecting;
        pending_ = Pending::DirectedAdvertising;
        return startPending();
      }
      return true;
    }
    return true;
  }

  void handleConnected(const EspBleConnection &connection)
  {
    if (connection.localRole != EspBleRole::Peripheral)
    {
      return;
    }
    peripheralConnectionId_ = connection.id;
    peripheralAddress_ = connection.peerAddress;
    reconnectDeadline_.cancel();
    suppressReconnect_ = false;
    loadSlots();
    size_t existingSlot = MaxSlots;
    for (size_t i = 0; i < MaxSlots; ++i)
    {
      if (slotUsed_[i] && peripheralAddress_.equalsIgnoreCase(slots_[i].peerAddress))
      {
        existingSlot = i;
        break;
      }
    }
    if (status_ == BluetoothTargetStatus::Pairing && existingSlot < MaxSlots)
    {
      // Ordinary advertising is required for a new Host, but an already-bonded
      // Host from another slot may race to reconnect. Keep pairing the selected
      // slot instead of silently switching to that existing Host.
      pending_ = Pending::PairingAdvertising;
      if (!ble_.disconnect(connection.id))
      {
        pending_ = Pending::None;
        fail("disconnect_failed");
      }
      return;
    }
    if (existingSlot < MaxSlots && status_ != BluetoothTargetStatus::Pairing)
    {
      selectedSlot_ = existingSlot + 1;
    }
    else if (status_ == BluetoothTargetStatus::Pairing)
    {
      pairingConnectionId_ = connection.id;
    }
    if (status_ == BluetoothTargetStatus::Switching &&
        peripheralAddress_.equalsIgnoreCase(targetBond_.peerAddress))
    {
      status_ = BluetoothTargetStatus::Connected;
      operation_ = BluetoothProfileOperation::None;
    }
  }

  void handleDisconnected(const EspBleConnection &connection)
  {
    if (connection.id != peripheralConnectionId_)
    {
      return;
    }
    peripheralConnectionId_ = 0;
    peripheralAddress_ = "";
    reconnectDeadline_.cancel();
    if (resetAfterDisconnect_)
    {
      resetAfterDisconnect_ = false;
      forceStopDisconnect_ = false;
      completeBluetoothReset();
      return;
    }
    if (forceStopDisconnect_ || suppressReconnect_)
    {
      forceStopDisconnect_ = false;
      operation_ = BluetoothProfileOperation::None;
      pending_ = Pending::None;
      status_ = BluetoothTargetStatus::Idle;
      return;
    }
    if (pairingConnectionId_ == connection.id)
    {
      pairingConnectionId_ = 0;
    if (cancellingDisconnect_)
    {
      cancellingDisconnect_ = false;
      operation_ = BluetoothProfileOperation::None;
      status_ = BluetoothTargetStatus::Idle;
      return;
    }
    }
    if (pending_ != Pending::None)
    {
      startPending();
      return;
    }

    // An ordinary link loss should remain locked to the selected bond instead
    // of reopening undirected pairing to every nearby host.
    loadSlots();
    if (slotUsed_[selectedSlot_ - 1])
    {
      targetBond_ = slots_[selectedSlot_ - 1];
      status_ = BluetoothTargetStatus::Switching;
      operation_ = BluetoothProfileOperation::Connecting;
      pending_ = Pending::DirectedAdvertising;
      startPending();
    }
    else
    {
      ble_.advertising().clearDirectedTarget();
      if (!applySelectedSlotName() || !ble_.advertising().start())
      {
        status_ = BluetoothTargetStatus::Failed;
      }
    }
  }

  void handleSecurity(const EspBleSecurityChanged &event)
  {
    if (event.connection.localRole != EspBleRole::Peripheral)
    {
      return;
    }
    if (status_ == BluetoothTargetStatus::Idle && event.success && event.connection.bonded)
    {
      // Also adopt a first-time pairing started by the sketch's initial normal
      // advertising, before a hotkey has ever been used.
      slotsLoaded_ = false;
      loadSlots();
      for (size_t i = 0; i < MaxSlots; ++i)
      {
        if (slotUsed_[i] &&
            event.connection.peerAddress.equalsIgnoreCase(slots_[i].peerAddress))
        {
          selectedSlot_ = i + 1;
          status_ = BluetoothTargetStatus::Connected;
          requestHostName();
          break;
        }
      }
      return;
    }
    if (status_ != BluetoothTargetStatus::Pairing ||
        event.connection.id != pairingConnectionId_)
    {
      if (event.success && event.connection.bonded &&
          event.connection.id == peripheralConnectionId_)
        requestHostName();
      return;
    }
    if (event.success && event.connection.bonded)
    {
      const size_t index = selectedSlot_ - 1;
      slots_[index].peerAddress = event.connection.peerAddress;
      slots_[index].peerAddressType = event.connection.peerAddressType;
      slotUsed_[index] = true;
      if (!registry_.assign(selectedSlot_, event.connection.peerAddress.c_str(),
                            static_cast<uint8_t>(event.connection.peerAddressType)) ||
          !saveRegistry())
      {
        fail("profile_storage_write_failed");
        return;
      }
      operation_ = BluetoothProfileOperation::None;
      status_ = BluetoothTargetStatus::Connected;
      requestHostName();
    }
    else
    {
      fail("pairing_security_failed");
    }
  }

  void requestHostName()
  {
    if (peripheralConnectionId_ == 0 ||
        !peripheralAddress_.equalsIgnoreCase(registry_.profile(selectedSlot_).address))
      return;
    ble_.discoverCharacteristic(peripheralConnectionId_, "1800", "2a00", 5000);
  }

  void handleNameDiscovered(const EspBleGattResult &result)
  {
    if (!result.serviceUuid.equalsIgnoreCase("1800") ||
        !result.characteristicUuid.equalsIgnoreCase("2a00")) return;
    if (!result.success || result.connectionId != peripheralConnectionId_ ||
        !peripheralAddress_.equalsIgnoreCase(registry_.profile(selectedSlot_).address))
      return;
    ble_.readCharacteristic(result.connectionId, "1800", "2a00", 5000);
  }

  void handleNameRead(const EspBleGattResult &result)
  {
    if (!result.serviceUuid.equalsIgnoreCase("1800") ||
        !result.characteristicUuid.equalsIgnoreCase("2a00")) return;
    if (!result.success || result.connectionId != peripheralConnectionId_ ||
        !peripheralAddress_.equalsIgnoreCase(registry_.profile(selectedSlot_).address))
      return;
    BluetoothProfileRegistry before = registry_;
    if (!registry_.setHostName(selectedSlot_, result.value.c_str()) || !saveRegistry())
      registry_ = before;
  }

  void loadSlots()
  {
    if (slotsLoaded_)
    {
      return;
    }
    slotsLoaded_ = true;
    const BluetoothProfileRegistry beforeReconciliation = registry_;
    for (size_t i = 0; i < MaxSlots; ++i)
    {
      slotUsed_[i] = false;
    }
    EspBleBond bonds[MaxSlots];
    bool assigned[MaxSlots] = {};
    const size_t count = ble_.bondCount() < MaxSlots ? ble_.bondCount() : MaxSlots;
    for (size_t i = 0; i < count; ++i)
    {
      if (!ble_.bond(i, bonds[i])) continue;
      const int savedSlot = registry_.findAddress(bonds[i].peerAddress.c_str(),
          static_cast<uint8_t>(bonds[i].peerAddressType));
      if (savedSlot > 0)
      {
        slots_[savedSlot - 1] = bonds[i];
        slotUsed_[savedSlot - 1] = true;
        assigned[i] = true;
      }
    }
    bool changed = false;
    for (size_t slot = 0; slot < MaxSlots; ++slot)
    {
      if (registry_.profile(slot + 1).occupied && !slotUsed_[slot])
      {
        registry_.clear(slot + 1);
        changed = true;
      }
    }
    for (size_t i = 0; i < count; ++i)
    {
      if (assigned[i] || bonds[i].peerAddress.isEmpty()) continue;
      for (size_t slot = 0; slot < MaxSlots; ++slot)
      {
        if (!slotUsed_[slot])
        {
          slots_[slot] = bonds[i];
          slotUsed_[slot] = true;
          registry_.assign(slot + 1, bonds[i].peerAddress.c_str(),
                           static_cast<uint8_t>(bonds[i].peerAddressType));
          changed = true;
          break;
        }
      }
    }
    if (changed && !saveRegistry())
    {
      registry_ = beforeReconciliation;
      fail("profile_storage_write_failed");
    }
  }

  bool saveRegistry()
  {
    return profileStore_ == nullptr || profileStore_->save(registry_);
  }

  bool fail(const char *message)
  {
    setError(message);
    operation_ = BluetoothProfileOperation::None;
    pending_ = Pending::None;
    cancellingDisconnect_ = false;
    reconnectDeadline_.cancel();
    status_ = BluetoothTargetStatus::Failed;
    return false;
  }

  bool reject(const char *message)
  {
    setError(message);
    return false;
  }

  void setError(const char *message)
  {
    memset(error_, 0, sizeof(error_));
    if (message != nullptr) strncpy(error_, message, sizeof(error_) - 1);
  }

  void clearError() { error_[0] = '\0'; }

  EspBle &ble_;
  String baseDeviceName_;
  size_t selectedSlot_ = 1;
  BluetoothTargetStatus status_ = BluetoothTargetStatus::Idle;
  BluetoothProfileOperation operation_ = BluetoothProfileOperation::None;
  Pending pending_ = Pending::None;
  BluetoothProfileSettingsStore *profileStore_ = nullptr;
  BluetoothProfileRegistry registry_;
  EspBleBond targetBond_;
  EspBleBond slots_[MaxSlots];
  bool slotUsed_[MaxSlots] = {};
  bool slotsLoaded_ = false;
  bool profilesLoaded_ = false;
  bool cancellingDisconnect_ = false;
  bool forceStopDisconnect_ = false;
  bool resetAfterDisconnect_ = false;
  bool suppressReconnect_ = false;
  size_t deleteSlot_ = 0;
  size_t reconnectSlot_ = 0;
  uint32_t pairingStartedMs_ = 0;
  static constexpr uint32_t PairingTimeoutMs = 120000;
  static constexpr uint32_t ReconnectTimeoutMs = 30000;
  BluetoothOperationDeadline reconnectDeadline_;
  char error_[96] = {};
  EspBleConnectionId peripheralConnectionId_ = 0;
  EspBleConnectionId pairingConnectionId_ = 0;
  String peripheralAddress_;
  EspBleListenerId connectedListener_ = EspBleInvalidListenerId;
  EspBleListenerId disconnectedListener_ = EspBleInvalidListenerId;
  EspBleListenerId securityListener_ = EspBleInvalidListenerId;
  EspBleListenerId characteristicDiscoveredListener_ = EspBleInvalidListenerId;
  EspBleListenerId characteristicReadListener_ = EspBleInvalidListenerId;
};

// ---------------------------------------------------------------------------
// Input: BLE HID host (central)
// ---------------------------------------------------------------------------
//
// The sketch drives connection setup itself (scan -> connect -> pair ->
// hidHost().discover()), exactly as in EspBle's own KeyboardHost example;
// these adapters only consume the resulting HID reports. Nothing here needs a
// connection to exist yet, so they can be registered with the bridge up front.

// BLE keyboard input (consumer keys included). All connected HID keyboards
// are merged (key union across peers); a disconnect releases only that peer's
// keys. The bridge lock state is forwarded to every keyboard's LEDs.
//
// Keyboard state comes from onKeyboardState: one format-independent 256-bit
// usage snapshot per changed report — boot, report-protocol and NKRO alike,
// modifier-only changes included — which maps directly onto the key set, the
// same way EspUsbHostKeyboardInputAdapter reads its stack.
class EspBleHidHostKeyboardInputAdapter : public InputAdapter
{
public:
  // Keyboard connections tracked simultaneously; reports from further ones
  // are ignored.
  static constexpr size_t MaxKeyboards = 4;
  // Connections holding a consumer usage. One entry per connection, not per
  // usage: the HOGP consumer report carries a single usage at a time.
  static constexpr size_t MaxConsumerSources = 4;

  explicit EspBleHidHostKeyboardInputAdapter(EspBle &ble) : ble_(ble) {}

  ~EspBleHidHostKeyboardInputAdapter() override
  {
    ble_.hidHost().removeListener(stateListener_);
    ble_.hidHost().removeListener(consumerListener_);
    ble_.removeConnectionListener(disconnectListener_);
  }

  void update() override
  {
    if (!subscribed_)
    {
      subscribed_ = true;
      stateListener_ = ble_.hidHost().addKeyboardStateListener(
          [this](const EspBleHidKeyboardState &state) { handleKeyboardState(state); });
      consumerListener_ = ble_.hidHost().addConsumerControlListener(
          [this](const EspBleHidConsumerControlEvent &event) { handleConsumer(event); });
      disconnectListener_ = ble_.addDisconnectedListener(
          [this](const EspBleConnection &connection) { handleDisconnect(connection.id); });
    }

    keys_.clear();
    for (size_t i = 0; i < keyboardCount_; ++i)
    {
      for (size_t byte = 0; byte < EspBleHidKeyboardState::BitmapSize; ++byte)
      {
        uint8_t bits = keyboards_[i].bitmap[byte];
        while (bits != 0)
        {
          const int bit = __builtin_ctz(bits);
          bits = static_cast<uint8_t>(bits & (bits - 1));
          const uint16_t usage = static_cast<uint16_t>(byte * 8 + bit);
          // Usages 0x01-0x03 are boot rollover/POST error codes, not keys.
          if (usage >= 0x04)
          {
            keys_.press(keyboardKey(usage));
          }
        }
      }
    }
    for (size_t i = 0; i < consumerCount_; ++i)
    {
      keys_.press(consumerKey(consumer_[i].usage));
    }
    connected_ = keyboardCount_ > 0 || consumerCount_ > 0;
  }

  const KeySet &keys() const override { return keys_; }

  // A keyboard counts as present from its first report; false again when
  // every known peer disconnected. A keyboard that never reported has no
  // state to lose.
  bool connected() const override { return connected_; }

  // Forwards the bridge lock state to the LEDs of every tracked keyboard.
  // EspBle carries kana too, so unlike the USB Host side nothing is dropped.
  void setLockState(const LockState &state) override
  {
    for (size_t i = 0; i < keyboardCount_; ++i)
    {
      ble_.hidHost().setKeyboardLeds(
          keyboards_[i].connectionId, state.numLock, state.capsLock, state.scrollLock, false,
          state.kana);
    }
  }

private:
  struct Keyboard
  {
    EspBleConnectionId connectionId = 0;
    uint8_t reportId = 0;
    uint8_t bitmap[EspBleHidKeyboardState::BitmapSize] = {};
  };

  struct ConsumerEntry
  {
    EspBleConnectionId connectionId = 0;
    uint16_t usage = 0;
  };

  void handleKeyboardState(const EspBleHidKeyboardState &state)
  {
    size_t index = keyboardCount_;
    for (size_t i = 0; i < keyboardCount_; ++i)
    {
      if (keyboards_[i].connectionId == state.connectionId &&
          keyboards_[i].reportId == state.reportId)
      {
        index = i;
        break;
      }
    }
    if (index == keyboardCount_ && keyboardCount_ < MaxKeyboards)
    {
      keyboards_[keyboardCount_].connectionId = state.connectionId;
      keyboards_[keyboardCount_].reportId = state.reportId;
      ++keyboardCount_;
    }
    if (index < keyboardCount_)
    {
      memcpy(keyboards_[index].bitmap, state.bitmap, EspBleHidKeyboardState::BitmapSize);
    }
  }

  // The HOGP consumer report carries one 16-bit usage at a time, so the event
  // reports the connection's *new* usage and 0 means "nothing held". A release
  // therefore arrives as usage 0 rather than as the usage that was let go —
  // unlike EspUsbHost, whose consumer event names the released usage. So this
  // replaces the connection's entry instead of looking for a matching usage
  // to remove; matching on the usage would never find one and the key would
  // stay stuck.
  void handleConsumer(const EspBleHidConsumerControlEvent &event)
  {
    size_t index = consumerCount_;
    for (size_t i = 0; i < consumerCount_; ++i)
    {
      if (consumer_[i].connectionId == event.connectionId)
      {
        index = i;
        break;
      }
    }

    if (event.usage == 0)
    {
      if (index < consumerCount_)
      {
        consumer_[index] = consumer_[consumerCount_ - 1];
        --consumerCount_;
      }
      return;
    }

    if (index == consumerCount_)
    {
      if (consumerCount_ >= MaxConsumerSources)
      {
        return;
      }
      index = consumerCount_++;
      consumer_[index].connectionId = event.connectionId;
    }
    consumer_[index].usage = event.usage;
  }

  // Any connection, not only keyboards.
  void handleDisconnect(EspBleConnectionId connectionId)
  {
    for (size_t i = keyboardCount_; i > 0; --i)
    {
      if (keyboards_[i - 1].connectionId == connectionId)
      {
        keyboards_[i - 1] = keyboards_[keyboardCount_ - 1];
        --keyboardCount_;
      }
    }
    for (size_t i = consumerCount_; i > 0; --i)
    {
      if (consumer_[i - 1].connectionId == connectionId)
      {
        consumer_[i - 1] = consumer_[consumerCount_ - 1];
        --consumerCount_;
      }
    }
  }

  EspBle &ble_;
  bool subscribed_ = false;
  EspBleListenerId stateListener_ = EspBleInvalidListenerId;
  EspBleListenerId consumerListener_ = EspBleInvalidListenerId;
  EspBleListenerId disconnectListener_ = EspBleInvalidListenerId;
  KeySet keys_;
  bool connected_ = false;
  Keyboard keyboards_[MaxKeyboards];
  size_t keyboardCount_ = 0;
  ConsumerEntry consumer_[MaxConsumerSources];
  size_t consumerCount_ = 0;
};

// BLE mouse input. Buttons become MouseButton keys in keys(); movement and
// wheel accumulate as relative axis deltas. All connected mice are merged
// (buttons union, deltas summed); a disconnect releases that mouse's buttons
// only. HID over GATT mouse reports carry no AC Pan field.
class EspBleHidHostMouseInputAdapter : public InputAdapter
{
public:
  static constexpr size_t MaxMice = 4;

  explicit EspBleHidHostMouseInputAdapter(EspBle &ble) : ble_(ble) {}

  ~EspBleHidHostMouseInputAdapter() override
  {
    ble_.hidHost().removeListener(mouseListener_);
    ble_.removeConnectionListener(disconnectListener_);
  }

  void update() override
  {
    if (!subscribed_)
    {
      subscribed_ = true;
      mouseListener_ = ble_.hidHost().addMouseListener(
          [this](const EspBleHidMouseEvent &event) { handleMouse(event); });
      disconnectListener_ = ble_.addDisconnectedListener(
          [this](const EspBleConnection &connection) { handleDisconnect(connection.id); });
    }

    keys_.clear();
    for (size_t i = 0; i < mouseCount_; ++i)
    {
      for (size_t bit = 0; bit < 8; ++bit)
      {
        if ((mice_[i].buttons & (1u << bit)) != 0)
        {
          keys_.press(mouseButtonKey(static_cast<uint16_t>(bit + 1)));
        }
      }
    }
    connected_ = mouseCount_ > 0;
  }

  const KeySet &keys() const override { return keys_; }
  bool connected() const override { return connected_; }

  int32_t takeAxisDelta(Axis axis) override
  {
    const size_t index = static_cast<size_t>(axis);
    const int32_t delta = axisDeltas_[index];
    axisDeltas_[index] = 0;
    return delta;
  }

private:
  struct Mouse
  {
    EspBleConnectionId connectionId = 0;
    uint8_t buttons = 0;
  };

  void handleMouse(const EspBleHidMouseEvent &event)
  {
    size_t index = mouseCount_;
    for (size_t i = 0; i < mouseCount_; ++i)
    {
      if (mice_[i].connectionId == event.connectionId)
      {
        index = i;
        break;
      }
    }
    if (index == mouseCount_ && mouseCount_ < MaxMice)
    {
      mice_[mouseCount_].connectionId = event.connectionId;
      mice_[mouseCount_].buttons = 0;
      ++mouseCount_;
    }
    if (index < mouseCount_)
    {
      mice_[index].buttons = event.buttons;
      axisDeltas_[static_cast<size_t>(Axis::X)] += event.x;
      axisDeltas_[static_cast<size_t>(Axis::Y)] += event.y;
      axisDeltas_[static_cast<size_t>(Axis::Wheel)] += event.wheel;
    }
  }

  // Any connection, not only mice.
  void handleDisconnect(EspBleConnectionId connectionId)
  {
    for (size_t i = 0; i < mouseCount_; ++i)
    {
      if (mice_[i].connectionId == connectionId)
      {
        mice_[i] = mice_[mouseCount_ - 1];
        --mouseCount_;
        break;
      }
    }
  }

  EspBle &ble_;
  bool subscribed_ = false;
  EspBleListenerId mouseListener_ = EspBleInvalidListenerId;
  EspBleListenerId disconnectListener_ = EspBleInvalidListenerId;
  KeySet keys_;
  bool connected_ = false;
  int32_t axisDeltas_[kAxisCount] = {};
  Mouse mice_[MaxMice];
  size_t mouseCount_ = 0;
};

// BLE gamepad input. Buttons and the hat/D-pad map to keys through the core's
// GamepadKeyMap, so mapping and hat decoding are identical to
// EspUsbHostGamepadInputAdapter. Only mapped controls produce output. All
// connected pads are merged; a disconnect drops only that pad's controls.
// Analog sticks and triggers are not mapped in this version.
class EspBleHidHostGamepadInputAdapter : public InputAdapter
{
public:
  static constexpr size_t MaxGamepads = 4;
  static constexpr size_t MaxButtons = GamepadKeyMap::MaxButtons;

  explicit EspBleHidHostGamepadInputAdapter(EspBle &ble) : ble_(ble) {}

  ~EspBleHidHostGamepadInputAdapter() override
  {
    ble_.hidHost().removeListener(gamepadListener_);
    ble_.removeConnectionListener(disconnectListener_);
  }

  bool mapButton(uint8_t number, Key key) { return map_.mapButton(number, key); }
  void mapHat(Key up, Key down, Key left, Key right) { map_.mapHat(up, down, left, right); }

  void update() override
  {
    if (!subscribed_)
    {
      subscribed_ = true;
      gamepadListener_ = ble_.hidHost().addGamepadListener(
          [this](const EspBleHidGamepadEvent &event) { handleGamepad(event); });
      disconnectListener_ = ble_.addDisconnectedListener(
          [this](const EspBleConnection &connection) { handleDisconnect(connection.id); });
    }

    keys_.clear();
    for (size_t i = 0; i < padCount_; ++i)
    {
      map_.addKeys(keys_, pads_[i].buttons, pads_[i].hat);
    }
    connected_ = padCount_ > 0;
  }

  const KeySet &keys() const override { return keys_; }
  bool connected() const override { return connected_; }

private:
  struct Pad
  {
    EspBleConnectionId connectionId = 0;
    uint8_t reportId = 0;
    uint32_t buttons = 0;
    int8_t hat = GamepadKeyMap::HatCentered;
  };

  void handleGamepad(const EspBleHidGamepadEvent &event)
  {
    uint32_t buttons = 0;
    int8_t hat = GamepadKeyMap::HatCentered;
    GamepadKeyMap::decodeFields(event.fields, event.fieldCount, buttons, hat);

    size_t index = padCount_;
    for (size_t i = 0; i < padCount_; ++i)
    {
      if (pads_[i].connectionId == event.connectionId && pads_[i].reportId == event.reportId)
      {
        index = i;
        break;
      }
    }
    if (index == padCount_ && padCount_ < MaxGamepads)
    {
      pads_[padCount_].connectionId = event.connectionId;
      pads_[padCount_].reportId = event.reportId;
      ++padCount_;
    }
    if (index < padCount_)
    {
      pads_[index].buttons = buttons;
      pads_[index].hat = hat;
    }
  }

  // Any connection, not only gamepads.
  void handleDisconnect(EspBleConnectionId connectionId)
  {
    for (size_t i = padCount_; i > 0; --i)
    {
      if (pads_[i - 1].connectionId == connectionId)
      {
        pads_[i - 1] = pads_[padCount_ - 1];
        --padCount_;
      }
    }
  }

  EspBle &ble_;
  bool subscribed_ = false;
  EspBleListenerId gamepadListener_ = EspBleInvalidListenerId;
  EspBleListenerId disconnectListener_ = EspBleInvalidListenerId;
  KeySet keys_;
  bool connected_ = false;
  GamepadKeyMap map_;
  Pad pads_[MaxGamepads];
  size_t padCount_ = 0;
};

} // namespace esp32keybridge
