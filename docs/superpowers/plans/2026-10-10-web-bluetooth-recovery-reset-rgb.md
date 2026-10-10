# Web Bluetooth Recovery, Reset, and HW-678 Status Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Provide an unauthenticated local settings portal that can recover Bluetooth operations, show remote host identity, perform two reset scopes, and drive the HW-678 GPIO 48 RGB status LED.

**Architecture:** Extend the transport-neutral Bluetooth profile model first, then add EspBle lifecycle behavior behind that interface. Keep reset coordination and RGB pattern calculation portable and unit-testable; the ESP32-S3 web server translates HTTP requests and the reference sketch alone touches the HW-678 LED and reboot API.

**Tech Stack:** C++17-style Arduino library code, Arduino ESP32 3.3.11, EspBle 1.3.0, WebServer/WiFi/DNSServer, Preferences-backed redundant records, WS2812 through `rgbLedWrite`, pytest repository checks, and Arduino CLI compile/upload.

**Spec:** `docs/superpowers/specs/2026-10-10-web-bluetooth-recovery-reset-rgb-design.md`

## Global Constraints

- ESP32-S3 is the only supported processor and HW-678 v0.0.0 is the reference board.
- There is exactly one wired USB-OTG input; Bluetooth remains output-only.
- The settings access point is named `KeyBridge-Setup` and has no Wi-Fi or HTTP authentication.
- There are exactly three Bluetooth output profiles.
- Reconnect timeout is 30 seconds; pairing timeout is 120 seconds.
- User labels remain limited to 24 UTF-8 bytes; learned remote names are limited to 64 UTF-8 bytes and are optional.
- Profile 1 is blue, profile 2 green, and profile 3 magenta.
- Only the GPIO 48 WS2812 is application-controlled; PWR, TX, and RX LEDs are untouched.
- Existing saved labels, addresses, address types, and selected slot must survive the persistence migration.
- Every implementation task follows red-green-refactor and ends in its own commit.

## Review Focus

- A deadline spanning `millis()` wraparound must still expire once and leave reconnect idle; Task 2 tests unsigned elapsed-time behavior.
- A late GATT name result from an old connection must not rename the newly selected profile; Task 2 tests connection-id and address matching before persistence.
- A learned name containing quotes, backslashes, or multibyte UTF-8 must be escaped correctly in API JSON; Task 5 adds serialization assertions.
- A force-stop requested while a disconnect callback is pending must be idempotent and must not restart advertising; Task 2 tests repeated stop transitions.
- Duplicate reset requests during the deferred reboot window must be rejected without repeating storage erases; Tasks 3 and 5 test coordinator and endpoint idempotence.

---

### Task 1: Persist the optional remote host name with backward migration

**Files:**
- Modify: `src/ESP32KeyBridgeBluetooth.h`
- Modify: `src/ESP32KeyBridgeBluetoothSettings.h`
- Modify: `src/ESP32KeyBridgeBluetoothSettings.cpp`
- Modify: `tests/unit/core_smoke/core_smoke.ino`

**Interfaces:**
- Consumes: existing `BluetoothProfileRegistry`, redundant `BluetoothProfileSettingsStore`, and version-1 160-byte records.
- Produces: `BluetoothProfileSnapshot::hostName`, `BluetoothProfileRecord::hostName`, `BluetoothProfileRegistry::setHostName(size_t, const char *)`, `BluetoothProfileSnapshot::MaxHostNameBytes = 64`, codec version 2 with `EncodedSize = 352`, and legacy decode support for version-1 records.

- [ ] **Step 1: Add failing registry and codec tests**

Add core-smoke cases that assert a valid 64-byte UTF-8 host name round-trips, 65-byte/invalid UTF-8 names are rejected without mutation, clearing a profile erases its host name while retaining its user label, and a fixed version-1 record decodes with label/address/selection preserved and an empty host name.

- [ ] **Step 2: Run the core-smoke compile and confirm RED**

Run: `arduino-cli compile tests/unit/core_smoke`

Expected: FAIL because `hostName`, `MaxHostNameBytes`, and `setHostName` do not exist.

- [ ] **Step 3: Implement the version-2 profile model and codec migration**

Use a 110-byte profile stride containing occupied/address type, 18-byte address, 25-byte label, and 65-byte host name; reserve remaining bytes before the checksum. Decode both the exact legacy 160-byte/version-1 layout and the new 352-byte/version-2 layout, while encoding only version 2.

- [ ] **Step 4: Verify core smoke and repository tests GREEN**

Run: `arduino-cli compile tests/unit/core_smoke`

Run: `uv run pytest tests/unit/test_repository_structure.py -q`

Expected: both commands exit 0.

- [ ] **Step 5: Commit the persistence change**

```bash
git add src/ESP32KeyBridgeBluetooth.h src/ESP32KeyBridgeBluetoothSettings.h src/ESP32KeyBridgeBluetoothSettings.cpp tests/unit/core_smoke/core_smoke.ino
git commit -m "Persist Bluetooth host names"
```

### Task 2: Make Bluetooth operations recoverable and learn host names

**Files:**
- Modify: `src/ESP32KeyBridgeBluetooth.h`
- Modify: `src/ESP32KeyBridgeEspBle.h`
- Modify: `tests/unit/core_smoke/core_smoke.ino`
- Modify: `tests/unit/test_repository_structure.py`

**Interfaces:**
- Consumes: Task 1 `setHostName`, EspBle connection/security events, generic GATT discovery/read listeners, bond deletion, and directed advertising.
- Produces: `BluetoothOperationDeadline::{start,cancel,active,expired}`, `BluetoothTargetController::{forceDisconnect,resetBluetooth}`, controller constants `ReconnectTimeoutMs = 30000` and `PairingTimeoutMs = 120000`, and best-effort GAP Device Name acquisition from `1800/2A00`.

- [ ] **Step 1: Add failing deadline, interface, and integration-shape tests**

Add core-smoke tests for inactive deadlines, normal expiry, and unsigned wraparound expiry; extend `FakeBluetoothTarget` to record `forceDisconnect()` and `resetBluetooth()`. Add repository assertions for the 30-second constant, GATT listener registration/removal, `discoverCharacteristic(..., "1800", "2a00", ...)`, connection-id/address checks, and `deleteAllBonds()`.

- [ ] **Step 2: Run focused tests and confirm RED**

Run: `arduino-cli compile tests/unit/core_smoke`

Run: `uv run pytest tests/unit/test_repository_structure.py -q`

Expected: FAIL on the missing deadline and controller recovery APIs.

- [ ] **Step 3: Implement reconnect timeout and idempotent force-stop**

Start the reconnect deadline whenever directed advertising begins. On expiry, stop advertising, clear pending/operation/error state, and set idle without automatic restart. `forceDisconnect()` must work from every state, stop advertising immediately, prevent `handleDisconnected()` from reconnecting, and settle idle after any requested disconnect callback.

- [ ] **Step 4: Implement Bluetooth reset and best-effort remote-name acquisition**

`resetBluetooth()` stops activity, requests disconnection if needed, calls `deleteAllBonds()`, clears the profile store and in-memory registry, selects slot 1, and suppresses reconnect. After successful bonded security, discover and read `1800/2A00`; accept a result only for the current peripheral connection and matching saved address, then persist a valid name without changing HID connection status. Failures remain silent in the profile snapshot.

- [ ] **Step 5: Verify focused tests and both BLE-consuming examples GREEN**

Run: `arduino-cli compile tests/unit/core_smoke`

Run: `uv run pytest tests/unit/test_repository_structure.py -q`

Run: `$env:ESP32KEYBRIDGE_EXAMPLES='UsbKeyboardToBle,BleKeyboardToUsb'; uv run pytest tests/examples_compile/test_examples_compile.py -q`

Expected: all commands exit 0.

- [ ] **Step 6: Commit Bluetooth recovery**

```bash
git add src/ESP32KeyBridgeBluetooth.h src/ESP32KeyBridgeEspBle.h tests/unit/core_smoke/core_smoke.ino tests/unit/test_repository_structure.py
git commit -m "Make Bluetooth profile control recoverable"
```

### Task 3: Add portable two-scope reset coordination

**Files:**
- Create: `src/ESP32KeyBridgeReset.h`
- Create: `src/ESP32KeyBridgeReset.cpp`
- Modify: `tests/unit/core_smoke/core_smoke.ino`
- Modify: `tests/unit/test_repository_structure.py`
- Modify: `keywords.txt`

**Interfaces:**
- Consumes: Task 2 `BluetoothTargetController::resetBluetooth()`, `KeyboardLayoutSettingsStore::clear`, and `KeyboardMacroSettingsStore::clear`.
- Produces: `SettingsResetScope::{BluetoothOnly,Everything}`, `SettingsResetErrorCode::{None,AlreadyInProgress,BluetoothFailed,LayoutStorageFailed,MacroStorageFailed}`, `SettingsResetError`, and `SettingsResetCoordinator::{enableLayoutReset,enableMacroReset,reset,inProgress,finish}`.

- [ ] **Step 1: Add failing reset-scope tests**

Add core-smoke fakes and cases proving Bluetooth-only calls Bluetooth reset but preserves layout/macro blobs, Everything clears all three domains, each dependency failure returns its exact error code, and a second request before `finish()` returns `AlreadyInProgress` without a second erase.

- [ ] **Step 2: Compile core smoke and confirm RED**

Run: `arduino-cli compile tests/unit/core_smoke`

Expected: FAIL because `ESP32KeyBridgeReset.h` and its types do not exist.

- [ ] **Step 3: Implement the coordinator**

Keep it Arduino-independent. Require both settings stores for `Everything`; set `inProgress` before mutation, retain it on success until the web layer calls `finish()` or the device reboots, and clear it on a reported failure so the user may retry.

- [ ] **Step 4: Verify core and structure tests GREEN**

Run: `arduino-cli compile tests/unit/core_smoke`

Run: `uv run pytest tests/unit/test_repository_structure.py -q`

Expected: both commands exit 0.

- [ ] **Step 5: Commit reset coordination**

```bash
git add src/ESP32KeyBridgeReset.h src/ESP32KeyBridgeReset.cpp tests/unit/core_smoke/core_smoke.ino tests/unit/test_repository_structure.py keywords.txt
git commit -m "Add scoped settings reset coordination"
```

### Task 4: Add the deterministic RGB pattern engine and HW-678 output

**Files:**
- Create: `src/ESP32KeyBridgeStatusLed.h`
- Modify: `examples/practical/UsbKeyboardToBle/UsbKeyboardToBle.ino`
- Modify: `tests/unit/core_smoke/core_smoke.ino`
- Modify: `tests/unit/test_repository_structure.py`
- Modify: `keywords.txt`

**Interfaces:**
- Consumes: `BluetoothTargetSnapshot`, uptime milliseconds, Bluetooth availability, and destructive-reset state.
- Produces: `RgbColor`, `BluetoothStatusLedPattern::{Off,Boot,Connected,Searching,Pairing,Disconnected,Destructive,Error}`, `BluetoothStatusLedFrame`, `bluetoothStatusLedFrame(...)`, and `bluetoothStatusLedPatternName(...)`.

- [ ] **Step 1: Add failing frame tests for every legend entry**

Test boot frames at white/red/green/blue boundaries, all three profile base colors, connected solid, searching triangle-wave breathing, 300 ms pairing alternation, 120 ms disconnected flash in a 3000 ms period, 150 ms amber pulse, three 150 ms red error flashes plus pause, unavailable off, and destructive/error precedence.

- [ ] **Step 2: Compile core smoke and confirm RED**

Run: `arduino-cli compile tests/unit/core_smoke`

Expected: FAIL because the status LED types and renderer do not exist.

- [ ] **Step 3: Implement the header-only pattern engine**

Use integer arithmetic and a maximum active channel value of 32. Treat uptime below 800 ms as boot: white 0-199 ms, red 200-399 ms, green 400-599 ms, and blue 600-799 ms. After boot, apply reset, error, and normal-state priority in that order.

- [ ] **Step 4: Wire the HW-678 sketch to GPIO 48 through the Arduino RGB API**

Guard on `RGB_BUILTIN`, assert/document that the generic ESP32-S3 profile maps it to the HW-678 GPIO 48 WS2812, call `rgbLedWrite(RGB_BUILTIN, red, green, blue)` only when the frame changes, and leave PWR/TX/RX untouched.

- [ ] **Step 5: Verify core, structure, and sketch compilation GREEN**

Run: `arduino-cli compile tests/unit/core_smoke`

Run: `uv run pytest tests/unit/test_repository_structure.py -q`

Run: `$env:ESP32KEYBRIDGE_EXAMPLES='UsbKeyboardToBle'; uv run pytest tests/examples_compile/test_examples_compile.py -q`

Expected: all commands exit 0.

- [ ] **Step 6: Commit RGB status support**

```bash
git add src/ESP32KeyBridgeStatusLed.h examples/practical/UsbKeyboardToBle/UsbKeyboardToBle.ino tests/unit/core_smoke/core_smoke.ino tests/unit/test_repository_structure.py keywords.txt
git commit -m "Add HW-678 Bluetooth status indicator"
```

### Task 5: Replace authentication with open access and expose recovery/reset controls

**Files:**
- Modify: `src/ESP32KeyBridgeWeb.h`
- Modify: `examples/practical/UsbKeyboardToBle/UsbKeyboardToBle.ino`
- Modify: `examples/practical/SettingsPortal/SettingsPortal.ino`
- Modify: `tests/unit/test_repository_structure.py`

**Interfaces:**
- Consumes: Tasks 1-4 profile snapshots, force-stop command, reset coordinator, and status-pattern names.
- Produces: open `SettingsAccessPointConfig` without credential fields, `Esp32S3SettingsWebServer::enableReset(SettingsResetCoordinator &)`, Bluetooth action `stop`, `POST /api/v1/reset`, host-name JSON, indicator JSON, and a deferred 1000 ms successful-reset reboot.

- [ ] **Step 1: Replace the Phase 7 structure test with failing open-access and control assertions**

Assert absence of `authenticate`, `requestAuthentication`, `DIGEST_AUTH`, `portalUsername`, and `portalPassword`; assert an open `WiFi.softAP` call; assert routes/actions for stop and reset; assert confirmation copy for both scopes; assert host-name fallback, escaped JSON use, LED legend text, action-specific disabled conditions, and a duplicate-reset 409 response.

- [ ] **Step 2: Run repository tests and confirm RED**

Run: `uv run pytest tests/unit/test_repository_structure.py -q`

Expected: FAIL because authentication remains and recovery/reset UI is absent.

- [ ] **Step 3: Remove Wi-Fi and HTTP authentication**

Remove credential fields, validation, buffers, `authorize()`, and authentication checks. Start the SoftAP with a null password while preserving channel, hidden, client limit, captive DNS, API-safe 404 behavior, and all security headers.

- [ ] **Step 4: Implement the Bluetooth UI/API changes**

Serialize `hostName` with the existing JSON escaper and render `Not reported by host` for empty names. Add a section-level Stop / Force disconnect control that is never disabled by Bluetooth busy state; keep Rename enabled during operations by allowing metadata-only updates, and gate Connect/Pair/Delete only on conflicting operations. Return actionable 409 messages rather than exposing `bluetooth_busy` alone.

- [ ] **Step 5: Implement both confirmed reset actions and deferred restart**

Decode only `{"scope":"bluetooth","confirm":true}` and `{"scope":"everything","confirm":true}`. On coordinator success, return 202 JSON, set the reboot deadline to `millis() + 1000`, keep duplicate requests rejected, and call `ESP.restart()` from `update()` after the deadline. Show distinct confirmation text and amber/reset status in the page.

- [ ] **Step 6: Wire the reference bridge to reset and both examples to open access**

`UsbKeyboardToBle` constructs one `SettingsResetCoordinator`, enables its layout and macro stores, and passes it to `enableReset()`. `SettingsPortal` demonstrates the open layout/macro portal without enabling the Bluetooth-dependent reset coordinator; the common page reports reset controls unavailable when no coordinator is registered. Neither sketch prints or declares credentials.

- [ ] **Step 7: Verify structure tests and both settings examples GREEN**

Run: `uv run pytest tests/unit/test_repository_structure.py -q`

Run: `$env:ESP32KEYBRIDGE_EXAMPLES='UsbKeyboardToBle,SettingsPortal'; uv run pytest tests/examples_compile/test_examples_compile.py -q`

Expected: both commands exit 0.

- [ ] **Step 8: Commit the website behavior**

```bash
git add src/ESP32KeyBridgeWeb.h examples/practical/UsbKeyboardToBle/UsbKeyboardToBle.ino examples/practical/SettingsPortal/SettingsPortal.ino tests/unit/test_repository_structure.py
git commit -m "Add open recovery and reset website"
```

### Task 6: Document, verify, publish, and upload the phase

**Files:**
- Modify: `CHANGELOG.md`
- Modify: `docs/SETTINGS.md`
- Modify: `docs/SCOPE.md`
- Modify: `examples/practical/UsbKeyboardToBle/README.md`
- Modify: `examples/practical/UsbKeyboardToBle/README.ja.md`
- Modify: `examples/practical/SettingsPortal/README.md`
- Modify: `examples/practical/SettingsPortal/README.ja.md`
- Modify: `tests/TEST_PLAN.ja.md`
- Modify: `tests/manual/README.md`
- Modify: `tests/manual/README.ja.md`
- Modify: `tests/unit/test_repository_structure.py`

**Interfaces:**
- Consumes: the complete implemented behavior and approved hardware legend.
- Produces: user-facing open-network warning, Bluetooth identity/recovery/reset instructions, exact RGB legend, manual HW-678 checks, published commits, and flashed `UsbKeyboardToBle` firmware.

- [ ] **Step 1: Add failing documentation assertions**

Extend repository tests to require the open-network warning, both reset scopes, remote-name fallback, GPIO 48 WS2812 legend, and force-disconnect recovery instructions in the English settings/reference documentation.

- [ ] **Step 2: Run repository tests and confirm RED**

Run: `uv run pytest tests/unit/test_repository_structure.py -q`

Expected: FAIL on missing documentation text.

- [ ] **Step 3: Update English/Japanese documentation and test plans**

Document that the AP is intentionally open, explain who can change settings, distinguish editable label/learned host name/MAC, list reset contents, reproduce the exact RGB legend, identify PWR/TX/RX as untouched, and add manual tests for timeout recovery, resets, LED self-test/patterns, and remote-name fallback.

- [ ] **Step 4: Run fresh full software verification**

Run: `git diff --check`

Run: `uv run pytest tests/unit/test_repository_structure.py -q`

Run: `arduino-cli compile tests/unit/core_smoke`

Run: `$env:ESP32KEYBRIDGE_EXAMPLES='UsbKeyboardToBle,SettingsPortal,BleKeyboardToUsb'; uv run pytest tests/examples_compile/test_examples_compile.py -q`

Expected: every command exits 0 with no failures.

- [ ] **Step 5: Commit the completed phase**

```bash
git add CHANGELOG.md docs/SETTINGS.md docs/SCOPE.md examples/practical/UsbKeyboardToBle/README.md examples/practical/UsbKeyboardToBle/README.ja.md examples/practical/SettingsPortal/README.md examples/practical/SettingsPortal/README.ja.md tests/TEST_PLAN.ja.md tests/manual/README.md tests/manual/README.ja.md tests/unit/test_repository_structure.py
git commit -m "Document Bluetooth recovery and HW-678 status"
```

- [ ] **Step 6: Publish the verified commits**

Run: `git push`

Expected: the current branch is pushed successfully without rewriting remote history.

- [ ] **Step 7: Detect the HW-678 serial port without guessing**

Run: `arduino-cli board list`

Expected: identify the connected ESP32-S3/HW-678 UART COM port. If more than one plausible ESP32-S3 port exists, stop and ask the user which port to flash.

- [ ] **Step 8: Upload the verified reference firmware**

Run: `arduino-cli upload -p <detected-COM-port> --profile esp32s3 examples/practical/UsbKeyboardToBle`

Expected: upload exits 0 and reports successful reset/reboot. Do not erase flash outside the firmware's two explicit web reset operations.

- [ ] **Step 9: Report hardware-only checks still requiring observation**

Report the flashed port and commit, then list the red/green/blue boot self-test, open `KeyBridge-Setup` join, profile name/MAC/host name fields, force-stop recovery, two resets, and status patterns as the remaining on-device observations if they cannot be automated from the connected workstation.
