# MidiToKeys

[日本語](README.ja.md)

For when you want to turn a USB MIDI controller's pads or keys into keyboard shortcuts. Hitting a pad presses a key on the PC. The defaults map to **F13–F16** (function keys that apps assign to macros easily and that don't collide with anything else) and **two media keys**. The PC sees an ordinary keyboard, so no driver or MIDI software is needed.

- **Note On (velocity > 0) = press, Note Off = release**. Hold a pad down and the key stays held. Only mapped notes do anything, and the Key type is free (keyboard / consumer / mouse button / virtual key). You can also stack remaps and layers.
- This version uses **Note On/Off only** (control change, pitch bend, etc. are ignored). Velocity is used only to decide press vs. release.
- `midi.setChannel(n)` (0–15) narrows it to a single channel. The default is all channels.

## How to find note numbers

**Note numbers differ per controller** (drum pads often start at 36 = C1). If a different pad than intended fires, adjust the numbers in `mapNote()`. You can check a pad's note number in your DAW or MIDI monitor.

## Assignments (defaults)

| Note | Key |
|---|---|
| 36 | F13 |
| 37 | F14 |
| 38 | F15 |
| 39 | F16 |
| 40 | Play/Pause |
| 41 | Mute |

## Hardware setup (ESP32-P4)

An ESP32-P4 is required because two USB channels (Host + Device) are used (single-USB chips such as the ESP32-S3 will not work on their own).

- **USB Device = HS port**: plug into the PC.
- **USB Host = FS port**: plug in the MIDI controller.

For details on the P4's USB ports, [../SwapCtrlCapsLock/README.md](../SwapCtrlCapsLock/README.md) is the reference.
