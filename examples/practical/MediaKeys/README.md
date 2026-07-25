# MediaKeys

[日本語](README.ja.md)

For when you have a keyboard without media keys and want to turn unused keys (F13–F15, etc.) into volume and play/pause keys.

- Because a remap can cross key types, you can write keyboard key → consumer key assignments one line at a time (F13 → volume down, F14 → volume up, F15 → play/pause).
- While held, the key is delivered as held down, and the PC interprets things like continuous volume changes.
- The USB Device output (`EspUsbDeviceHidOutputAdapter`) always appears to the PC as a **composite HID device** of keyboard + consumer + mouse. Media keys are sent from the consumer interface; an unused interface (mouse, in this example) simply sends no reports and does no harm. The USB interface layout is fixed at `usbDevice.begin()` time and cannot be added or removed dynamically, by the nature of USB.

## Hardware setup (ESP32-P4)

An ESP32-P4 is required because two USB channels (Host + Device) are used.

- **USB Device (PC side) = fixed HS** (a constraint of arduino-esp32 3.3.10).
- **USB Host (keyboard side) = FS explicitly specified**. On boards where the keyboard connector is wired to the FS CDC pins (GPIO24/25), uncomment the PHY swap line in the sketch.

For details on the P4's USB ports, notes on boards whose markings differ from the actual wiring (the M5Stack Tab5 case), and the constraints of using HS for the Host, see [SwapCtrlCapsLock/README.md](../SwapCtrlCapsLock/README.md).
