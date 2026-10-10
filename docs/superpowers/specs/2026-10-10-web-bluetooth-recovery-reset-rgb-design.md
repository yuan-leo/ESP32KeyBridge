# Web Bluetooth Recovery, Reset, and HW-678 Status Design

## Purpose

Make the ESP32-S3 KeyBridge recoverable and understandable without serial-console access. A nearby phone must be able to join the settings network without credentials, interrupt a stuck Bluetooth operation, manage profiles, reset either Bluetooth data or all user settings, and decode the selected profile and Bluetooth state from the HW-678 board's programmable LED.

## Supported Hardware and Roles

- The only supported processor is ESP32-S3.
- The reference board is HW-678 v0.0.0.
- Exactly one wired USB-OTG input device is in scope.
- Bluetooth is output-only: the ESP32-S3 presents a BLE HID keyboard to computers, phones, or tablets.
- The HW-678 has one firmware-controlled WS2812 RGB LED on GPIO 48. Its separate power, transmit, and receive indicators retain their hardware-defined behavior.

## Local Settings Access

The reference firmware creates an open `KeyBridge-Setup` Wi-Fi access point. It has no WPA password, portal username, portal password, HTTP Basic authentication, or HTTP Digest authentication. Every settings page and API route is accessible to a device joined to that local network.

The documentation must clearly state that anyone within Wi-Fi range can change settings or reset the device while the access point is active.

Authentication fields and validation are removed from the public settings-web configuration rather than retained as inactive options. An empty access-point password starts an open ESP32 SoftAP.

## Bluetooth Profile Identity

Each of the three output profiles exposes these independent values:

1. A user-editable label of at most 24 UTF-8 bytes.
2. A best-effort remote host name reported by the connected phone, computer, or tablet.
3. The full stored Bluetooth MAC address.
4. Selection, connection, and operation state.

After a connection is encrypted, the controller attempts to read the remote Generic Access service Device Name characteristic (`0x1800` / `0x2A00`). A valid non-empty UTF-8 result is saved with the matching profile. Failure, timeout, absence of the service, or invalid data does not fail or delay HID operation. The website displays `Not reported by host` when no usable name has been obtained.

Bluetooth profile persistence advances to a new record version that includes the remote host name. Existing version-1 records migrate in place: user labels, MAC addresses, address types, and the selected slot are preserved, while the new host-name field starts empty. Newly saved records retain the existing redundant-record and read-back verification behavior.

## Bluetooth Recovery

The current permanent `bluetooth_busy` failure is caused by directed reconnect entering the connecting operation without a controller-level timeout. The website then disables every profile action whenever any operation is active, leaving no recovery control.

Remembered-host reconnect has a 30-second timeout. Pairing retains its 120-second timeout. On reconnect timeout, the controller stops advertising, clears the pending operation, preserves every profile, and enters an intentionally disconnected idle state. It must not immediately restart the same directed reconnect.

The Bluetooth controller adds an explicit force-disconnect operation. It is accepted while idle, connected, switching, pairing, deleting, or failed. It:

- stops advertising;
- cancels pending operations;
- requests disconnection of the active peripheral connection, if one exists;
- clears transient error and busy state once disconnection completes;
- preserves bonds, profile metadata, and selected profile; and
- leaves Bluetooth intentionally disconnected until the user selects Connect or Pair.

The web interface always leaves `Stop / Force disconnect` enabled when Bluetooth management is available. Other buttons are disabled only when their individual action would conflict. A forced stop therefore permits immediate rename, delete, reconnect, or pair/replace after the controller reaches idle.

API failures use actionable names and messages. The raw internal string `bluetooth_busy` is not presented as the only recovery guidance.

## Reset Operations

The website provides two separately confirmed destructive actions.

### Reset Bluetooth Only

This operation:

- force-disconnects and stops Bluetooth activity;
- deletes all BLE bonds;
- clears all three profile records, user labels, learned host names, and stored MAC addresses;
- restores selected profile 1; and
- reboots the ESP32-S3.

Layout and macro settings remain unchanged.

### Reset Everything

This operation performs the Bluetooth-only reset and additionally clears persisted keyboard-layout and macro settings. On reboot, the compiled firmware defaults are loaded and applied. It resets user configuration; it does not erase or replace the firmware image.

Each reset requires a confirmation whose text lists what will be erased. The HTTP response is completed before a short deferred restart so the browser can report that the reset was accepted. If any required storage or bond deletion fails, the endpoint reports the failing category and does not claim reset success.

## Website Behavior

Each profile card displays the editable label, host-reported name or its fallback, MAC address, and current state. Polling continues during operations and after recoverable errors.

The Bluetooth section includes:

- Connect;
- Pair / Replace;
- Delete;
- Rename;
- Cancel Pairing when applicable; and
- Stop / Force disconnect as an independent recovery control.

A separate reset section contains `Reset Bluetooth only` and `Reset everything`. Destructive controls use distinct warning styling and cannot be activated by a single unconfirmed request.

The page also shows the current RGB status description and a permanent legend matching firmware behavior.

## HW-678 RGB Status

At startup, the GPIO 48 WS2812 performs a brief red, green, then blue self-test. This identifies the programmable LED and verifies its three channels. The power, transmit, and receive LEDs are never driven by the application.

Profile color encodes the selected output profile:

- Profile 1: blue.
- Profile 2: green.
- Profile 3: magenta.

Animation encodes state:

- Solid profile color: connected.
- Slowly breathing profile color: searching or reconnecting to a remembered host.
- Alternating profile color and white: open for pairing.
- One brief profile-color flash every three seconds: selected and intentionally disconnected.
- Fast amber pulse: deleting data or resetting.
- Three red flashes followed by a pause: Bluetooth error.
- White startup pulse followed by the red/green/blue self-test: booting.
- Off: RGB support or Bluetooth initialization is unavailable.

Reset and error indications take priority over ordinary Bluetooth state. Pattern selection and RGB calculations are hardware-independent and deterministic from a status snapshot and elapsed milliseconds. The sketch supplies the resulting color to the Arduino ESP32 RGB API for GPIO 48.

## Component Boundaries

The Bluetooth target controller owns operation state, timeouts, remote-name acquisition, force-disconnect, bond removal, and Bluetooth profile reset. It exposes snapshots and commands without depending on HTTP or HTML.

A reset coordinator owns the two reset scopes. It receives the Bluetooth controller and enabled settings stores, performs the requested clears, and schedules restart only after a successful API response.

The RGB pattern engine consumes Bluetooth/reset state and time, and produces an RGB value plus a human-readable pattern identifier. It has no Arduino dependency. The HW-678 adapter writes that value to GPIO 48.

The web server translates HTTP requests into those interfaces and renders snapshots. It does not directly alter BLE internals or storage namespaces.

## Error Handling

- Remote-name discovery is optional and never blocks keyboard output.
- Reconnect and pairing timeouts leave a recoverable state.
- Force-disconnect is idempotent.
- Storage writes retain read-back verification.
- Partial reset failures are reported explicitly and do not schedule reboot.
- A reset request already in progress rejects duplicate reset requests.
- Restart is deferred until the response has been sent.
- Invalid, oversized, or non-UTF-8 host names are discarded and displayed as unavailable.

## Verification

Host-side and compile tests cover:

- migration of version-1 Bluetooth records to the new host-name record;
- valid, missing, invalid, and oversized remote host names;
- force-disconnect from idle, connected, switching, pairing, deleting, and failed states;
- the 30-second reconnect and 120-second pairing deadlines;
- Bluetooth-only reset preserving layout and macros;
- complete reset clearing layout, macros, profiles, and bonds;
- failure of each reset dependency preventing a success response and reboot;
- all profile-color and animation combinations, including priority rules;
- open SoftAP configuration with all web authentication removed;
- action-specific web controls and confirmation requirements;
- API serialization of labels, host names, MAC addresses, state, and RGB legend identifiers; and
- successful ESP32-S3 compilation of `UsbKeyboardToBle` and `SettingsPortal`.

Hardware verification on the HW-678 confirms the GPIO 48 red/green/blue self-test, the complete status legend, open Wi-Fi access, both reset scopes, force recovery from an unavailable remembered host, and firmware upload through the detected serial port.
