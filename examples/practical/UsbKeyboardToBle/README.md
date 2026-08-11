# UsbKeyboardToBle

[日本語](README.ja.md)

Make a wired USB keyboard wireless. The keyboard plugs into the ESP32, and the PC / tablet / phone sees a **Bluetooth keyboard**. Keep the keyboard you like on a machine that has no free USB port, or on a tablet that has none at all.

- The keyboard is read through the USB Host input adapter, and the result goes out through the BLE HID output adapter (`EspBleHidOutputAdapter`) as a HID over GATT keyboard + consumer + mouse.
- Everything the bridge can do on the way through still applies: remap, disable, momentary layers, text macros, layout conversion. This sketch adds a hold-CapsLock navigation layer, because a wireless keyboard usually ends up somewhere without a mouse.
- Lock state comes from the Bluetooth host: its LED output report is the authority, and the bridge forwards it back to the USB keyboard's LEDs. Press Caps Lock on the keyboard and the LED follows the host, not a guess.

## Only one USB port is needed

The PC side is Bluetooth, not USB, so only the keyboard side uses USB. That means a single-USB chip is enough — an **ESP32-S3**, not the ESP32-P4 the USB-to-USB examples (`SwapCtrlCapsLock`, `NavLayer`, ...) require.

## Hardware setup (ESP32-S3)

- Plug the keyboard into the ESP32-S3's native USB port (read as USB Host). The board has to supply bus power to the keyboard: use a board with a USB-A host connector, or a USB-OTG cable plus external 5V.
- Power the board from its UART port or a battery. The serial console on UART0 shows the BLE state.

## Pairing

The device advertises as `KeyBridge Keyboard`. Pair with it once from the host's Bluetooth settings; bonding keeps it paired, and the sketch re-advertises after a disconnect so the host can come back on its own.

## ble.update() and bridge.update()

EspBle delivers its callbacks from the context that calls `ble.update()`, so it belongs in the same `loop()` as `bridge.update()` — and before it, so a report that just arrived is part of this cycle:

```cpp
void loop() { ble.update(); bridge.update(); delay(1); }
```
