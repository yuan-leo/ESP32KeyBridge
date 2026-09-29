# Changelog / 変更履歴

## Unreleased
- (EN) Add the ESP32-S3 local-configuration Phase 2 network foundation: a WPA2 SoftAP, offline read-only settings page, configuration/status JSON endpoints, security headers, and the `SettingsPortal` bring-up example.
- (JA) ESP32-S3 local configuration の Phase 2 として、WPA2 SoftAP、offline read-only 設定page、設定/status JSON endpoint、security header、bring-up 用 `SettingsPortal` example を追加。
- (EN) Start the ESP32-S3 local-configuration milestone with validated, versioned keyboard-layout settings and a redundant Preferences/NVS store. Alternating CRC-protected records preserve the previous valid settings across interrupted writes or corruption, with safe defaults when neither record is usable.
- (JA) ESP32-S3 の local configuration milestone 第1段階として、検証・version管理された keyboard layout 設定と二重化した Preferences/NVS 保存を追加。CRC 付きrecordを交互に保存し、書込み中断・破損時は直前の有効設定へ復旧し、どちらも使えない場合は安全なdefaultへ戻す。
- (EN) Add bonded BLE target switching: `BluetoothHotkeyInputAdapter` consumes Scroll Lock + top-row 1..3 to select an EspBle bond slot and Scroll Lock + 0 to replace the selected slot in pairing mode. `EspBleHidTargetController` uses directed advertising for reconnects, and best-effort Scroll Lock LED patterns preserve and restore the effective lock state. `UsbKeyboardToBle` demonstrates the complete flow.
- (JA) bond 済み BLE 接続先切替を追加。`BluetoothHotkeyInputAdapter` が Scroll Lock + 上段 1..3 で EspBle bond slot を選択し、Scroll Lock + 0 で選択中 slot を pairing mode から入れ替える。`EspBleHidTargetController` は再接続に directed advertising を使い、Scroll Lock LED の best-effort 表示後に本来の lock 状態を復元する。`UsbKeyboardToBle` に一連の使い方を反映。

## 1.0.1
- (EN) Add Bluetooth Classic HID adapters through EspBle 1.3.0 on the original ESP32: composite keyboard/consumer/mouse output and decoded keyboard/mouse Host input. Add `ButtonsToClassic`, `ClassicKeyboardToBle`, one-board smoke tests, and two-board peer tests. Document one-link/address-based Host operation, keyboard/mouse-only Host decoding, the 214-byte SDP budget, original-ESP32-only support, and BLE/Classic dual-host limits.
- (JA) EspBle 1.3.0を使う無印ESP32向けBluetooth Classic HIDアダプタを追加。複合keyboard/consumer/mouse出力と、復号済みkeyboard/mouse Host入力を提供する。`ButtonsToClassic`、`ClassicKeyboardToBle`、1台smoke test、2台peer testを追加。Hostは1接続・address指定、Host復号はkeyboard/mouseのみ、SDPは214 byte、無印ESP32限定、BLE/Classic dual-host制限を文書化。

## 1.0.0
- (EN) Initial release of ESP32KeyBridge: a portable key model and bridge pipeline with remapping, layers, text macros, layout conversion, lock-state forwarding, relative axes, and USB/BLE/GPIO/serial adapters. Includes practical examples, host unit tests, compile checks, and two-board USB/BLE peer tests.
- (JA) ESP32KeyBridge初回リリース。移植可能なキーモデルとbridge pipelineに、remap、layer、text macro、layout変換、lock状態転送、相対軸、USB/BLE/GPIO/serialアダプタを収録。実用example、host unit test、build確認、USB/BLEの2台peer testを含む。
