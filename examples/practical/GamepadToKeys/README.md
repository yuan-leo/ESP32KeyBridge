# GamepadToKeys

[日本語](README.ja.md)

For when you want to operate a PC as a keyboard with a USB gamepad. **The D-pad becomes the arrow keys, and the face buttons become Space / Enter / Escape / Backspace**, and the PC sees an ordinary keyboard. Good for kiosk terminals, a living-room media PC, retro emulators that only accept a keyboard, accessibility use, and so on.

- The gamepad adapter maps each control to a Key (any kind you like), and from there the bridge handles it as ordinary keyboard input. You can layer remaps and layers on top too. **Only the controls you assign** do anything.
- Supported: buttons (HID Button page) and the hat/D-pad (HID Hat switch, 8 directions; diagonals press the two adjacent keys together). Analog sticks/triggers are not supported in this version.

## How to find button numbers

**Button numbers differ per controller.** If a button other than the one you intended fires, adjust the numbers in `mapButton()` against the real device. An example procedure:

- Assign buttons 1-8 to distinct keys (e.g. 1-8), flash it, and press each button to identify which number it is.
- Or read the raw button numbers and usages with EspUsbHost's gamepad sample.

## Key assignments (defaults)

| Control | Key |
|---|---|
| D-pad | arrows (up/down/left/right) |
| Button 1 | Space |
| Button 2 | Enter |
| Button 3 | Escape |
| Button 4 | Backspace |

## Hardware setup (ESP32-P4)

Because two USB lines (Host + Device) are used, an ESP32-P4 is required (it will not work on a single-USB chip like the ESP32-S3 alone).

- **USB Device = HS port**: plug into the PC.
- **USB Host = FS port**: plug in the gamepad.

For the details of the P4's USB ports, [../SwapCtrlCapsLock/README.md](../SwapCtrlCapsLock/README.md) is the reference.
