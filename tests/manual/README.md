# Manual Tests

Manual checks are used when physical wiring, host OS behavior, Bluetooth pairing, or visual confirmation is part of the test.

Examples:

- GPIO matrix wiring.
- Real USB keyboard / barcode reader input.
- USB hub with multiple keyboards.
- USB HID keyboard recognition by a host OS.
- BLE HID pairing and reconnect.

## ESP32-S3 captive-portal discovery

- Flash `SettingsPortal` or `UsbKeyboardToBle` to an ESP32-S3.
- Join `KeyBridge-Setup` from an Android or iOS phone using the example
  password `keybridge`.
- Expect the phone to offer or open the ESP32 KeyBridge settings page. If the
  OS suppresses its captive prompt, open any plain-HTTP site and expect a
  redirect to `http://192.168.4.1/`.
- Verify the settings page works without internet access and an unknown
  `/api/v1/...` path still returns JSON 404 rather than the HTML page.

