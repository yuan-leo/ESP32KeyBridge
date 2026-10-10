# UsbKeyboardToBle

[日本語](README.ja.md)

Make a wired USB keyboard wireless. The keyboard plugs into the ESP32, and the PC / tablet / phone sees a **Bluetooth keyboard**. Keep the keyboard you like on a machine that has no free USB port, or on a tablet that has none at all.

- The keyboard is read through the USB Host input adapter, and the result goes out through the BLE HID output adapter (`EspBleHidOutputAdapter`) as a HID over GATT keyboard + consumer + mouse.
- Everything the bridge can do on the way through still applies: remap, disable, momentary layers, text macros, layout conversion. This sketch adds a hold-CapsLock navigation layer, because a wireless keyboard usually ends up somewhere without a mouse.
- Lock state comes from the Bluetooth host: its LED output report is the authority, and the bridge forwards it back to the USB keyboard's LEDs. Press Caps Lock on the keyboard and the LED follows the host, not a guess.
- Hold **Scroll Lock** and press top-row **1**, **2**, or **3** to switch to that bonded host slot. Press **Scroll Lock + 0** to replace the selected slot and enter pairing mode. The Scroll Lock LED flashes while switching or pairing, then the bridge restores the host's real Scroll Lock state.
- Each slot advertises its own suffix: `KeyBridge Keyboard 1`, `KeyBridge Keyboard 2`, or `KeyBridge Keyboard 3`. A host may retain an older cached name until the device is removed and paired again.

## Local layout settings

The ESP32-S3 also hosts the layout-settings page. Connect a phone to the
intentionally open `KeyBridge-Setup` network. The phone should offer or open
the local page through captive-portal discovery; if it does not, open
`http://192.168.4.1/`. There is no Wi-Fi password or website login. Anyone
connected can change settings. Saving verifies the redundant NVS record before
the new input/host layouts, conversion state, and toggle key are applied.

The same page configures four persistent text macros. Select a unique F1-F24
trigger and enter up to 31 characters using the phone. Pressing that function
key on the wired keyboard consumes the key and types the saved text using the
configured host layout.

After releasing any keys already held, the attached wired keyboard controls the
open page over the local Wi-Fi connection: use **Tab / Shift+Tab** or the arrow keys to move,
**Enter / Space** to activate, and **Esc** to reload the saved values. Keyboard
keys are withheld from the BLE host during this capture. If the page closes or
stops polling, capture expires automatically after about 1.2 seconds; keys
that were held at expiry remain suppressed until released.

## Only one USB port is needed

The PC side is Bluetooth, not USB, so only the keyboard side uses USB. That means a single-USB chip is enough — an **ESP32-S3**, not the ESP32-P4 the USB-to-USB examples (`SwapCtrlCapsLock`, `NavLayer`, ...) require.

## Hardware setup (ESP32-S3)

- Plug the keyboard into the ESP32-S3's native USB port (read as USB Host). The board has to supply bus power to the keyboard: use a board with a USB-A host connector, or a USB-OTG cable plus external 5V.
- Power the board from its UART port or a battery. The serial console on UART0 shows the BLE state.
- Select the **Minimal SPIFFS (1.9MB APP with OTA)** partition scheme when building manually. The example's `sketch.yaml` selects it automatically; the combined USB Host, BLE, and Wi-Fi firmware does not fit the default 1.25MB application partition.
- On HW-678 v0.0.0, the programmable **WS2812 on GPIO 48** is the status light. The separate **PWR, TX, and RX** LEDs are not controlled by the firmware.

## HW-678 RGB status legend

- Profile 1: blue; Profile 2: green; Profile 3: magenta.
- Solid profile color: connected.
- Slowly breathing profile color: searching or reconnecting.
- Alternating profile color and white: pairing.
- One brief profile-color flash every three seconds: intentionally disconnected.
- Fast amber pulses: bond deletion or factory reset.
- Three quick red flashes followed by a pause: error (triple red).
- Boot self-test: white, red, green, then blue. Off means RGB or Bluetooth is unavailable.

## Pairing

The initially selected slot advertises as `KeyBridge Keyboard 1`. Press **Scroll Lock + 0**, then pair it from the host's Bluetooth settings. Slots 2 and 3 use the corresponding suffix after selection. Bonding keeps that host in the selected slot. Up to three slots are available (the bond limit of EspBle's bundled NimBLE build), and a normal disconnect reconnects only to the selected slot through directed advertising. The website shows the editable label, MAC address, and best-effort host-reported name (`Not reported by host` when unavailable). Use **Stop / Force disconnect** to recover a stuck operation. **Bluetooth only** clears bonds/profiles; **Reset everything** also clears layouts and macros.

## ble.update() and bridge.update()

EspBle delivers its callbacks from the context that calls `ble.update()`, so it belongs in the same `loop()` as `bridge.update()` — and before it, so a report that just arrived is part of this cycle:

```cpp
void loop() { ble.update(); bridge.update(); settingsWeb.update(); delay(1); }
```
