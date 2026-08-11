# UsbKeyboardToBle

[English](README.md)

有線 USB キーボードを無線化します。キーボードは ESP32 に挿し、PC / タブレット / スマートフォンからは **Bluetooth キーボード**に見えます。USB ポートに空きがないマシンや、そもそも USB のないタブレットで、好きなキーボードをそのまま使えます。

- キーボードは USB Host 入力アダプタで読み、結果を BLE HID 出力アダプタ(`EspBleHidOutputAdapter`)から HID over GATT の keyboard + consumer + mouse として出します。
- 途中でできる変換はすべてそのまま使えます(リマップ、無効化、モーメンタリレイヤー、テキストマクロ、レイアウト変換)。このスケッチでは CapsLock 長押しのナビゲーションレイヤーを足しています。無線キーボードはマウスのない場所で使うことが多いためです。
- lock 状態は Bluetooth ホストが正本です。ホストの LED output report を受け取り、bridge が USB キーボードの LED へ転送します。キーボードで Caps Lock を押したときの LED は推測ではなくホストに追従します。

## USB ポートは 1 つで足りる

PC 側が USB ではなく Bluetooth なので、USB を使うのはキーボード側だけです。したがって USB が 1 つのチップで足ります — USB 対 USB の example(`SwapCtrlCapsLock`、`NavLayer` など)が要求する ESP32-P4 ではなく、**ESP32-S3** で動きます。

## ハードウェア構成(ESP32-S3)

- キーボードを ESP32-S3 のネイティブ USB ポートに挿します(USB Host として読みます)。キーボードへのバス電源供給が必要なので、USB-A ホストコネクタ付きのボードか、USB-OTG ケーブル + 外部 5V を使います。
- ボード自体は UART ポートまたはバッテリから給電します。UART0 のシリアルコンソールに BLE の状態が出ます。

## ペアリング

`KeyBridge Keyboard` という名前でアドバタイズします。ホストの Bluetooth 設定から一度ペアリングすれば、ボンディングで以後も繋がります。切断後は再アドバタイズするので、ホスト側から戻ってこられます。

## ble.update() と bridge.update()

EspBle は `ble.update()` を呼んだコンテキストからコールバックを配送します。したがって `bridge.update()` と同じ `loop()` に置き、届いたばかりの report を同じサイクルに含めるために **先に** 呼びます:

```cpp
void loop() { ble.update(); bridge.update(); delay(1); }
```
