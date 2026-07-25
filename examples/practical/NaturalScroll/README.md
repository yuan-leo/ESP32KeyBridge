# NaturalScroll

[日本語](README.ja.md)

For when you want to invert the mouse wheel direction without touching the PC's settings. On a locked-down work PC or a shared PC where you cannot change the settings, everything is handled on the mouse side (this bridge), so you get the same feel across operating systems.

- The wheel flows through as a relative axis, and the negative scale in `config.setAxisScale(Axis::Wheel, -1)` inverts it. Buttons and pointer movement pass through untouched.
- The scale doubles as a multiplier: `-2` = inverted + 2x speed, `2` = same direction + 2x speed.
- If you connect multiple mice, the same transform applies to all of them together (the input adapter merges them).

## Advanced: swap the buttons for left-handed use

Mouse buttons are just another kind of key (`MouseUsage::Left` / `Right` / `Middle` / `Back` / `Forward`; unnamed numbered buttons use `mouseButtonKey(n)`), so you can swap them with a remap:

```cpp
config.global.remap(esp32keybridge::MouseUsage::Left, esp32keybridge::MouseUsage::Right);
config.global.remap(esp32keybridge::MouseUsage::Right, esp32keybridge::MouseUsage::Left);
```

Combined with inverting left/right pointer movement (`setAxisScale(Axis::X, -1)`), you can even set it up like a trackball placed in a different orientation.

## Hardware setup (ESP32-P4)

You need an ESP32-P4 because it uses two USB paths (Host + Device).

- **USB Device (PC side) = fixed to HS** (a constraint of arduino-esp32 3.3.10).
- **USB Host (mouse side) = explicitly set to FS**. On boards where the mouse connector is wired to the FS CDC pins (GPIO24/25), uncomment the PHY swap line in the sketch.

For details on the P4's USB ports, the caveat about boards whose labels differ from the actual wiring (the M5Stack Tab5 case), and the constraints when using HS for the Host, see [SwapCtrlCapsLock/README.md](../SwapCtrlCapsLock/README.md).
