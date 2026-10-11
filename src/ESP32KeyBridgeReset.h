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
  bool inProgress() const { return inProgress_; }
  void finish() { inProgress_ = false; }

private:
  bool fail(SettingsResetErrorCode code, const char *message, SettingsResetError *error);

  BluetoothTargetController &bluetooth_;
  KeyboardLayoutSettingsStore *layoutStore_ = nullptr;
  KeyboardMacroSettingsStore *macroStore_ = nullptr;
  bool inProgress_ = false;
};

} // namespace esp32keybridge
