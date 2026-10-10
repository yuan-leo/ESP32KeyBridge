# Persistent local settings (ESP32-S3)

Phase 1 of the locally hosted configuration work provides the layout settings
and persistence layer. Phases 2 through 7's network view, editor, keyboard
navigation, macro settings, captive discovery, and open local access are demonstrated by
[`SettingsPortal`](../examples/practical/SettingsPortal/).

The Phase 3 API accepts a complete settings document at `PUT /api/v1/config`.
Every field is required exactly once; unknown fields, duplicates, JSON escapes,
unsupported layouts, reserved keyboard usages, trailing data, and documents
over 512 bytes are rejected. The portable decoder lives in
`ESP32KeyBridgeSettingsJson.h`, independent of the web transport.

## Stored settings

`KeyboardLayoutSettings` contains:

- the attached keyboard's engraving/input layout;
- the host operating system's layout;
- the keyboard usage that toggles conversion (`0` disables the hotkey); and
- whether layout conversion starts enabled.

The model represents one logical keyboard input. If an adapter merges several
physical keyboards, they share the same engraving layout.

Layout names are checked with `KeyboardLayout::byName()`. Toggle keys accept
only the named values exposed by `KeyboardUsage`, or zero. Failed setters leave
the previous value unchanged.

## Persistence and recovery

`KeyboardLayoutSettingsStore` is platform-independent and uses two blob slots.
Each save writes the slot that does not contain the newest valid record, then
reads it back and verifies its version, contents, and CRC-32. The previous
record therefore remains available if a write is interrupted or the newest
record is damaged.

`PreferencesLayoutSettingsStorage` supplies the ESP32 NVS backend. It stores
the slots as `layout_a` and `layout_b` in the `ekb` namespace.

At boot, `loadOrDefault()` returns one of these outcomes:

- `Loaded`: newest valid saved settings loaded;
- `LoadedRecovered`: the other slot was bad, but a valid record was recovered;
- `DefaultsNoSavedSettings`: first boot or settings were cleared;
- `DefaultsInvalidSettings`: saved records existed but neither was valid; or
- `DefaultsStorageError`: NVS could not be read.

Every non-loaded result leaves `KeyboardLayoutSettings` at safe defaults:
`en_us` input, `en_us` host, conversion disabled, and Pause as the conversion
toggle.

## Boot integration

```cpp
#include <ESP32KeyBridge.h>
#include <ESP32KeyBridgePreferences.h>
#include <ESP32KeyBridgeSettings.h>

esp32keybridge::ESP32KeyBridge bridge;
esp32keybridge::PreferencesLayoutSettingsStorage settingsNvs;
esp32keybridge::KeyboardLayoutSettingsStore settingsStore(settingsNvs);
esp32keybridge::KeyboardLayoutSettings layoutSettings;
esp32keybridge::ESP32KeyBridgeConfig config;

// Use the USB, BLE, or other S3 adapters appropriate for the product.
// `keyboard` and `output` below stand for those adapter instances.

void setup()
{
  esp32keybridge::LayoutSettingsError error;
  const esp32keybridge::LayoutSettingsLoadStatus loadStatus =
      settingsStore.loadOrDefault(layoutSettings, &error);
  (void)loadStatus; // Report or log recovery here if desired.

  esp32keybridge::InputConfig &keyboardConfig = config.addInputConfig();
  bridge.addInput(keyboard, keyboardConfig);
  bridge.addOutput(output);

  // Keep this call in the same context that will call bridge.update().
  layoutSettings.apply(bridge, config, keyboardConfig, &error);
}
```

When the web UI receives an edit, it builds and validates a complete candidate,
then saves and applies it only after every submitted field is accepted.
`Esp32S3SettingsWebServer::update()` handles HTTP requests synchronously, so
call it from the same loop context as `bridge.update()`; do not move settings
application to a network task.

```cpp
esp32keybridge::KeyboardLayoutSettings candidate = layoutSettings;
esp32keybridge::LayoutSettingsError error;

if (candidate.setInputLayoutName("en_us", &error) &&
    candidate.setHostLayoutName("ja_jp", &error) &&
    settingsStore.save(candidate, &error))
{
  layoutSettings = candidate;
  layoutSettings.apply(bridge, config, keyboardConfig, &error);
}
```

The host layout is a declaration of how the connected computer already
interprets HID keys. Applying this setting does not change the operating
system's keyboard layout.

## Wired-keyboard page navigation

`SettingsNavigationInputAdapter` decorates the raw keyboard input before it is
registered with the bridge. Place it inside side-effecting decorators such as
Bluetooth target hotkeys so captured settings keys cannot trigger them. Bind
the same navigation object to the web server:

```cpp
SettingsNavigationInputAdapter settingsKeyboard(keyboardTransport);
BluetoothHotkeyInputAdapter keyboard(settingsKeyboard, bluetoothTargets);
Esp32S3SettingsWebServer settingsWeb(layoutSettings, bridge);

bridge.addInput(keyboard, keyboardConfig);
settingsWeb.enableKeyboardNavigation(settingsKeyboard);
```

The page polls `GET /api/v1/navigation` while open. Each successful poll renews
a 1.2-second capture lease and receives at most one queued navigation action.
Capture starts only after the adapter observes that every key has been
released, avoiding a mid-chord transition in downstream decorators. During
capture, it withholds keys from the normal bridge output. When polling stops,
capture expires; any key still held remains suppressed until release so it
cannot appear as a fresh BLE key press. The event queue is fixed at 16 entries
and drops the oldest entry on overflow.

## Text macro settings

Phase 5 adds `KeyboardMacroSettings`, an independent full-table model with four
slots. A slot is either disabled (`triggerUsage: 0`, empty text) or binds one
unique F1-F24 key to 1-31 UTF-8 characters, with at most 124 encoded bytes.
Invalid UTF-8, duplicate triggers, other keys, control characters other than
Tab/newline/carriage return, and partially specified documents are rejected.

`KeyboardMacroSettingsStore` uses the same alternating-record strategy as the
layout store, but writes separate 532-byte `macro_a` / `macro_b` records in the
`ekb` NVS namespace. Each record is versioned, padded canonically, protected by
CRC-32, and read back before a save succeeds. Corruption of the newest record
recovers the previous valid macro table without affecting layout settings.

The complete JSON document is available at `GET /api/v1/macros` and replaced
with `PUT /api/v1/macros`. Persistence is verified before
`ESP32KeyBridgeConfig::clearTextMacros()` and the new table are applied. Failed
validation leaves both active and persisted macros unchanged.

Function-key triggers are consumed by the bridge and the macro text is encoded
using the configured host layout. Use the phone's text input to edit macro
content; the attached wired keyboard navigation channel moves between controls
and activates buttons, but intentionally does not forward printable keys into
browser fields while capture is active.

## Captive-portal discovery

Phase 6 adds opt-in discovery to `SettingsAccessPointConfig`:

```cpp
esp32keybridge::SettingsAccessPointConfig accessPoint;
accessPoint.ssid = "KeyBridge-Setup";
accessPoint.captivePortal = true;
settingsWeb.begin(accessPoint);
```

When enabled, a wildcard DNS responder resolves requested names to the
ESP32-S3 SoftAP address. Unknown non-API GET paths, including common operating
system connectivity probes, receive a 302 redirect to that fixed local IP.
The redirect never reflects the request's `Host` header. Registered routes
behave normally; unknown `/api/` routes and non-GET requests retain JSON 404
responses. `update()` services DNS alongside HTTP, and `end()` stops both.

The `KeyBridge-Setup` Wi-Fi and website are intentionally open: there is no
Wi-Fi password and no HTTP login. Anyone connected to the setup network can
view and change settings. Use the portal only where physical proximity is an
acceptable access boundary, and turn the device off when configuration access
should not be available.
# Bluetooth profiles

`UsbKeyboardToBle` can optionally bind its shared Bluetooth controller to the
settings server. The page then shows all three remembered host slots, the
selected and connected slot, full bonded address, persistent local label,
best-effort host-reported Bluetooth name, and any operation. When the host does
not expose a usable name, the page shows `Not reported by host`. Only one BLE HID host is connected at a
time. The three-slot limit comes from EspBle's bundled NimBLE bond store.

`Scroll Lock+1`, `+2`, or `+3` selects that exact slot. An occupied slot starts
a directed reconnect; an empty slot is selected without starting pairing.
`Scroll Lock+0` pairs the selected slot. Pairing can be cancelled and expires
after 120 seconds. Replacing a slot removes its old bond before advertising,
so cancel or timeout can leave it empty. Delete retains the label, never starts
pairing, and can briefly disconnect the active host because EspBle cannot
delete any bond while a BLE connection is open.

If an operation becomes stuck or the page reports busy, use **Stop / Force disconnect**.
It stops advertising or disconnects the current host, suppresses automatic
reconnect, and returns control to idle. Directed reconnect attempts also stop
automatically after 30 seconds; pairing stops after 120 seconds.

Open endpoints are:

- `GET /api/v1/bluetooth` — current overall and three-slot snapshot.
- `PUT /api/v1/bluetooth/label` — `{"slot":1,"label":"Laptop"}`.
- `POST /api/v1/bluetooth/action` —
  `{"slot":1,"action":"connect","confirm":false}`. Actions are `connect`,
  `pair`, `cancel`, `delete`, and `stop`; pair/delete require `confirm:true`.
- `POST /api/v1/reset` — confirmed `bluetooth` or `everything` scope.

Labels are editable UTF-8 metadata of at most 24 bytes. The MAC address is the
saved peer identity. The host name is learned independently from the remote GAP
Device Name characteristic and may remain unavailable.

**Bluetooth only** deletes every BLE bond, profile, label, learned name, and
MAC address, selects Profile 1, and restarts. **Reset everything** also clears
the saved keyboard layouts and text macros before restarting. Duplicate reset
requests during the restart window are rejected.
