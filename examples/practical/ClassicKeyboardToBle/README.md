# ClassicKeyboardToBle

[日本語](README.ja.md)

Makes an old Bluetooth Classic-only HID keyboard usable by a BLE HID Host. `EspBleClassicHidHost*InputAdapter` reads the Classic keyboard/mouse and the existing `EspBleHidOutputAdapter` publishes the result over BLE.

## Constraints

- **Original ESP32 only**; no other ESP32 SoC has a Classic radio.
- Replace `keyboardAddress`. Classic connects by Bluetooth address instead of scanning a BLE service advertisement. EspBle's `Classic/Inquiry` example can find it.
- Classic HID Host supports one link. Both adapters work when keyboard and mouse reports come from one composite device.
- EspBle 1.3.0 decodes keyboard and mouse reports on the Classic Host side. Consumer/gamepad reports reach the raw callback but are not inputs to these adapters.
- Running BLE and Classic together is experimental dual-host mode in EspBle. Use it only when the conversion genuinely needs both transports.
- Two host stacks consume substantial flash. With arduino-esp32 3.3.11's default partition this example uses about 97% of the app slot. Select a partition scheme with a larger app for product-specific additions.

Classic has no counterpart to BLE's `setAutoReconnect()` / `setAutoRediscover()`. A product must decide when to call `connect(address)` again after a disconnect; this minimal example connects once at startup.
