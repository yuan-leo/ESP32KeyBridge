# VolumeKnob

[日本語](README.ja.md)

For when you want a physical volume knob on your desk. Turning the rotary encoder raises and lowers the PC's volume, and pressing it mutes. Since it works as a USB media key, no PC-side settings or resident software are required.

- The encoder is counted in hardware by the PCNT peripheral (no interrupts). Even if the loop blocks for a long time, rotation accumulates in the counter, so nothing is dropped (it is only reflected late). One notch (detent) = one tap, assigned via `mapToKeys(clockwise key, counter-clockwise key)`.
- The encoder's push switch is just a GPIO key, so use `GpioKeyInputAdapter` alongside it (you can add as many adapters as you like).
- Internally it is a full x4 quadrature decode of both the A and B phases on both edges, so one quadrature period yields 4 counts. That is divided by `setPulsesPerDetent` (default 4 = one notch of an EC11 = a full period) to normalize to "one notch = one tap / ±1" (the remainder is carried over, so there is no drift). For a type that emits 2 per notch or only half, match it with `knob.setPulsesPerDetent(n)`. For details, see "Rotary encoder: quadrature decode and detents" in [../../../docs/ADAPTERS.ja.md](../../../docs/ADAPTERS.ja.md).
- To add more encoders, add one adapter each (up to the number of PCNT units; the ESP32-S3 has 4 units).

## Advanced: turning it into a scroll dial or jog

Instead of taps, feed a relative axis to use it as a wheel or pointer:

```cpp
knob.mapToAxis(esp32keybridge::Axis::Wheel);
```

The direction and speed can be changed on the config side (`config.setAxisScale(Axis::Wheel, -2)` = invert + 2x. See the NaturalScroll example). It can be used the same way for a video-editing jog (`Axis::X`).

## Hardware setup (ESP32-S3)

- Plug the ESP32-S3's USB OTG port into the PC (it appears as a composite HID device including a keyboard).
- EC11-type encoder: A phase = GPIO5, B phase = GPIO6, common = GND, push switch = GPIO7 and GND (change to match your wiring).
