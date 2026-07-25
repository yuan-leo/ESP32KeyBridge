# LayerKeypad

[日本語](README.ja.md)

For when you want a small homemade keypad to have "two functions per key". Four keys act as **document/slide controls by default** (Page Up/Down, Home, End), and become **media keys (volume, track skip) only while you hold the Fn key**. One physical pad, two faces of functionality — the same idea as a laptop's Fn layer, built out of plain buttons.

- **How the layer works**: the Fn key is a virtual key (`VirtualUsage::V1`) used only to trigger the layer (it never reaches the PC). The `layer.remap()` substitutions apply only while Fn is held; release Fn and each key returns to its base function.
- **Crossing key types**: a layer remap can cross key types. In this example keyboard keys (PageUp, etc.) are swapped for consumer keys (volume, etc.).
- **Resolved on press**: a key's function is decided the moment it is pressed and held until release. Even if you release Fn while a key is still held, that key keeps the function it had when pressed (it does not get stuck).

## Key assignments

| Pin | Base | While holding Fn |
|---|---|---|
| GPIO4 | **Fn** (layer key) | — |
| GPIO5 | Page Up | Volume + |
| GPIO6 | Page Down | Volume − |
| GPIO7 | Home | Previous track |
| GPIO8 | End | Next track |

To add keys or change assignments, just add more `addKey()` and `fn.remap()` calls. To stack more layers, make Fn a different virtual key and add one more `addLayer()` (up to `MaxLayers`).

## Hardware setup (ESP32-S3)

- Plug the ESP32-S3's USB OTG port into the PC (it appears as a composite HID device that includes a keyboard).
- Connect a momentary switch between each pin and GND (internal pull-up, closed = pressed). Change the pin numbers to match your wiring.
