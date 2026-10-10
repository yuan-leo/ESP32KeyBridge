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

# Bluetooth profile acceptance

With `UsbKeyboardToBle` flashed, verify the website and keyboard agree for
Profiles 1, 2, and 3. For each empty slot, `Scroll Lock+N` must select it without
pairing and the page must show it selected. For each occupied slot it must start
a directed reconnect. Verify Pair/Replace, Cancel Pairing, the 120-second
timeout, label persistence after reboot, deleting active and inactive slots,
and that full addresses and Selected/Connected states remain attached to the
same profile after reboot.

