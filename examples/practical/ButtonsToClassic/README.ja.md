# ButtonsToClassic

[English](README.md)

GPIO スイッチ 3 個を Bluetooth Classic（BR/EDR）の keyboard / media controller にします。BLE HID を受け付けない旧世代 PC、ゲーム機、産業機器向けです。相手が BLE HID を使えるなら、対応 SoC が広く省電力な BLE を優先してください。

## ハードウェア

**無印 ESP32 専用**です。ESP32-S3 / C3 / C6 / H2 / P4 には Bluetooth Classic 無線がないため動きません。スイッチを GPIO 25 / 26 / 27 と GND の間につなぎます（内部 pull-up、Low で押下）。

## 構成とペアリング

`EspBleClassicHidOutputAdapter` は BLE 出力と同じ keyboard 6KRO + consumer 1 usage + mouse report を、Classic HID Device として送ります。アダプタを `bluetooth.begin()` より前に構築してください。Host 側の Bluetooth 設定から `KeyBridge Classic` をペアリングします。

Classic では Report Descriptor と device identity 文字列が 1 個の SDP record に入り、合計 214 byte が上限です。このアダプタの 3 profile は既定文字列なら収まりますが、長い `EspBleClassicHidProfileConfig` の文字列を渡すと `begin()` が失敗することがあります。

イベントは `bluetooth.update()` から配送されるため、同じ `loop()` で `bridge.update()` より先に呼びます。
