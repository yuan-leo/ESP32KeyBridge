# Persistent layout settings (ESP32-S3)

Phase 1 of the locally hosted configuration work provides the settings and
persistence layer. Phases 2 and 3's network view and editor are demonstrated by
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
