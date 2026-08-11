# BleKeyboardToUsb

[English](README.md)

逆方向です。**Bluetooth キーボードとペアリングできない機器で Bluetooth キーボードを使う**ためのもの。BIOS/UEFI 画面、KVM 切替器、計測器、ゲーム機、古い PC など。ESP32 が Bluetooth ホストとして BLE キーボードに接続し、PC からは普通の有線 USB キーボードに見えます。ドライバ不要、PC 側のペアリングも不要です。

- キーボードは `EspBleHidHostKeyboardInputAdapter`(HID over GATT central)で読み、USB Device HID 出力アダプタから PC へ流します。
- BLE マウスも `EspBleHidHostMouseInputAdapter` で拾えます。ボタンはマウスボタン、移動とホイールは相対軸になり、PC には普通の USB マウスとしても見えます。キーボード + マウス一体型なら 1 接続で両方流れます。
- lock 状態は逆向きに流れます。PC の Caps/Num/Scroll LED report が正本で、入力アダプタ経由で BLE キーボードの LED まで届きます。USB Host 側と違い、BLE 経路では JIS かな LED も転送できます。

## USB ポートは 1 つで足りる

キーボード側が USB ではなく Bluetooth なので、USB を使うのは PC 側だけです。USB が 1 つの **ESP32-S3** で足ります。

## ハードウェア構成(ESP32-S3)

- ESP32-S3 のネイティブ USB ポートを PC に挿します(USB HID デバイスとして列挙されます)。
- UART0 のシリアルコンソールに接続状態が出ます。

## ペアリング

キーボードをペアリングモードにしてボードをリセットします。HID サービス(`0x1812`)をアドバタイズする最初の接続可能デバイスへ接続し、ペアリングとボンディングを行います。以後は `setAutoReconnect(true)` と `setAutoRediscover(true)` が自動で復帰させます。

接続の手順はあえてスケッチ側に残しています(アダプタは HID report を消費するだけ)。近くに複数のキーボードがある場合は、`onResult` を「最初に見つかった HID デバイス」ではなく名前やアドレスで絞り込んでください。

## ble.update() と bridge.update()

EspBle は `ble.update()` を呼んだコンテキストからコールバックを配送します。したがって `bridge.update()` と同じ `loop()` に置き、届いたばかりの report を同じサイクルに含めるために **先に** 呼びます:

```cpp
void loop() { ble.update(); bridge.update(); delay(1); }
```
