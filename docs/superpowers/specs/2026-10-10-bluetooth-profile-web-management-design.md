# Bluetooth Profile Web Management Design

## Purpose

Extend the ESP32-S3 local settings website so a user can monitor and manage
the three remembered Bluetooth host profiles without relying only on keyboard
hotkeys. The website and `Scroll Lock` hotkeys must operate on one shared
controller and always report the same selected profile and operation state.

Success means the user can:

- See the number of currently connected Bluetooth hosts (0 or 1).
- See all three profile slots, including their bond state, selected/connected
  state, full peer address, and persistent editable label.
- Select and reconnect a remembered profile.
- Start replacement pairing, cancel pairing, and see a two-minute countdown.
- Delete a profile without automatically entering pairing mode.
- Continue using `Scroll Lock + 1/2/3` and `Scroll Lock + 0` consistently.
- Reboot without losing profile-to-address association or editable labels.

The settings SoftAP is independent of the BLE HID link. A BLE disconnect
caused by switching or deleting a profile must not interrupt the browser.

## Scope

This feature applies to the BLE HID peripheral flow demonstrated by
`UsbKeyboardToBle`. In this design, “profile” means one of its three remembered
host slots, not a Bluetooth HID service such as keyboard, mouse, or consumer
controls.

Friendly names learned from remote hosts are not available in the current
EspBle bond record. The website therefore identifies a host with both a
user-editable label and its full Bluetooth address.

## Architecture

### Shared controller boundary

`BluetoothTargetController` becomes the single transport-neutral boundary for
both the keyboard hotkey decorator and the web server. It exposes:

- A complete immutable snapshot of overall state and all profile slots.
- Asynchronous commands to select/connect, pair/replace, cancel pairing, and
  delete a profile.
- A bounded label update operation.
- `update()` for timeouts and deferred state transitions.

`EspBleHidTargetController` implements those operations. The website never
calls EspBle directly. This preserves transport isolation and permits host-side
tests with a real in-memory controller implementation or focused fake.

The existing keyboard decorator continues to call the same controller. Web and
keyboard commands are serialized by the controller; only one mutating BLE
operation may be active at a time.

### Profile snapshot

The overall snapshot contains:

- Availability.
- Connected-device count (0 or 1 for this HID peripheral design).
- Selected profile number.
- Controller status: idle, switching, pairing, connected, deleting, or failed.
- Active operation, if any.
- Pairing time remaining in seconds.
- Last stable error code suitable for API/UI display.

Each of three profile snapshots contains:

- One-based profile number.
- Persistent label.
- Full peer address when occupied.
- `occupied`, `selected`, and `connected` flags.
- Derived display state: empty, remembered, selected, switching, pairing,
  connected, deleting, or failed.

Snapshots are copied into the response; the web layer does not retain pointers
to mutable controller state.

## Persistent Profile Registry

A separate versioned, checksummed NVS record stores three profile records. Each
record contains:

- Last known peer address and address type.
- An optional UTF-8 user label, limited to 24 bytes and guaranteed to be
  null-terminated in memory.

The storage follows the repository's existing redundant two-record pattern:
alternating generations, CRC validation, read-back verification, fallback to
the previous valid record, and safe defaults when neither record is valid.

At startup, the controller reconciles stored addresses with EspBle bonds:

1. Match every stored address to the corresponding current bond and preserve
   its profile number and label.
2. Clear stale addresses whose bonds no longer exist, retaining the user label
   so an intentionally named empty profile remains recognizable.
3. Assign any unrecognized bonds to the lowest empty profiles in deterministic
   bond enumeration order.
4. Persist the reconciled registry only when its contents changed.

This prevents a bond enumeration reorder from moving “Laptop” from Profile 1
to another profile after reboot.

## Controller State and Operations

All operations are asynchronous and advance from `update()` and EspBle event
callbacks. A command is rejected while another mutating operation is active.

### Select/connect

Selecting Profile 1, 2, or 3 always changes `selectedSlot`, including when the
profile is empty.

- Occupied profile: disconnect the current BLE HID host if necessary, apply
  the profile-specific advertised name, and start directed advertising to the
  stored bond.
- Empty profile: update selection and advertised-name state only. Do not start
  pairing automatically and do not treat the selection as a failure.

This is also the required hotkey behavior:

- `Scroll Lock + 1/2/3` selects three distinct profiles.
- `Scroll Lock + 0` starts pairing in the currently selected profile.

The current hotkey decoder already maps the three digit usages distinctly in
unit coverage. The present controller rejects empty-slot selection, which can
make selection appear stuck on Profile 1 when the occupancy is unknown. The
implementation must add failing tests for both empty and occupied profiles
before changing that behavior, and hardware verification must cover both paths.

### Pair/replace

Pairing selects the requested profile, disconnects the current BLE link, and
permanently removes the profile's existing bond before ordinary discoverable
advertising begins. The profile-specific advertised name is applied first.

Pairing expires after 120 seconds. Cancel or timeout stops ordinary pairing
advertising. If the replaced bond was already removed, the profile remains
empty; cryptographic bond material cannot be restored. On successful bonded
security, the new address is assigned to the profile and persisted without
discarding its label.

### Delete

Delete clears the bond/address but retains the editable label. It never enters
pairing mode automatically.

EspBle refuses bond deletion while any BLE connection exists. Therefore:

- Deleting the connected profile disconnects it, deletes the bond, and leaves
  the selected profile empty and idle.
- Deleting an inactive profile may briefly disconnect the active host, delete
  the requested bond, and reconnect the previously selected occupied profile.

The UI warns about this temporary disconnect before sending the command.

### Cancel pairing

Cancel is valid only while pairing. It stops discoverable advertising and
leaves the selected profile empty or remembered according to the actual bond
state. A stale cancel request returns a conflict instead of altering another
operation.

### Failure handling

Failures move the controller to a stable failed state containing a bounded
error code. The slot registry is persisted only after the corresponding bond
operation succeeds. A later valid command clears the displayed error. No web
request blocks while waiting for a BLE disconnect, security event, or timeout.

## Authenticated HTTP API

All new endpoints use the settings server's existing HTTP Digest authorization,
no-store and security headers, and same-origin policy.

### `GET /api/v1/bluetooth`

Returns the complete snapshot:

```json
{
  "available": true,
  "connectedCount": 1,
  "selectedSlot": 2,
  "status": "connected",
  "operation": "none",
  "pairingSecondsRemaining": 0,
  "error": "",
  "profiles": [
    {
      "slot": 1,
      "label": "Laptop",
      "address": "AA:BB:CC:DD:EE:FF",
      "occupied": true,
      "selected": false,
      "connected": false,
      "state": "remembered"
    }
  ]
}
```

When no controller is bound, the route returns `available: false` with an empty
profile list rather than exposing transport-specific behavior.

### `PUT /api/v1/bluetooth/label`

Accepts exactly:

```json
{"slot": 1, "label": "Laptop"}
```

The slot must be 1 through 3. The label may be empty, must be valid UTF-8, and
must contain at most 24 bytes. Unknown fields, trailing data, or malformed JSON
are rejected. A successful update is persisted with read-back verification and
returns the updated snapshot.

### `POST /api/v1/bluetooth/action`

Accepts exactly:

```json
{"slot": 1, "action": "connect", "confirm": false}
```

Valid actions are `connect`, `pair`, `cancel`, and `delete`. Pair and delete
require `confirm: true`; the API does not rely only on browser confirmation.
Cancel targets the active pairing operation and must identify its selected
slot. Request bodies are strictly bounded and validated.

Response behavior:

- `200`: synchronous label update, cancel, or no-op state response.
- `202`: accepted asynchronous connect, pair, or delete operation, with the
  updated snapshot.
- `400`: malformed body, invalid slot/action, label, or confirmation field.
- `404`: Bluetooth management is not configured.
- `409`: empty profile cannot connect, controller busy, stale cancel, or
  operation conflicts with current state.
- `500`: storage or controller failure.

## Website

A responsive “Bluetooth Profiles” section appears near the top of the existing
page. It is rendered from the status snapshot and polls once per second.

The summary shows:

- “0 devices connected” or “1 device connected”.
- Selected profile.
- Overall state and active operation.
- Pairing countdown when applicable.

Three profile cards show profile number, label input, full address, and
Selected/Connected/state badges. Controls are state-derived:

- Occupied: Connect, Pair/Replace, Delete, Save Label.
- Empty: Pair, Save Label; Connect is unavailable.
- Pairing profile: Cancel Pairing and countdown.
- Busy controller: conflicting controls are disabled on all cards.

Pair/Replace confirmation states that the old bond is removed immediately and
cancel/timeout may leave the profile empty. Delete confirmation states that the
current Bluetooth host may briefly disconnect. The API's `confirm` field is set
only after the corresponding dialog is accepted.

Success messages are concise. Failures remain visible until the next user
action. Poll failures show the monitor as temporarily unavailable without
discarding form input currently being edited.

The cards remain usable on a phone and through existing wired-keyboard page
navigation. Controls use native buttons and inputs so Tab, arrows, Enter, and
Space work without a custom interaction model.

## Integration

The web server gains an optional `enableBluetoothManagement(...)` binding that
must be called before `begin()`. The referenced controller and profile store
must outlive the server. `UsbKeyboardToBle` binds the existing
`EspBleHidTargetController` and profile registry to the settings server.

The main loop order remains:

1. `ble.update()` delivers connection/security events.
2. `bridge.update()` updates keyboard input, including the shared Bluetooth
   controller via the hotkey decorator.
3. `settingsWeb.update()` serves API/UI requests.

Controller `update()` must be idempotent within a loop because it can be
reached by the keyboard decorator and management binding.

## Testing

Implementation follows test-driven development. Required automated coverage:

- Registry encoding, CRC/version validation, redundant persistence, recovery,
  label bounds/UTF-8 validation, and address reconciliation after bond reorder.
- Controller snapshots for all empty/remembered/selected/connected/busy states.
- Distinct `Scroll Lock + 1`, `+2`, and `+3` commands.
- Empty-profile selection followed by `Scroll Lock + 0` pairing.
- Occupied-profile selection and directed reconnect.
- Pair success, replacement, cancel, and 120-second timeout.
- Delete active profile, delete inactive profile with active-host reconnect,
  and storage failure handling.
- Strict JSON label/action parsing, confirmation enforcement, HTTP status
  mapping, and exact status encoding.
- Existing hotkey tap/chord behavior, LED feedback, layout settings, macros,
  navigation, captive portal, and authentication remaining unchanged.
- Repository structure checks for all new source and documentation files.

Verification before completion:

- Run the full available unit suite and ESP32-S3 core smoke build.
- Compile `UsbKeyboardToBle` with its pinned dependency profile.
- Upload to the attached ESP32-S3.
- On hardware, verify website polling and controls, all three hotkeys with both
  empty and occupied profiles, two-minute pairing timeout, cancellation,
  deletion, temporary reconnect, label persistence, and reboot reconciliation.

## Documentation

Update the English and Japanese practical example guides, settings API guide,
basic guide, data model, test plan, scope, and changelog. Document that only one
BLE HID host can be connected at a time, profile labels are local metadata, a
host may cache advertised device names, replacement pairing cannot restore the
old bond after cancel/timeout, and deleting an inactive profile can briefly
disconnect the active host.

## Out of Scope

- More than three NimBLE bonds.
- Simultaneous BLE HID connections to multiple remembered hosts.
- Discovering or persisting the remote host's friendly Bluetooth name.
- Managing Bluetooth Classic profiles from this page.
- Forcing a host operating system to refresh a cached advertised device name.
