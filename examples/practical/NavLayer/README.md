# NavLayer

[日本語](README.ja.md)

For when you want to add a "navigation layer that only exists while held down" to any USB keyboard. **While CapsLock is held, H/J/K/L become the arrow keys** (U/O become Home/End). It feels just like a laptop's Fn layer or vim's arrows, and nothing is installed on the PC.

- **How the layer works**: CapsLock is remapped to a virtual key (`VirtualUsage::V1`), and one layer is set up to be enabled by that virtual key. The substitutions in `layer.remap()` only take effect while the layer is active, and keys with no entry pass through as-is, so you can still type normally while holding CapsLock.
- **Decided on press**: a key's identity ("what it becomes") is fixed the instant it is pressed, and held until released. Even if you release CapsLock (the trigger) first, an arrow key you are still holding stays as an arrow (it does not get stuck).
- CapsLock becomes a **dedicated layer (Fn) key** and never reaches the PC (the Caps toggle is gone too). If you want to keep CapsLock, change the trigger key in the sketch to a different key.
- You can register multiple layers (up to `MaxLayers`), and by giving each a different virtual key as its trigger you can stack them.

## Customization

Just follow `config.addLayer(...)` with `nav.remap(source key, replacement)`. The replacement can cross key types (for example, turning the number row into media keys while the layer is active):

```cpp
nav.remap(esp32keybridge::KeyboardUsage::Digit1,
          esp32keybridge::ConsumerUsage::VolumeDecrement);
```

## Hardware setup (ESP32-P4)

You need an ESP32-P4 because it uses two USB paths (Host + Device) (a single-USB chip like the ESP32-S3 will not work on its own).

- **USB Device = HS port**: plug this into the PC.
- **USB Host = FS port**: plug the keyboard in here.

For details on the P4's USB ports (which connector is wired to which pin, when the FS PHY swap is needed, the M5Stack Tab5 case, etc.), [../SwapCtrlCapsLock/README.md](../SwapCtrlCapsLock/README.md) is the reference. This example follows the same wiring.
