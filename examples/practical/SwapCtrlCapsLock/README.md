# SwapCtrlCapsLock

[日本語](README.ja.md)

For when you want to sit between a keyboard and a PC and swap CapsLock and Ctrl, without changing any setting on either the PC or the keyboard.

- A remap is a single-stage lookup and does not chain, so registering two entries is exactly a "swap". Long presses, chords, and host-side auto-repeat are all preserved.
- Even if you toggle Caps using the Ctrl position after the swap, the keyboard's LED follows correctly, because the LED report from the PC is forwarded through.

## Hardware setup (ESP32-P4)

This uses two USB buses (Host + Device), so an ESP32-P4 is required. The P4 has three USB ports:

| Port | Pins | Role in this example |
|---|---|---|
| HS (High-Speed) | - | **USB Device (plug into the PC)**. Because of a library constraint in arduino-esp32 3.3.10, Device is fixed to HS |
| FS CDC | GPIO24/25 | On some boards this is used for USB Host (see the swap below) |
| FS OTG | GPIO26/27 | The default for **USB Host (plug the keyboard in here)** |

In addition, some boards have a port going through an external USB-to-serial converter chip. Which port is wired to which connector differs from board to board, so check first.

### Swapping the FS ports

USB Host explicitly selects the FS side (`ESP_USB_HOST_PORT_FULL_SPEED`). On boards where the connector you plug the keyboard into is wired to the FS CDC pins (GPIO24/25), swap the CDC and OTG PHYs **before USB initialization**. Uncomment the following line in the sketch:

```cpp
usb_wrap_ll_phy_select(&USB_WRAP, 0);
```

Note that **a board's labeling does not always match its actual wiring**. Do not judge by the label alone; check which pins the connector is actually wired to (OTG = GPIO26/27 / CDC = GPIO24/25).

### Example: M5Stack Tab5

- Type-A = HS = **USB Device**. This is the side you connect to the PC (it looks like it should be for a host, but it is the opposite).
- Type-C = **labeled OTG but actually FS CDC** = USB Host. Connect the keyboard here, and the PHY swap above is required. This is a case where the label and the actual wiring differ.

### Constraints when using the HS side for USB Host (for reference)

The change that makes USB Device usable over FS is a pull request submitted to arduino-esp32 and is not merged as of 3.3.10 (which is why this example fixes Device = HS). Once it is merged, a configuration with Host on the HS side will also be possible, but in that case the following constraints apply:

- **A Host on the HS port does not support a mix of HS / FS devices.** Even if you connect an FS device (such as a typical USB keyboard) behind an HS-capable USB hub, it will not work.
- Therefore, when using the HS side for Host, **connect a single device directly, without a hub**.
