# ESP32KeyBridge

> English: [README.md](README.md)

ESP32KeyBridge は、ESP32 を**キーボードと、それを読む機器の間に挟む**ための Arduino ライブラリです。1 つ以上の入力アダプタ(USB Host、BLE / Bluetooth Classic HID Host、GPIO スイッチ、キーマトリクス、ロータリーエンコーダ)からキー入力を受け取り、共通のキーモデルへ正規化し、リマップ・レイヤー・マクロ・キーボードレイアウト変換を適用して、1 つ以上の出力(USB Device HID、BLE / Classic HID、シリアルの `Print`)へ書き出します。

> **Scope 方針:** このリポジトリは存在するソフトウェアを説明するもので、新しい product scope を作りません。scope は関連する ChatGPT 会話で明示的に合意した機能だけです。履歴として残す未実装案は、そこで明示的に scope へ戻さない限り scope 外です。
>
> 現在合意されている追加機能と状態は [docs/SCOPE.md](docs/SCOPE.md) に記録します。

できあがるのは、PC から見て普通のキーボードと区別のつかない機器です。ドライバも、ホスト側のソフトも、キーボード側の設定変更も要りません。

bridge の core は **Arduino / ESP-IDF / ハードウェアのいずれにも依存しない**移植可能な C++ です。時刻を読まず、動的確保をせず、単一の `update()` から駆動されます。プラットフォーム依存はすべてアダプタ層にあるので、変換ロジック全体をホスト PC 上の unit test で固定できます。

## ESP32KeyBridge を使う理由

- **どのデバイスも 1 つのキーモデルで扱える。** キーは「種別 + 値」です(keyboard usage / consumer usage / マウスボタン / bridge 内部の virtual キー)。**種別をまたぐリマップ** — F13 を音量アップに、ゲームパッドのボタンを Escape に — はテーブル 1 行であって、特別扱いではありません。
- **stuck key が構造的に起きない。** キーは押下時に一度だけ解決され、離すまでその結果を保持します。押下中にレイヤーが変わっても `applyConfig()` を呼んでも、押したものと違うキーが release されることはありません。入力が切断されればそのキーは保持されずに解放されます。
- **入力を統合し、出力へ分配する。** 複数のキーボード・マクロパッド・エンコーダが 1 つの押下集合になり(和集合。解放は入力ごと)、同じ結果を USB ホストと BLE ホストへ同時に出せます。
- **lock 状態の正本は常に 1 つ。** lock を報告できる出力(USB Device / BLE HID)が接続している間はそのホストが正本で、bridge は各入力の LED へ中継するだけです。そういう出力が無ければ bridge 自身が終端ホストになり、Caps/Num/Scroll を自分でトグルします。
- **文字は文字のまま流れる。** `typeText()` は文字を文字として bridge に通し、出力の縁で**ホスト側の**レイアウトを使って打鍵へ展開します(Caps Lock 補正と、ユーザーが押している修飾キーの一時退避つき)。レイアウト変換も同じ仕組みなので、日本語設定のままの PC で US 配列を刻印どおり打てます。
- **USB・BLE・Classic で report の挙動が同じ。** HID 出力は core の report builder を共有し、BLE / Classic は EspBle 内でも report packing を共有します。接続方式や対応 SoC の違いはアダプタの外周に留まります。
- **アダプタは薄く、差し替えられる。** 入力アダプタの実体は `update()` と `keys()` だけです。I2C エキスパンダ、センサ、ネットワーク入力を書くのも 1 ページ程度で済みます(実例が `TouchButtons`)。

## 機能

| 領域 | 提供するもの | 主な API |
| --- | --- | --- |
| キーモデル | 種別 + 値によるキー同一性(HID keyboard page / consumer page / マウスボタン / virtual)、固定長の押下集合、修飾キーの正規化 | `Key`、`KeyboardUsage`、`ConsumerUsage`、`MouseUsage`、`VirtualUsage`、`KeySet` |
| 統合 | 入力 8・出力 4 まで。キーは入力をまたいだ和集合で、保持している全入力が離すまで押下のまま。切断はその入力の分だけを解放 | `addInput()`、`addOutput()`、`mergedKeys()`、`outputKeys()` |
| 変換 | 1 段の remap / 無効化(入力ごと + 全体。連鎖しないので 2 行で入れ替えになる)、トリガキーを消費するモーメンタリレイヤー | `TransformConfig`、`InputConfig`、`addLayer()`、`LayerConfig` |
| 文字列 | キーに紐づく UTF-8 マクロと、バックプレッシャ付きの文字ストリーム(出力の縁で打鍵へ展開) | `textMacro()`、`typeText()`、`typeChar()`、`typeAvailable()` |
| レイアウト | 16 ロケール × 4 プレーン(base / Shift / AltGr / AltGr+Shift)。入力の「刻印」としても、ホストの「解釈」としても使う | `KeyboardLayout`、`hostLayout`、`convertLayout()`、`layoutConversionToggle` |
| lock 状態 | 正本の連鎖(ホスト出力 → bridge のシャドウ → 終端ホスト)と、全入力の LED への転送。トランスポートが運べるなら kana も含む | `LockState`、`lockState()`、`lockAuthorityPresent()`、`setLockState()` |
| 相対軸 | X / Y / Wheel / Pan と軸ごとのスケール(負で反転 = ナチュラルスクロール)。入力をまたいで合算 | `Axis`、`setAxisScale()`、`takeAxisDelta()`、`writeAxisDelta()` |
| HID report | boot 6KRO / 32 キー rollover / consumer / 相対マウスの純粋な builder(軸は int8 飽和 + 繰り越し) | `buildHidKeyboardReport()` ほか builder 群 |
| ゲームパッド | ボタン番号とハット/十字を任意の `Key` へ写像。ハットのデコードは USB / BLE アダプタで共通 | `GamepadKeyMap`、`mapButton()`、`mapHat()` |

## アダプタ

アダプタヘッダはヘッダオンリーなので、**include したアダプタの分だけ**依存ライブラリが増えます。core ヘッダの依存はゼロです。

| ヘッダ | 依存 | アダプタ |
| --- | --- | --- |
| `ESP32KeyBridge.h` | なし | `ManualInputAdapter` / `ManualOutputAdapter`(リファレンス実装・テストダブル) |
| `ESP32KeyBridgeEspUsbHost.h` | [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost) 2.6.0 以上 | USB キーボード(consumer キー込み)・マウス・ゲームパッド・MIDI 入力 |
| `ESP32KeyBridgeEspUsbDevice.h` | [EspUsbDevice](https://github.com/tanakamasayuki/EspUsbDevice) 2.0.0 以上 | 複合 USB HID 出力(keyboard + consumer + mouse) |
| `ESP32KeyBridgeEspBle.h` | [EspBle](https://github.com/tanakamasayuki/EspBle) 1.3.0 以上 | BLE HID 出力(peripheral)、bond 済み接続先切替、BLE キーボード / マウス / ゲームパッド入力(central) |
| `ESP32KeyBridgeBluetooth.h` | — | Scroll Lock + 数字の接続先 hotkey と LED 状態表示を行う transport 非依存 wrapper |
| `ESP32KeyBridgeEspBleClassic.h` | EspBle 1.3.0 以上 | 無印 ESP32 が必要な legacy Bluetooth Classic 実装(**非サポート / scope 外**) |
| `ESP32KeyBridgeGpio.h` | なし(Arduino GPIO。エンコーダは PCNT) | 単独キー、スキャン式キーマトリクス、ロータリーエンコーダ |
| `ESP32KeyBridgeSerial.h` | なし(Arduino `Print`) | 文字出力、人間可読のデバッグログ |

アダプタごとの挙動、`update()` 頻度に対する耐性、PC から見える USB 構成の詳細は [docs/ADAPTERS.ja.md](docs/ADAPTERS.ja.md) にあります。

## 対応環境

正式にサポートする target は **ESP32-S3 のみ**です。

| 機能 | ESP32-S3 |
| --- | --- |
| GPIO キー / マトリクス | ✅ |
| ロータリーエンコーダ(PCNT) | ✅ |
| USB Host 入力 | ✅ |
| USB Device 出力 | ✅ |
| BLE HID 入出力 | ✅ |
| Bluetooth Classic HID | — |

S3 の native USB controller は 1 つなので、USB Host または USB Device の一方と、反対側の BLE を組み合わせます。他の ESP32 variant 向けコードや過去の example が残っていても、正式サポートおよび合意済み scope には含まれません。

開発と example ビルドは arduino-esp32 3.3.11 で行っています。各 example の `sketch.yaml` に検証済みの core とライブラリのバージョンを固定してあります。

## 使いはじめる

Arduino Library Manager から、または Arduino CLI でインストールします。

```sh
arduino-cli lib install ESP32KeyBridge
```

example にはすべて検証済みバージョンを固定した `sketch.yaml` が付いているので、追加設定なしでビルドできます。

```sh
arduino-cli compile examples/practical/SwapCtrlCapsLock
```

どのスケッチも「1) ハードウェア起動 → 2) ブリッジ配線 → 3) 設定を組んで適用」の 3 段構成です。

```cpp
#include <ESP32KeyBridge.h>
#include <ESP32KeyBridgeEspUsbHost.h>
#include <ESP32KeyBridgeEspUsbDevice.h>
#include <EspUsbHost.h>
#include <EspUsbDevice.h>

EspUsbHost usbHost;     // キーボード側
EspUsbDevice usbDevice; // PC 側

esp32keybridge::ESP32KeyBridge bridge;
esp32keybridge::EspUsbHostKeyboardInputAdapter keyboard(usbHost);
esp32keybridge::EspUsbDeviceHidOutputAdapter pc(usbDevice);

void setup() {
  // 1) ハードウェアを起動する(スタックはアダプタではなくスケッチの所有物)。
  usbHost.begin();
  usbDevice.begin();

  // 2) ブリッジを配線する。
  bridge.addInput(keyboard);
  bridge.addOutput(pc);

  // 3) 設定する。remap は 1 段で連鎖しないので、2 行で入れ替えになる。
  esp32keybridge::ESP32KeyBridgeConfig config;
  config.global.remap(esp32keybridge::KeyboardUsage::CapsLock,
                      esp32keybridge::KeyboardUsage::LeftCtrl);
  config.global.remap(esp32keybridge::KeyboardUsage::LeftCtrl,
                      esp32keybridge::KeyboardUsage::CapsLock);
  bridge.applyConfig(config);
}

void loop() {
  bridge.update();  // 入力・変換・出力はすべてここから走る
  delay(1);
}
```

`begin()` はありません。`addInput()` / `addOutput()`、`applyConfig()`、アダプタの起動に順序の制約もありません。登録と設定は状態を持つだけで、最初の `update()` からすべてが動き始めます。

example の一覧は [examples/README.ja.md](examples/README.ja.md) にあります。すべて実ハードウェア構成でそのまま書き込めるスケッチです。

## ドキュメント

**どの文書をどの順で読むかは [ドキュメント案内](docs/README.ja.md)** にまとめています。

設計ドキュメントは日本語のみ、README と example は日英併記です。

- [入門ガイド(初めて使う人向け)](docs/GUIDE_BASICS.ja.md)
- [アダプタ一覧・挙動・依存](docs/ADAPTERS.ja.md)
- [データモデル(キー同一性・lock 状態・レイアウト変換)](docs/DATA_MODEL.ja.md)
- [core 設計と時間・並行性の境界](docs/CORE_DESIGN.ja.md)
- [設計検証に使ったユースケース](docs/USE_CASES.ja.md)
- [要件](docs/REQUIREMENTS.ja.md)
- [設計決定の台帳](docs/DECISIONS.ja.md)
- [設定まわりの設計メモ](docs/CONFIGURATION.ja.md)
- [開発計画と残作業](docs/DEVELOPMENT_PLAN.ja.md)
- [テスト計画](tests/TEST_PLAN.ja.md) / [テスト構成](tests/README.ja.md)
- [リリースチェックリスト](docs/RELEASE_CHECKLIST.ja.md)

## テスト

ホストの unit test と example の build smoke test はハードウェア不要です。

```sh
cd tests
uv run pytest unit/ examples_compile/
```

実機テストは必要な機材で分けています。`single/`(ESP32-S3 1 台)、`peer/`(2 台。片方が実 USB でもう片方を駆動)、`manual/`(配線・ホスト OS の挙動・ペアリング)。詳しくは [tests/README.ja.md](tests/README.ja.md) を参照してください。

## 関連ライブラリ

- [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost) — USB 入力アダプタの土台になる USB Host スタック
- [EspUsbDevice](https://github.com/tanakamasayuki/EspUsbDevice) — USB HID 出力アダプタの土台になる USB Device スタック
- [EspBle](https://github.com/tanakamasayuki/EspBle) — BLE HID アダプタの土台になる BLE スタック

この 3 つはキーボードレイアウトのテーブル、HID usage の扱い、listener API の形を共有しています。どのスタックに載せてもアダプタが同じ形になるのはそのためです。

## ライセンス

MIT License
