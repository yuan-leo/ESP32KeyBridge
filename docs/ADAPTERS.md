# Adapters

> 日本語: [ADAPTERS.ja.md](ADAPTERS.ja.md)

The list of adapters, their implementation status, and how the PC sees them (USB device composition). Updated alongside implementation progress (step 7 of the implementation order in [DEVELOPMENT_PLAN.ja.md](DEVELOPMENT_PLAN.ja.md)).

> Official support is limited to **ESP32-S3**. Adapter code that requires another ESP32 variant is retained as historical/legacy implementation and is not supported scope.

Status legend: **implemented**. Entries also state how far they have been confirmed on real hardware — **hardware-verified** means an automated test under `tests/peer/` or a confirmed check on real hardware passed; **not yet hardware-verified** means builds and unit tests only. Unimplemented historical ideas are marked **out of scope**, not planned.

## Input adapters

| Class | Header | Requires | Status | What it does |
|---|---|---|---|---|
| `ManualInputAdapter` | `ESP32KeyBridge.h` (core) | none | **implemented** | Inject logical presses with `press()` / `release()`. Relative deltas (`addAxisDelta`), simulated disconnect, and a record of received LockState. The minimal reference for a custom input |
| `EspUsbHostKeyboardInputAdapter` | `ESP32KeyBridgeEspUsbHost.h` | EspUsbHost 2.3.0+ | **implemented (hardware-verified: peer `usb_host_keyboard` / `usb_host_consumer`)** | USB keyboards (every keyboard on the stack is merged, up to 4; media keys and remotes merge too, 8 simultaneous usages). The 256-bit `onKeyboardState` snapshot (boot, report-ID boot, and NKRO alike, modifier-only changes included) maps onto the KeySet. Boot rollover error codes (usages 0x01-0x03) are excluded. LockState is forwarded to every keyboard's LEDs (num/caps/scroll; kana has no transport in EspUsbHost. EspUsbHost 2.3.0 can send LEDs to report-ID/NKRO-only keyboards as well, lifting the 2.2.0 restriction to boot-declaring keyboards). Presence starts at the first report; a disconnect releases only that device's keys |
| `EspUsbHostMouseInputAdapter` | `ESP32KeyBridgeEspUsbHost.h` | EspUsbHost | **implemented (hardware-verified: peer `usb_host_mouse`)** | USB mice (every mouse on the stack is merged, up to 4). Buttons become MouseButton keys (union); movement, wheel, and AC Pan (horizontal wheel / tilt, absent from a boot mouse report and therefore 0) become relative deltas (summed). Presence starts at the first report (there is no enumeration query API); a disconnect releases only that mouse's buttons |
| `EspUsbHostGamepadInputAdapter` | `ESP32KeyBridgeEspUsbHost.h` | EspUsbHost 2.3.0+ | **implemented (hardware-verified: peer `usb_host_gamepad`)** | USB gamepads (input only). `mapButton(number, key)` maps a HID button (1-based) to any Key; `mapHat(up, down, left, right)` maps the D-pad / hat (8 directions, diagonals press two keys). Only mapped controls produce output. All pads merge (up to 4); a disconnect drops only that pad. Analog sticks and triggers are not supported in this version |
| `EspUsbHostMidiInputAdapter` | `ESP32KeyBridgeEspUsbHost.h` | EspUsbHost 2.3.0+ | **implemented (hardware-verified: peer `usb_host_midi`)** | USB MIDI (input only). `mapNote(note, key)` maps a MIDI note (0-127) to any Key (Note On = press, Note Off or velocity 0 = release). `setChannel(ch)` narrows to one channel (default: all). Only note on/off are used (CC, pitch bend, etc. are ignored). Only mapped notes produce output; when every MIDI device disconnects, remaining notes are cleared |
| `GpioKeyInputAdapter` | `ESP32KeyBridgeGpio.h` | Arduino GPIO only | **implemented (not yet hardware-verified)** | `addKey(pin, key, activeLow, pullUp)`, up to `SOC_GPIO_PIN_COUNT` keys. Debouncing is built in (5 ms default, `setDebounceMillis()`). Pins are configured on the first `update()` after registration. Polling at the loop rate (about 1 kHz) is enough |
| `EspBleHidHostKeyboardInputAdapter` | `ESP32KeyBridgeEspBle.h` | EspBle 1.3.0+ | **implemented (hardware-verified: peer `ble_hid_keyboard`)** | BLE keyboards (HID over GATT central; every connected keyboard merges, up to 4 links; media keys merge too, one usage per link because the HOGP consumer report carries only one at a time). The 256-bit `onKeyboardState` snapshot (boot, report protocol, and NKRO alike, modifier-only changes included) maps onto the KeySet — the same path as the USB Host side. LockState is forwarded to every keyboard's LEDs (**kana included**, which the USB Host side cannot do). Presence starts at the first report; a disconnect releases only that link's keys. **A consumer release event means something different from EspUsbHost**: EspUsbHost reports the usage that was released, whereas EspBle reports that link's new usage (0 on release), so the adapter replaces the per-link entry rather than searching for a matching usage |
| `EspBleHidHostMouseInputAdapter` | `ESP32KeyBridgeEspBle.h` | EspBle 1.3.0+ | **implemented (hardware-verified: peer `ble_hid_keyboard`)** | BLE mice (every connected mouse merges, up to 4 links). Buttons become MouseButton keys (union); movement and wheel become relative deltas (summed). HOGP mouse reports carry no AC Pan |
| `EspBleHidHostGamepadInputAdapter` | `ESP32KeyBridgeEspBle.h` | EspBle 1.3.0+ | **implemented (hardware-verified: peer `ble_hid_gamepad`)** | BLE gamepads. Mapping through `mapButton()` / `mapHat()` and hat decoding share the core's `GamepadKeyMap`, so behaviour is identical to the USB Host side. All pads merge (up to 4 links); a disconnect drops only that pad |
| `EspBleClassicHidHostKeyboardInputAdapter` | `ESP32KeyBridgeEspBleClassic.h` | EspBle 1.3.0+ | **implemented (hardware-verified: peer `classic_hid_keyboard`)** | Bluetooth Classic HID Host keyboard (**original ESP32 only, one link**). Maps EspBleClassic's descriptor-decoded 256-bit usage plus modifier snapshot to the KeySet and sends lock LEDs back. Classic Host does not decode consumer reports, so media-key input is excluded. The sketch connects by address |
| `EspBleClassicHidHostMouseInputAdapter` | `ESP32KeyBridgeEspBleClassic.h` | EspBle 1.3.0+ | **implemented (hardware-verified: peer `classic_hid_keyboard`)** | Bluetooth Classic HID Host mouse (**original ESP32 only, one link**). Maps decoded buttons, X/Y, and wheel; no AC Pan |
| `GpioMatrixInputAdapter` | `ESP32KeyBridgeGpio.h` | Arduino GPIO only | **implemented (hardware-verified)** | A key matrix (up to 16 rows x 16 columns. Scanning, debouncing, and ghost blocking run in a dedicated 1 kHz task, so a blocked loop loses nothing. Started on the first `update()`; inactive rows are high-Z). Wiring goes in `setRowPins()` / `setColPins()` (rows drive, columns read through pull-ups) and the keymap in `setKeys()` in row-major order — the code layout mirrors the physical one, with no coordinates or indices; gaps are the invalid key `Key()`. Diodes are optional (without them, ghost blocking limits guaranteed simultaneous presses to 2; `setHasDiodes(true)` removes the limit). Debouncing is built in |
| `RotaryEncoderInputAdapter` | `ESP32KeyBridgeGpio.h` | PCNT (in ESP-IDF; chips with `SOC_PCNT_SUPPORTED` only) | **implemented (not yet hardware-verified)** | A rotary encoder (PCNT x4 quadrature decoding plus a glitch filter, no interrupts, one encoder per adapter, up to the chip's PCNT unit count). One detent becomes a key tap (`mapToKeys(cw, ccw)`) or a relative delta of +/-1 (`mapToAxis(axis)`). `setPulsesPerDetent()` (default 4 = EC11). The push switch belongs to a GpioKeyInputAdapter |
| ~~IR / I2C / SPI expanders~~ | - | - | **out of scope** | Repository-originated idea; not an agreed feature |

Note: text input (serial commands, serial barcode readers) is not an adapter — use the bridge's first-class `typeText()` / `typeChar()` entry points (example: SerialTextTyper).

## Output adapters

| Class | Header | Requires | Status | Reports locks | What it does |
|---|---|---|---|---|---|
| `ManualOutputAdapter` | `ESP32KeyBridge.h` (core) | none | **implemented** | simulated | Records the pressed set, characters, and relative deltas it receives. Can simulate a host's LED report (for tests and as a reference for custom outputs) |
| `EspUsbDeviceHidOutputAdapter` | `ESP32KeyBridgeEspUsbDevice.h` | EspUsbDevice | **implemented (hardware-verified: peer `usb_device_keyboard_output`)** | yes | A composite HID device (keyboard + consumer + mouse). Keyboard = boot 6KRO, consumer = one 16-bit usage, relative deltas become a mouse report (int8 saturation with carry; Pan is unsupported and dropped). Each report is sent only on change and retried on the next update when the endpoint was busy. Receiving LED output reports makes it eligible as the lock authority (kana included). `connected()` = mounted on a host |
| `TextOutputAdapter` | `ESP32KeyBridgeSerial.h` | none (Arduino `Print`) | **implemented (not yet hardware-verified)** | no | A text-native output. It consumes only the text stream (`writeText()`) and writes UTF-8 to a `Print` (Serial, Serial1, SoftwareSerial, ...). Characters come from `typeText()` and from presses on inputs configured with `convertLayout()` (decoded through the engraving; Enter to newline and Tab to tab are added by the core). **The output holds no layout of its own** (HID barcode to text is decided by the input's convertLayout) |
| `LogOutputAdapter` | `ESP32KeyBridgeSerial.h` | none (Arduino `Print`) | **implemented (not yet hardware-verified)** | no | A debug monitor. Writes pressed-set changes (`KEYS ...`), typed characters (`TEXT ...`), and relative deltas (`AXIS ...`) to a `Print` in human-readable form. Use it beside a real output or on its own for bring-up |
| `EspBleHidOutputAdapter` | `ESP32KeyBridgeEspBle.h` | EspBle 1.3.0+ | **implemented (hardware-verified: peer `ble_hid_output`)** | yes | A BLE HID peripheral (HID over GATT). Composite HID (keyboard + consumer + mouse) with report formats identical to the USB Device output (keyboard = boot 6KRO, consumer = one 16-bit usage, mouse = 8 buttons + X/Y/Wheel; Pan is unsupported and dropped). Each report is sent only on change and retried on the next update when it could not be sent. Receiving LED output reports makes it eligible as the lock authority (kana included). `connected()` = the host is connected, encrypted, and subscribed to input reports |
| `EspBleClassicHidOutputAdapter` | `ESP32KeyBridgeEspBleClassic.h` | EspBle 1.3.0+ | **implemented (hardware-verified: peer `classic_hid_output`)** | yes | Bluetooth Classic HID Device (**original ESP32 only**) with the same keyboard 6KRO + consumer + mouse reports as BLE. The three profiles must fit Classic's 214-byte SDP budget (Report Descriptor plus identity strings); `bluetooth.begin()` fails on overflow. One connected HID Host |

## How the PC sees it (USB device composition)

There is one USB Device output, and it always appears to the PC as a **composite HID device**.

| Output adapter | What the PC sees | Reports sent |
|---|---|---|
| `EspUsbDeviceHidOutputAdapter` | composite HID device (keyboard + consumer + mouse) | keyboard (boot 6KRO) / consumer (one 16-bit usage) / mouse (8 buttons + X/Y/Wheel, int8 saturation with carry; Pan is absent from a boot mouse report and dropped) |

Rules:

- **Always present the full composition.** An unused interface merely never sends a report, which is harmless on the USB side, so there is no keyboard-only variant (if a minimal composition is needed for an old BIOS, plug the real keyboard in directly). The keyboard interface supports the boot protocol, so BIOS/UEFI work.
- **The interface composition is fixed at `usbDevice.begin()`** and cannot change afterwards (adding or removing one requires re-enumeration, i.e. a replug). Adding a mouse or media keys to the configuration later causes no re-enumeration, because the descriptor already covers them.
- VID / PID / product name and other device identity go in `EspUsbDeviceConfig` (the stack belongs to the sketch).
- **It receives LED output reports**, so while connected it is a candidate for the lock authority chain (see Lock state in [DATA_MODEL.md](DATA_MODEL.md)).
- The rollover (32-key) report builder is in the core, but the USB Device output intentionally uses boot 6KRO. ~~An NKRO output adapter extension~~ is not in scope.

### The BLE HID output's composition

`EspBleHidOutputAdapter` follows the same idea and always publishes keyboard + consumer + mouse in one HID service (HID over GATT). The report contents match the USB Device output, so keys look the same whether you swap the output or register both. Only two things differ.

- The HID service composition is fixed at `ble.begin()` (the counterpart of `usbDevice.begin()`), so construct the output adapter before `ble.begin()`.
- `connected()` is not "mounted" but "connected + encrypted + subscribed to input reports". HOGP forbids notifications until the host subscribes, so it is briefly false right after pairing.

`BluetoothHotkeyInputAdapter` can wrap any keyboard input and drive an
`EspBleHidTargetController`. Scroll Lock + top-row 1..3 selects one of EspBle's
three stored bond slots; Scroll Lock + 0 replaces the selected slot in pairing
mode. The controller reconnects with directed advertising, while the wrapper
consumes the control chord and flashes only the physical keyboard's Scroll Lock
LED before restoring the effective lock state.

### The Bluetooth Classic HID output's composition

`EspBleClassicHidOutputAdapter` sends the same report payloads and composes keyboard + consumer + mouse into one Classic HID Device. Construct it before `bluetooth.begin()`, when the SDP record is registered. Unlike BLE, the Report Descriptor and `EspBleClassicHidProfileConfig` identity strings share a **214-byte** SDP budget. The three profiles fit with default strings; long custom strings can make `begin()` fail.

Classic connects to one address and has no HOGP GATT subscription, so `connected()` means the profile is configured and a Classic HID Host is connected.

## `update()` rate and tolerance to lost input

Sampling accuracy is the adapter's responsibility (see "the time and concurrency boundary" in [CORE_DESIGN.md](CORE_DESIGN.md)). The `update()` interval affects only how late the result is reflected (transform and output), never whether input is lost — with one exception, `GpioKeyInputAdapter`.

| Adapter | Sampling mechanism | If the loop blocks for a long time |
|---|---|---|
| `GpioKeyInputAdapter` | reads inside `update()` (**loop-rate dependent**) | **presses shorter than the block are lost**. An accepted trade-off for inputs held a long time, such as pedals and buttons |
| `GpioMatrixInputAdapter` | dedicated 1 kHz task (scanning, debouncing, ghost blocking; started on the first `update()`) | nothing lost (the pressed state is already settled in the task; only reflection is late) |
| `RotaryEncoderInputAdapter` | hardware counting in the PCNT peripheral | nothing lost (the counter accumulates; only reflection is late) |
| `EspUsbHostKeyboardInputAdapter` / `EspUsbHostMouseInputAdapter` / `EspUsbHostGamepadInputAdapter` / `EspUsbHostMidiInputAdapter` | callbacks from the EspUsbHost task (shared state across a critical section) | nothing lost (state snapshot held; mouse deltas are summed; MIDI keeps note on/off state) |
| `EspBleHidHostKeyboardInputAdapter` / `EspBleHidHostMouseInputAdapter` / `EspBleHidHostGamepadInputAdapter` | the EspBle event queue (dispatched in the `ble.update()` context) | nothing lost (the stack task queues events; only reflection is late). But **nothing arrives at all unless `ble.update()` is called** |
| `EspBleClassicHidHostKeyboardInputAdapter` / `EspBleClassicHidHostMouseInputAdapter` | the EspBleClassic event queue (dispatched in the `bluetooth.update()` context) | nothing lost (the adapter retains state and deltas). But **nothing arrives unless `bluetooth.update()` is called** |
| `ManualInputAdapter` | the sketch calls `press()` / `release()` directly | up to the sketch (whatever was called stays) |

Outputs (`EspUsbDeviceHidOutputAdapter` and friends) are state-snapshot based, so a late `update()` only means the last state is sent — nothing breaks (typing and macros simply key at a rate proportional to the update interval).

## Rotary encoder: quadrature decoding and detents

`RotaryEncoderInputAdapter` counts on **both** the A and B phases and on **both** the rising and falling edges — full x4 quadrature decoding. Here is how it works, and why one detent produces one output.

### x4 quadrature decoding (both phases, both edges)

Two PCNT channels reference each other's phase.

- Channel A: edge input = phase A, level input = phase B
- Channel B: edge input = phase B, level input = phase A

Each channel counts +/-1 on **both edges** and takes its direction (up or down) from the other phase's level. The result is both A edges plus both B edges = **4 counts per quadrature cycle**. The sign follows which phase leads (with a single phase and a single edge, direction is unavailable and resolution drops to a quarter).

A glitch filter (1 µs) removes contact bounce. No interrupts are used, and the counter advances continuously in hardware (PCNT), so rotation is never lost even when `update()` runs infrequently — only reflection is late.

### Folding into detents

Because it is x4, the raw count rises as "turn amount x 4", so **one detent produces several counts**. `pulsesPerDetent_` (default 4) normalizes that into detents.

- On a typical clicked encoder such as the EC11, **one detent = one quadrature cycle = 4 counts** — 4 / 4 = **one detent**. `mapToKeys` produces one tap, `mapToAxis` produces +/-1.
- **The remainder carries into an accumulator**, so nothing is truncated and **no drift accumulates**. Several detents piling up between infrequent `update()` calls are folded into the correct count all at once.

### Adjusting for other types and reversed rotation

Pulses per detent vary by encoder type, so match it against real hardware.

- **Two per detent** — a type whose detent falls on a half cycle. `setPulsesPerDetent(2)`.
- More or fewer than expected — check the datasheet's detents-per-revolution against cycles (pulses) per revolution and set `setPulsesPerDetent(n)`.
- **Turning the wrong way** — swap the arguments to `mapToKeys(cw, ccw)`, or for `mapToAxis` invert with `config.setAxisScale(axis, -1)` (the same as in the NaturalScroll example).

### How many

One adapter = one encoder = one PCNT unit. You can place as many as the chip has PCNT units (four on the ESP32-S3). On chips without `SOC_PCNT_SUPPORTED` the adapter is not compiled.

## Additional library dependencies

The core (`ESP32KeyBridge.h`) has no dependencies (pure C++). Adapter headers are **header-only**, so you only need the libraries for the adapters you include (include none, need none).

| Adapter header | Library | Where | Example `sketch.yaml` entry |
|---|---|---|---|
| `ESP32KeyBridgeEspUsbHost.h` | [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost) 2.6.0 or newer (`onKeyboardState`, LED sending to report-ID/NKRO keyboards, and 2.6.0's `addDeviceDisconnectedListener()` / `addMidiMessageListener()` — the multi-listener API that keeps the adapter from occupying single-slot callbacks) | Arduino Library Manager | `- EspUsbHost` |
| `ESP32KeyBridgeEspUsbDevice.h` | [EspUsbDevice](https://github.com/tanakamasayuki/EspUsbDevice) 2.0.0 or newer (`ledState()`) | Arduino Library Manager | `- EspUsbDevice` |
| `ESP32KeyBridgeEspBle.h` | [EspBle](https://github.com/tanakamasayuki/EspBle) 1.2.0 or newer | Arduino Library Manager | `- EspBle` |
| `ESP32KeyBridgeEspBleClassic.h` | EspBle 1.3.0 or newer (**original ESP32 and arduino-esp32 3.2.0+ only**) | Arduino Library Manager | `- EspBle (1.3.0)` |
| `ESP32KeyBridgeGpio.h` | none (Arduino GPIO only) | - | - |
| `ESP32KeyBridgeSerial.h` | none (Arduino `Print` only) | - | - |

- Board support is arduino-esp32 (the examples ship a 3.3.10 profile). Constraints such as the ESP32-P4's USB Device being fixed to HS are covered in the [SwapCtrlCapsLock README](../examples/practical/SwapCtrlCapsLock/README.md).
- When developing with the repositories checked out side by side, `sketch.yaml`'s `libraries:` also accepts directory references such as `- dir: ../../../../EspUsbHost`.

## Rules common to every adapter

- **The stack (EspUsbHost, EspUsbDevice, ...) belongs to the sketch** and is started with the stack's own config. An adapter takes a reference in its constructor and has **no `begin()`** (subscription happens on the first `update()`; only USB device interface registration happens at construction).
- Time, interrupts, and tasks stay inside the adapter and are handed thread-safely to the `update()` calling context (see "the time and concurrency boundary" in [CORE_DESIGN.md](CORE_DESIGN.md)).
- BLE and USB subscriptions use the **multi-listener API (`add*Listener()`)**, so the sketch can keep using its `onKeyboardState`, `onMouse`, and disconnect hooks. EspBle 1.3.0's Classic HID Host callbacks are single-slot, so the Classic keyboard/mouse adapters own the corresponding `onKeyboardState()` / `onMouse()` hook.
  - The **keyboard LED output report** (`onOutputReport`) is also single-slot and is owned by each HID output adapter. Poll `ledState()` instead.
- **Which context a callback runs in differs per library.** EspUsbHost and EspUsbDevice call from their own task, so the adapter crosses a critical section. EspBle and EspBleClassic dispatch from their respective `update()` caller, so call `ble.update()` / `bluetooth.update()` before `bridge.update()` in the same context.
- An output adapter emits only what it can represent and silently drops the rest (virtual keys never reach an output).
