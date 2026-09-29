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
- Expect a login challenge. Verify an incorrect login is rejected, then sign in
  with the example username `admin` and password `configure`.
- Verify the settings page works without internet access and an unknown
  `/api/v1/...` path still returns JSON 404 rather than the HTML page.
- In a private/incognito browser session, request `/api/v1/status` directly and
  verify it returns 401 until valid portal credentials are entered.

