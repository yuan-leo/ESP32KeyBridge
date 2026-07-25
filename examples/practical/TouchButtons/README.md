# TouchButtons

[日本語](README.ja.md)

This example shows **how to write your own input adapter**. As a subject it uses the ESP32-S3's built-in capacitive touch: touching a pad sends a media key to the PC. No external library and no extra parts required (a touch GPIO reacts to a short wire, foil, or even just the pin itself).

There are countless things that "can be an input": I2C expanders, all kinds of sensors, network reception, and so on. Instead of bundling them all into this library, we provide this one example so you can **write an adapter yourself**. Touch is merely the subject.

## The adapter contract (this is all)

Subclass `esp32keybridge::InputAdapter` and implement the following.

- **`update()` (required)**: read the hardware and update the internal `KeySet` with `press()` / `release()`. Called once per `bridge.update()`.
- **`keys()` (required)**: return the "set of keys currently pressed". This is the only thing the bridge reads.

Optional overrides:

- `connected()` (default: always connected) — reporting a disconnect removes that adapter's keys from the merge (prevents stuck keys).
- `setLockState(state)` (default: ignored) — when you want to reflect the host's lock state (CapsLock, etc.) on your own device (LEDs, etc.).
- `takeAxisDelta(axis)` (default: 0) — when you want to emit a relative amount such as mouse motion, wheel, or an encoder.

In `update()`, read whatever hardware you like (I2C, sensors, network), reflect presses into the `KeySet`, and return it from `keys()`. The bridge handles the rest — merging, remapping, layers, and output. The `TouchButtons` class in this example is that minimal template.

## Points specific to this subject

- **Lazy initialization**: the no-touch baseline is measured in the first `update()`, not in the constructor (the peripheral is reliably ready by then; the same style as the GPIO / encoder adapters).
- **The ESP32-S3's `touchRead()` value "increases" on touch** (the old ESP32 was the opposite). Chattering is prevented with hysteresis: a press when the value is well above the baseline, a release when it returns near the baseline. Tune the responsiveness with the coefficients (print `value` to see the raw reading).
- **Touch-capable GPIOs**: on the ESP32-S3 these are GPIO1–14. Change them to match your wiring.

## Key assignment (defaults)

| Pin | Key |
|---|---|
| GPIO4 | Volume − |
| GPIO5 | Play/Pause |
| GPIO6 | Volume + |

The `key` in `addPad(pin, key)` can be any kind (keyboard / consumer / mouse button / virtual key). You can also make it a virtual key and put it on a remap or layer on the config side.

## Hardware setup (ESP32-S3)

- Plug the ESP32-S3's USB OTG port into the PC (it appears as a composite HID device including media keys).
- Attaching a wire or foil to the touch GPIOs (4/5/6) gives you more touch area.
