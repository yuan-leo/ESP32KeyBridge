# UsbKeyboardToBle

[English](README.md)

有線 USB キーボードを無線化します。キーボードは ESP32 に挿し、PC / タブレット / スマートフォンからは **Bluetooth キーボード**に見えます。USB ポートに空きがないマシンや、そもそも USB のないタブレットで、好きなキーボードをそのまま使えます。

- キーボードは USB Host 入力アダプタで読み、結果を BLE HID 出力アダプタ(`EspBleHidOutputAdapter`)から HID over GATT の keyboard + consumer + mouse として出します。
- 途中でできる変換はすべてそのまま使えます(リマップ、無効化、モーメンタリレイヤー、テキストマクロ、レイアウト変換)。このスケッチでは CapsLock 長押しのナビゲーションレイヤーを足しています。無線キーボードはマウスのない場所で使うことが多いためです。
- lock 状態は Bluetooth ホストが正本です。ホストの LED output report を受け取り、bridge が USB キーボードの LED へ転送します。キーボードで Caps Lock を押したときの LED は推測ではなくホストに追従します。
- **Scroll Lock** を押しながら上段の **1**、**2**、**3** を押すと、その slot に bond 済みの host へ切り替えます。**Scroll Lock + 0** で選択中 slot を入れ替える pairing mode に入ります。切替 / pairing 中は Scroll Lock LED が点滅し、終了後は host の本来の Scroll Lock 状態へ戻ります。

## local layout 設定

ESP32-S3 は layout 設定 page も host します。phone を `KeyBridge-Setup`、
password `keybridge` へ接続し、`http://192.168.4.1/` を開きます。保存時は
二重化 NVS record の読戻し確認後に input/host layout、変換状態、toggle key
を適用します。実運用前にこの開発用 credential を変更してください。

同じ page で4個の永続 text macro も設定できます。重複しない F1-F24 trigger
を選び、phone から31文字以内の text を入力します。有線 keyboard でその
function key を押すと key 自体を消費し、設定済み host layout で text を
type します。

page を開いて、押したままの key をいったん全て release すると、接続した
有線 keyboard から local Wi-Fi 経由で操作できます。**Tab / Shift+Tab** または矢印 key で移動し、**Enter / Space**
で実行、**Esc** で保存済み値を再読込みします。capture 中の keyboard key
は BLE host へ送りません。page を閉じるか polling が止まると約 1.2 秒後に
capture が自動解除され、解除時に押したままの key は release まで抑止されます。

## USB ポートは 1 つで足りる

PC 側が USB ではなく Bluetooth なので、USB を使うのはキーボード側だけです。したがって USB が 1 つのチップで足ります — USB 対 USB の example(`SwapCtrlCapsLock`、`NavLayer` など)が要求する ESP32-P4 ではなく、**ESP32-S3** で動きます。

## ハードウェア構成(ESP32-S3)

- キーボードを ESP32-S3 のネイティブ USB ポートに挿します(USB Host として読みます)。キーボードへのバス電源供給が必要なので、USB-A ホストコネクタ付きのボードか、USB-OTG ケーブル + 外部 5V を使います。
- ボード自体は UART ポートまたはバッテリから給電します。UART0 のシリアルコンソールに BLE の状態が出ます。
- 手動 build では partition scheme に **Minimal SPIFFS (1.9MB APP with OTA)** を選択してください。example の `sketch.yaml` は自動で選択します。USB Host、BLE、Wi-Fi を組み合わせた firmware は default の 1.25MB app partition には収まりません。

## ペアリング

`KeyBridge Keyboard` という名前でアドバタイズします。**Scroll Lock + 0** を押してから host の Bluetooth 設定で pair してください。bond は選択中 slot に保持されます。EspBle 同梱 NimBLE の bond 上限に合わせて 3 slot が使え、通常の切断後は directed advertising で選択中 slot だけへ再接続します。

## ble.update() と bridge.update()

EspBle は `ble.update()` を呼んだコンテキストからコールバックを配送します。したがって `bridge.update()` と同じ `loop()` に置き、届いたばかりの report を同じサイクルに含めるために **先に** 呼びます:

```cpp
void loop() { ble.update(); bridge.update(); settingsWeb.update(); delay(1); }
```
