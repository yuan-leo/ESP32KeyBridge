# UsKeyboardOnJapanesePc

[日本語](README.ja.md)

For when you want to type as engraved with a US-layout keyboard or barcode reader, on a PC left on the Japanese (ja_jp) setting. The OS settings are not changed.

- Printable keys are turned back into characters by their engraving (en_us), then re-encoded into keystrokes for the PC-side layout (ja_jp). Any needed Shift is synthesized or suppressed automatically (`'` → Shift+7, etc.).
- What the settings mean: the engraving (`keyboardConfig.convertLayout(enUs)`) is a **per-input-device** attribute, while `config.hostLayout = jaJp()` is a declaration of "**how the PC is already configured**" (nothing on the PC side is changed). Since every output receives the same set of presses, the destination interpretation is held in the bridge in exactly one place.
- Shortcuts involving Ctrl/Alt/GUI are passed through unchanged.
- **The Pause key toggles conversion on/off.** In environments that interpret keys differently from the bridge, such as a BIOS, turn it off to pass the raw keys through.
- Characters that cannot be typed are discarded and counted (`layoutConvertFailCount()`).

## Hardware setup (ESP32-P4)

This uses two USB buses (Host + Device), so an ESP32-P4 is required.

- **USB Device (PC side) = fixed to HS** (a constraint of arduino-esp32 3.3.10).
- **USB Host (keyboard side) = explicitly set to FS**. On boards where the keyboard's connector is wired to the FS CDC pins (GPIO24/25), uncomment the PHY swap line in the sketch.

For details on the P4's USB ports, a note about boards where the label and actual wiring differ (the M5Stack Tab5 case), and the constraints when using HS for Host, see [SwapCtrlCapsLock/README.md](../SwapCtrlCapsLock/README.md).
