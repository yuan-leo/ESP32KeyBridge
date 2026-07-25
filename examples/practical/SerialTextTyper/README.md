# SerialTextTyper

[日本語](README.ja.md)

For when you want to type text received over serial into the PC as keystrokes. Useful for test automation, initial setup of kiosks, and feeding text into "a machine you can only touch as a keyboard".

- There is no input adapter. A character stream (`typeText()` / `typeChar()`) is a first-class input to the bridge.
- Characters are **typed the moment they arrive** (no line buffering). Only for multi-byte UTF-8 characters does it wait until the continuation bytes arrive. A newline (CR / LF / CRLF) becomes a single Enter via the control-character table.
- Characters are expanded into keystrokes using `config.hostLayout` (en_us by default). If the layout setting on the PC differs, match it:

```cpp
esp32keybridge::ESP32KeyBridgeConfig config;
config.hostLayout = esp32keybridge::KeyboardLayout::jaJp();
bridge.applyConfig(config);
```
- `typeText()` accepts UTF-8. What can be turned into keystrokes are "printable characters present in the host layout table" (en_us = ASCII printable characters / ja_jp = ASCII printable characters + `¥`) and 5 kinds of control character (TAB / LF / CR / BS / ESC); anything else (kana, kanji, emoji, etc.) is discarded and counted (`textEncodeFailCount()`). For details, see "Character streams" in [DATA_MODEL.ja.md](../../../docs/DATA_MODEL.ja.md).
- Keystrokes advance one phase per update. The loop period is exactly the typing pace (about 1ms/phase in this example).
- Because reception is much faster than typing (115200bps ≒ 10,000 chars/sec vs about 333 chars/sec), it is regulated by **backpressure**: it reads from Serial only while there is room in the text queue (`bridge.typeAvailable()`), and while it is full the bytes stay in the UART RX buffer. No timeout, no blocking, and no characters are lost.

## Hardware setup (ESP32-S3)

- Plug the ESP32-S3's USB OTG port into the PC (it appears as a composite HID device including a keyboard).
- Text is received over UART serial (pins, or the board's USB-to-serial converter chip). Since the OTG port is used for HID, serial is on the UART side.
