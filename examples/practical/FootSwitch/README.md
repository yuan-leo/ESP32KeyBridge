# FootSwitch

[日本語](README.ja.md)

For when you want to connect foot switches or homemade buttons to a PC as a keyboard. Use them for one-action input when your hands are full.

This example is a 3-switch setup:

| Pin | Key | Purpose |
|---|---|---|
| GPIO4 | virtual key (-> text macro) | type canned text (an email address, etc.) |
| GPIO5 | Consumer PlayPause | media pedal (play/pause) |
| GPIO6 | PageDown | turn pages in a presentation |

- Switches are registered with `addKey(pin, key, activeLow, pullUp)` (polarity and pull-up are specified per pin). The limit is simply the chip's GPIO count (`SOC_GPIO_PIN_COUNT`). Debounce is done inside the adapter.
- GPIO keys are sampled inside `update()` (= the loop rate). In a sketch that blocks the loop for a long time, presses shorter than the block are dropped — a deliberate trade-off aimed at long-press inputs like pedals and buttons (for a matrix where you must reliably catch short taps, use the dedicated-task `GpioMatrixInputAdapter`; see the list: [ADAPTERS.ja.md](../../../docs/ADAPTERS.ja.md)).
- A pedal can directly press any kind of key. Assign a consumer key and it becomes a media pedal as-is.
- Virtual keys are internal to the bridge and never reach the PC. Use the predefined slots `VirtualUsage::V1` through `V16` (if that is not enough, `virtualKey(n)` for any number 1-65535), naming them in the sketch (in this example, `kEmailPedal = VirtualUsage::V1`).
- Text is expanded into keystrokes using `config.hostLayout` (default en_us). Match it to the PC's layout setting.

## Advanced: add a momentary layer pedal

When used together with a keyboard input adapter, making a pedal the trigger for a "layer that is active only while held down" lets you, for example, turn J/K into Down/Up (layers work across inputs). Add one pedal and assign it a dedicated virtual key:

```cpp
const esp32keybridge::Key kNavPedal = esp32keybridge::VirtualUsage::V2;

pedals.addKey(GPIO_NUM_7, kNavPedal, /*activeLow=*/true, /*pullUp=*/true);

esp32keybridge::LayerConfig &nav = config.addLayer(kNavPedal);
nav.remap(esp32keybridge::KeyboardUsage::J, esp32keybridge::KeyboardUsage::Down);
nav.remap(esp32keybridge::KeyboardUsage::K, esp32keybridge::KeyboardUsage::Up);
```

## Hardware setup (ESP32-S3)

- Plug the ESP32-S3's USB OTG port into the PC (it appears as a composite HID device including a keyboard).
- Connect each switch between its pin and GND (pressed = closed). Change the pins to match your wiring.
