# ESP32KeyBridge: a beginner's guide

> 日本語: [GUIDE_BASICS.ja.md](GUIDE_BASICS.ja.md)

For anyone using ESP32KeyBridge for the first time: a document about **what is actually happening**. Every term is explained here.

The real code lives in the examples. This document concentrates on the concepts and links to the example that shows each one.

Because this library binds together several very different interfaces — USB Host, USB Device, BLE, GPIO — into one key model, chapters 5 and 6 cover the traits and limits of each interface individually.

---

## 1. What ESP32KeyBridge is

### 1.1 A library for building something you put in between

ESP32KeyBridge puts an ESP32 **between a keyboard and whatever reads it**.

```text
[USB keyboard] --(USB)--> [ESP32] --(USB)--> [PC]
                            ^ the conversion happens here
```

What the PC sees is an ordinary keyboard. **No driver, no resident software on the PC, and no configuration change on the keyboard either.** "Make Caps Lock a Ctrl", "type a US-engraved keyboard as engraved on a PC set to Japanese", "use a Bluetooth keyboard on a BIOS screen" — this library solves those without touching either device.

What you put it between is not limited to USB. The input can be USB, BLE, or GPIO; the output can be USB, BLE, or serial; and you can **merge several inputs and fan out to several outputs** at once.

| Can be an input | Can be an output |
|---|---|
| USB keyboard, mouse, gamepad, MIDI | USB HID device (an ordinary keyboard to the PC) |
| BLE keyboard, mouse, gamepad | BLE HID device (a Bluetooth keyboard to the PC) |
| GPIO switches, key matrix, rotary encoder | Serial (text out, or a debug log) |
| Your own adapter (touch, I2C expander, network, ...) | Your own adapter |

### 1.2 Three layers

Before reading any code, hold on to the fact that there are three layers.

| Layer | Responsibility | Concretely |
|---|---|---|
| **Input adapter** | Read the hardware and **normalize to the common key model** | `EspUsbHostKeyboardInputAdapter`, ... |
| **Core (the bridge)** | Merge and transform. Knows nothing about hardware | the `ESP32KeyBridge` class |
| **Output adapter** | Emit the result in whatever form it can represent | `EspUsbDeviceHidOutputAdapter`, ... |

The core is portable C++ that **depends on neither Arduino, nor ESP-IDF, nor any hardware**. It never reads a clock, never allocates dynamically, and uses neither exceptions nor RTTI. Everything platform-specific lives in the adapter layer.

That separation buys two things: the whole transformation logic can be **pinned down by unit tests on a host PC**, and supporting new hardware is "write one adapter".

### 1.3 First principle — everything runs from `update()`

**There is no `begin()`.** Registration and configuration only store state; things actually happen in `bridge.update()`, called from `loop()`.

```cpp
void loop() {
  bridge.update();  // reading inputs, transforming, and writing outputs all run from here
  delay(1);
}
```

One `update()` does all of this:

1. Calls `update()` on every input adapter
2. Reads each input's pressed set and diffs it against last time (pressed / released)
3. Applies the transforms and builds the output pressed set
4. Updates the lock state
5. Writes to every output adapter

So there is **no ordering constraint** between `addInput()` / `addOutput()`, `applyConfig()`, and starting the stacks. Write them in any order; everything starts working from the first `update()`.

The flip side: **forget `update()` and nothing happens at all.**

### 1.4 Second principle — a key is "kind + code"

Inside this library, keys and buttons are all one type, `Key`, holding a **(kind, code)** pair.

| Kind (`KeyKind`) | What the code means | Examples |
|---|---|---|
| `Keyboard` | **Numerically identical** to the HID keyboard page (0x07) Usage ID | A=0x04, Enter=0x28, LeftCtrl=0xE0 |
| `Consumer` | Numerically identical to the HID consumer page (0x0C) Usage ID | Volume Up=0x00E9, Play/Pause=0x00CD |
| `MouseButton` | Button number, 1-origin | left=1, right=2, middle=3 |
| `Virtual` | **Bridge-internal only.** Never reaches an output | layer triggers, macro triggers |

In a sketch you use the enums, so `Key` itself rarely appears.

```cpp
esp32keybridge::KeyboardUsage::CapsLock          // a keyboard key
esp32keybridge::ConsumerUsage::VolumeIncrement   // a media key
esp32keybridge::MouseUsage::Middle               // a mouse button
esp32keybridge::VirtualUsage::V1                 // an internal-only key
```

Where this pays off is **remapping across kinds**. "F13 to volume up", "a gamepad button to Escape", "a mouse side button to a key" are each **one table row**, not a special case.

```cpp
config.global.remap(esp32keybridge::KeyboardUsage::F13,
                    esp32keybridge::ConsumerUsage::VolumeIncrement);
```

One more property matters. **A `Keyboard` code identifies a physical key position, never a character.** On a JIS keyboard and a US keyboard alike, the key where A sits sends 0x04. Interpreting the engraving is the PC's job. That is exactly why a US keyboard on a PC set to Japanese produces the wrong symbols — and what the layout conversion in 3.4 is for.

### 1.5 Third principle — a key is resolved the moment it is pressed

A key is transformed **once, at press time**, and that result is held until it is released. If a layer changes while it is held, or `applyConfig()` swaps the configuration, **the key that gets released is never a different key from the one that was pressed.**

This is the rule that structurally prevents stuck keys. If the transform were re-run at release time, this would happen: hold Fn, press H, Left arrow goes down, release Fn first, release H — and the Left arrow stays down forever.

For the same reason, **when an input disconnects, its keys are all released.** A BLE keyboard's battery dying cannot leave a modifier stuck.

### 1.6 The three-step sketch

Every example has the same three steps.

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
  // 1) Start the hardware (the stack belongs to the sketch, not to the adapter).
  usbHost.begin();
  usbDevice.begin();

  // 2) Wire the bridge.
  bridge.addInput(keyboard);
  bridge.addOutput(pc);

  // 3) Build a configuration and apply it.
  esp32keybridge::ESP32KeyBridgeConfig config;
  config.global.remap(esp32keybridge::KeyboardUsage::CapsLock,
                      esp32keybridge::KeyboardUsage::LeftCtrl);
  config.global.remap(esp32keybridge::KeyboardUsage::LeftCtrl,
                      esp32keybridge::KeyboardUsage::CapsLock);
  bridge.applyConfig(config);
}

void loop() {
  bridge.update();
  delay(1);
}
```

There is one ownership rule to remember. **The USB and BLE stacks (`EspUsbHost` / `EspUsbDevice` / `EspBle`) belong to the sketch and are started by the sketch.** An adapter only takes a reference and has no `begin()` of its own. Port selection, VID/PID, the BLE device name and security settings go into the **stack's own config**, not the adapter's.

---

## 2. How data flows

### 2.1 The pipeline

```text
physical input
  -> input adapter      produces keys / characters / relative deltas; holds a per-input pressed set
  -> per-input transform  per-input remap and disable, (opt-in) layout conversion
  -> merge              pressed sets union; one-shot events join in arrival order
  -> global transform   global remap / layers / macros / disable
  -> output edge        characters expand into keystrokes; relative deltas are summed
  -> output adapter     turns what it can represent into reports and sends them

the other way:
  output adapter (lock reporting) -> internal LockState -> every input adapter (LEDs, ...)
```

### 2.2 Only two kinds of traffic exist

Everything flowing through the intermediate representation falls into one of two classes. **When you consider a new input or output, decide this first and the processing rules follow mechanically.**

| | **Held (state)** | **One-shot (event)** |
|---|---|---|
| Meaning | Means something *while it is held* | The moment it happens is all there is; consumed once |
| Representation | Pressed set (a snapshot) | An ordered queue |
| Merging | Union | Joined in arrival order |
| On disconnect | Cleared, released in full | Pending items are discarded |
| Examples | Keyboard keys, modifiers, media keys, mouse buttons | Characters, mouse movement, wheel |

Two of these need care:

- **Media keys (consumer) are held state.** Whether a key repeats while held (volume) or acts on the edge (Play/Pause) is the **PC's interpretation**, and the bridge takes no position on it — the same attitude it takes toward the difference between Enter and Caps Lock.
- **For a mouse, buttons are held state while movement and wheel are one-shot** (relative deltas).

### 2.3 Merging — union, and what disconnect means

Register several inputs and the pressed sets become a **union**.

- A key is released only when **every input holding it has let go**
- When one input disconnects, **only its share** is released

That property is what makes "a USB keyboard plus extra GPIO buttons, seen by the PC as one keyboard" work (`MergeKeyboards`).

The limits are fixed-size, because the core never allocates:

| Subject | Limit |
|---|---|
| Input adapters | 8 |
| Output adapters | 4 |
| Simultaneous presses (merged set) | 32 keys |
| Held presses tracked per input | 64 |

### 2.4 Per-input vs global

Transforms come in a **per-input** flavour and a **post-merge** one. They mean different things.

| | Per-input (`InputConfig`) | Global (`config.global`) |
|---|---|---|
| Applies to | Only keys from the bound inputs | Keys from any input |
| Offers | remap / disable / **layout conversion** | remap / disable |
| Typical use | "treat only this keyboard as US-engraved" | "Caps Lock is Ctrl everywhere" |

Layout conversion is per-input because **the engraving is a property of the input device**.

```cpp
esp32keybridge::ESP32KeyBridgeConfig config;
esp32keybridge::InputConfig &scanner = config.addInputConfig();
scanner.convertLayout(esp32keybridge::KeyboardLayout::enUs());  // this input is US-engraved
bridge.addInput(barcode, scanner);   // bind it
bridge.addInput(keyboard);           // this one has no per-input settings
```

`addInput()` takes only the handle's identity; **the contents take effect when `applyConfig()` copies them**. So you may fill in an `addInputConfig()` before or after the `addInput()` call.

### 2.5 An output emits only what it can represent

An output adapter emits what it can represent and **silently drops the rest**.

- `Virtual` keys never reach an output (they are meant to be consumed inside the pipeline)
- Handing relative deltas to an output with no mouse does nothing
- The text stream is received only by text-native outputs (serial); HID outputs see the synthesized keystrokes instead

Dropping silently rather than erroring is deliberate: it is what lets you swap outputs without touching the configuration. The cost is that **when the output is not what you expected, the first thing to check is what that output adapter can represent** (chapter 6).

---

## 3. Transforms

Configuration is always built into an `ESP32KeyBridgeConfig` and handed to `applyConfig()`. The object is **copied by value**, so it is fine for the local variable to go out of scope afterwards.

### 3.1 Remap and disable

```cpp
config.global.remap(from, to);
config.global.disable(key);
```

**A remap is a single-step lookup and never chains.** Writing `CapsLock->LeftCtrl` and `LeftCtrl->CapsLock` is therefore a swap (if it chained, it would either loop forever or work in one direction only).

Limits are 32 remaps and 32 disables (each per-input config carries its own budget of the same size).

### 3.2 Layers — how to build an Fn key

**A physical Fn key never appears on HID.** The keyboard handles it internally, so it cannot be observed, shared, or remapped. That is a limit of the world, not of this library.

So the bridge builds the equivalent differently: remap some key that *does* reach HID into a `Virtual` key, consume it, and overlay a substitution table while it is held.

```cpp
constexpr esp32keybridge::Key kFn = esp32keybridge::virtualKey(1);

config.global.remap(esp32keybridge::KeyboardUsage::CapsLock, kFn);  // Caps Lock becomes Fn
esp32keybridge::LayerConfig &nav = config.addLayer(kFn);
nav.remap(esp32keybridge::KeyboardUsage::H, esp32keybridge::KeyboardUsage::Left);
nav.remap(esp32keybridge::KeyboardUsage::J, esp32keybridge::KeyboardUsage::Down);
nav.remap(esp32keybridge::KeyboardUsage::K, esp32keybridge::KeyboardUsage::Up);
nav.remap(esp32keybridge::KeyboardUsage::L, esp32keybridge::KeyboardUsage::Right);
```

- **The trigger key is consumed and never reaches an output**
- Layers act after merging, so by default they affect **every input** (this is what makes a split keyboard or a foot-pedal Fn work)
- Per 1.5, the substitution is fixed at press time: **release the layer first and what comes up is still the arrow key**
- Limits are 4 layers, 16 remaps each

On a keyboard you built yourself, the adapter can of course emit `Virtual` keys directly.

**Tap/hold (tap for the original key, hold for Fn) is out of scope in this version.** It is a time-dependent decision, which conflicts with the core holding no clock.

### 3.3 Text — macros and `typeText()`

There is a separate path that carries **characters** rather than keys.

```cpp
config.textMacro(esp32keybridge::VirtualUsage::V2, "user@example.com");  // a macro
bridge.typeText("Hello, world!\n");                                      // direct injection
```

Characters **stay characters all the way to the output edge**, where `config.hostLayout` (the PC's layout) expands them into keystrokes. Because of that split, **a macro definition does not depend on the output's layout**: the same macro types correctly on a US-configured PC and a Japanese-configured one.

Expansion handles the tedious parts for you:

- Synthesizing the Shift a character needs (whether `@` requires Shift is layout-dependent)
- **Caps Lock compensation**, from the internal lock-state shadow
- **Parking and restoring modifiers the user is holding** — each character's frame is atomic and nothing interleaves
- Digits expand to the main row, not the keypad, so nothing depends on Num Lock

Characters that cannot be typed (kana, kanji, emoji, symbols absent from that layout) are **skipped and counted in `textEncodeFailCount()`**. Skipped characters still reach text-native outputs (serial).

Only five control characters carry meaning:

| Character | Keystroke |
|---|---|
| 0x09 TAB | Tab |
| 0x0A LF / 0x0D CR | Enter (CRLF produces one Enter) |
| 0x08 BS | Backspace |
| 0x1B ESC | Escape |

**Escapes are not interpreted.** Turning `"\n"` into a real newline is the compiler's job; the bridge only ever sees actual control characters.

#### Backpressure

The text queue is a fixed 64 characters. `typeText()` **stops instead of dropping when it fills, and returns the number of bytes consumed.** Feed a long string like this:

```cpp
const char *p = text;
while (*p != '\0') {
  p += bridge.typeText(p);
  bridge.update();   // typing advances here and the queue drains
}
```

`typeAvailable()` reports the free space, so a `Serial.available()`-style loop works too (`SerialTextTyper`). Only the paths that cannot resume — macros and `typeChar()` — drop on overflow and count it in `textOverflowCount()`.

Limits are 4 macros of 31 characters each.

### 3.4 Layout conversion — a US keyboard on a PC set to Japanese

Per 1.4, HID carries key positions rather than characters. Plug a US-engraved keyboard into a PC set to Japanese and the letters and digits are right while the symbols are wrong.

Layout conversion is a **(key, shift) -> (key, shift)** transform. Substituting the usage alone is not enough: Shift+2 is `@` on US, but on ja_jp it is 0x2F unshifted.

```cpp
esp32keybridge::InputConfig &kb = config.addInputConfig();
kb.convertLayout(esp32keybridge::KeyboardLayout::enUs());   // the input's engraving
config.hostLayout = esp32keybridge::KeyboardLayout::jaJp(); // the PC's interpretation
config.layoutConversionToggle = esp32keybridge::KeyboardUsage::F12;  // the bypass key
bridge.addInput(keyboard, kb);
```

What to know:

- **Identity keys pass straight through** — letters, digits, navigation, modifiers. Only symbols convert (about 15 keys between US and JA)
- **Shift is both suppressed and synthesized.** Held physical Shift may be dropped on the way out; absent Shift may be added. It is maintained while held, so long-press and auto-repeat survive
- **Modifier combinations (Ctrl / Alt / GUI held) are not converted by default**, because shortcuts are usually bound positionally
- **A way to turn it off is required in practice.** BIOS, UEFI, and recovery environments interpret keys differently, so leaving conversion on garbles them instead. Assign a key to `layoutConversionToggle`
- 16 locales (`en_us` / `ja_jp` plus 14 generated) x 4 planes (base / Shift / AltGr / AltGr+Shift). Fetch with `KeyboardLayout::byName("fr_fr")`

The two directions are not equally useful. **US engraving to a ja_jp host covers ASCII 100%**, while **ja_jp engraving to an en_us host cannot type `¥`** and leaves gaps.

Mixed engravings across several keyboards still work, because conversion is per-input and happens before the union. Plugging each keyboard into the PC directly cannot solve this at all: the OS has exactly one layout.

### 3.5 Relative axes — scale and inversion

Mouse movement, wheel, horizontal wheel, and encoder rotation flow as deltas on an axis (`Axis::X` / `Y` / `Wheel` / `Pan`).

```cpp
config.setAxisScale(esp32keybridge::Axis::Wheel, -1);  // natural scrolling
config.setAxisScale(esp32keybridge::Axis::X, 2);       // double speed
```

Negative inverts; the magnitude multiplies. **Curved transforms such as pointer acceleration are out of scope** (the core keeps only linear ones).

Deltas are summed across inputs, saturated to the report range (int8) at the output, and **the remainder carries into the next report**. Move fast and the total distance still comes out right.

### 3.6 Related examples

| Example | What it shows |
|---|---|
| [SwapCtrlCapsLock](../examples/practical/SwapCtrlCapsLock/) | A swap in two remap lines. **The reference explanation of the P4's USB ports** |
| [MediaKeys](../examples/practical/MediaKeys/) | Remapping across kinds (F13 to volume) |
| [NavLayer](../examples/practical/NavLayer/) | Hold Caps Lock and HJKL become arrows |
| [LayerKeypad](../examples/practical/LayerKeypad/) | One key, two functions, on a homemade keypad |
| [UsKeyboardOnJapanesePc](../examples/practical/UsKeyboardOnJapanesePc/) | Layout conversion |
| [SerialTextTyper](../examples/practical/SerialTextTyper/) | `typeText()` and backpressure |
| [FootSwitch](../examples/practical/FootSwitch/) | Text macros and media control |
| [NaturalScroll](../examples/practical/NaturalScroll/) | Axis inversion and button swapping |

---

## 4. Lock state and LEDs

Caps Lock, Num Lock, and Scroll Lock state belongs to **the PC, not the keyboard**. The keyboard only reports "this was pressed"; the instruction to light the LED comes back from the PC as an output report. So anything sitting in between is responsible for relaying that reverse flow.

### 4.1 There is exactly one authority

| Situation | Authority | What the bridge does |
|---|---|---|
| A lock-reporting output (USB Device / BLE HID) is connected | **That PC** | Relays only; never toggles by itself |
| No such output (serial only, USB unplugged) | **The bridge itself** | Acts as the terminal host and toggles Caps/Num/Scroll |

When an external authority appears, its first report overwrites the shadow (external wins); when it disappears, the bridge carries on from the last value.

If several lock-reporting outputs are registered, **the first one registered is the authority**. Drift against the other host is not tracked (not forbidden, not reconciled either). One lock-reporting output is the recommendation.

### 4.2 Forwarding to input LEDs

The bridge always keeps one internal shadow of the lock state and **notifies every input adapter** when it changes. Each adapter then reflects it its own way: an LED report for a USB keyboard, an output report for a BLE keyboard, a pin for a keyboard you built.

**An input that connects later is notified with the current state too.** A PC only sends a report when something changes, so without that, a keyboard plugged in afterwards would sit there with its Caps Lock LED dark.

The kana LED (`kana`) is the exception: **the bridge forwards it but never toggles it.** Whether it can be carried at all depends on the transport (4.3).

### 4.3 What each transport can carry

| Path | num / caps / scroll | kana |
|---|---|---|
| USB Device output -> bridge | yes | yes |
| BLE HID output -> bridge | yes | yes |
| Bridge -> USB Host keyboard LEDs | yes | no (EspUsbHost has no way to send it) |
| Bridge -> BLE HID Host keyboard LEDs | yes | yes |

---

## 5. Input adapters: the traits and limits of each interface

From here on it is interface-specific. **Every adapter looks the same to the core** — just `update()` and `keys()` — but what happens behind that differs completely.

### 5.1 Rules common to all adapters

Three rules hold everywhere.

1. **The stack belongs to the sketch.** An adapter takes a reference in its constructor and has no `begin()`. Subscription happens on the first `update()` (the sole exception is USB Device interface registration, which happens at construction).
2. **Subscription uses the multi-listener API (`add*Listener()`).** An adapter never takes a single-slot `on*()` hook. **Your sketch can keep using `onKeyboardState`, `onMouse`, `onDeviceDisconnected` and the rest exactly as before.** An adapter drops the listener ids it took when it is destroyed.
   - The exception is the **keyboard LED output report** (`onOutputReport`). All three libraries treat LEDs as state rather than an event and keep that slot single, so the **output adapter** takes it. Poll `keyboard.ledState()` / `ble.hidKeyboard().ledState()` instead — the value updates whether or not a callback is installed.
3. **Time, interrupts, and tasks stay inside the adapter.** The core runs in a single context.

### 5.2 Callbacks run in different contexts per library

This is **the easiest thing to get wrong in this library.**

| Stack | Callbacks run from | What the adapter does | What the sketch must do |
|---|---|---|---|
| **EspUsbHost / EspUsbDevice** | the stack's own task | hands data to the `update()` context across a critical section | nothing |
| **EspBle** | **the context that called `EspBle::update()`** | no synchronization needed | **call `ble.update()` in the same context as `bridge.update()`** |

So with BLE, `loop()` looks like this:

```cpp
void loop() {
  ble.update();     // BLE events are dispatched here
  bridge.update();  // right after, so a report that just arrived is part of this cycle
  delay(1);
}
```

**Without `ble.update()`, no BLE input ever arrives.** The order matters too: `ble.update()` first. Put it after and every input is reflected one cycle late.

### 5.3 What happens when `update()` is late

Sampling accuracy is **the adapter's responsibility**. A slow `update()` delays *reflection* only and does not lose input — **with exactly one exception**.

| Adapter | Sampling mechanism | If the loop blocks for a long time |
|---|---|---|
| `GpioKeyInputAdapter` | **reads inside `update()`** | ⚠ **presses shorter than the block are lost** |
| `GpioMatrixInputAdapter` | dedicated 1 kHz task | nothing lost |
| `RotaryEncoderInputAdapter` | PCNT peripheral counts in hardware | nothing lost |
| USB Host family | callbacks from the EspUsbHost task | nothing lost (state snapshot held) |
| BLE HID Host family | the EspBle event queue | nothing lost (but `ble.update()` is required) |

`GpioKeyInputAdapter`'s trade-off is "for inputs that stay pressed a long time" — pedals, panel buttons. If you need short taps caught reliably, use `GpioMatrixInputAdapter` (which works fine as a 1x1 matrix of one key).

### 5.4 USB Host input

Header: `ESP32KeyBridgeEspUsbHost.h` / requires [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost) 2.6.0 or newer

| Adapter | Devices | What it gives you |
|---|---|---|
| `EspUsbHostKeyboardInputAdapter` | up to 4, merged | keyboards and their media keys |
| `EspUsbHostMouseInputAdapter` | up to 4, merged | buttons, movement, wheel, horizontal wheel |
| `EspUsbHostGamepadInputAdapter` | up to 4, merged | button and hat/D-pad mapping |
| `EspUsbHostMidiInputAdapter` | up to 4 | MIDI note mapping |

**Keyboard** state arrives as a **256-bit snapshot** through `onKeyboardState`. Boot, report-ID boot, and NKRO reports — and modifier-only changes — are all absorbed into that one shape, so **every keyboard takes the same path**. The boot rollover error codes (usages 0x01-0x03) are excluded. Media keys go up to 8 simultaneous usages.

**Mouse** presence is decided by the first report that arrives, because there is no API to enumerate devices — plugging one in is not by itself enough to be counted as present. Horizontal wheel (AC Pan) is absent from a boot mouse report and reads 0 there.

**Gamepad** emits only the controls you mapped. Button numbers differ per controller, so **attach a `LogOutputAdapter` first and watch which numbers arrive** before deciding.

```cpp
gamepad.mapButton(1, esp32keybridge::KeyboardUsage::Space);
gamepad.mapHat(esp32keybridge::KeyboardUsage::Up,
               esp32keybridge::KeyboardUsage::Down,
               esp32keybridge::KeyboardUsage::Left,
               esp32keybridge::KeyboardUsage::Right);  // diagonals press two keys
```

**Analog sticks and triggers are not supported in this version** — buttons and the hat only.

**MIDI**: Note On with velocity > 0 presses, Note Off (or Note On with velocity 0) releases. Every channel is listened to by default; `setChannel(ch)` narrows it to one. **Control changes and pitch bend are ignored.**

### 5.5 BLE HID Host input

Header: `ESP32KeyBridgeEspBle.h` / requires [EspBle](https://github.com/tanakamasayuki/EspBle) 1.2.0 or newer

| Adapter | Connections | What it gives you |
|---|---|---|
| `EspBleHidHostKeyboardInputAdapter` | up to 4, merged | keyboards and their media keys |
| `EspBleHidHostMouseInputAdapter` | up to 4, merged | buttons, movement, wheel |
| `EspBleHidHostGamepadInputAdapter` | up to 4, merged | button and hat/D-pad mapping |

Keyboards are read through **the same 256-bit snapshot** as the USB Host side. Gamepad mapping and hat decoding share the core's `GamepadKeyMap`, so the behaviour is **identical to USB**.

Four things are specific to BLE.

**1. The sketch drives the connection.** The adapters only consume HID reports once a link exists. The scan -> connect -> pair -> `hidHost().discover()` chain stays in the sketch, so you are free to filter by name or address. Register the adapters with the bridge up front; having no connection yet is harmless.

```cpp
ble.scanner().onResult([](const EspBleScanResult &result) {
  if (result.connectable && result.advertisesService("1812")) {  // HID Service
    ble.scanner().stop();
    ble.connect(result);
  }
});
ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
  if (event.success) {
    ble.hidHost().discover(event.connection.id);  // discover after encryption
  }
});
ble.setAutoReconnect(true);
ble.hidHost().setAutoRediscover(true);
ble.scanner().start();
```

**2. Encryption is effectively mandatory.** Commercial keyboards refuse to hand out HID reports over an unencrypted link. Set `bleConfig.security.enabled = true` and `bonding = true`, and **call `discover()` only after pairing succeeds** — touching a HID attribute before encryption returns an ATT error.

**3. One media-key usage per connection.** The HOGP consumer report carries only one usage at a time (the USB Host side carries 8). Press two media keys at once and only one arrives.

**4. It can forward the kana LED**, which the USB Host side cannot (4.3).

#### 5.5.1 Bluetooth Classic HID Host input

Header: `ESP32KeyBridgeEspBleClassic.h` / requires EspBle 1.3.0+ / **original ESP32 only**

`EspBleClassicHidHostKeyboardInputAdapter` and `EspBleClassicHidHostMouseInputAdapter` map the keyboard/mouse reports decoded from the Classic peer's Report Descriptor onto the same KeySet and relative axes as BLE. Report content is compatible; connection mechanics are not.

- Classic HID Host has **one link**; both adapters can read a composite keyboard + mouse on it
- Instead of scanning a BLE service advertisement (`0x1812`), obtain an address through inquiry or configuration and pass it to `hidHost().connect(address)`
- Start both `bluetooth.begin()` and `bluetooth.hidHost().begin()`; the sketch owns reconnection too
- EspBle 1.3.0 decodes keyboard and mouse on the Classic Host side. Consumer/gamepad reports remain available raw but are not inputs to these adapters
- Classic callbacks are single-slot: the keyboard adapter owns `onKeyboardState()` and the mouse adapter owns `onMouse()`
- Call `bluetooth.update()` before `bridge.update()` in the same context

```cpp
EspBleClassic bluetooth;
EspBleClassicHidHostKeyboardInputAdapter keyboard(bluetooth);

bluetooth.begin();
bluetooth.hidHost().begin();
bluetooth.hidHost().connect("00:11:22:33:44:55");

void loop() { bluetooth.update(); bridge.update(); }
```

The **original ESP32 is the only ESP32 SoC with a Classic BR/EDR radio**. Use BLE on S3/C3/C6/H2/P4.

Starting both BLE and Classic on one original ESP32 is experimental dual-host mode in EspBle and consumes substantial flash. `ClassicKeyboardToBle` uses about 97% of the default app partition, so product additions need a partition scheme with a larger app slot.

### 5.6 GPIO input

Header: `ESP32KeyBridgeGpio.h` / no external library (Arduino GPIO, plus PCNT for the encoder)

#### `GpioKeyInputAdapter` — individual switches

```cpp
keys.addKey(4, esp32keybridge::KeyboardUsage::F13);           // defaults: activeLow + pullUp
keys.addKey(5, esp32keybridge::ConsumerUsage::PlayPause);
keys.setDebounceMillis(5);  // default 5 ms
```

Pins are configured on the **first `update()`** after registration. Reading and debouncing happen inside `update()`, so per 5.3 this one is **loop-rate dependent** — the accepted trade-off for foot switches and panel buttons.

#### `GpioMatrixInputAdapter` — a key matrix

Up to 16 rows x 16 columns. Its distinguishing trait is that **wiring and keymap are written separately**.

```cpp
matrix.setRowPins({4, 5, 6, 7});      // rows are driven one at a time
matrix.setColPins({15, 16, 17});      // columns are read through pull-ups
matrix.setKeys({                       // row-major: the code layout mirrors the physical one
  esp32keybridge::KeyboardUsage::Keypad7, esp32keybridge::KeyboardUsage::Keypad8, esp32keybridge::KeyboardUsage::Keypad9,
  esp32keybridge::KeyboardUsage::Keypad4, esp32keybridge::KeyboardUsage::Keypad5, esp32keybridge::KeyboardUsage::Keypad6,
  esp32keybridge::KeyboardUsage::Keypad1, esp32keybridge::KeyboardUsage::Keypad2, esp32keybridge::KeyboardUsage::Keypad3,
  esp32keybridge::KeyboardUsage::Keypad0, esp32keybridge::Key(),                  esp32keybridge::KeyboardUsage::KeypadEnter,
});
```

No coordinates and no indices appear. Gaps are an invalid key, `Key()`. Configure all of this **before the first `update()`**.

Scanning, debouncing, and ghost blocking run in a **dedicated 1 kHz task**, so short taps survive a blocked loop.

**Diodes are optional.** Without them, ghost blocking kicks in: a new press that would complete a 2x2 rectangle is held back. All four corners of such a rectangle read identically, so the real key cannot be told from its phantom, and **holding it back beats emitting the wrong key**. The consequence is that **without diodes, only 2 simultaneous keys are guaranteed**. `setHasDiodes(true)` removes the restriction.

#### `RotaryEncoderInputAdapter` — a rotary encoder

Full **x4 quadrature decoding** (both phases, both edges) through the PCNT peripheral. No interrupts, and the counter advances in hardware, so rotation is never lost regardless of how often `update()` runs.

```cpp
encoder.mapToKeys(esp32keybridge::ConsumerUsage::VolumeIncrement,   // clockwise
                  esp32keybridge::ConsumerUsage::VolumeDecrement);  // counter-clockwise
// or
encoder.mapToAxis(esp32keybridge::Axis::Wheel);   // one detent = +/-1
encoder.setPulsesPerDetent(4);  // default 4 (EC11 and similar clicked encoders)
```

Because it is x4, the raw count is four per quadrature cycle; `pulsesPerDetent` divides it down to detents. **The remainder carries over, so there is no drift.**

- Two counts per detent -> `setPulsesPerDetent(2)`
- **Turning the wrong way** -> swap the arguments to `mapToKeys()`, or for `mapToAxis()` use `config.setAxisScale(axis, -1)`
- **One adapter = one encoder = one PCNT unit** (the ESP32-S3 has 4). On chips without `SOC_PCNT_SUPPORTED` the adapter is not compiled
- The push switch is separate: register it as a `GpioKeyInputAdapter` key

### 5.7 Writing your own input adapter

An input adapter is **`update()` and `keys()`**, nothing more. An I2C expander, a sensor, or a network input takes about a page.

```cpp
class MyInput : public esp32keybridge::InputAdapter {
public:
  void update() override { /* read the hardware, update keys_ */ }
  const esp32keybridge::KeySet &keys() const override { return keys_; }
  // optional: connected() / setLockState() / takeAxisDelta()
private:
  esp32keybridge::KeySet keys_;
};
```

What `keys()` returns is a set of **logical presses**; a one-to-one correspondence with physical switches is not required. Toggles (each stomp alternately presses and releases Shift), one-shots, and encoder taps are all synthesized by the adapter.

When you detect a disconnect, return `false` from `connected()`. **Clearing the pressed set is the core's job.**

The worked example is [TouchButtons](../examples/practical/TouchButtons/) (the ESP32-S3's built-in capacitive touch); the minimal reference is `ManualInputAdapter`.

### 5.8 Input adapters at a glance

| Adapter | Requires | Devices | Loses input | Specific limits |
|---|---|---|---|---|
| `EspUsbHostKeyboardInputAdapter` | EspUsbHost | 4 | no | 8 simultaneous media keys; cannot send the kana LED |
| `EspUsbHostMouseInputAdapter` | EspUsbHost | 4 | no | presence from the first report |
| `EspUsbHostGamepadInputAdapter` | EspUsbHost | 4 | no | no analog axes |
| `EspUsbHostMidiInputAdapter` | EspUsbHost | 4 | no | Note On/Off only |
| `EspBleHidHostKeyboardInputAdapter` | EspBle | 4 links | no* | one media usage per link; encryption effectively required |
| `EspBleHidHostMouseInputAdapter` | EspBle | 4 links | no* | no horizontal wheel |
| `EspBleHidHostGamepadInputAdapter` | EspBle | 4 links | no* | no analog axes |
| `EspBleClassicHidHostKeyboardInputAdapter` | EspBle 1.3.0 | 1 link | no** | original ESP32 only; no decoded consumer input |
| `EspBleClassicHidHostMouseInputAdapter` | EspBle 1.3.0 | 1 link | no** | original ESP32 only; no horizontal wheel |
| `GpioKeyInputAdapter` | none | pin count | ⚠ **yes** | for long presses only |
| `GpioMatrixInputAdapter` | none | 16x16 | no | 2 simultaneous keys without diodes |
| `RotaryEncoderInputAdapter` | PCNT | 1 per adapter | no | up to the chip's PCNT unit count |

\* as long as `ble.update()` is being called.

\** as long as `bluetooth.update()` is being called.

---

## 6. Output adapters

### 6.1 USB Device HID output

Header: `ESP32KeyBridgeEspUsbDevice.h` / requires [EspUsbDevice](https://github.com/tanakamasayuki/EspUsbDevice) 2.0.0 or newer

The PC always sees a **composite HID device: keyboard + consumer + mouse**.

| Report | Format |
|---|---|
| keyboard | boot 6KRO (one modifier byte plus 6 keys) |
| consumer | one 16-bit usage |
| mouse | 8 buttons + X / Y / Wheel (int8 saturation with carry; **Pan is unsupported and dropped**) |

**There is no keyboard-only variant.** An interface that never sends a report is harmless on the USB side. The keyboard interface speaks the boot protocol, so BIOS and UEFI work.

Two constraints:

- **The interface composition is fixed at `usbDevice.begin()`** and cannot change afterwards. So **construct the output adapter before `usbDevice.begin()`** — it registers its interfaces at construction. Adding a mouse or media keys to the configuration later causes no re-enumeration, because the descriptor already covers them
- **Only one media key can be sent at a time** (the consumer report holds a single 16-bit usage)

Each report is sent only when it changes, and a report that could not be sent because the endpoint was busy is retried on the next `update()`. VID / PID / product name go in `EspUsbDeviceConfig` (a stack-side setting).

**It receives LED output reports, so this output can be the lock authority** (chapter 4).

### 6.2 BLE HID output

Header: `ESP32KeyBridgeEspBle.h` / requires EspBle 1.2.0 or newer

Same idea as the USB Device output: keyboard + consumer + mouse in one HID service. **The report contents are identical to the USB Device output**, so swapping outputs — or registering both at once — does not change how keys look.

Only two things differ:

- The HID service composition is fixed at **`ble.begin()`**, so construct the output adapter before that call (the counterpart of `usbDevice.begin()`)
- **`connected()` means something else.** Where USB means "mounted", BLE means "connected, encrypted, and input reports subscribed". HOGP forbids notifications until the host subscribes, so **it is briefly `false` right after pairing**

Advertising is the sketch's business. To resume it after a disconnect, say so:

```cpp
ble.advertising().setName(bleConfig.deviceName);
ble.advertising().start();
ble.onDisconnected([](const EspBleConnection &) { ble.advertising().start(); });
```

This output also receives LED output reports, so it too can be the lock authority (kana included).

#### 6.2.1 Bluetooth Classic HID output

Header: `ESP32KeyBridgeEspBleClassic.h` / requires EspBle 1.3.0+ / **original ESP32 only**

`EspBleClassicHidOutputAdapter` sends the same keyboard 6KRO + consumer + mouse reports as the BLE adapter, but as a Bluetooth Classic HID Device. The sketch owns `EspBleClassic`, and the adapter must be constructed before `bluetooth.begin()`.

The Classic-specific limit is its SDP record: the Report Descriptor and name/description/provider strings in `EspBleClassicHidProfileConfig` share a **214-byte** budget. All three profiles fit with default strings; long custom strings can make `bluetooth.begin()` fail.

Classic accepts one HID Host and has no BLE advertising or GATT subscription. Pair from the Host's Bluetooth settings. Prefer BLE when the peer supports it because BLE covers more SoCs and uses less power.

### 6.3 Serial output

Header: `ESP32KeyBridgeSerial.h` / no external library (just Arduino's `Print`)

#### `TextOutputAdapter` — write characters out

An output that emits **characters rather than HID reports**. It consumes only the text stream (`writeText()`) and writes UTF-8 to a `Print` (Serial, Serial1, SoftwareSerial, ...).

Characters come from two sources:

1. `typeText()` / `typeChar()` / macros
2. **Presses on an input configured with `convertLayout()`** — decoded through the engraving. Enter becomes a newline and Tab a tab, added by the core

The second is what makes `BarcodeToText` work. A USB HID barcode reader is a machine that types characters, so decoding its keystrokes with the engraving layout recovers the string.

**This output carries no layout of its own.** Which character you get is decided in exactly one place: the input's `convertLayout()`.

#### `LogOutputAdapter` — a diagnostic monitor

Writes human-readable lines whenever the pressed set changes (`KEYS ...`), a character is typed (`TEXT ...`), or an axis moves (`AXIS ...`). Add it **alongside** a real output to watch what is being produced, or use it on its own to bring up input hardware. **Never acts as the lock authority.**

Finding a gamepad's button numbers, checking matrix wiring, and confirming an encoder's direction all start here.

### 6.4 Output adapters at a glance

| Adapter | Keys | Media keys | Mouse | Text | Lock authority |
|---|---|---|---|---|---|
| `EspUsbDeviceHidOutputAdapter` | 6KRO | 1 usage | 8 buttons + X/Y/Wheel | expanded to keystrokes | yes |
| `EspBleHidOutputAdapter` | 6KRO | 1 usage | 8 buttons + X/Y/Wheel | expanded to keystrokes | yes |
| `EspBleClassicHidOutputAdapter` | 6KRO | 1 usage | 8 buttons + X/Y/Wheel | expanded to keystrokes | yes |
| `TextOutputAdapter` | — | — | — | yes, as characters | — |
| `LogOutputAdapter` | displayed | displayed | displayed | displayed | — |

Outputs are **state-snapshot based**, so a late `update()` only means the last state is sent — nothing breaks. Typing and macros simply key at a rate proportional to the `update()` interval.

---

## 7. Choosing a chip and a port

What you can bridge is decided by **which adapters run on that chip**.

| | ESP32 | S2 | S3 | C3 / C6 / H2 | P4 |
|---|---|---|---|---|---|
| GPIO keys / matrix | yes | yes | yes | yes | yes |
| Rotary encoder (PCNT) | yes | yes | yes | yes | yes |
| USB Host input | — | yes | yes | — | yes |
| USB Device output | — | yes | yes | — | yes |
| BLE HID in/out | yes | — | yes | yes | via ESP-Hosted |
| Bluetooth Classic HID in/out | yes | — | — | — | — |

The thing that trips people up here is **how many USB roles you need**.

- **USB to USB** (USB keyboard in, USB HID out) uses **two USB roles at once**, so it **needs an ESP32-P4**
- **With BLE on one side, USB is only one role**, so a single-USB chip like the **ESP32-S3** suffices. That covers `UsbKeyboardToBle` (make a wired keyboard wireless) and `BleKeyboardToUsb` (use a Bluetooth keyboard on a BIOS screen)

There is a power consideration on the USB Host side too: the board must be able to **supply bus power to the keyboard** — one with a USB-A host connector, or a USB-OTG cable plus external 5 V.

The details of the P4's USB ports are in the [SwapCtrlCapsLock README](../examples/practical/SwapCtrlCapsLock/), which is the reference for that topic.

Dependencies grow **only with the adapters you include**. The core header (`ESP32KeyBridge.h`) has none.

| Header | Requires |
|---|---|
| `ESP32KeyBridge.h` | nothing |
| `ESP32KeyBridgeEspUsbHost.h` | EspUsbHost 2.6.0+ |
| `ESP32KeyBridgeEspUsbDevice.h` | EspUsbDevice 2.0.0+ |
| `ESP32KeyBridgeEspBle.h` | EspBle 1.2.0+ |
| `ESP32KeyBridgeEspBleClassic.h` | EspBle 1.3.0+ (original ESP32 and arduino-esp32 3.2.0+) |
| `ESP32KeyBridgeGpio.h` | nothing |
| `ESP32KeyBridgeSerial.h` | nothing |

---

## 8. When it does not work

| Symptom | What to suspect |
|---|---|
| **Nothing happens at all** | Is `bridge.update()` being called from `loop()`? |
| **No BLE input arrives** | Is `ble.update()` called in the same context as `bridge.update()`, and **first**? (5.2) |
| **BLE output sends nothing** | `connected()` means connected + encrypted + subscribed; it is briefly false right after pairing (6.2) |
| **A BLE keyboard sends no reports** | `security.enabled = true`, and `hidHost().discover()` **after pairing succeeds** (5.5) |
| **The USB output is missing an interface** | Is the output adapter constructed **before** `usbDevice.begin()` / `ble.begin()`? (6.1, 6.2) |
| **Short button presses are lost** | `GpioKeyInputAdapter` is loop-rate dependent; move to `GpioMatrixInputAdapter` (5.3) |
| **The third key in a matrix does not register** | Ghost blocking without diodes. `setHasDiodes(true)`, or add diodes (5.6) |
| **The encoder runs backwards or too far** | Swap the `mapToKeys()` arguments, `setAxisScale(axis, -1)`, or `setPulsesPerDetent()` (5.6) |
| **Gamepad buttons do nothing** | Only mapped controls are emitted; find the numbers with `LogOutputAdapter` (5.4) |
| **Only the symbols come out wrong** | The engraving (`convertLayout`) and `hostLayout` pair (3.4) |
| **Symbols are garbled in the BIOS** | Layout conversion is still on; bypass it with `layoutConversionToggle` (3.4) |
| **Part of a macro does not type** | Characters absent from that host layout. Check `textEncodeFailCount()` (3.3) |
| **A long string gets truncated** | Are you resuming from `typeText()`'s return value? Check `textOverflowCount()` (3.3) |
| **Only one media key works at a time** | The consumer report carries a single 16-bit usage. Working as specified (6.1) |
| **The Caps Lock LED stays dark** | Is a lock-reporting output connected? Without one the bridge toggles as the terminal host (chapter 4) |
| **A key is stuck** | Structurally impossible through the bridge. Check `mergedOverflow()` / `outputOverflow()` — dropped keys never reach the output, so they cannot stick either |

There are counters for all of this. Print them first.

```cpp
bridge.textOverflowCount();      // characters dropped because the queue was full
bridge.textEncodeFailCount();    // characters the host layout could not type
bridge.layoutConvertFailCount(); // presses that could not be converted
bridge.mergedOverflow();         // the merged pressed set overflowed
bridge.outputOverflow();         // the transformed pressed set overflowed
bridge.lockAuthorityPresent();   // whether an external lock authority exists
```

---

## 9. What is impossible in principle

Things the specification cannot solve. Mitigations are noted where they exist.

| # | Item | Note |
|---|---|---|
| 1 | **Some characters cannot be typed** | A character absent from the target layout cannot be generated over HID (`¥` to an en_us host) |
| 2 | **Shortcut semantics cannot be told apart** | Whether an app binds a shortcut by character or by position is not observable. Layout conversion breaks some cases whichever way it leans |
| 3 | **The host has exactly one modifier state** | Simultaneously holding keys with conflicting Shift requirements (`@` and `"`) cannot be represented |
| 4 | **Parts of the host's state are invisible** | IME on/off and the focused application cannot be observed. Text features assume the IME is off |
| 5 | **A physical Fn key cannot be handled** | It is processed inside the keyboard and never reaches HID. The substitute is the layer in 3.2 |

Separately, these are implementation gaps that could be filled later:

- Gamepad **analog sticks and triggers** (buttons and hat only)
- **Tap/hold** (time-dependent; out of scope for v1)
- **NKRO on the USB Device output** (the report builder is in the core, but the v1 output is boot 6KRO)
- **Horizontal wheel (Pan) on outputs** (USB Host input can receive it)

---

## 10. What to read next

| I want to... | Read |
|---|---|
| Find a sketch for my hardware | [examples/README.md](../examples/README.md) — every one is flashable as-is |
| See per-adapter detail and hardware verification status | [ADAPTERS.md](ADAPTERS.md) |
| Understand why the key model looks like this | [DATA_MODEL.md](DATA_MODEL.md) |
| Understand the core/adapter boundary and concurrency | [CORE_DESIGN.md](CORE_DESIGN.md) |
| Write my own adapter | The `InputAdapter` / `OutputAdapter` comments in `src/ESP32KeyBridge.h` -> [TouchButtons](../examples/practical/TouchButtons/) -> [CORE_DESIGN.md](CORE_DESIGN.md) |
| Know why something was designed this way | [DECISIONS.ja.md](DECISIONS.ja.md) (Japanese) |
| See the guide to all documents | [README.md](README.md) |
