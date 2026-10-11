#pragma once

#include "ESP32KeyBridgeBluetooth.h"
#include "ESP32KeyBridgeMacroSettings.h"
#include "ESP32KeyBridgeSettings.h"

#include <stdint.h>

namespace esp32keybridge
{

enum class SettingsResetScope : uint8_t
{
  BluetoothOnly = 0,
  Everything,
};

enum class SettingsResetErrorCode : uint8_t
{
  None = 0,
  AlreadyInProgress,
  BluetoothFailed,
  LayoutStorageFailed,
  MacroStorageFailed,
};

struct SettingsResetError
{
  SettingsResetErrorCode code = SettingsResetErrorCode::None;
  const char *message = "";
};

class SettingsResetCoordinator
{
public:
  explicit SettingsResetCoordinator(BluetoothTargetController &bluetooth)
      : bluetooth_(bluetooth) {}

  void enableLayoutReset(KeyboardLayoutSettingsStore &store) { layoutStore_ = &store; }
  void enableMacroReset(KeyboardMacroSettingsStore &store) { macroStore_ = &store; }
  bool reset(SettingsResetScope scope, SettingsResetError *error = nullptr);
  // Complete settings removal only once the Bluetooth bond erase finishes.
  void update();
  bool inProgress() const { return inProgress_; }
  bool completed() const { return inProgress_ && completed_; }
  bool succeeded() const { return completed() && succeeded_; }
  void finish() { inProgress_ = false; completed_ = false; succeeded_ = false; }

private:
  bool fail(SettingsResetErrorCode code, const char *message, SettingsResetError *error);

  BluetoothTargetController &bluetooth_;
  KeyboardLayoutSettingsStore *layoutStore_ = nullptr;
  KeyboardMacroSettingsStore *macroStore_ = nullptr;
  bool inProgress_ = false;
  bool completed_ = false;
  bool succeeded_ = false;
  SettingsResetScope pendingScope_ = SettingsResetScope::BluetoothOnly;
};

} // namespace esp32keybridge
