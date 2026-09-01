# ClassicKeyboardToBle

[English](README.md)

Bluetooth Classic HID だけを話す古いキーボードを、BLE HID Host から使えるように変換します。Classic keyboard/mouse を `EspBleClassicHidHost*InputAdapter` で読み、既存の `EspBleHidOutputAdapter` へ渡します。

## 制約

- **無印 ESP32 専用**です。Classic 無線を持つ ESP32 SoC は無印だけです。
- `keyboardAddress` を対象の Bluetooth address に書き換えてください。Classic は BLE の service advertisement を scan して選ぶ方式ではありません。address は EspBle の `Classic/Inquiry` example で調べられます。
- Classic HID Host は 1 接続です。keyboard と mouse が同じ複合 device なら両方を取得できます。
- EspBle 1.3.0 の Classic Host が標準復号するのは keyboard / mouse です。consumer / gamepad report は raw callback には届きますが、このアダプタの入力にはなりません。
- BLE と Classic を同時に動かす dual-host mode は EspBle では実験扱いです。旧機器変換のように両方が本当に必要な場合だけ使ってください。
- 2 つの host stack を含むため flash を多く使います。arduino-esp32 3.3.11 の既定 partition ではこの example だけで app 領域の約 97% です。機能を追加する製品では app の大きい partition scheme を選んでください。

Classic 側には BLE の `setAutoReconnect()` / `setAutoRediscover()` 相当がありません。切断後の再接続方針（再度 `connect(address)` する時機）は製品側で決めます。この最小例は起動時に 1 回だけ接続します。
