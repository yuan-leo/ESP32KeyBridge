#include "ESP32KeyBridgeReset.h"

namespace esp32keybridge
{

bool SettingsResetCoordinator::fail(SettingsResetErrorCode code, const char *message,
                                    SettingsResetError *error)
{
  inProgress_ = false;
  if (error != nullptr)
  {
    error->code = code;
    error->message = message;
  }
  return false;
}

bool SettingsResetCoordinator::reset(SettingsResetScope scope, SettingsResetError *error)
{
  if (error != nullptr) *error = SettingsResetError();
  if (inProgress_)
  {
    if (error != nullptr)
    {
      error->code = SettingsResetErrorCode::AlreadyInProgress;
      error->message = "reset_already_in_progress";
    }
    return false;
  }
  inProgress_ = true;

  if (!bluetooth_.resetBluetooth())
    return fail(SettingsResetErrorCode::BluetoothFailed, "bluetooth_reset_failed", error);
  if (scope == SettingsResetScope::BluetoothOnly) return true;

  if (layoutStore_ == nullptr || !layoutStore_->clear())
    return fail(SettingsResetErrorCode::LayoutStorageFailed, "layout_storage_reset_failed", error);
  if (macroStore_ == nullptr || !macroStore_->clear())
    return fail(SettingsResetErrorCode::MacroStorageFailed, "macro_storage_reset_failed", error);
  return true;
}

} // namespace esp32keybridge
