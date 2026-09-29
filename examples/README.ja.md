# Examples

> 正式サポート target は **ESP32-S3 のみ**です。他の ESP32 variant を指定する example は履歴として残しますが、support および scope の対象外です。

[English](README.md)

examples はすべて**実用例**です。実ハードウェア構成でそのまま書き込んで使うスケッチで、中身は「1) ハードウェア起動 → 2) ブリッジ配線 → 3) 設定を組んで適用」の 3 段構成に統一しています。

## Practical Examples

- `BarcodeToText`: USB HID バーコードリーダーをシリアルの文字列に変換したいとき。ESP32-S3 + USB Host。
- `BleKeyboardToUsb`: Bluetooth キーボード(とマウス)を、ペアリングできない機器 — BIOS、KVM、ゲーム機 — で使いたいとき。ESP32-S3 + BLE HID Host/USB Device。
- `ButtonsToClassic`: BLE HID 非対応の旧機器を GPIO ボタンから操作したいとき。無印 ESP32 + Bluetooth Classic HID Device。
- `ClassicKeyboardToBle`: Classic HID 専用の古いキーボードを BLE HID Host から使いたいとき。無印 ESP32 の Classic/BLE dual-host。
- `FootSwitch`: フットスイッチや自作ボタンで定型文入力・メディア操作・ページ送りをしたいとき。ESP32-S3 + GPIO。
- `GamepadToKeys`: USB ゲームパッドで PC をキーボード操作(十字=矢印、ボタン=Space/Enter など)したいとき。ESP32-P4 + USB Host/Device。
- `KeyMonitor`: 入力ハードのブリングアップ・監視用に、ブリッジ出力をシリアルに表示したいとき。ESP32-S3 + GPIO。
- `LayerKeypad`: 自作キーパッドで「1 キー 2 機能」(Fn 長押しでナビ→メディアに切替)をやりたいとき。ESP32-S3 + GPIO。
- `MediaKeys`: 使っていないキー(F13 など)を音量・再生/停止キーにしたいとき。ESP32-P4。
- `MergeKeyboards`: 複数の入力(USB キーボード + GPIO 追加ボタンなど)を 1 つのキーボードとして PC に見せたいとき。ESP32-P4 + USB Host/Device + GPIO。
- `MidiToKeys`: USB MIDI コントローラのパッド/鍵盤をキーボードショートカットにしたいとき。ESP32-P4 + USB Host/Device。
- `NavLayer`: どんな USB キーボードにも「CapsLock 長押しで HJKL が矢印」のナビ(Fn)レイヤーを足したいとき。ESP32-P4 + USB Host/Device。
- `NumPad`: 自作テンキー・マクロパッド・自作キーボードを作りたいとき(スイッチマトリクスを GPIO 直結)。ESP32-S3 + GPIO。
- `NaturalScroll`: マウスのホイール方向を PC の設定に手を入れずに反転したいとき(左利き用のボタン入れ替えも)。ESP32-P4。
- `ScrollDial`: 手元に物理スクロールダイヤル(回すとホイール、押すと中クリック。ジョグにも)が欲しいとき。ESP32-S3 + GPIO(エンコーダ)。
- `SettingsPortal`: phone を ESP32-S3 へ直接 Wi-Fi 接続し、保存済み layout 設定を self-hosted offline page で確認したいとき。ESP32-S3 + Wi-Fi SoftAP。
- `SerialTextTyper`: シリアルで送った文字列を PC への打鍵にしたいとき(テスト自動化など)。ESP32-S3。
- `SwapCtrlCapsLock`: キーボードと PC の間に挟んで、両方の設定を変えずに CapsLock と Ctrl を入れ替えたいとき。ESP32-P4。**P4 の USB ポート事情の説明はここが基準です。**
- `TouchButtons`: 自作インプットアダプタの書き方を知りたいとき(題材 = ESP32-S3 内蔵の静電容量タッチをボタンに)。ESP32-S3。
- `UsKeyboardOnJapanesePc`: US 配列キーボードやバーコードリーダーを、日本語設定のままの PC で刻印どおり打ちたいとき。ESP32-P4。
- `UsbKeyboardToBle`: 有線 USB キーボードを無線化したいとき(PC / タブレット / スマートフォンからは Bluetooth キーボードに見える)。ESP32-S3 + USB Host/BLE HID。
- `VolumeKnob`: 手元に物理的な音量つまみが欲しいとき(ロータリーエンコーダ。スクロールダイヤルにも)。ESP32-S3 + GPIO。

## 自作の入出力をつなぎたいとき

次を入口にしてください。

- **自作インプットアダプタの書き方の実例: `TouchButtons`**(`InputAdapter` を継承して `update()` でハードを読み、`keys()` で押下集合を返す最小テンプレート。I2C・センサ・ネットワーク等も同じ型で書けます)。
- 最小のリファレンス実装: `ManualInputAdapter` / `ManualOutputAdapter`(`press()` / `release()` を呼ぶだけで入力になる・受け取った押下集合や文字を記録する)。
- アダプタの責務と API: `InputAdapter` / `OutputAdapter` のヘッダコメントと、各アダプタヘッダ(`ESP32KeyBridgeEspUsbHost.h` など)。
- report の組み立て: `buildHidKeyboardReport()` 等の builder 群。

## 過去の追加候補(scope 外)

- ~~BLE ゲームパッド専用 example~~ — アダプタ自体は実装済み。専用 example は合意済み scope に含まれません。
