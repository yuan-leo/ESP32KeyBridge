#pragma once

#include <ESP32KeyBridge.h>

#include <stddef.h>
#include <stdint.h>

namespace esp32keybridge
{

// Builds the advertised name for a one-based target slot (for example,
// "KeyBridge Keyboard 2"). The destination is cleared on invalid input or
// insufficient capacity.
inline bool formatBluetoothSlotName(const char *baseName, size_t slot,
                                    char *destination, size_t capacity)
{
  if (destination == nullptr || capacity == 0)
  {
    return false;
  }
  destination[0] = '\0';
  if (baseName == nullptr || slot == 0)
  {
    return false;
  }

  size_t baseLength = 0;
  while (baseName[baseLength] != '\0')
  {
    ++baseLength;
  }
  char digits[3 * sizeof(size_t)] = {};
  size_t digitCount = 0;
  for (size_t value = slot; value != 0; value /= 10)
  {
    digits[digitCount++] = static_cast<char>('0' + value % 10);
  }
  if (baseLength + 1 + digitCount + 1 > capacity)
  {
    return false;
  }
  for (size_t i = 0; i < baseLength; ++i)
  {
    destination[i] = baseName[i];
  }
  destination[baseLength] = ' ';
  for (size_t i = 0; i < digitCount; ++i)
  {
    destination[baseLength + 1 + i] = digits[digitCount - 1 - i];
  }
  destination[baseLength + 1 + digitCount] = '\0';
  return true;
}

// State exposed by a Bluetooth output target while a hotkey request is in
// progress.  Transport implementations update this asynchronously from their
// normal update/event path; the hotkey adapter uses it only for LED feedback.
enum class BluetoothTargetStatus : uint8_t
{
  Idle = 0,
  Switching,
  Pairing,
  Connected,
  Failed,
};

// Transport-neutral boundary used by BluetoothHotkeyInputAdapter.  Slot
// numbers are one-based, matching the top-row number printed on the keyboard.
class BluetoothTargetController
{
public:
  virtual ~BluetoothTargetController() = default;
  virtual void update() {}
  virtual size_t slotCount() const = 0;
  virtual bool selectSlot(size_t slot) = 0;
  virtual bool pairSlot(size_t slot) = 0;
  virtual size_t selectedSlot() const = 0;
  virtual BluetoothTargetStatus status() const = 0;
  virtual uint32_t nowMilliseconds() const = 0;
};

// Decorates any keyboard-shaped InputAdapter with the Bluetooth target chord:
// hold Scroll Lock and press a top-row number.  1..slotCount() select a stored
// peer; 0 puts the currently selected slot in pairing mode.  The control keys
// are consumed and never reach the bridge.  A Scroll Lock tap with no selector
// is preserved as a one-update pulse, so the key retains its normal function.
//
// Lock reports are delegated to the wrapped input.  During an operation only
// the Scroll Lock LED is temporarily overridden; the latest effective lock
// state is cached and restored when indication ends.  Keyboards without that
// LED simply ignore the report and target switching still works.
class BluetoothHotkeyInputAdapter : public InputAdapter
{
public:
  explicit BluetoothHotkeyInputAdapter(
      InputAdapter &input,
      BluetoothTargetController &target,
      Key modifier = keyboardKey(KeyboardUsage::ScrollLock))
      : input_(input), target_(target), modifier_(modifier)
  {
  }

  void update() override
  {
    input_.update();
    target_.update();
    refreshIndication();

    const KeySet &source = input_.keys();
    const bool modifierDown = source.contains(modifier_);
    const size_t slots = target_.slotCount() > 9 ? 9 : target_.slotCount();

    int pressedSelector = -1;
    bool preheldSelector = false;
    for (uint8_t digit = 0; digit <= 9; ++digit)
    {
      const Key key = digitKey(digit);
      const bool down = source.contains(key);
      const bool supported = digit == 0 || digit <= slots;
      if (modifierDown && down && !previousDigits_[digit] && supported &&
          pressedSelector < 0)
      {
        pressedSelector = digit;
      }
      if (modifierDown && !modifierWasDown_ && down && previousDigits_[digit] && supported)
      {
        preheldSelector = true;
      }
      previousDigits_[digit] = down;
    }

    if (preheldSelector)
    {
      bypassChord_ = true;
    }

    if (modifierDown && !commandHandled_ && !bypassChord_)
    {
      // Scroll Lock remains pending while it is the only key held.  Any key
      // other than a supported selector makes this an ordinary chord.
      for (size_t i = 0; i < source.count(); ++i)
      {
        const Key key = source.at(i);
        const int digit = topRowDigit(key);
        if (key != modifier_ &&
            !(digit >= 0 && (digit == 0 || static_cast<size_t>(digit) <= slots)))
        {
          bypassChord_ = true;
          break;
        }
      }
    }

    if (pressedSelector >= 0 && !bypassChord_)
    {
      commandHandled_ = true;
      const bool accepted = pressedSelector == 0
          ? target_.pairSlot(target_.selectedSlot())
          : target_.selectSlot(static_cast<size_t>(pressedSelector));
      if (!accepted)
      {
        localFailure_ = true;
        localFailureStarted_ = target_.nowMilliseconds();
      }
      refreshIndication(true);
    }

    keys_.clear();
    for (size_t i = 0; i < source.count(); ++i)
    {
      const Key key = source.at(i);
      const int digit = topRowDigit(key);
      const bool controlKey = key == modifier_ ||
          (digit >= 0 && (digit == 0 || static_cast<size_t>(digit) <= slots));
      if ((commandHandled_ || (!bypassChord_ && modifierDown)) && controlKey)
      {
        continue;
      }
      keys_.press(key);
    }

    // The modifier was withheld to see whether a selector followed.  If it
    // did not, emit a normal tap on release.
    if (modifierWasDown_ && !modifierDown)
    {
      if (!commandHandled_ && !bypassChord_)
      {
        keys_.press(modifier_);
        tapPulse_ = true;
      }
      commandHandled_ = false;
      bypassChord_ = false;
    }
    else if (tapPulse_)
    {
      tapPulse_ = false;
    }
    modifierWasDown_ = modifierDown;
  }

  const KeySet &keys() const override { return keys_; }
  bool connected() const override { return input_.connected(); }

  void setLockState(const LockState &state) override
  {
    lockState_ = state;
    haveLockState_ = true;
    applyIndicatedLockState();
  }

  int32_t takeAxisDelta(Axis axis) override { return input_.takeAxisDelta(axis); }

private:
  static Key digitKey(uint8_t digit)
  {
    return keyboardKey(digit == 0 ? static_cast<uint16_t>(KeyboardUsage::Digit0)
                                  : static_cast<uint16_t>(KeyboardUsage::Digit1) + digit - 1);
  }

  static int topRowDigit(Key key)
  {
    if (key.kind != KeyKind::Keyboard)
    {
      return -1;
    }
    if (key.code == static_cast<uint16_t>(KeyboardUsage::Digit0))
    {
      return 0;
    }
    const uint16_t first = static_cast<uint16_t>(KeyboardUsage::Digit1);
    const uint16_t last = static_cast<uint16_t>(KeyboardUsage::Digit9);
    return key.code >= first && key.code <= last ? static_cast<int>(key.code - first + 1) : -1;
  }

  void refreshIndication(bool force = false)
  {
    const BluetoothTargetStatus next =
        localFailure_ ? BluetoothTargetStatus::Failed : target_.status();
    if (force || next != indicatedStatus_)
    {
      indicatedStatus_ = next;
      indicationStarted_ = target_.nowMilliseconds();
      applyIndicatedLockState(true);
    }

    if (localFailure_ && elapsed(target_.nowMilliseconds(), localFailureStarted_) >= 700)
    {
      localFailure_ = false;
      refreshIndication(true);
      return;
    }
    applyIndicatedLockState();
  }

  static uint32_t elapsed(uint32_t now, uint32_t then) { return now - then; }

  bool indicatedScrollLock() const
  {
    const uint32_t age = elapsed(target_.nowMilliseconds(), indicationStarted_);
    switch (indicatedStatus_)
    {
      case BluetoothTargetStatus::Switching:
        return ((age / 125) & 1u) == 0 ? !lockState_.scrollLock : lockState_.scrollLock;
      case BluetoothTargetStatus::Pairing:
        return ((age / 250) & 1u) == 0 ? !lockState_.scrollLock : lockState_.scrollLock;
      case BluetoothTargetStatus::Connected:
        return age < 400 ? !lockState_.scrollLock : lockState_.scrollLock;
      case BluetoothTargetStatus::Failed:
        return age < 600 && ((age / 100) & 1u) == 0
            ? !lockState_.scrollLock
            : lockState_.scrollLock;
      case BluetoothTargetStatus::Idle:
      default:
        return lockState_.scrollLock;
    }
  }

  void applyIndicatedLockState(bool force = false)
  {
    if (!haveLockState_)
    {
      return;
    }
    LockState shown = lockState_;
    shown.scrollLock = indicatedScrollLock();
    if (force || !haveAppliedLockState_ || shown != appliedLockState_)
    {
      input_.setLockState(shown);
      appliedLockState_ = shown;
      haveAppliedLockState_ = true;
    }
  }

  InputAdapter &input_;
  BluetoothTargetController &target_;
  Key modifier_;
  KeySet keys_;
  bool modifierWasDown_ = false;
  bool previousDigits_[10] = {};
  bool commandHandled_ = false;
  bool bypassChord_ = false;
  bool tapPulse_ = false;
  LockState lockState_;
  LockState appliedLockState_;
  bool haveLockState_ = false;
  bool haveAppliedLockState_ = false;
  BluetoothTargetStatus indicatedStatus_ = BluetoothTargetStatus::Idle;
  uint32_t indicationStarted_ = 0;
  bool localFailure_ = false;
  uint32_t localFailureStarted_ = 0;
};

} // namespace esp32keybridge
