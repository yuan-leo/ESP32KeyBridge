# ESP32KeyBridge

> 日本語版: [README.ja.md](README.ja.md)

ESP32KeyBridge is an Arduino library that puts an ESP32 **between a keyboard and the machine that reads it**. It takes keyboard-shaped input from one or more adapters (USB Host, BLE or Bluetooth Classic HID Host, GPIO switches, a matrix, a rotary encoder), normalizes it into one key model, applies remapping, layers, macros and keyboard-layout conversion, and writes the result to one or more outputs (USB Device HID, BLE or Classic HID, a serial `Print`).

> **Scope policy:** this repository documents the software that exists. It does not create product scope. Only features explicitly agreed in the associated ChatGPT conversation are in scope; unimplemented ideas retained in historical notes are out of scope unless explicitly brought back into scope there.
>
> The currently agreed additions and their status are tracked in [docs/SCOPE.md](docs/SCOPE.md).

The result is a device the PC cannot tell apart from an ordinary keyboard — no drivers, no software on the host, and nothing to configure on the keyboard either.

The bridge core is portable C++ with **no dependency on Arduino, ESP-IDF, or any hardware**: it never reads a clock, never allocates, and runs from a single `update()` call. Everything platform-specific lives in adapters, so the whole transformation layer is unit-tested on the host machine.

## Why use ESP32KeyBridge?

- **One key model for every device.** A key is a kind plus a code (keyboard usage, consumer usage, mouse button, or a bridge-internal virtual key). Remapping *across* kinds — turn F13 into Volume Up, or a gamepad button into Escape — is one table entry, not a special case.
- **No stuck keys, by construction.** A key is resolved once at press time and held until release, so switching layers or calling `applyConfig()` while a key is down can never release something other than what was pressed. A disconnected input drops its keys instead of leaving them held.
- **Merge inputs, then fan out to outputs.** Several keyboards, a macro pad, and an encoder combine into one key set (union, with per-source release), and the same result can go to a USB host and a BLE host at once.
- **Lock state has exactly one owner.** While a lock-reporting output (USB Device / BLE HID) is connected, that host is the authority and the bridge only relays it back to every input's LEDs; with no such output the bridge becomes the terminal host and toggles Caps/Num/Scroll itself.
- **Characters travel as characters.** `typeText()` carries text through the bridge and expands it into keystrokes at the output edge using the *host's* layout, with Caps Lock compensation and user-held modifiers parked automatically. Layout conversion uses the same machinery, so a US keyboard types as engraved on a PC that stays on ja_jp.
- **The same report behavior on USB, BLE, and Classic.** HID outputs share the core's report builders, while EspBle shares report packing between BLE and Classic. Connection mechanics and SoC support stay at the adapter boundary.
- **Adapters are thin and replaceable.** An input adapter is `update()` plus `keys()`; writing one for an I2C expander, a sensor, or a network source is about a page of code (`TouchButtons` is a worked example).

## Features

| Area | What it provides | Key APIs |
| --- | --- | --- |
| Key model | Kind + code identity covering the HID keyboard page, consumer page, mouse buttons, and virtual keys; a fixed-capacity pressed-key set with modifier normalization | `Key`, `KeyboardUsage`, `ConsumerUsage`, `MouseUsage`, `VirtualUsage`, `KeySet` |
| Merging | Up to 8 inputs and 4 outputs; keys are a union across inputs and stay pressed until every holder releases them; a disconnect clears that input only | `addInput()`, `addOutput()`, `mergedKeys()`, `outputKeys()` |
| Transforms | Single-step remap and disable, per input and globally (never chains, so two entries swap a pair); momentary layers bound to a trigger key that is consumed | `TransformConfig`, `InputConfig`, `addLayer()`, `LayerConfig` |
| Text | UTF-8 macros bound to a key, and a text stream with backpressure that the typing engine expands into keystrokes at the output edge | `textMacro()`, `typeText()`, `typeChar()`, `typeAvailable()` |
| Layouts | 16 bundled locales with 4 planes each (base / Shift / AltGr / AltGr+Shift), used both as an input's engraving and as the host's interpretation | `KeyboardLayout`, `hostLayout`, `convertLayout()`, `layoutConversionToggle` |
| Lock state | Authority chain (host output → bridge shadow → terminal-host fallback), forwarded to every input's LEDs; Kana included where the transport carries it | `LockState`, `lockState()`, `lockAuthorityPresent()`, `setLockState()` |
| Relative axes | X / Y / Wheel / Pan deltas with a per-axis scale (a negative scale inverts, e.g. natural scrolling), summed across inputs | `Axis`, `setAxisScale()`, `takeAxisDelta()`, `writeAxisDelta()` |
| HID reports | Pure report builders for boot 6KRO, 32-key rollover, consumer, and relative mouse, with int8 saturation and carry for axes | `buildHidKeyboardReport()` and the rest of the builder family |
| Gamepads | Button-number and hat/D-pad mapping to any `Key`, with hat decoding shared by the USB and BLE adapters | `GamepadKeyMap`, `mapButton()`, `mapHat()` |

## Adapters

Adapter headers are header-only, so a sketch pulls in a dependency only by including the adapter it uses. The core header has no dependencies at all.

| Header | Depends on | Adapters |
| --- | --- | --- |
| `ESP32KeyBridge.h` | — | `ManualInputAdapter`, `ManualOutputAdapter` (reference implementations and test doubles) |
| `ESP32KeyBridgeEspUsbHost.h` | [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost) 2.6.0+ | USB keyboard (consumer keys included), mouse, gamepad, MIDI input |
| `ESP32KeyBridgeEspUsbDevice.h` | [EspUsbDevice](https://github.com/tanakamasayuki/EspUsbDevice) 2.0.0+ | Composite USB HID output (keyboard + consumer + mouse) |
| `ESP32KeyBridgeEspBle.h` | [EspBle](https://github.com/tanakamasayuki/EspBle) 1.3.0+ | BLE HID output (peripheral), bonded-target switching, and BLE keyboard / mouse / gamepad input (central) |
| `ESP32KeyBridgeBluetooth.h` | — | Transport-neutral Scroll Lock + number target hotkeys and LED status wrapper |
| `ESP32KeyBridgeEspBleClassic.h` | EspBle 1.3.0+ | Legacy Bluetooth Classic implementation requiring the original ESP32 (**unsupported / out of scope**) |
| `ESP32KeyBridgeGpio.h` | — (Arduino GPIO; the encoder uses PCNT) | Single keys, a scanned key matrix, a rotary encoder |
| `ESP32KeyBridgeSerial.h` | — (Arduino `Print`) | Text output and a human-readable debug log |

The full table with per-adapter behavior, `update()` timing tolerance, and USB device composition is in [docs/ADAPTERS.md](docs/ADAPTERS.md).

## Compatibility

The only officially supported target is **ESP32-S3**.

| Capability | ESP32-S3 |
| --- | --- |
| GPIO keys / matrix | ✅ |
| Rotary encoder (PCNT) | ✅ |
| USB Host input | ✅ |
| USB Device output | ✅ |
| BLE HID input / output | ✅ |
| Local Wi-Fi layout / macro settings UI | ✅ |
| Bluetooth Classic HID | — |

The S3 has one native USB controller, so a design may use it as USB Host or USB Device while using BLE on the other side. Source code or historical examples for other ESP32 variants remain unsupported and outside the agreed scope.

Development and the example builds run on arduino-esp32 3.3.11; each example's `sketch.yaml` pins the verified core and library versions.

## Getting started

Install from Arduino Library Manager, or with Arduino CLI:

```sh
arduino-cli lib install ESP32KeyBridge
```

Every example ships a `sketch.yaml` pinned to the verified versions, so it builds with no extra setup:

```sh
arduino-cli compile examples/practical/SwapCtrlCapsLock
```

Every sketch has the same three steps — start the hardware, wire the bridge, build and apply the configuration:

```cpp
#include <ESP32KeyBridge.h>
#include <ESP32KeyBridgeEspUsbHost.h>
#include <ESP32KeyBridgeEspUsbDevice.h>
#include <EspUsbHost.h>
#include <EspUsbDevice.h>

EspUsbHost usbHost;     // keyboard side
EspUsbDevice usbDevice; // PC side

esp32keybridge::ESP32KeyBridge bridge;
esp32keybridge::EspUsbHostKeyboardInputAdapter keyboard(usbHost);
esp32keybridge::EspUsbDeviceHidOutputAdapter pc(usbDevice);

void setup() {
  // 1) Start the hardware (the stacks belong to the sketch, not the adapters).
  usbHost.begin();
  usbDevice.begin();

  // 2) Wire the bridge.
  bridge.addInput(keyboard);
  bridge.addOutput(pc);

  // 3) Configure. A remap is a single step and never chains, so two
  //    entries swap the pair.
  esp32keybridge::ESP32KeyBridgeConfig config;
  config.global.remap(esp32keybridge::KeyboardUsage::CapsLock,
                      esp32keybridge::KeyboardUsage::LeftCtrl);
  config.global.remap(esp32keybridge::KeyboardUsage::LeftCtrl,
                      esp32keybridge::KeyboardUsage::CapsLock);
  bridge.applyConfig(config);
}

void loop() {
  bridge.update();  // inputs, transforms, and outputs all run from here
  delay(1);
}
```

There is no `begin()` and no ordering between `addInput()` / `addOutput()`, `applyConfig()`, and adapter startup: registration and configuration only store state, and everything starts working at the first `update()`.

See the [examples index](examples/README.md) for the full list — every example is a complete sketch for a real hardware setup.

## Documents

**Looking for a specific document? See the [documentation guide](docs/README.md)** — it shows the reading order and each document's role.

Design documents are Japanese-only; the READMEs and the examples are bilingual.

- [Beginner's guide (start here)](docs/GUIDE_BASICS.md)
- [Agreed product scope](docs/SCOPE.md)
- [Adapter list, behavior, and dependencies](docs/ADAPTERS.md)
- [Data model: key identity, lock state, layout conversion](docs/DATA_MODEL.md)
- [Core design and the time / concurrency boundary](docs/CORE_DESIGN.md)
- [Use cases the design was validated against](docs/USE_CASES.ja.md)
- [Requirements](docs/REQUIREMENTS.ja.md)
- [Design decision ledger](docs/DECISIONS.ja.md)
- [Configuration design notes](docs/CONFIGURATION.ja.md)
- [Implementation history and closed scope](docs/DEVELOPMENT_PLAN.ja.md)
- [Test plan](tests/TEST_PLAN.ja.md) and [test layout](tests/README.ja.md)
- [Release checklist](docs/RELEASE_CHECKLIST.md)

## Tests

Host unit tests and example build smoke tests need no hardware:

```sh
cd tests
uv run pytest unit/ examples_compile/
```

Hardware tests are split by what they need: `single/` (one ESP32-S3), `peer/` (two boards, one driving the other over real USB), and `manual/` (wiring, host OS behavior, pairing). See [tests/README.ja.md](tests/README.ja.md).

## Sibling libraries

- [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost) — USB Host stack behind the USB input adapters
- [EspUsbDevice](https://github.com/tanakamasayuki/EspUsbDevice) — USB Device stack behind the USB HID output adapter
- [EspBle](https://github.com/tanakamasayuki/EspBle) — BLE stack behind the BLE HID adapters

The three share the same keyboard-layout tables, HID usage conventions, and listener API shape, which is why an adapter looks the same whichever one it sits on.

## License

MIT License
