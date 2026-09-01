#pragma once

#include <ESP32KeyBridge.h>
#include <EspBleClassic.h>

#include <string.h>

// Bluetooth Classic HID adapters backed by EspBle 1.3.0 or newer.
//
// These adapters are only useful on the original ESP32. It is the only ESP32
// SoC with Bluetooth BR/EDR; ESP32-S2/S3/C/P/H chips cannot run Classic HID.
// Use ESP32KeyBridgeEspBle.h for HID over GATT on those chips.
//
// The sketch owns EspBleClassic and calls bluetooth.begin() and
// bluetooth.update(). Construct an output adapter before begin(), because its
// keyboard + consumer + mouse Report Descriptor is registered at that point.
// For input, the sketch also starts hidHost() and connects to an address; the
// adapters only consume the decoded reports.
//
// EspBleClassic dispatches callbacks from update(), so call bluetooth.update()
// and bridge.update() from the same context. Classic HID callbacks are
// single-slot: an input adapter owns its corresponding onKeyboardState() or
// onMouse() callback, and the output adapter owns onOutputReport().

namespace esp32keybridge
{

// Composite Classic HID Device: keyboard + consumer control + mouse. The HID
// report payloads are the same as EspBleHidOutputAdapter. Classic differs in
// transport and discovery, and also caps the composed SDP descriptor plus its
// identity strings at 214 bytes. These three profiles fit with the default
// short identity strings; begin() reports an error if custom strings do not.
class EspBleClassicHidOutputAdapter : public OutputAdapter
{
public:
  explicit EspBleClassicHidOutputAdapter(
      EspBleClassic &bluetooth,
      const EspBleClassicHidProfileConfig &config = EspBleClassicHidProfileConfig())
      : bluetooth_(bluetooth)
  {
    bluetooth_.hidKeyboard().configure(config);
    bluetooth_.hidConsumerControl().configure(config);
    bluetooth_.hidMouse().configure(config);
    bluetooth_.hidKeyboard().onOutputReport(
        [this](const EspBleClassicHidKeyboardLeds &) { ledSeen_ = true; });
  }

  ~EspBleClassicHidOutputAdapter() override
  {
    bluetooth_.hidKeyboard().onOutputReport({});
  }

  void write(const KeySet &keys) override
  {
    writeKeyboard(keys);
    writeConsumer(keys);
    writeMouse(keys);
  }

  bool connected() const override { return bluetooth_.hidKeyboard().ready(); }

  bool reportsLockState() const override { return true; }
  bool getLockState(LockState &out) const override
  {
    if (!ledSeen_)
    {
      return false;
    }
    const EspBleClassicHidKeyboardLeds report = bluetooth_.hidKeyboard().ledState();
    out.numLock = report.numLock;
    out.capsLock = report.capsLock;
    out.scrollLock = report.scrollLock;
    out.kana = report.kana;
    return true;
  }

  void writeAxisDelta(Axis axis, int32_t delta) override
  {
    if (axis != Axis::Pan)
    {
      pendingAxis_[static_cast<size_t>(axis)] += delta;
    }
  }

private:
  void writeKeyboard(const KeySet &keys)
  {
    const HidKeyboardReport report = buildHidKeyboardReport(keys);
    EspBleHidKeyboardInputReport classicReport;
    classicReport.modifiers = report.modifiers;
    for (size_t i = 0; i < HidKeyboardReport::MaxKeys; ++i)
    {
      classicReport.keys[i] = i < report.keyCount ? report.keys[i] : 0;
    }

    if (!keyboardPending_ &&
        memcmp(&classicReport, &lastKeyboardReport_, sizeof(classicReport)) == 0)
    {
      return;
    }
    lastKeyboardReport_ = classicReport;
    keyboardPending_ = !bluetooth_.hidKeyboard().sendReport(classicReport);
  }

  void writeConsumer(const KeySet &keys)
  {
    const HidConsumerReport report = buildHidConsumerReport(keys);
    if (!consumerPending_ && report.usage == lastConsumerUsage_)
    {
      return;
    }
    lastConsumerUsage_ = report.usage;
    consumerPending_ = !bluetooth_.hidConsumerControl().sendUsage(report.usage);
  }

  void writeMouse(const KeySet &keys)
  {
    HidMouseReport report = buildHidMouseReport(keys);
    for (size_t axis = 0; axis < kAxisCount; ++axis)
    {
      pendingAxis_[axis] = report.applyAxisDelta(static_cast<Axis>(axis), pendingAxis_[axis]);
    }
    const bool moved = report.x != 0 || report.y != 0 || report.wheel != 0;
    if (!moved && report.buttons == lastMouseButtons_)
    {
      return;
    }

    EspBleHidMouseReport classicReport;
    classicReport.buttons = report.buttons;
    classicReport.x = report.x;
    classicReport.y = report.y;
    classicReport.wheel = report.wheel;
    if (bluetooth_.hidMouse().sendReport(classicReport))
    {
      lastMouseButtons_ = report.buttons;
    }
    else
    {
      pendingAxis_[static_cast<size_t>(Axis::X)] += report.x;
      pendingAxis_[static_cast<size_t>(Axis::Y)] += report.y;
      pendingAxis_[static_cast<size_t>(Axis::Wheel)] += report.wheel;
    }
  }

  EspBleClassic &bluetooth_;
  EspBleHidKeyboardInputReport lastKeyboardReport_;
  bool keyboardPending_ = false;
  uint16_t lastConsumerUsage_ = 0;
  bool consumerPending_ = false;
  uint8_t lastMouseButtons_ = 0;
  int32_t pendingAxis_[kAxisCount] = {};
  bool ledSeen_ = false;
};

// Classic HID Host keyboard input. EspBleClassic supports one HID Host link at
// a time. Its decoded snapshot contains a 256-bit usage bitmap plus the eight
// modifier bits; consumer-control Host decoding is not available yet.
class EspBleClassicHidHostKeyboardInputAdapter : public InputAdapter
{
public:
  explicit EspBleClassicHidHostKeyboardInputAdapter(EspBleClassic &bluetooth)
      : bluetooth_(bluetooth) {}

  ~EspBleClassicHidHostKeyboardInputAdapter() override
  {
    if (subscribed_)
    {
      bluetooth_.hidHost().onKeyboardState({});
    }
  }

  void update() override
  {
    subscribe();
    keys_.clear();
    if (!bluetooth_.hidHost().connected())
    {
      haveState_ = false;
      memset(bitmap_, 0, sizeof(bitmap_));
      modifiers_ = 0;
      return;
    }

    for (uint8_t bit = 0; bit < 8; ++bit)
    {
      if ((modifiers_ & static_cast<uint8_t>(1u << bit)) != 0)
      {
        keys_.press(keyboardKey(static_cast<uint16_t>(0xe0 + bit)));
      }
    }
    for (size_t byte = 0; byte < EspBleClassicHidKeyboardState::BitmapSize; ++byte)
    {
      uint8_t bits = bitmap_[byte];
      while (bits != 0)
      {
        const int bit = __builtin_ctz(bits);
        bits = static_cast<uint8_t>(bits & (bits - 1));
        const uint16_t usage = static_cast<uint16_t>(byte * 8 + bit);
        if (usage >= 0x04 && (usage < 0xe0 || usage > 0xe7))
        {
          keys_.press(keyboardKey(usage));
        }
      }
    }
  }

  const KeySet &keys() const override { return keys_; }
  bool connected() const override
  {
    return bluetooth_.hidHost().connected() && haveState_;
  }

  void setLockState(const LockState &state) override
  {
    if (bluetooth_.hidHost().connected())
    {
      bluetooth_.hidHost().setKeyboardLeds(
          state.numLock, state.capsLock, state.scrollLock, false, state.kana);
    }
  }

private:
  void subscribe()
  {
    if (subscribed_)
    {
      return;
    }
    subscribed_ = true;
    bluetooth_.hidHost().onKeyboardState(
        [this](const EspBleClassicHidKeyboardState &state) {
          modifiers_ = state.modifiers;
          memcpy(bitmap_, state.bitmap, sizeof(bitmap_));
          haveState_ = true;
        });
  }

  EspBleClassic &bluetooth_;
  bool subscribed_ = false;
  bool haveState_ = false;
  uint8_t modifiers_ = 0;
  uint8_t bitmap_[EspBleClassicHidKeyboardState::BitmapSize] = {};
  KeySet keys_;
};

// Classic HID Host mouse input. The single peer's buttons are a state; motion
// and wheel are queued deltas. As on BLE, AC Pan is not decoded.
class EspBleClassicHidHostMouseInputAdapter : public InputAdapter
{
public:
  explicit EspBleClassicHidHostMouseInputAdapter(EspBleClassic &bluetooth)
      : bluetooth_(bluetooth) {}

  ~EspBleClassicHidHostMouseInputAdapter() override
  {
    if (subscribed_)
    {
      bluetooth_.hidHost().onMouse({});
    }
  }

  void update() override
  {
    subscribe();
    keys_.clear();
    if (!bluetooth_.hidHost().connected())
    {
      buttons_ = 0;
      haveState_ = false;
      pendingX_ = pendingY_ = pendingWheel_ = 0;
      return;
    }
    for (uint8_t bit = 0; bit < 8; ++bit)
    {
      if ((buttons_ & static_cast<uint8_t>(1u << bit)) != 0)
      {
        keys_.press(mouseButtonKey(static_cast<uint16_t>(bit + 1)));
      }
    }
  }

  const KeySet &keys() const override { return keys_; }
  bool connected() const override
  {
    return bluetooth_.hidHost().connected() && haveState_;
  }

  int32_t takeAxisDelta(Axis axis) override
  {
    int32_t *value = nullptr;
    if (axis == Axis::X) value = &pendingX_;
    else if (axis == Axis::Y) value = &pendingY_;
    else if (axis == Axis::Wheel) value = &pendingWheel_;
    if (value == nullptr) return 0;
    const int32_t result = *value;
    *value = 0;
    return result;
  }

private:
  void subscribe()
  {
    if (subscribed_)
    {
      return;
    }
    subscribed_ = true;
    bluetooth_.hidHost().onMouse(
        [this](const EspBleClassicHidMouseEvent &event) {
          buttons_ = event.buttons;
          pendingX_ += event.x;
          pendingY_ += event.y;
          pendingWheel_ += event.wheel;
          haveState_ = true;
        });
  }

  EspBleClassic &bluetooth_;
  bool subscribed_ = false;
  bool haveState_ = false;
  uint8_t buttons_ = 0;
  int32_t pendingX_ = 0;
  int32_t pendingY_ = 0;
  int32_t pendingWheel_ = 0;
  KeySet keys_;
};

} // namespace esp32keybridge
