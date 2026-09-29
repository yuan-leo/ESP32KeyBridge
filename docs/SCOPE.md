# Agreed Product Scope

This file records only product features explicitly added to scope in the associated ChatGPT conversation. Repository notes, TODOs, examples, and historical design ideas do not add scope by themselves.

## Supported hardware

The only supported target device is **ESP32-S3**. Code paths, examples, or historical notes for other ESP32 variants do not make those devices supported or in scope.

## Current milestone: Bluetooth target switching

Status: **implemented**.

- A keyboard hotkey switches which paired Bluetooth device is connected.
- The default hotkey modifier is **Scroll Lock held down**.
- While Scroll Lock is held, a **top-row number key** selects the corresponding Bluetooth device slot.
- Slots run from `1` through the maximum number of devices supported by the selected Bluetooth stack and target hardware. This document does not invent a fixed maximum.
- Connection switching uses the selected stored/paired device rather than requiring a new pairing operation for every switch.
- `Scroll Lock` + top-row `0` puts Bluetooth into pairing mode for the currently selected device slot.
- When the physical keyboard supports a Scroll Lock LED output report, the bridge flashes that LED to indicate Bluetooth switching/pairing status.
- Status indication is best-effort: keyboards without a Scroll Lock LED, or keyboards that ignore LED reports, must still support switching and pairing without visual feedback.
- Temporary status flashing must preserve and restore the effective Scroll Lock state after indication finishes.

Default chord examples:

- `Scroll Lock` + top-row `1` selects Bluetooth device slot 1.
- `Scroll Lock` + top-row `2` selects Bluetooth device slot 2.
- The same pattern continues through the supported slot limit.
- `Scroll Lock` + top-row `0` starts pairing for whichever slot is currently selected.

Implementation details: a Scroll Lock tap with no selector is passed through as a normal tap. Switching flashes at 125 ms, pairing at 250 ms, success shows a 400 ms pulse, and failure uses three 100 ms pulses. `EspBleHidTargetController` exposes the three bond slots supported by EspBle's bundled NimBLE build; another target controller can expose a different stack/hardware limit.

## Later milestone: locally hosted configuration UI

Status: **implemented (phases 1 through 6)**.

- The only supported target for this milestone is **ESP32-S3**.
- The S3 has one USB OTG role at a time. A wired USB keyboard input therefore
  uses BLE for the computer output; a wired USB computer output requires a BLE
  keyboard input. Dual wired USB bridging is not part of this milestone.
- Phase 1 provides a versioned keyboard-layout settings model, strict
  validation, redundant NVS persistence, and boot-time fallback to safe
  defaults or the previous valid record. Integration details are in
  [SETTINGS.md](SETTINGS.md).
- Phase 1 models one keyboard input. Several physical keyboards merged by one
  adapter share that input's engraving layout.
- Phase 2 provides an ESP32-S3 WPA2 SoftAP, an offline read-only settings page,
  and versioned `GET` endpoints for configuration and live status. It does not
  expose mutation endpoints, captive-portal DNS, or keyboard navigation.
- Phase 3 adds a strict versioned JSON settings decoder, a single
  `PUT /api/v1/config` mutation endpoint, layout discovery, and the editing UI.
  A successful update is written and read-back verified through the redundant
  NVS store before it is applied to the running bridge. Invalid requests leave
  both active and persisted settings unchanged.
- Phase 4 lets the USB-host-connected wired keyboard navigate that page over
  the local Wi-Fi link. Tab/Shift+Tab, arrows, Enter/Space, Escape, Home, and
  End are transported as bounded navigation events. While the page holds its
  short polling lease, keyboard keys are withheld from the BLE host. Capture
  begins only after an all-released keyboard snapshot, expires automatically,
  and keys held at expiry stay suppressed until release.
- Phase 5 adds four persisted text-macro slots. Each slot is disabled or binds
  a unique F1-F24 key to 1-31 UTF-8 characters. Macro settings have their own
  versioned, CRC-protected alternating NVS records and strict complete-document
  JSON endpoint. The verified record replaces the live macro table atomically.
- Phase 6 adds optional captive-portal discovery. When enabled, wildcard DNS
  resolves names to the SoftAP and unknown non-API `GET` probe paths redirect
  to the fixed local portal address. Unknown API paths and non-GET requests
  retain their JSON 404 response, and WPA2 remains required.

- Provide a locally hosted web interface.
- Configure keyboard-layout switching through that interface.
- Configure macros through that interface.

The current development portal uses an ESP32-S3 WPA2 SoftAP with fixed example
credentials. Production credential provisioning and an authentication model
beyond WPA2 are not yet specified. Macro persistence is stored independently
rather than being added to the layout-settings record.

## Explicitly not inferred

No other repository roadmap item, struck historical proposal, or unimplemented adapter is in scope unless it is explicitly added in ChatGPT.
