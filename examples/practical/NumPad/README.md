# NumPad

[日本語](README.ja.md)

For when you want to build your own numeric keypad, macro pad, or custom keyboard. Wire the switches as a matrix (each switch sits at the intersection of a row line and a column line), connect them directly to the GPIOs, and connect it to the PC as a USB keyboard. This example is a 4x3 = 12-key numeric keypad.

- The wiring and the keymap are kept separate. `setRowPins()` / `setColPins()` define the wiring (rows = scan outputs, columns = internal pull-up inputs), and `setKeys()` is the keymap, where the **row-major order = the physical layout** (the way the code looks is the key layout itself; the same convention as QMK keymaps). No coordinates or indices appear.
- Put the invalid key `esp32keybridge::Key()` at positions you do not use. If the element count does not match rows x columns, `setKeys()` returns false.
- Scanning and debounce (`setDebounceMillis()`) run in a dedicated task inside the adapter (1kHz), so even if `loop` blocks for a long time, you will not drop a short tap.
- **Diodes are optional.** Without them, the adapter blocks the ghosting that occurs when three or more keys are pressed at once (the phenomenon where a key you are not pressing appears to be pressed), so reliable simultaneous presses are limited to 2 keys — an ambiguous 3rd key that would complete a 2x2 rectangle is held back rather than risk emitting a phantom. If you add a diode to every switch, `pad.setHasDiodes(true)` removes the block (unlimited N-key rollover).
- The keypad's number keys switch between digits and cursor movement based on the **PC's NumLock state** (just like a real numeric keypad). The PC is the source of truth for the lock state.

## Advanced: make it a macro pad

There are two ways, and the criterion for choosing between them is "**does that key need to be seen by the PC?**"

- **Assign a shortcut on the app side**: the app watches for the key and interprets it, so you need a real key that reaches the PC. `KeyboardUsage::F13` through `F24` (extended function keys that hardly exist on any physical keyboard) will not collide with existing keys.
- **Type canned text**: the expansion into characters happens **inside the bridge**, so the trigger key itself does not need to be seen by the PC. Use a virtual key (`VirtualUsage::V1`, etc.; it does not reach the PC) + `config.textMacro(...)` (the same pattern as the FootSwitch example).

## Hardware setup (ESP32-S3)

- Plug the ESP32-S3's USB OTG port into the PC (it appears as a composite HID device that includes a keyboard).
- The switches connect at the intersections of the row and column lines. The wiring in this example: rows = GPIO4/5/6/7, columns = GPIO10/11/12 (change these to match your wiring).
