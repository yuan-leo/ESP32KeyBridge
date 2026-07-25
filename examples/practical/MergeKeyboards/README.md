# MergeKeyboards

[日本語](README.ja.md)

For when you want several input devices to appear to the PC as a single keyboard. It combines a USB keyboard with extra GPIO buttons and sends them to the PC as a composite HID device. Keys pressed on either input come out together, so it feels like adding physical keys to your existing keyboard.

## Two kinds of merging

- **The USB Host keyboard adapter automatically merges every keyboard on the stack** (plug multiple keyboards into a USB hub and up to 4 are merged). This is a single `addInput()`.
- **Adding another `addInput()` for a different kind of input** (a GPIO macro pad here) merges across adapters.

Merging is a **union**. A given key stays pressed until every input holding it releases it. Pressing the same key on two devices does not cause it to release the moment one of them is released (no getting stuck).

## Key assignments (GPIO macro pad)

| Pin | Key |
|---|---|
| GPIO4 | Mute |
| GPIO5 | Play/Pause |
| GPIO6 | F13 (for assigning an app shortcut) |

Add buttons by adding more `macroPad.addKey()`. The key type is free (keyboard / consumer / mouse button, etc.).

## Hardware setup (ESP32-P4)

An ESP32-P4 is required because two USB channels (USB keyboard input + USB Device output) are used. **On single-USB chips such as the ESP32-S3, USB keyboard input and USB Device output cannot coexist, so it will not work on its own** (merging GPIO only is still possible on the S3).

- **USB Device = HS port**: plug into the PC.
- **USB Host = FS port**: plug in the keyboard (or a hub with multiple keyboards).
- GPIO macro buttons: a momentary switch between each pin and GND (internal pull-up, closed = pressed).

For details on the P4's USB ports (connector-to-pin mapping, FS PHY swap, the M5Stack Tab5 case), [../SwapCtrlCapsLock/README.md](../SwapCtrlCapsLock/README.md) is the reference.
