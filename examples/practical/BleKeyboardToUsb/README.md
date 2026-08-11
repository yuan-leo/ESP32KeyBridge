# BleKeyboardToUsb

[日本語](README.ja.md)

The other direction: use a **Bluetooth keyboard on a machine that will not pair with one**. A BIOS/UEFI screen, a KVM switch, an instrument, a game console, an old PC. The ESP32 connects to the BLE keyboard as a Bluetooth host and appears to the PC as an ordinary wired USB keyboard — no drivers, no pairing on the PC side.

- The keyboard is read through `EspBleHidHostKeyboardInputAdapter` (HID over GATT central) and forwarded to the PC through the USB Device HID output adapter.
- A BLE mouse is picked up as well by `EspBleHidHostMouseInputAdapter`: buttons become mouse buttons, movement and wheel become relative axes, and the PC gets a plain USB mouse too. A combo keyboard+mouse device works through one connection.
- Lock state flows the other way: the PC's Caps/Num/Scroll LED report is the authority and reaches the BLE keyboard's LEDs through the input adapter. Unlike the USB Host side, the BLE path also carries the JIS Kana LED.

## Only one USB port is needed

The keyboard side is Bluetooth, not USB, so only the PC side uses USB — a single-USB **ESP32-S3** is enough.

## Hardware setup (ESP32-S3)

- Plug the ESP32-S3's native USB port into the PC (it enumerates as the USB HID device).
- The serial console on UART0 shows the connection state.

## Pairing

Put the keyboard in pairing mode and reset the board. The sketch connects to the first connectable device advertising the HID service (`0x1812`), pairs, and bonds. After that `setAutoReconnect(true)` + `setAutoRediscover(true)` bring it back on their own.

Connection setup stays in the sketch on purpose — the adapters only consume HID reports. Filter `onResult` by name or address instead of "first HID device found" when more than one keyboard is in range.

## ble.update() and bridge.update()

EspBle delivers its callbacks from the context that calls `ble.update()`, so it belongs in the same `loop()` as `bridge.update()` — and before it, so a report that just arrived is part of this cycle:

```cpp
void loop() { ble.update(); bridge.update(); delay(1); }
```
