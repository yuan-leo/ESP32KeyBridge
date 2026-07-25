# KeyMonitor

[日本語](README.ja.md)

A bring-up tool for input hardware that prints what the bridge outputs to the serial console. Once you have wired up a button, watch presses and releases flow by and check your wiring before connecting a real output. Placing it alongside a real output to monitor the traffic flowing through works the same way.

- `LogOutputAdapter` emits a human-readable line whenever the set of pressed keys changes, a character is typed, or a relative axis moves. It is not the source of truth for lock state.
- This example uses no real HID output; it only prints logs to `Serial` (the board's USB-serial).

Example serial output:

```text
KEYS keyboard:04
KEYS keyboard:04 keyboard:05
KEYS (empty)
KEYS consumer:00cd
AXIS wheel +3
```

## Advanced: place it alongside a real output

Just add one monitor to any example and you can monitor in parallel with the real output:

```cpp
esp32keybridge::LogOutputAdapter monitor(Serial1);
bridge.addOutput(pc);       // real output
bridge.addOutput(monitor);  // monitor (to a separate stream)
```

## Hardware setup (ESP32-S3)

- Connect buttons between each pin and GND (in this example: GPIO4/5/6; change to match your wiring).
- Logs come out on `Serial` (the board's USB-serial).
