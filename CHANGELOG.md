# Changelog / 変更履歴

## Unreleased

## 1.0.1
- (EN) Add Bluetooth Classic HID adapters through EspBle 1.3.0 on the original ESP32: composite keyboard/consumer/mouse output and decoded keyboard/mouse Host input. Add `ButtonsToClassic`, `ClassicKeyboardToBle`, one-board smoke tests, and two-board peer tests. Document one-link/address-based Host operation, keyboard/mouse-only Host decoding, the 214-byte SDP budget, original-ESP32-only support, and BLE/Classic dual-host limits.
- (JA) EspBle 1.3.0を使う無印ESP32向けBluetooth Classic HIDアダプタを追加。複合keyboard/consumer/mouse出力と、復号済みkeyboard/mouse Host入力を提供する。`ButtonsToClassic`、`ClassicKeyboardToBle`、1台smoke test、2台peer testを追加。Hostは1接続・address指定、Host復号はkeyboard/mouseのみ、SDPは214 byte、無印ESP32限定、BLE/Classic dual-host制限を文書化。

## 1.0.0
- (EN) Initial release of ESP32KeyBridge: a portable key model and bridge pipeline with remapping, layers, text macros, layout conversion, lock-state forwarding, relative axes, and USB/BLE/GPIO/serial adapters. Includes practical examples, host unit tests, compile checks, and two-board USB/BLE peer tests.
- (JA) ESP32KeyBridge初回リリース。移植可能なキーモデルとbridge pipelineに、remap、layer、text macro、layout変換、lock状態転送、相対軸、USB/BLE/GPIO/serialアダプタを収録。実用example、host unit test、build確認、USB/BLEの2台peer testを含む。
