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
- Join the open `KeyBridge-Setup` network from an Android or iOS phone.
- Expect the phone to offer or open the ESP32 KeyBridge settings page. If the
  OS suppresses its captive prompt, open any plain-HTTP site and expect a
  redirect to `http://192.168.4.1/`.
- Verify there is no Wi-Fi password and no website login.
- Verify the settings page works without internet access and an unknown
  `/api/v1/...` path still returns JSON 404 rather than the HTML page.
- From a second connected client, request `/api/v1/status` and verify it is
  reachable, confirming that anyone connected can change settings.

# Bluetooth profile acceptance

With `UsbKeyboardToBle` flashed, verify the website and keyboard agree for
Profiles 1, 2, and 3. For each empty slot, `Scroll Lock+N` must select it without
pairing and the page must show it selected. For each occupied slot it must start
a directed reconnect. Verify reconnect stops after 30 seconds, Pair/Replace,
Cancel Pairing, the 120 seconds pairing timeout, label persistence after reboot,
deleting active and inactive slots, and that full addresses and
Selected/Connected states remain attached to the same profile after reboot.
Verify the editable label, MAC, and remote name are distinct; if the host does
not report its name, expect `Not reported by host`. Start an operation, press
**Stop / Force disconnect**, and verify the page returns to idle and remains
editable. Verify **Bluetooth only** preserves layouts/macros while erasing bonds
and profiles, and **Reset everything** clears all three domains and restarts.

## HW-678 v0.0.0 status LED

On the GPIO 48 WS2812, verify the white/red/green/blue boot self-test, profile
colors (Profile 1: blue, Profile 2: green, Profile 3: magenta), solid connected,
breathing reconnect, color/white pairing, brief disconnected flash, fast amber
reset pulse, and triple red error pattern. Confirm the separate PWR, TX, and RX
LEDs remain hardware-controlled.

