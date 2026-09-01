# ButtonsToClassic

[日本語](README.ja.md)

Turns three GPIO switches into a Bluetooth Classic (BR/EDR) keyboard/media controller for a legacy PC, console, or industrial Host that does not accept BLE HID. Prefer BLE when the Host supports it: BLE works on more ESP32 SoCs and uses less power.

## Hardware

This is for the **original ESP32 only**. ESP32-S3/C3/C6/H2/P4 have no Bluetooth Classic radio. Connect switches between GPIO 25/26/27 and GND (internal pull-ups, active low).

## Composition and pairing

`EspBleClassicHidOutputAdapter` sends the same keyboard 6KRO, one consumer usage, and mouse report as the BLE output, but as a Classic HID Device. Construct it before `bluetooth.begin()`, then pair with `KeyBridge Classic` from the Host's Bluetooth settings.

Classic stores the Report Descriptor and identity strings in one SDP record with a 214-byte limit. The adapter's three profiles fit with the default strings; unusually long strings in `EspBleClassicHidProfileConfig` can make `begin()` fail.

Callbacks are dispatched by `bluetooth.update()`, so call it before `bridge.update()` in the same `loop()`.
