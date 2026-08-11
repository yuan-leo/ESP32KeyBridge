#pragma once

#include <ESP32KeyBridge.h>
#include <EspUsbHost.h>

#include <string.h>

// USB Host input adapters backed by the EspUsbHost library.
//
// Ownership model: the sketch owns the EspUsbHost stack and starts it with
// the library's own configuration (port selection, etc.). Adapters take a
// reference and subscribe to events lazily on their first update(), so they
// have no begin() and no ordering against the bridge or the stack.
//
// Subscription uses the multi-listener API (EspUsbHost 2.6.0+):
// add*Listener() registers alongside every other observer and returns an id
// the adapter drops again in its destructor. The single-slot on*() hooks are
// left untouched, so a sketch can keep using all of them — including
// onKeyboardState / onMouse / onDeviceDisconnected — next to an adapter.
// EspBle and EspUsbDevice expose the same shape, so the three stacks are
// wired identically; see ESP32KeyBridgeEspBle.h.
//
// Listeners fire from the EspUsbHost task, so each adapter hands the data to
// the update() context behind a critical section.

namespace esp32keybridge
{

// USB keyboard input (consumer keys included). All keyboards on the stack
// are merged (key union across devices); a disconnect releases only that
// device's keys. The bridge lock state is forwarded to every keyboard's
// LEDs.
//
// Keyboard state comes from onKeyboardState (EspUsbHost 2.2.0+): one
// format-independent 256-bit snapshot per changed report — boot, report-ID
// boot, and NKRO alike, modifier-only changes included — which maps
// directly onto the key set. Consumer keys (media keys on keyboards,
// remotes) arrive as onConsumerControl press/release events.
class EspUsbHostKeyboardInputAdapter : public InputAdapter
{
public:
  // Keyboard interfaces tracked simultaneously (per USB address +
  // interface); reports from further ones are ignored.
  static constexpr size_t MaxKeyboards = 4;
  // Simultaneously held consumer usages across all devices.
  static constexpr size_t MaxConsumerKeys = 8;

  explicit EspUsbHostKeyboardInputAdapter(EspUsbHost &usb) : usb_(usb) {}

  ~EspUsbHostKeyboardInputAdapter() override
  {
    for (EspUsbHostListenerId id : listeners_)
    {
      usb_.removeListener(id);
    }
  }

  void update() override
  {
    if (!subscribed_)
    {
      subscribed_ = true;
      listeners_[0] = usb_.addKeyboardStateListener(
          [this](const EspUsbHostKeyboardState &state) { handleKeyboardState(state); });
      listeners_[1] = usb_.addConsumerControlListener(
          [this](const EspUsbHostConsumerControlEvent &event) { handleConsumer(event); });
      listeners_[2] = usb_.addDeviceDisconnectedListener(
          [this](const EspUsbHostDeviceInfo &info) { handleDisconnect(info.address); });
    }

    // Snapshot the callback-fed state, then rebuild outside the lock.
    uint8_t bitmaps[MaxKeyboards][ESP_USB_HOST_KEYBOARD_BITMAP_SIZE];
    size_t keyboardCount;
    ConsumerEntry consumer[MaxConsumerKeys];
    size_t consumerCount;
    portENTER_CRITICAL(&mux_);
    keyboardCount = keyboardCount_;
    for (size_t i = 0; i < keyboardCount_; ++i)
    {
      memcpy(bitmaps[i], keyboards_[i].bitmap, ESP_USB_HOST_KEYBOARD_BITMAP_SIZE);
    }
    consumerCount = consumerCount_;
    for (size_t i = 0; i < consumerCount_; ++i)
    {
      consumer[i] = consumer_[i];
    }
    portEXIT_CRITICAL(&mux_);

    keys_.clear();
    for (size_t i = 0; i < keyboardCount; ++i)
    {
      for (size_t byte = 0; byte < ESP_USB_HOST_KEYBOARD_BITMAP_SIZE; ++byte)
      {
        uint8_t bits = bitmaps[i][byte];
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
    for (size_t i = 0; i < consumerCount; ++i)
    {
      keys_.press(consumerKey(consumer[i].usage));
    }
    connected_ = keyboardCount > 0 || consumerCount > 0;
  }

  const KeySet &keys() const override { return keys_; }

  // A keyboard counts as present from its first changed report (EspUsbHost
  // has no enumeration query); false again when every known device
  // disconnected. A keyboard that never reported has no state to lose.
  bool connected() const override { return connected_; }

  // Forwards the bridge lock state to the LEDs of every keyboard on the
  // stack. EspUsbHost carries num/caps/scroll; kana is not forwardable.
  void setLockState(const LockState &state) override
  {
    usb_.setKeyboardLeds(state.numLock, state.capsLock, state.scrollLock);
  }

private:
  struct Keyboard
  {
    uint8_t address = 0;
    uint8_t interfaceNumber = 0;
    uint8_t bitmap[ESP_USB_HOST_KEYBOARD_BITMAP_SIZE] = {};
  };

  struct ConsumerEntry
  {
    uint8_t address = 0;
    uint16_t usage = 0;
  };

  // Fires from the EspUsbHost task.
  void handleKeyboardState(const EspUsbHostKeyboardState &state)
  {
    portENTER_CRITICAL(&mux_);
    size_t index = keyboardCount_;
    for (size_t i = 0; i < keyboardCount_; ++i)
    {
      if (keyboards_[i].address == state.address &&
          keyboards_[i].interfaceNumber == state.interfaceNumber)
      {
        index = i;
        break;
      }
    }
    if (index == keyboardCount_ && keyboardCount_ < MaxKeyboards)
    {
      keyboards_[keyboardCount_].address = state.address;
      keyboards_[keyboardCount_].interfaceNumber = state.interfaceNumber;
      ++keyboardCount_;
    }
    if (index < keyboardCount_)
    {
      memcpy(keyboards_[index].bitmap, state.bitmap, ESP_USB_HOST_KEYBOARD_BITMAP_SIZE);
    }
    portEXIT_CRITICAL(&mux_);
  }

  // Fires from the EspUsbHost task.
  void handleConsumer(const EspUsbHostConsumerControlEvent &event)
  {
    portENTER_CRITICAL(&mux_);
    if (event.released)
    {
      for (size_t i = 0; i < consumerCount_; ++i)
      {
        if (consumer_[i].address == event.address && consumer_[i].usage == event.usage)
        {
          consumer_[i] = consumer_[consumerCount_ - 1];
          --consumerCount_;
          break;
        }
      }
    }
    if (event.pressed && event.usage != 0 && consumerCount_ < MaxConsumerKeys)
    {
      bool held = false;
      for (size_t i = 0; i < consumerCount_; ++i)
      {
        if (consumer_[i].address == event.address && consumer_[i].usage == event.usage)
        {
          held = true;
          break;
        }
      }
      if (!held)
      {
        consumer_[consumerCount_].address = event.address;
        consumer_[consumerCount_].usage = event.usage;
        ++consumerCount_;
      }
    }
    portEXIT_CRITICAL(&mux_);
  }

  // Fires from the EspUsbHost task (any device, not only keyboards).
  void handleDisconnect(uint8_t address)
  {
    portENTER_CRITICAL(&mux_);
    for (size_t i = keyboardCount_; i > 0; --i)
    {
      if (keyboards_[i - 1].address == address)
      {
        keyboards_[i - 1] = keyboards_[keyboardCount_ - 1];
        --keyboardCount_;
      }
    }
    for (size_t i = consumerCount_; i > 0; --i)
    {
      if (consumer_[i - 1].address == address)
      {
        consumer_[i - 1] = consumer_[consumerCount_ - 1];
        --consumerCount_;
      }
    }
    portEXIT_CRITICAL(&mux_);
  }

  EspUsbHost &usb_;
  bool subscribed_ = false;
  EspUsbHostListenerId listeners_[3] = {};
  KeySet keys_;
  bool connected_ = false;
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  Keyboard keyboards_[MaxKeyboards];
  size_t keyboardCount_ = 0;
  ConsumerEntry consumer_[MaxConsumerKeys];
  size_t consumerCount_ = 0;
};

// USB mouse input. Buttons become MouseButton keys in keys(); movement,
// wheel and AC Pan (horizontal wheel / tilt, 0 on a boot mouse) accumulate
// as relative axis deltas. All mice on the stack are merged (buttons union,
// deltas summed); a disconnect releases that mouse's buttons only.
class EspUsbHostMouseInputAdapter : public InputAdapter
{
public:
  // Mice tracked simultaneously (per USB address); reports from further
  // addresses are ignored.
  static constexpr size_t MaxMice = 4;

  explicit EspUsbHostMouseInputAdapter(EspUsbHost &usb) : usb_(usb) {}

  ~EspUsbHostMouseInputAdapter() override
  {
    for (EspUsbHostListenerId id : listeners_)
    {
      usb_.removeListener(id);
    }
  }

  void update() override
  {
    if (!subscribed_)
    {
      subscribed_ = true;
      listeners_[0] = usb_.addMouseListener(
          [this](const EspUsbHostMouseEvent &event) { handleMouse(event); });
      listeners_[1] = usb_.addDeviceDisconnectedListener(
          [this](const EspUsbHostDeviceInfo &info) { handleDisconnect(info.address); });
    }

    // Snapshot the callback-fed state, then rebuild outside the lock.
    uint8_t buttons[MaxMice];
    size_t mouseCount;
    int32_t deltas[kAxisCount];
    portENTER_CRITICAL(&mux_);
    mouseCount = mouseCount_;
    for (size_t i = 0; i < mouseCount_; ++i)
    {
      buttons[i] = mice_[i].buttons;
    }
    for (size_t axis = 0; axis < kAxisCount; ++axis)
    {
      deltas[axis] = pendingAxis_[axis];
      pendingAxis_[axis] = 0;
    }
    portEXIT_CRITICAL(&mux_);

    keys_.clear();
    for (size_t i = 0; i < mouseCount; ++i)
    {
      for (size_t bit = 0; bit < 8; ++bit)
      {
        if ((buttons[i] & (1u << bit)) != 0)
        {
          keys_.press(mouseButtonKey(static_cast<uint16_t>(bit + 1)));
        }
      }
    }
    for (size_t axis = 0; axis < kAxisCount; ++axis)
    {
      axisDeltas_[axis] += deltas[axis];
    }
    connected_ = mouseCount > 0;
  }

  const KeySet &keys() const override { return keys_; }

  // A mouse counts as present from its first report (EspUsbHost has no
  // enumeration query); false again when every known mouse disconnected.
  // A mouse that never reported has no state to lose.
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
    uint8_t address = 0;
    uint8_t buttons = 0;
  };

  // Fires from the EspUsbHost task.
  void handleMouse(const EspUsbHostMouseEvent &event)
  {
    portENTER_CRITICAL(&mux_);
    size_t index = mouseCount_;
    for (size_t i = 0; i < mouseCount_; ++i)
    {
      if (mice_[i].address == event.address)
      {
        index = i;
        break;
      }
    }
    if (index == mouseCount_ && mouseCount_ < MaxMice)
    {
      mice_[mouseCount_].address = event.address;
      mice_[mouseCount_].buttons = 0;
      ++mouseCount_;
    }
    if (index < mouseCount_)
    {
      mice_[index].buttons = event.buttons;
      pendingAxis_[static_cast<size_t>(Axis::X)] += event.x;
      pendingAxis_[static_cast<size_t>(Axis::Y)] += event.y;
      pendingAxis_[static_cast<size_t>(Axis::Wheel)] += event.wheel;
      // AC Pan (horizontal wheel / tilt); always 0 for a boot mouse report.
      pendingAxis_[static_cast<size_t>(Axis::Pan)] += event.pan;
    }
    portEXIT_CRITICAL(&mux_);
  }

  // Fires from the EspUsbHost task (any device, not only mice).
  void handleDisconnect(uint8_t address)
  {
    portENTER_CRITICAL(&mux_);
    for (size_t i = 0; i < mouseCount_; ++i)
    {
      if (mice_[i].address == address)
      {
        mice_[i] = mice_[mouseCount_ - 1];
        --mouseCount_;
        break;
      }
    }
    portEXIT_CRITICAL(&mux_);
  }

  EspUsbHost &usb_;
  bool subscribed_ = false;
  EspUsbHostListenerId listeners_[2] = {};
  KeySet keys_;
  bool connected_ = false;
  int32_t axisDeltas_[kAxisCount] = {};
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  Mouse mice_[MaxMice];
  size_t mouseCount_ = 0;
  int32_t pendingAxis_[kAxisCount] = {};
};

// USB gamepad input. Buttons and the hat/D-pad map to keys; only mapped
// controls produce output, so the sketch decides what each control means
// (any Key kind - keyboard, consumer, mouse button, or a virtual key to
// remap/layer later). All gamepads on the stack are merged; a disconnect
// drops only that pad's controls. Analog sticks and triggers are not mapped
// in this version.
//
// Reports arrive as onGamepad (EspUsbHost 2.3.0): decoded HID fields. Button
// page (0x09) fields are buttons (usage = HID button number, 1-based); the
// Generic Desktop hat switch (usage 0x39) is the D-pad. Button numbers vary
// by controller - watch a LogOutputAdapter to find which is which.
class EspUsbHostGamepadInputAdapter : public InputAdapter
{
public:
  static constexpr size_t MaxGamepads = 4;
  static constexpr size_t MaxButtons = GamepadKeyMap::MaxButtons;

  explicit EspUsbHostGamepadInputAdapter(EspUsbHost &usb) : usb_(usb) {}

  ~EspUsbHostGamepadInputAdapter() override
  {
    for (EspUsbHostListenerId id : listeners_)
    {
      usb_.removeListener(id);
    }
  }

  // Control -> Key mapping (shared with the BLE gamepad adapter; see
  // GamepadKeyMap in the core).
  bool mapButton(uint8_t number, Key key) { return map_.mapButton(number, key); }
  void mapHat(Key up, Key down, Key left, Key right) { map_.mapHat(up, down, left, right); }

  void update() override
  {
    if (!subscribed_)
    {
      subscribed_ = true;
      listeners_[0] = usb_.addGamepadListener(
          [this](const EspUsbHostGamepadEvent &event) { handleGamepad(event); });
      listeners_[1] = usb_.addDeviceDisconnectedListener(
          [this](const EspUsbHostDeviceInfo &info) { handleDisconnect(info.address); });
    }

    uint32_t buttons[MaxGamepads];
    int8_t hats[MaxGamepads];
    size_t padCount;
    portENTER_CRITICAL(&mux_);
    padCount = padCount_;
    for (size_t i = 0; i < padCount_; ++i)
    {
      buttons[i] = pads_[i].buttons;
      hats[i] = pads_[i].hat;
    }
    portEXIT_CRITICAL(&mux_);

    keys_.clear();
    for (size_t i = 0; i < padCount; ++i)
    {
      map_.addKeys(keys_, buttons[i], hats[i]);
    }
    connected_ = padCount > 0;
  }

  const KeySet &keys() const override { return keys_; }
  bool connected() const override { return connected_; }

private:
  struct Pad
  {
    uint8_t address = 0;
    uint8_t interfaceNumber = 0;
    uint32_t buttons = 0;
    int8_t hat = GamepadKeyMap::HatCentered;
  };

  // Fires from the EspUsbHost task.
  void handleGamepad(const EspUsbHostGamepadEvent &event)
  {
    uint32_t buttons = 0;
    int8_t hat = GamepadKeyMap::HatCentered;
    GamepadKeyMap::decodeFields(event.fields, event.fieldCount, buttons, hat);

    portENTER_CRITICAL(&mux_);
    size_t index = padCount_;
    for (size_t i = 0; i < padCount_; ++i)
    {
      if (pads_[i].address == event.address && pads_[i].interfaceNumber == event.interfaceNumber)
      {
        index = i;
        break;
      }
    }
    if (index == padCount_ && padCount_ < MaxGamepads)
    {
      pads_[padCount_].address = event.address;
      pads_[padCount_].interfaceNumber = event.interfaceNumber;
      ++padCount_;
    }
    if (index < padCount_)
    {
      pads_[index].buttons = buttons;
      pads_[index].hat = hat;
    }
    portEXIT_CRITICAL(&mux_);
  }

  // Fires from the EspUsbHost task (any device, not only gamepads).
  void handleDisconnect(uint8_t address)
  {
    portENTER_CRITICAL(&mux_);
    for (size_t i = padCount_; i > 0; --i)
    {
      if (pads_[i - 1].address == address)
      {
        pads_[i - 1] = pads_[padCount_ - 1];
        --padCount_;
      }
    }
    portEXIT_CRITICAL(&mux_);
  }

  EspUsbHost &usb_;
  bool subscribed_ = false;
  EspUsbHostListenerId listeners_[2] = {};
  KeySet keys_;
  bool connected_ = false;
  GamepadKeyMap map_;
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  Pad pads_[MaxGamepads];
  size_t padCount_ = 0;
};

// USB MIDI input. MIDI notes map to keys: Note On (velocity > 0) presses the
// mapped key, Note Off (or Note On with velocity 0) releases it. Only mapped
// notes produce output, and any Key kind is allowed, so a pad or piano key
// can become a shortcut, a media key, or a virtual key to remap/layer later.
//
// Messages arrive as onMidiMessage (EspUsbHost 2.3.0). Every channel is
// listened to by default; setChannel() narrows it to one. Only note on/off
// are used - control changes, pitch bend, etc. are ignored in this version.
class EspUsbHostMidiInputAdapter : public InputAdapter
{
public:
  explicit EspUsbHostMidiInputAdapter(EspUsbHost &usb) : usb_(usb) {}

  ~EspUsbHostMidiInputAdapter() override
  {
    for (EspUsbHostListenerId id : listeners_)
    {
      usb_.removeListener(id);
    }
  }

  // Map a MIDI note (0-127) to any Key. Returns false if the note is out of
  // range; unmapped notes produce nothing.
  bool mapNote(uint8_t note, Key key)
  {
    if (note > 127)
    {
      return false;
    }
    noteKeys_[note] = key;
    return true;
  }

  // Listen to one MIDI channel (0-15), or all channels with -1 (default).
  void setChannel(int channel = -1) { channel_ = channel; }

  void update() override
  {
    if (!subscribed_)
    {
      subscribed_ = true;
      listeners_[0] = usb_.addMidiMessageListener(
          [this](const EspUsbHostMidiMessage &message) { handleMidi(message); });
      listeners_[1] = usb_.addDeviceDisconnectedListener(
          [this](const EspUsbHostDeviceInfo &info) { handleDisconnect(info.address); });
    }

    uint8_t onNotes[16];
    size_t deviceCount;
    portENTER_CRITICAL(&mux_);
    memcpy(onNotes, onNotes_, sizeof(onNotes));
    deviceCount = deviceCount_;
    portEXIT_CRITICAL(&mux_);

    keys_.clear();
    for (size_t note = 0; note < 128; ++note)
    {
      if ((onNotes[note >> 3] & (1u << (note & 7))) != 0 && isValid(noteKeys_[note]))
      {
        keys_.press(noteKeys_[note]);
      }
    }
    connected_ = deviceCount > 0;
  }

  const KeySet &keys() const override { return keys_; }
  bool connected() const override { return connected_; }

private:
  static constexpr size_t MaxDevices = 4;

  // Fires from the EspUsbHost task.
  void handleMidi(const EspUsbHostMidiMessage &message)
  {
    const uint8_t type = message.status & 0xf0;
    const uint8_t channel = message.status & 0x0f;
    const bool noteOn = (type == 0x90) && (message.data2 > 0);
    const bool noteOff = (type == 0x80) || ((type == 0x90) && (message.data2 == 0));

    portENTER_CRITICAL(&mux_);
    // Track the device for presence (and clearing stuck notes on unplug).
    bool known = false;
    for (size_t i = 0; i < deviceCount_; ++i)
    {
      if (devices_[i] == message.address)
      {
        known = true;
        break;
      }
    }
    if (!known && deviceCount_ < MaxDevices)
    {
      devices_[deviceCount_++] = message.address;
    }

    if ((channel_ < 0 || channel == channel_) && (noteOn || noteOff))
    {
      const uint8_t note = message.data1 & 0x7f;
      if (noteOn)
      {
        onNotes_[note >> 3] |= static_cast<uint8_t>(1u << (note & 7));
      }
      else
      {
        onNotes_[note >> 3] &= static_cast<uint8_t>(~(1u << (note & 7)));
      }
    }
    portEXIT_CRITICAL(&mux_);
  }

  // Fires from the EspUsbHost task (any device, not only MIDI).
  void handleDisconnect(uint8_t address)
  {
    portENTER_CRITICAL(&mux_);
    for (size_t i = deviceCount_; i > 0; --i)
    {
      if (devices_[i - 1] == address)
      {
        devices_[i - 1] = devices_[deviceCount_ - 1];
        --deviceCount_;
      }
    }
    // No MIDI device left: drop any notes still marked on, so they do not
    // reappear when a device is plugged back in.
    if (deviceCount_ == 0)
    {
      memset(onNotes_, 0, sizeof(onNotes_));
    }
    portEXIT_CRITICAL(&mux_);
  }

  EspUsbHost &usb_;
  bool subscribed_ = false;
  EspUsbHostListenerId listeners_[2] = {};
  KeySet keys_;
  bool connected_ = false;
  int channel_ = -1;
  Key noteKeys_[128] = {};
  uint8_t onNotes_[16] = {};
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  uint8_t devices_[MaxDevices] = {};
  size_t deviceCount_ = 0;
};

} // namespace esp32keybridge
