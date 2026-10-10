# Changelog / 変更履歴

## Unreleased
- (EN) Add authenticated local-web monitoring and control for all three remembered BLE HID host profiles, including persistent labels, full addresses, directed reconnect, pair/replace with a two-minute timeout, cancel, and delete. Fix `Scroll Lock+1/2/3` so empty slots remain distinctly selected without implicitly pairing.
- (JA) 記憶済み BLE HID host 3プロファイルの認証付きWeb監視・操作を追加。永続ラベル、完全なaddress、指定再接続、2分timeout付きpair/replace、cancel、deleteに対応。`Scroll Lock+1/2/3` は空slotも自動pairせず個別に選択するよう修正。
- (EN) Add four built-in Dvorak layouts (standard, I/U-swapped, and QWERTY-position shortcut variants) and advertise BLE target slots with `1`-`3` appended to the device name.
- (JA) Dvorak 4 variant（標準、I/U 入替、QWERTY 位置 shortcut 版）を内蔵し、BLE 接続先 slot ごとの device name に `1`〜`3` を付加。
- (EN) Add Phase 7 opt-in HTTP Digest authentication for the ESP32-S3 settings page, health check, and all registered API routes, with strict paired credential validation and development login examples.
- (JA) Phase 7 の opt-in HTTP Digest authentication を追加。ESP32-S3 設定 page、health check、全登録 API route を保護し、credential pair の厳密な検証と開発用 login example を実装。
- (EN) Add Phase 6 opt-in captive-portal discovery: wildcard local DNS, fixed-AP-address redirects for unknown non-API GET probes, API-safe 404 behavior, and automatic discovery enabled in both ESP32-S3 settings examples.
- (JA) Phase 6 の opt-in captive portal discovery を追加。local wildcard DNS、未知の非 API GET probe から固定 AP address への redirect、API の安全な 404 動作を実装し、両 ESP32-S3 設定 example で自動検出を有効化。
- (EN) Add Phase 5 macro configuration: four F1-F24 text-macro slots, strict UTF-8 and versioned JSON validation, independent redundant NVS persistence with read-back verification, atomic live full-table replacement, and responsive browser editing.
- (JA) Phase 5 の macro 設定を追加。F1-F24 を trigger にする4個の text macro slot、厳密な UTF-8 / version付き JSON 検証、独立した二重化 NVS と読戻し確認、live macro table の atomic な全置換、responsive browser 編集を実装。
- (EN) Add Phase 4 wired-keyboard navigation for the ESP32-S3 settings page. A bounded input decorator sends Tab, arrow, activation, cancel, Home, and End actions to the browser while suppressing captured keys from the BLE host; a short polling lease releases capture safely when the page disappears.
- (JA) Phase 4 として ESP32-S3 設定 page の有線 keyboard 操作を追加。bounded input decorator が Tab、矢印、実行、cancel、Home、End action を browser へ送り、capture key は BLE host へ流さない。短い polling lease により page 消失時も安全に capture を解除する。
- (EN) Add Phase 3 layout editing: enumerate bundled layouts, strictly decode the complete versioned JSON document, expose one configuration `PUT`, verify redundant NVS persistence before live apply, and provide responsive browser controls with explicit save/error state.
- (JA) Phase 3 の layout 編集を追加。bundled layout 列挙、version 付き完全 JSON document の厳密 decode、単一の設定 `PUT`、live apply 前の二重化 NVS 読戻し確認、保存・error 状態を明示する browser UI を実装。
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
