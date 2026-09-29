# SettingsPortal

[日本語](README.ja.md)

ESP32-S3-hosted settings website bring-up. It starts a WPA2 SoftAP and serves a
responsive, offline page for editing the persisted layout configuration and
viewing live bridge status. Saves are strictly validated, written to the
redundant NVS store, read back, and then applied to the bridge.

This standalone bring-up sketch does not attach a keyboard, so its navigation
endpoint reports unavailable. The `UsbKeyboardToBle` example binds the Phase 4
wired-keyboard navigation adapter. Captive-portal DNS is not implemented.

## Try it

1. Flash the sketch to an ESP32-S3 and open its serial console at 115200 baud.
2. Connect a phone to Wi-Fi network `KeyBridge-Setup` with password
   `keybridge`.
3. Browse to `http://192.168.4.1/` or the address printed on the serial console.

The development credentials are intentionally visible in the sketch. Change
them before deploying a device. `SettingsAccessPointConfig` rejects open or
short-password access points; passwords must contain 8 to 63 bytes.

The page and APIs are compiled into firmware and use no internet-hosted assets:

- `GET /` — settings page
- `GET /api/v1/config` — persisted layout settings and edit capability
- `PUT /api/v1/config` — validate, save, and atomically apply settings
- `GET /api/v1/layouts` — supported layout names
- `GET /api/v1/navigation` — renew keyboard capture and receive one action
- `GET /api/v1/status` — live conversion, client count, and overflow status
- `GET /healthz` — plain-text health check

Call `settingsWeb.update()` frequently. In a real keyboard sketch, keep
`bridge.update()` (and `ble.update()` where applicable) ahead of it so keyboard
processing remains the primary loop work.
