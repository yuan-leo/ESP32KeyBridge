# BarcodeToText

[日本語](README.ja.md)

For when you want to turn a USB HID barcode reader (the kind that "types" barcodes as key input) into a **text** stream on the serial port. Read it in the PC's serial monitor, pass it to another device, and so on. No keyboard focus or foreground app required.

- The barcode reader is read through the USB Host input adapter (a HID scanner in keyboard mode works as-is).
- **The layout is a setting on the input side.** Give the scanner input `convertLayout(en_us)` (the scanner's engraving) and the bridge decodes each press by its engraving into a character, which then flows into the character stream. `TextOutputAdapter` just writes those characters to a `Print` (here, `Serial`); it holds no layout on the output side.
- Shift is honored too, and the Enter that terminates a scan comes out as a newline (Tab as a tab).
- For a scanner set to JIS, make the input side `convertLayout(esp32keybridge::KeyboardLayout::jaJp())`.

## Advanced: send to another device over UART / stream typed characters too

You can change the destination just by swapping the `Print`. To send to another MCU or terminal instead of the PC, pass `Serial1` (a hardware UART):

```cpp
esp32keybridge::TextOutputAdapter text(Serial1);
```

Characters injected with `typeText()` flow to the same output too (injected text and the characters derived from `convertLayout` input presses flow as the same character stream).

## Hardware setup (ESP32-S3)

- Plug the barcode reader into the ESP32-S3's USB OTG port (it is read as USB Host).
- The decoded characters come out on `Serial` (the board's USB-serial = the PC-side serial port).
