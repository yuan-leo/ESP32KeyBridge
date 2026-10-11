#include "ESP32KeyBridgeReset.h"

namespace esp32keybridge
{

bool SettingsResetCoordinator::fail(SettingsResetErrorCode code, const char *message,
                                    SettingsResetError *error)
{
  inProgress_ = false;
  completed_ = false;
  succeeded_ = false;
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
  completed_ = false;
  succeeded_ = false;
  pendingScope_ = scope;

  if (!bluetooth_.resetBluetooth())
    return fail(SettingsResetErrorCode::BluetoothFailed, "bluetooth_reset_failed", error);
  update();
  if (completed_ && !succeeded_)
  {
    return fail(SettingsResetErrorCode::BluetoothFailed,
                "reset_failed", error);
  }
  return true;
}

void SettingsResetCoordinator::update()
{
  if (!inProgress_ || completed_ || !bluetooth_.resetComplete()) return;
  completed_ = true;
  if (!bluetooth_.resetSucceeded()) return;
  if (pendingScope_ == SettingsResetScope::Everything)
  {
    if (layoutStore_ == nullptr || !layoutStore_->clear()) return;
    if (macroStore_ == nullptr || !macroStore_->clear()) return;
  }
  succeeded_ = true;
}

} // namespace esp32keybridge
