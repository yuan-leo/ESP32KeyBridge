# SettingsPortal

[日本語](README.ja.md)

ESP32-S3-hosted settings website bring-up. It starts an open SoftAP and serves a
responsive, offline page for editing the persisted layout configuration and
four F1-F24 text macros while viewing live bridge status. Saves are strictly validated, written to the
redundant NVS store, read back, and then applied to the bridge.

This standalone bring-up sketch does not attach a keyboard, so its navigation
endpoint reports unavailable. The `UsbKeyboardToBle` example binds the Phase 4
wired-keyboard navigation adapter. Phase 6 captive-portal discovery is enabled:
the phone should offer or open the local settings page after joining the Wi-Fi.

## Try it

1. Flash the sketch to an ESP32-S3 and open its serial console at 115200 baud.
2. Connect a phone to the open Wi-Fi network `KeyBridge-Setup`.
3. Accept the phone's settings-page prompt. If it does not appear, browse to
   `http://192.168.4.1/` or the address printed on the serial console.
4. No Wi-Fi password or website login is required. Anyone connected can change settings.

The page and APIs are compiled into firmware and use no internet-hosted assets:

- `GET /` — settings page
- `GET /api/v1/config` — persisted layout settings and edit capability
- `PUT /api/v1/config` — validate, save, and atomically apply settings
- `GET /api/v1/layouts` — supported layout names
- `GET /api/v1/macros` — complete persisted macro table
- `PUT /api/v1/macros` — validate, save, and atomically replace macros
- `GET /api/v1/navigation` — renew keyboard capture and receive one action
- `GET /api/v1/status` — live conversion, client count, and overflow status
- `GET /healthz` — plain-text health check

With `accessPoint.captivePortal = true`, wildcard DNS maps requested host names
to the ESP32-S3. Unknown non-API GET paths used by phone connectivity checks
redirect to the fixed SoftAP address. Unknown `/api/` paths and non-GET
requests remain 404 responses. The network and page are intentionally open;
physical proximity is the access boundary. This standalone example has no
Bluetooth controller, so reset controls report unavailable.

Call `settingsWeb.update()` frequently. In a real keyboard sketch, keep
`bridge.update()` (and `ble.update()` where applicable) ahead of it so keyboard
processing remains the primary loop work.
