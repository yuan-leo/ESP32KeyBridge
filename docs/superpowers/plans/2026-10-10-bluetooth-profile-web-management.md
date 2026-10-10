# Bluetooth Profile Web Management Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add reliable three-slot Bluetooth profile monitoring and management to the local settings website, make Scroll Lock+1/2/3 select empty as well as occupied slots correctly, and verify the finished firmware on the attached ESP32-S3.

**Architecture:** Extend `BluetoothTargetController` into the single transport-neutral command and snapshot boundary used by both hotkeys and HTTP. Store stable slot-to-bond metadata and user labels in a redundant versioned NVS record, while `EspBleHidTargetController` reconciles that registry with the bonds EspBle actually reports and runs all connect, pair, cancel, and delete operations asynchronously from `update()`. The web server remains optional: `enableBluetoothManagement(...)` binds the controller before `begin()`, authenticated handlers expose strict JSON endpoints, and the embedded page polls the snapshot once per second.

**Tech Stack:** Arduino C++17, ESP32 Arduino `Preferences`, EspBle/NimBLE, `WebServer`, embedded HTML/CSS/JavaScript, pytest, pytest-embedded, Arduino CLI.

**Spec:** `docs/superpowers/specs/2026-10-10-bluetooth-profile-web-management-design.md`

## Global Constraints

- Preserve the public three-slot limit imposed by the bundled EspBle bond store.
- Slot numbers are one-based everywhere visible to users and in controller calls.
- Selecting an empty slot changes selection only. It never starts pairing.
- Pairing starts only through Scroll Lock+0 or an explicit confirmed web action.
- At most one Bluetooth-mutating operation may run at a time.
- Pairing expires after 120 seconds and can be cancelled explicitly.
- Pair and delete require `confirm: true`; label edits and connect do not.
- Bluetooth web management is opt-in and must not break sketches that do not bind a controller.
- All web endpoints use the existing HTTP Digest protection when configured.
- Avoid heap-heavy JSON libraries; follow the repository's bounded-buffer and strict-parser patterns.
- Keep English and Japanese user documentation aligned.
- Do not erase unrelated NVS data or user worktree changes.

## Review Focus

- Slot identity must remain stable across reboot even when EspBle returns bonds in a different order.
- Delete of an inactive slot must safely handle EspBle's restriction that no bond can be deleted while any peripheral connection exists, then restore the prior selected connection where appropriate.
- Cancel and timeout must close discoverable advertising without silently reconnecting or pairing another host.
- A failed persistence write must not report a successful rename, replacement, or deletion.
- The embedded page must escape all device-controlled strings and remain keyboard-accessible.
- Hotkey tests must distinguish Digit1, Digit2, and Digit3 and cover both empty and occupied target slots.

---

## Task 1: Define the Bluetooth profile state and command contract

**Files:**

- Modify: `src/ESP32KeyBridgeBluetooth.h`
- Modify: `tests/unit/core_smoke/core_smoke.ino`

**Step 1: Write failing controller and hotkey tests**

Expand the existing fake Bluetooth controller and add tests that assert:

- Scroll Lock+1, +2, and +3 each call `selectSlot()` with the exact matching slot.
- Selecting an empty slot is accepted and changes `selectedSlot()` without invoking pairing.
- Scroll Lock+0 calls `pairSlot()` for the currently selected slot.
- Unsupported digits remain ordinary keyboard input.
- Busy and failed commands still drive the existing LED feedback without leaking chord keys.

Run:

```powershell
Set-Location tests
uv run pytest unit/core_smoke -q
```

Expected: FAIL because the richer snapshot and command surface does not exist yet.

**Step 2: Add transport-neutral profile types**

In `src/ESP32KeyBridgeBluetooth.h`, add bounded, allocation-free public types:

```cpp
enum class BluetoothProfileOperation : uint8_t {
  None, Connecting, Pairing, Cancelling, Deleting
};

enum class BluetoothProfileDisplayState : uint8_t {
  Empty, Saved, Selected, Connecting, Pairing, Connected, Failed
};

struct BluetoothProfileSnapshot {
  static constexpr size_t MaxLabelBytes = 24;
  size_t slot = 0;
  char label[MaxLabelBytes + 1] = {};
  char address[18] = {};
  bool occupied = false;
  bool selected = false;
  bool connected = false;
  BluetoothProfileDisplayState state = BluetoothProfileDisplayState::Empty;
};

struct BluetoothTargetSnapshot {
  static constexpr size_t MaxSlots = 3;
  BluetoothProfileSnapshot profiles[MaxSlots];
  size_t profileCount = 0;
  size_t connectedCount = 0;
  size_t selectedSlot = 1;
  BluetoothTargetStatus status = BluetoothTargetStatus::Idle;
  BluetoothProfileOperation operation = BluetoothProfileOperation::None;
  uint32_t pairingSecondsRemaining = 0;
  char error[96] = {};
};
```

Extend `BluetoothTargetController` with:

```cpp
virtual bool snapshot(BluetoothTargetSnapshot &out) const = 0;
virtual bool cancelPairing() = 0;
virtual bool deleteSlot(size_t slot) = 0;
virtual bool setSlotLabel(size_t slot, const char *label) = 0;
virtual bool busy() const = 0;
```

Keep `selectSlot()` and `pairSlot()` as the shared hotkey/web entry points. Update test fakes to implement the new interface.

**Step 3: Make empty-slot selection an explicit contract**

Document that `selectSlot(validSlot)` must succeed even when the slot is empty, changing only selection and status. Do not put occupancy policy into `BluetoothHotkeyInputAdapter`; it must forward the correct number and let the controller decide how to act.

**Step 4: Run the focused test**

Run:

```powershell
Set-Location tests
uv run pytest unit/core_smoke -q
```

Expected: PASS, including distinct slot 1/2/3 assertions.

**Step 5: Commit**

```powershell
git add src/ESP32KeyBridgeBluetooth.h tests/unit/core_smoke/core_smoke.ino
git commit -m "Define Bluetooth profile management contract"
```

## Task 2: Add redundant persistent Bluetooth profile metadata

**Files:**

- Create: `src/ESP32KeyBridgeBluetoothSettings.h`
- Create: `src/ESP32KeyBridgeBluetoothSettings.cpp`
- Modify: `src/ESP32KeyBridgePreferences.h`
- Modify: `tests/unit/core_smoke/core_smoke.ino`

**Step 1: Write failing persistence tests**

Add an in-memory `BluetoothProfileBlobStorage` fake and tests for:

- default registry with three empty slots and selected slot 1;
- saving and loading full address, address type, occupied bit, selected slot, and a UTF-8 label up to 24 bytes;
- rejecting invalid slot numbers, malformed addresses, overlong labels, invalid UTF-8, unsupported versions, reserved values, and CRC corruption;
- alternating A/B records, generation ordering, read-back verification, and fallback to the older valid record;
- atomic setters: a rejected update leaves the previous registry unchanged.

Run the core smoke test and confirm the new tests fail to compile.

**Step 2: Implement the registry and codec**

Create:

```cpp
struct BluetoothProfileRecord;
class BluetoothProfileRegistry;
class BluetoothProfileSettingsCodec;
class BluetoothProfileBlobStorage;
class BluetoothProfileSettingsStore;
```

Use exactly three records. Persist normalized uppercase colon-separated addresses plus address type so identity comparisons do not depend on EspBle enumeration order. Use a fixed-size, endian-stable version-1 record with CRC-32 and a generation field, following `KeyboardLayoutSettingsStore` semantics.

Expose registry operations needed by the transport adapter:

```cpp
bool select(size_t slot, BluetoothProfileSettingsError *error = nullptr);
bool assign(size_t slot, const char *address, uint8_t addressType,
            BluetoothProfileSettingsError *error = nullptr);
bool clear(size_t slot, BluetoothProfileSettingsError *error = nullptr);
bool setLabel(size_t slot, const char *label,
              BluetoothProfileSettingsError *error = nullptr);
int findAddress(const char *address, uint8_t addressType) const;
```

**Step 3: Add the ESP32 NVS backend**

Add `PreferencesBluetoothProfileStorage` to `src/ESP32KeyBridgePreferences.h`, reusing namespace `ekb` with distinct keys `btprof_a` and `btprof_b`. It must only read/write its own two blobs.

**Step 4: Run persistence tests**

Run:

```powershell
Set-Location tests
uv run pytest unit/core_smoke -q
```

Expected: PASS for round-trip, recovery, corruption, UTF-8, and atomicity cases.

**Step 5: Commit**

```powershell
git add src/ESP32KeyBridgeBluetoothSettings.h src/ESP32KeyBridgeBluetoothSettings.cpp src/ESP32KeyBridgePreferences.h tests/unit/core_smoke/core_smoke.ino
git commit -m "Persist Bluetooth profile metadata"
```

## Task 3: Implement and test the asynchronous profile state machine

**Files:**

- Create: `src/ESP32KeyBridgeBluetoothProfiles.h`
- Create: `src/ESP32KeyBridgeBluetoothProfiles.cpp`
- Modify: `src/ESP32KeyBridgeBluetooth.h`
- Modify: `tests/unit/core_smoke/core_smoke.ino`

**Step 1: Write failing state-machine tests with a fake backend**

Define a transport-neutral backend fake and test:

- boot reconciliation maps known addresses back to their prior slots even when bond order is reversed;
- previously unknown bonds fill empty registry slots deterministically without moving known labels;
- missing bonds clear occupancy/address while preserving the label for that slot;
- selecting an empty slot succeeds without advertising or pairing;
- connecting an occupied slot uses directed advertising and becomes connected only to the requested address;
- pair/replace disconnects, deletes the selected bond, starts undirected advertising, records the new secured bond, and preserves the slot label;
- an existing host racing into a pairing operation is rejected and pairing continues;
- cancel and 120-second timeout stop pairing advertising and leave an unfilled replacement slot empty;
- delete active and inactive slots handles the global disconnect/delete restriction and reconnects the previously active selected profile when required;
- concurrent mutating commands return busy and do not alter the in-flight operation;
- transport and save failures produce a stable error in the snapshot.

Run the core smoke test and confirm these tests fail.

**Step 2: Add a testable backend boundary**

Create `BluetoothProfileBackend`, describing only the transport capabilities the state machine needs: enumerate bonds, get the peripheral connection, stop/start directed or undirected advertising, disconnect, and delete a bond. Keep EspBle-specific `String`, event, and listener types out of this interface.

**Step 3: Implement `BluetoothProfileManager`**

Make `BluetoothProfileManager` implement `BluetoothTargetController`. It owns:

- the reconciled `BluetoothProfileRegistry` and `BluetoothProfileSettingsStore`;
- the selected slot and current operation;
- pairing start time and 120-second timeout;
- pending disconnect/delete/reconnect stages;
- the last bounded user-facing error;
- snapshot construction.

All public commands validate synchronously and enqueue work; `update()` advances at most the safe next state based on backend state/events. Persist only after a registry mutation is internally valid, and report failure if verified saving fails.

**Step 4: Run state-machine tests**

Run:

```powershell
Set-Location tests
uv run pytest unit/core_smoke -q
```

Expected: PASS for empty/occupied selection, reordered bonds, pair, cancel, timeout, delete/reconnect, busy, and failure paths.

**Step 5: Commit**

```powershell
git add src/ESP32KeyBridgeBluetoothProfiles.h src/ESP32KeyBridgeBluetoothProfiles.cpp src/ESP32KeyBridgeBluetooth.h tests/unit/core_smoke/core_smoke.ino
git commit -m "Add Bluetooth profile state machine"
```

## Task 4: Adapt EspBle to the shared state machine

**Files:**

- Modify: `src/ESP32KeyBridgeEspBle.h`
- Modify: `examples/practical/UsbKeyboardToBle/UsbKeyboardToBle.ino`
- Modify: `tests/unit/test_repository_structure.py`

**Step 1: Add failing structural expectations**

Require the reference sketch to construct `PreferencesBluetoothProfileStorage`, `BluetoothProfileSettingsStore`, and the EspBle controller with persisted profile management. Require the sketch to keep calling the controller through the existing hotkey adapter and web server binding.

Run:

```powershell
Set-Location tests
uv run pytest unit/test_repository_structure.py -q
```

Expected: FAIL before the example is wired.

**Step 2: Implement the EspBle backend adapter**

Refactor `EspBleHidTargetController` to compose the tested `BluetoothProfileManager` and implement `BluetoothProfileBackend`. Translate EspBle bonds and events into normalized backend state. Preserve:

- three maximum slots;
- per-slot advertised names (`KeyBridge Keyboard 1`, `2`, `3`);
- peripheral-role filtering;
- existing listener registration/removal;
- directed reconnect behavior.

Do not block inside controller calls. Ensure `update()` advances the manager and that connection/security callbacks only record state for the next update.

**Step 3: Wire persistent metadata in `UsbKeyboardToBle`**

Construct storage/store before the controller, load the profile registry during setup after `ble.begin()`, and fail with a clear serial diagnostic only for an unrecoverable storage error. Bind the same controller to both `BluetoothHotkeyInputAdapter` and the web server.

**Step 4: Compile the reference sketch**

Run:

```powershell
Set-Location tests
uv run pytest unit/test_repository_structure.py -q
uv run pytest examples_compile/test_examples_compile.py -q -k UsbKeyboardToBle
```

Expected: PASS.

**Step 5: Commit**

```powershell
git add src/ESP32KeyBridgeEspBle.h examples/practical/UsbKeyboardToBle/UsbKeyboardToBle.ino tests/unit/test_repository_structure.py
git commit -m "Wire persistent EspBle profile management"
```

## Task 5: Add strict Bluetooth JSON codecs

**Files:**

- Create: `src/ESP32KeyBridgeBluetoothJson.h`
- Create: `src/ESP32KeyBridgeBluetoothJson.cpp`
- Modify: `tests/unit/core_smoke/core_smoke.ino`

**Step 1: Write failing codec tests**

Cover exact snapshot serialization and strict request decoding for:

- label body `{\"slot\":2,\"label\":\"Office PC\"}`;
- action body `{\"slot\":2,\"action\":\"connect\",\"confirm\":false}`;
- pair/delete without `confirm: true` rejected;
- missing, duplicate, unknown, wrong-type, out-of-range, invalid UTF-8, and trailing fields rejected;
- quotes, backslashes, and control characters escaped in snapshot labels/errors;
- the full Bluetooth address emitted only for occupied slots;
- `connectedCount`, selected slot, operation, timeout, per-slot flags, and display state emitted.

Run the core smoke test and confirm failure.

**Step 2: Implement bounded codecs**

Add request structs and functions:

```cpp
bool decodeBluetoothLabelRequest(const char *json, size_t length,
                                 BluetoothLabelRequest &out,
                                 BluetoothJsonError *error = nullptr);
bool decodeBluetoothActionRequest(const char *json, size_t length,
                                  BluetoothActionRequest &out,
                                  BluetoothJsonError *error = nullptr);
bool encodeBluetoothSnapshotJson(const BluetoothTargetSnapshot &snapshot,
                                 char *out, size_t capacity,
                                 size_t &written,
                                 BluetoothJsonError *error = nullptr);
```

Follow the repository's existing strict scanner conventions. Reject truncation instead of emitting partial JSON.

**Step 3: Run codec tests**

Run:

```powershell
Set-Location tests
uv run pytest unit/core_smoke -q
```

Expected: PASS.

**Step 4: Commit**

```powershell
git add src/ESP32KeyBridgeBluetoothJson.h src/ESP32KeyBridgeBluetoothJson.cpp tests/unit/core_smoke/core_smoke.ino
git commit -m "Add Bluetooth management JSON codecs"
```

## Task 6: Expose authenticated web endpoints

**Files:**

- Modify: `src/ESP32KeyBridgeWeb.h`
- Modify: `tests/unit/test_repository_structure.py`
- Modify: `examples/practical/UsbKeyboardToBle/UsbKeyboardToBle.ino`

**Step 1: Add failing route and opt-in tests**

Extend repository structure checks for:

- `enableBluetoothManagement(BluetoothTargetController &controller)`;
- authenticated registrations for `GET /api/v1/bluetooth`, `PUT /api/v1/bluetooth/label`, and `POST /api/v1/bluetooth/action`;
- the reference sketch calling the enable method before `begin()`;
- unchanged construction working without Bluetooth management.

Run the focused test and confirm failure.

**Step 2: Add optional controller binding**

Implement:

```cpp
bool enableBluetoothManagement(BluetoothTargetController &controller);
bool bluetoothManagementEnabled() const;
```

Reject binding after `begin()`. The referenced controller must outlive the server. In `Esp32S3SettingsWebServer::update()`, call only request handling; the controller remains updated by the existing input/hotkey path so it is not advanced twice per loop.

**Step 3: Implement handlers and response rules**

- `GET /api/v1/bluetooth`: `200` snapshot or `404` when not enabled.
- `PUT /api/v1/bluetooth/label`: `200` updated snapshot after verified save.
- `POST /api/v1/bluetooth/action`: `202` updated snapshot for accepted async work; `200` for a completed selection-only action.
- `400`: malformed/invalid JSON or missing confirmation.
- `404`: Bluetooth management absent or invalid slot.
- `409`: controller busy or state conflict such as connect on empty slot.
- `500`: transport or persistence failure.

Use the same `authorize()` call and security headers as every other protected API. Return a stable `{\"error\":...,\"message\":...}` shape on failures.

**Step 4: Run web structure and compilation checks**

Run:

```powershell
Set-Location tests
uv run pytest unit/test_repository_structure.py -q
uv run pytest examples_compile/test_examples_compile.py -q -k "SettingsPortal or UsbKeyboardToBle"
```

Expected: PASS.

**Step 5: Commit**

```powershell
git add src/ESP32KeyBridgeWeb.h tests/unit/test_repository_structure.py examples/practical/UsbKeyboardToBle/UsbKeyboardToBle.ino
git commit -m "Expose Bluetooth profile management API"
```

## Task 7: Build the responsive Bluetooth Profiles UI

**Files:**

- Modify: `src/ESP32KeyBridgeWeb.h`
- Modify: `tests/unit/test_repository_structure.py`

**Step 1: Add failing UI structure checks**

Assert the embedded page includes:

- a `Bluetooth Profiles` section near the top;
- connected count and current operation summary;
- three profile-card containers with label, address, and state fields;
- Connect, Pair/Replace, Delete, Rename, and Cancel Pairing controls;
- one-second Bluetooth polling;
- confirmation paths for pair/replace and delete;
- persistent error presentation with an accessible live region.

Run the structure test and confirm failure.

**Step 2: Add semantic markup and responsive styling**

Use native buttons, inputs, labels, and headings. Render three cards from the snapshot, highlighting selected and connected states independently. Show empty slots clearly. Disable Connect for empty slots and show Pair instead. Keep touch targets usable on a phone and preserve tab/arrow/Enter/Space/Esc navigation behavior already present.

**Step 3: Add safe rendering and polling**

Use `textContent`, `value`, and DOM construction only; never inject labels, addresses, status, or errors through `innerHTML`. Poll `GET /api/v1/bluetooth` every 1000 ms while visible. Pause cleanly when management is unavailable without breaking layout/macro controls.

**Step 4: Add actions and confirmations**

- Connect posts `connect` for an occupied slot.
- Pair/Replace asks for confirmation and posts `pair` with `confirm: true`.
- Delete asks for confirmation and posts `delete` with `confirm: true`.
- Rename edits and saves the 24-byte UTF-8 label.
- Cancel Pairing is visible/enabled only while pairing.
- Accepted `202` operations remain visible through polling until terminal state.
- Errors stay visible until a later successful action or explicit dismissal.

**Step 5: Run focused tests and compile**

Run:

```powershell
Set-Location tests
uv run pytest unit/test_repository_structure.py -q
uv run pytest examples_compile/test_examples_compile.py -q -k UsbKeyboardToBle
```

Expected: PASS.

**Step 6: Commit**

```powershell
git add src/ESP32KeyBridgeWeb.h tests/unit/test_repository_structure.py
git commit -m "Add Bluetooth profile controls to settings UI"
```

## Task 8: Document the behavior in English and Japanese

**Files:**

- Modify: `README.md`
- Modify: `README.ja.md`
- Modify: `docs/GUIDE_BASICS.md`
- Modify: `docs/GUIDE_BASICS.ja.md`
- Modify: `docs/SETTINGS.md`
- Modify: `tests/manual/README.md`
- Modify: `tests/manual/README.ja.md`
- Modify: `tests/TEST_PLAN.ja.md`
- Modify: `CHANGELOG.md`

**Step 1: Update user documentation**

Document:

- the three remembered profiles and why the limit is three;
- per-slot advertised names;
- Scroll Lock+1/2/3 selection, including empty-slot behavior;
- Scroll Lock+0 pairing for the selected slot;
- web monitor fields, label persistence, all actions, confirmations, cancel, and two-minute timeout;
- delete/reconnect behavior and the fact that deleting does not auto-pair;
- local website authentication and exact Bluetooth API request/response shapes.

Keep English and Japanese meanings aligned.

**Step 2: Update manual acceptance coverage**

Add a test matrix for slots 1/2/3 in both empty and occupied states, website/hotkey cross-control, label persistence across reboot, bond reorder reconciliation, replace/cancel/timeout, deleting active/inactive slots, and all displayed addresses/states.

**Step 3: Run documentation/structure tests**

Run:

```powershell
Set-Location tests
uv run pytest unit/test_repository_structure.py -q
```

Expected: PASS.

**Step 4: Commit**

```powershell
git add README.md README.ja.md docs/GUIDE_BASICS.md docs/GUIDE_BASICS.ja.md docs/SETTINGS.md tests/manual/README.md tests/manual/README.ja.md tests/TEST_PLAN.ja.md CHANGELOG.md
git commit -m "Document Bluetooth profile management"
```

## Task 9: Full verification, device upload, and publication

**Files:**

- Verify all changed files.
- Do not add generated build output.

**Step 1: Run the complete automated suite**

Run:

```powershell
Set-Location tests
uv run pytest unit/ examples_compile/
```

Expected: all tests PASS with no unexpected skips or failures.

**Step 2: Compile the exact firmware target**

Use the pinned `UsbKeyboardToBle` sketch profile from its `sketch.yaml`:

```powershell
arduino-cli compile --profile esp32s3 examples/practical/UsbKeyboardToBle
```

Expected: successful ESP32-S3 build with size below flash/RAM limits.

**Step 3: Request a code review**

Use `superpowers:requesting-code-review`. Address all High/Medium findings and rerun the affected tests. Record Low findings that are intentionally deferred.

**Step 4: Upload to the attached board**

Detect the current ESP32-S3 COM port rather than assuming it is unchanged, then upload the exact successful build:

```powershell
arduino-cli board list
arduino-cli upload --profile esp32s3 -p <detected-port> examples/practical/UsbKeyboardToBle
```

Expected: upload completes and the tool resets the board successfully.

**Step 5: Perform hardware acceptance**

Using the local website and physical keyboard, verify as far as attached hosts permit:

- website loads and reports 0 or 1 connected Bluetooth devices;
- three slots show distinct names, labels, addresses, occupancy, selection, and connection state;
- Scroll Lock+1, +2, +3 each selects its own slot;
- empty slots select without pairing;
- occupied slots attempt directed reconnect;
- web selection and hotkey selection stay synchronized;
- pair/replace, cancel, two-minute timeout, rename persistence, and delete behave per the manual matrix.

If a second or third Bluetooth host is unavailable, report those specific physical scenarios as unverified rather than inferring success from unit tests.

**Step 6: Inspect final diff and status**

Run:

```powershell
git diff --check
git status --short
git log --oneline -12
```

Expected: no whitespace errors and no uncommitted implementation files.

**Step 7: Commit any verification fixes**

If review or hardware verification required changes:

```powershell
git add .
git commit -m "Fix Bluetooth profile verification findings"
```

Rerun the complete automated suite and exact firmware compile after the final code change.

**Step 8: Publish**

```powershell
git push origin main
```

Expected: the remote `main` advances through the design, plan, and implementation commits. Report the final pushed commit, automated test result, upload port/result, and any physical Bluetooth scenarios that could not be exercised.
