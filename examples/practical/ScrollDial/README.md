# ScrollDial

[日本語](README.ja.md)

For when you want a physical scroll dial on your desk. Turning the rotary encoder scrolls the mouse wheel, and pushing the axis in gives a middle click. The PC sees an ordinary USB mouse (wheel + buttons), so no drivers or settings are needed.

- Where VolumeKnob uses `mapToKeys()` (tapping media keys), this is the `mapToAxis()` version that **feeds one detent into a relative axis**. The encoder uses hardware counting (PCNT), so even if `loop` gets stalled it will not drop any rotation (it just reflects it late).
- The push switch is just a plain GPIO key, so it is used together with `GpioKeyInputAdapter` and assigned to the mouse's middle button. The dial and the switch are merged as two inputs into one mouse.
- The **speed and direction** of scrolling are set with `config.setAxisScale(Axis::Wheel, n)`. A negative value inverts it (equivalent to macOS "natural" scrolling), and a larger absolute value makes it faster.

## Advanced: make it a jog dial

If you feed `Axis::X` instead of `Axis::Wheel`, you can use it for a video-editing jog (scrub) or horizontal scrolling:

```cpp
dial.mapToAxis(esp32keybridge::Axis::X);
```

For the number of pulses per detent (default 4 = EC11) and the details of x4 quadrature decoding, see "Rotary encoder: quadrature decoding and detents" in [../../../docs/ADAPTERS.ja.md](../../../docs/ADAPTERS.ja.md).

## Hardware setup (ESP32-S3)

- Plug the ESP32-S3's USB OTG port into the PC (it appears as a composite HID device that includes a mouse).
- An EC11-type encoder: phase A = GPIO5, phase B = GPIO6, common = GND, push switch = GPIO7 and GND (change these to match your wiring).
