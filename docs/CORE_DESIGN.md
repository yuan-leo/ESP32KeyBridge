# Core design

> 日本語: [CORE_DESIGN.ja.md](CORE_DESIGN.ja.md)

The `ESP32KeyBridge` core is a merge-and-transform engine for keyboard-style input that depends on no particular I/O method. The intermediate data representation is defined in [DATA_MODEL.md](DATA_MODEL.md); this document defines the pipeline and the module boundaries.

USB Host / USB Device, GPIO, BLE, UART, WebSerial, NVS, JSON and the like all live outside the core. Whatever dependency is needed belongs to an adapter or to an example.

## Data flow

```text
physical input
  -> input adapter        produces key events / text / relative deltas; a pressed set per input
  -> per-input transform  per-input remap and disable, (opt-in) layout conversion
  -> merge                pressed sets union; one-shot events join in arrival order
  -> global transform     global remap / layers / macros / disable
  -> output edge          text expands into keystrokes, action queue, relative deltas summed
  -> output adapter       turns what it can represent into reports and sends them

the other way:
  output adapter (lock reporting) -> internal LockState shadow -> every input adapter (LEDs, ...)
```

- Per-input and global transforms mean different things. Per-input is "only keys from this input device"; global is "from any input, after merging". Layout conversion is per-input because the engraving is a property of the input device.
- Layer state and macros operate after merging and by default apply to every input.

## What an input adapter is responsible for

- Reading raw input (USB reports, BLE, a GPIO scan, serial) and converting it into key events / text / relative deltas. Keyboard keys go in as raw HID usages — **an input adapter does not interpret layout**.
- Containing everything hardware- and transport-specific: scanning, debouncing, coordinate-to-key assignment (matrix), pairing (BLE), protocol interpretation.
- **Time-dependent work (debouncing, timeouts, repeat decisions) belongs to the adapter.** The core holds no clock.
- Synthesizing logical presses (toggles, one-shots, turning encoder pulses into taps).
- Detecting disconnection and disappearance (once detected, the core clears the pressed set).
- Optionally, receiving LockState notifications from the core and reflecting them onto the device (LED report, GPIO pins).

## What an output adapter is responsible for

- Turning the merged, transformed state and one-shot events into reports within what it can represent, and sending them (silently dropping what it cannot).
- Consuming the action queue at its own send rate (the HID poll interval). Modifier changes and keystrokes go in separate reports.
- For USB device and BLE HID adapters, receiving the host's LED output report and reporting lock state to the core (a lock-reporting output).
- USB descriptors (including composite compositions), protocol (boot / report), and send timing are the adapter's domain. The composition is fixed at enumeration time.

## The time and concurrency boundary

Time, interrupts, and tasks are **confined inside adapters**, keeping the core and the `update()` calling context single.

- Sampling accuracy (matrix scan period, debouncing, not missing encoder edges) is guaranteed by a task, ISR, or peripheral inside the adapter. As implemented: the GPIO matrix scans at 1 kHz in a dedicated task (debouncing and ghost blocking included), and the rotary encoder counts in hardware through the PCNT (pulse counter) peripheral, with `update()` reading the counter difference (no ISR needed).
- USB Host / USB Device / BLE already have stack-side tasks. However, **which context user callbacks run in differs per stack**, so how they are handed over differs to match.
  - EspUsbHost / EspUsbDevice: callbacks run from the stack's task. The adapter hands data to shared state across a critical section.
  - EspBle: the stack task only queues events, and user callbacks are dispatched from the context that called `EspBle::update()`. So no synchronization is needed inside the adapter — instead, it is a precondition that **the sketch calls `ble.update()` in the same context as `bridge.update()`**.
- An adapter's responsibility ends at "thread-safe handover from its internal concurrency to the `update()` calling context". Reflecting into the pressed set, transforming, and output all happen in the single `update()` context.
- Because of this split, input is not lost even when the sketch's loop is stalled by other work (it has already been settled inside the adapter); only transformation and output are late. **The exception is `GpioKeyInputAdapter`** (sampling happens inside `update()`, hence loop-rate dependent), accepted as being for inputs held a long time, such as pedals and buttons. The per-adapter table is under "`update()` rate and tolerance to lost input" in [ADAPTERS.md](ADAPTERS.md).
- A mechanism for driving the bridge automatically from a task is not going into the core (the mutual exclusion would spread into `applyConfig()` and `typeText()`). If it becomes necessary, it can be added as an opt-in Arduino-layer wrapper (see the future notes in [DECISIONS.ja.md](DECISIONS.ja.md)).

## The control plane

Apart from the data path, the bridge holds a small number of **mode states**.

- Which layers are active (changed by pressing virtual keys)
- Layout conversion on/off (changed by a hotkey — a virtual key — or an external command)
- (Future) selecting the active output

A virtual key is consumed by one of three things: a layer, launching a macro, or a mode change. Configuration changes from outside (a serial CLI, WebSerial) enter the same control plane. Even when keys are held at the moment a configuration change is applied, releases do not break, because of the resolve-at-press-time principle.

## Dependency boundary and portability

The core is implemented as portable, pure C++, with everything platform-dependent in the adapter layer. The adapter layer is effectively the HAL.

What the core may depend on:

- C++ language features up to C++17, plus `<stdint.h>` / `<stddef.h>`.
- Nothing else. Not the Arduino API, ESP-IDF, FreeRTOS, `millis()`, `PROGMEM`, or anything else platform-specific.

Rules the core follows:

- **Never read a clock.** When a timestamp is needed, the adapter (or the caller) attaches it. Time-dependent work happens in the adapter, or by injecting a time into `update()`.
- **Never allocate dynamically.** Everything is fixed-capacity. Exceptions and RTTI are not used either.
- **Driven from a single context.** The core assumes one execution context calls `update()`; serializing events posted from interrupts or other tasks is the adapter's responsibility.
- Constant data such as layout descriptions is `const` (placed in flash / rodata) and assumes no platform-specific access method.

This boundary is what lets the core compile and run tests directly on a host PC (the unit tests exist because of this property) and be ported to platforms beyond the ESP32 by writing adapters alone. Per-SoC feature differences (whether USB OTG exists, whether BLE exists) show up as which adapters are available, never inside the core.

## Configuration boundary

The core receives a configuration object and applies it.

What the core does not prescribe:

- Where configuration comes from, or where it is stored.
- External formats such as JSON, binary, or a C++ builder.
- Transports such as WebSerial, UART, BLE, or TCP. Configuration UIs.

What the core does have:

- C++ API entry points for building, validating, and applying a configuration.

What a configuration contains: the remap table (crossing kinds), disables, layer definitions, macros (key sequences and text), per-input layout conversion settings, the output's host layout, and relative axis inversion and scaling.

## Implementation status

`src/` is the implementation of this specification (the core = implementation steps 1-6, done; adapters = step 7, in progress). For the order and progress, see [DEVELOPMENT_PLAN.ja.md](DEVELOPMENT_PLAN.ja.md).
