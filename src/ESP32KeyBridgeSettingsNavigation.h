#pragma once

#include <ESP32KeyBridge.h>

#include <stddef.h>
#include <stdint.h>

namespace esp32keybridge
{

enum class SettingsNavigationAction : uint8_t
{
  None = 0,
  Next,
  Previous,
  Up,
  Down,
  Left,
  Right,
  Activate,
  Cancel,
  Home,
  End,
};

inline const char *settingsNavigationActionName(SettingsNavigationAction action)
{
  switch (action)
  {
  case SettingsNavigationAction::Next:
    return "next";
  case SettingsNavigationAction::Previous:
    return "previous";
  case SettingsNavigationAction::Up:
    return "up";
  case SettingsNavigationAction::Down:
    return "down";
  case SettingsNavigationAction::Left:
    return "left";
  case SettingsNavigationAction::Right:
    return "right";
  case SettingsNavigationAction::Activate:
    return "activate";
  case SettingsNavigationAction::Cancel:
    return "cancel";
  case SettingsNavigationAction::Home:
    return "home";
  case SettingsNavigationAction::End:
    return "end";
  case SettingsNavigationAction::None:
  default:
    return "none";
  }
}

struct SettingsNavigationEvent
{
  uint32_t sequence = 0;
  SettingsNavigationAction action = SettingsNavigationAction::None;
};

// Transport-neutral boundary between a keyboard decorator and the settings
// website. The website holds capture only while its browser page is polling.
class SettingsNavigationSource
{
public:
  virtual ~SettingsNavigationSource() = default;
  virtual void setNavigationCaptureEnabled(bool enabled) = 0;
  virtual bool navigationCaptureEnabled() const = 0;
  virtual bool popNavigationEvent(SettingsNavigationEvent &event) = 0;
  virtual uint32_t navigationOverflowCount() const = 0;
};

// Decorates a keyboard-shaped input. During capture, key presses become a
// small browser-navigation command stream and no keys reach the bridge. Keys
// that are still held when capture ends stay suppressed until released, so a
// stale key cannot leak to the BLE host when the browser closes.
class SettingsNavigationInputAdapter : public InputAdapter,
                                       public SettingsNavigationSource
{
public:
  static constexpr size_t EventCapacity = 16;

  explicit SettingsNavigationInputAdapter(InputAdapter &input) : input_(input) {}

  void update() override
  {
    input_.update();
    const KeySet &source = input_.keys();

    // Do not make a keyboard disappear from a downstream decorator midway
    // through a chord. Capture starts only after an all-released snapshot.
    if (captureRequested_ && !captureEnabled_ && source.count() == 0)
    {
      captureEnabled_ = true;
      previous_.clear();
      suppressed_.clear();
    }

    if (captureEnabled_)
    {
      for (size_t i = 0; i < source.count(); ++i)
      {
        const Key key = source.at(i);
        if (!previous_.contains(key))
        {
          enqueue(actionForPress(key, source));
        }
      }
      suppressed_ = source;
      keys_.clear();
    }
    else
    {
      KeySet stillSuppressed;
      keys_.clear();
      for (size_t i = 0; i < source.count(); ++i)
      {
        const Key key = source.at(i);
        if (suppressed_.contains(key))
        {
          stillSuppressed.press(key);
        }
        else
        {
          keys_.press(key);
        }
      }
      suppressed_ = stillSuppressed;
    }

    previous_ = source;
  }

  const KeySet &keys() const override { return keys_; }
  bool connected() const override { return input_.connected(); }
  void setLockState(const LockState &state) override { input_.setLockState(state); }
  int32_t takeAxisDelta(Axis axis) override { return input_.takeAxisDelta(axis); }

  void setNavigationCaptureEnabled(bool enabled) override
  {
    captureRequested_ = enabled;
    if (!enabled)
    {
      captureEnabled_ = false;
      eventHead_ = 0;
      eventCount_ = 0;
    }
  }

  bool navigationCaptureEnabled() const override { return captureEnabled_; }

  bool popNavigationEvent(SettingsNavigationEvent &event) override
  {
    if (eventCount_ == 0)
    {
      return false;
    }
    event = events_[eventHead_];
    eventHead_ = (eventHead_ + 1) % EventCapacity;
    --eventCount_;
    return true;
  }

  uint32_t navigationOverflowCount() const override { return overflowCount_; }

private:
  static bool shiftDown(const KeySet &source)
  {
    return source.contains(keyboardKey(KeyboardUsage::LeftShift)) ||
           source.contains(keyboardKey(KeyboardUsage::RightShift));
  }

  static SettingsNavigationAction actionForPress(Key key, const KeySet &source)
  {
    if (key.kind != KeyKind::Keyboard)
    {
      return SettingsNavigationAction::None;
    }
    switch (static_cast<KeyboardUsage>(key.code))
    {
    case KeyboardUsage::Tab:
      return shiftDown(source) ? SettingsNavigationAction::Previous
                               : SettingsNavigationAction::Next;
    case KeyboardUsage::Up:
      return SettingsNavigationAction::Up;
    case KeyboardUsage::Down:
      return SettingsNavigationAction::Down;
    case KeyboardUsage::Left:
      return SettingsNavigationAction::Left;
    case KeyboardUsage::Right:
      return SettingsNavigationAction::Right;
    case KeyboardUsage::Enter:
    case KeyboardUsage::KeypadEnter:
    case KeyboardUsage::Space:
      return SettingsNavigationAction::Activate;
    case KeyboardUsage::Escape:
      return SettingsNavigationAction::Cancel;
    case KeyboardUsage::Home:
      return SettingsNavigationAction::Home;
    case KeyboardUsage::End:
      return SettingsNavigationAction::End;
    default:
      return SettingsNavigationAction::None;
    }
  }

  void enqueue(SettingsNavigationAction action)
  {
    if (action == SettingsNavigationAction::None)
    {
      return;
    }
    if (eventCount_ == EventCapacity)
    {
      eventHead_ = (eventHead_ + 1) % EventCapacity;
      --eventCount_;
      ++overflowCount_;
    }
    ++sequence_;
    if (sequence_ == 0)
    {
      ++sequence_;
    }
    const size_t tail = (eventHead_ + eventCount_) % EventCapacity;
    events_[tail].sequence = sequence_;
    events_[tail].action = action;
    ++eventCount_;
  }

  InputAdapter &input_;
  KeySet keys_;
  KeySet previous_;
  KeySet suppressed_;
  SettingsNavigationEvent events_[EventCapacity] = {};
  size_t eventHead_ = 0;
  size_t eventCount_ = 0;
  uint32_t sequence_ = 0;
  uint32_t overflowCount_ = 0;
  bool captureRequested_ = false;
  bool captureEnabled_ = false;
};

} // namespace esp32keybridge
