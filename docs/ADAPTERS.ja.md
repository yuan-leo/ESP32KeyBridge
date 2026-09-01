# アダプタ一覧

> English: [ADAPTERS.md](ADAPTERS.md)

アダプタの一覧・実装状況・PC からどう見えるか(USB デバイス構成)をまとめます。実装の進行([DEVELOPMENT_PLAN.ja.md](DEVELOPMENT_PLAN.ja.md) 実装順 7)に合わせて更新します。

状況の凡例: **実装済み** / **モック**(スケッチ向け API は確定、ビルドのみ可) / **構想**(ヘッダ未作成)。実装済みには実機での確認状況を併記します — **実機検証済み** は `tests/peer/` の自動テストか実機での確認が通ったもの、**実機検証待ち** はビルドと unit test までのものです。

## 入力アダプタ

| クラス | ヘッダ | 依存 | 状況 | 機能 |
|---|---|---|---|---|
| `ManualInputAdapter` | `ESP32KeyBridge.h`(core) | なし | **実装済み** | `press()` / `release()` で論理押下を注入。相対値(`addAxisDelta`)、切断模擬、LockState 受信の記録。自作入力の最小リファレンス |
| `EspUsbHostKeyboardInputAdapter` | `ESP32KeyBridgeEspUsbHost.h` | EspUsbHost 2.3.0+ | **実装済み(実機検証済み: peer `usb_host_keyboard / usb_host_consumer`)** | USB キーボード(スタック上の全キーボードを合算、最大 4 台。consumer キー = メディアキーやリモコンも合算、同時 8 usage)。`onKeyboardState` の 256-bit スナップショット(boot / report-ID boot / NKRO 共通、修飾キー単独変化含む)を KeySet に写像。boot のロールオーバーエラーコード(usage 0x01〜0x03)は除外。LockState を全キーボードの LED へ転送(num/caps/scroll。kana は EspUsbHost に転送手段なし。EspUsbHost 2.3.0 で report-ID/NKRO 専用キーボードへも LED を送れる = boot 宣言キーボードにしか送れなかった 2.2.0 の制限は解消)。presence は最初の report から、切断でそのデバイスのキーだけ解放 |
| `EspUsbHostMouseInputAdapter` | `ESP32KeyBridgeEspUsbHost.h` | EspUsbHost | **実装済み(実機検証済み: peer `usb_host_mouse`)** | USB マウス(スタック上の全マウスを合算、最大 4 台)。ボタン = MouseButton キー(union)、移動・ホイール・AC Pan(横ホイール/チルト。boot mouse report には無いので 0)= 相対軸(合算)。presence は最初の report から(列挙照会 API が無いため)、切断でそのマウスのボタンだけ解放 |
| `EspUsbHostGamepadInputAdapter` | `ESP32KeyBridgeEspUsbHost.h` | EspUsbHost 2.3.0+ | **実装済み(実機検証済み: peer `usb_host_gamepad`)** | USB ゲームパッド(入力専用)。`mapButton(番号, key)` で HID ボタン(1 始まり)を任意の Key に、`mapHat(up,down,left,right)` で十字/ハット(8 方向、斜めは 2 キー)を写像。登録したコントロールだけ出力。全パッド合算(最大 4 台)、切断でそのパッドだけ解放。アナログスティック/トリガは未対応(現状) |
| `EspUsbHostMidiInputAdapter` | `ESP32KeyBridgeEspUsbHost.h` | EspUsbHost 2.3.0+ | **実装済み(実機検証済み: peer `usb_host_midi`)** | USB MIDI(入力専用)。`mapNote(ノート, key)` で MIDI ノート(0-127)を任意の Key に写像(Note On=押下 / Note Off・vel0=解放)。`setChannel(ch)` で 1 ch に限定(既定 全 ch)。Note On/Off のみ使用(CC・ピッチベンド等は無視)。登録したノートだけ出力、全 MIDI デバイス切断で残ノートをクリア |
| `GpioKeyInputAdapter` | `ESP32KeyBridgeGpio.h` | Arduino GPIO のみ | **実装済み(実機検証待ち)** | `addKey(pin, key, activeLow, pullUp)` で最大 `SOC_GPIO_PIN_COUNT` キー。デバウンス内蔵(既定 5ms、`setDebounceMillis()` で調整)。ピン設定は登録後最初の `update()`。loop レート(≈1kHz)のポーリングで足りる |
| `EspBleHidHostKeyboardInputAdapter` | `ESP32KeyBridgeEspBle.h` | EspBle 1.2.0+ | **実装済み(実機検証済み: peer `ble_hid_keyboard`)** | BLE キーボード(HID over GATT central。接続中の全キーボードを合算、最大 4 接続。consumer キーも合算、1 接続あたり 1 usage = HOGP の consumer report が同時に 1 usage しか運ばないため)。`onKeyboardState` の 256-bit スナップショット(boot / report / NKRO 共通、修飾キー単独変化含む)を KeySet に写像 = USB Host 側と同じ経路。LockState を全キーボードの LED へ転送(**kana も転送可能** = USB Host 側にはない)。presence は最初の report から、切断でその接続のキーだけ解放。**consumer の解放イベントは EspUsbHost と意味が違う**(EspUsbHost は「離された usage」を報告するが、EspBle は「その接続の新しい usage」= 解放時は 0 を報告する)ので、アダプタは usage の一致を探すのではなく接続ごとのエントリを置き換える |
| `EspBleHidHostMouseInputAdapter` | `ESP32KeyBridgeEspBle.h` | EspBle 1.2.0+ | **実装済み(実機検証済み: peer `ble_hid_keyboard`)** | BLE マウス(接続中の全マウスを合算、最大 4 接続)。ボタン = MouseButton キー(union)、移動・ホイール = 相対軸(合算)。HOGP のマウス report に AC Pan は無し |
| `EspBleHidHostGamepadInputAdapter` | `ESP32KeyBridgeEspBle.h` | EspBle 1.2.0+ | **実装済み(実機検証済み: peer `ble_hid_gamepad`)** | BLE ゲームパッド。`mapButton()` / `mapHat()` の写像とハットのデコードは core の `GamepadKeyMap` を共有するので USB Host 側と完全に同じ挙動。全パッド合算(最大 4 接続)、切断でそのパッドだけ解放 |
| `EspBleClassicHidHostKeyboardInputAdapter` | `ESP32KeyBridgeEspBleClassic.h` | EspBle 1.3.0+ | **実装済み(実機検証済み: peer `classic_hid_keyboard`)** | Bluetooth Classic HID Host キーボード(**無印 ESP32 のみ、1 接続**)。EspBleClassic が Report Descriptor から復号した 256-bit usage + modifier snapshot を KeySet に写像し、lock LED を相手へ返す。Classic Host は consumer report を標準復号しないためメディアキー入力は対象外。接続は sketch が address を指定して行う |
| `EspBleClassicHidHostMouseInputAdapter` | `ESP32KeyBridgeEspBleClassic.h` | EspBle 1.3.0+ | **実装済み(実機検証済み: peer `classic_hid_keyboard`)** | Bluetooth Classic HID Host マウス(**無印 ESP32 のみ、1 接続**)。復号済みのボタン・X/Y・wheel を入力へ写像。AC Pan は無し |
| `GpioMatrixInputAdapter` | `ESP32KeyBridgeGpio.h` | Arduino GPIO のみ | **実装済み(実機検証済み)** | キーマトリクス(最大 16 行 × 16 列。スキャン・デバウンス・ゴースト判定は専用タスク 1kHz = loop がブロックしても取りこぼさない。初回 `update()` で起動、非アクティブ行は high-Z)。配線は `setRowPins()` / `setColPins()`(行 = スキャン出力、列 = プルアップ入力)、キーマップは `setKeys()` に行優先の並びで渡す(見た目 = 物理配置、座標番号なし。歯抜けは無効キー `Key()`)。ダイオード任意(無しはゴーストブロック = 確実な同時押し 2 キーまで。`setHasDiodes(true)` で無制限)。デバウンス内蔵 |
| `RotaryEncoderInputAdapter` | `ESP32KeyBridgeGpio.h` | PCNT(ESP-IDF 内蔵。`SOC_PCNT_SUPPORTED` のチップのみ) | **実装済み(実機検証待ち)** | ロータリーエンコーダ(PCNT ×4 直交デコード + グリッチフィルタ、割り込みなし、1 アダプタ 1 エンコーダ = チップの PCNT ユニット数まで)。1 ノッチ → キータップ(`mapToKeys(cw, ccw)`)または相対軸 ±1(`mapToAxis(axis)`)。`setPulsesPerDetent()`(既定 4 = EC11)。押し込みスイッチは GpioKeyInputAdapter で |
| IR / I2C・SPI エキスパンダ | - | - | **構想** | [DEVELOPMENT_PLAN.ja.md](DEVELOPMENT_PLAN.ja.md) の対応予定 |

補足: 文字列入力(シリアル指令、シリアル型バーコードリーダー)はアダプタではなく、ブリッジの一級入力 `typeText()` / `typeChar()` を使います(example: SerialTextTyper)。

## 出力アダプタ

| クラス | ヘッダ | 依存 | 状況 | lock 報告 | 機能 |
|---|---|---|---|---|---|
| `ManualOutputAdapter` | `ESP32KeyBridge.h`(core) | なし | **実装済み** | 模擬可 | 受け取った押下集合・文字・相対値を記録。ホストの LED report を模擬できる(テスト・自作出力のリファレンス) |
| `EspUsbDeviceHidOutputAdapter` | `ESP32KeyBridgeEspUsbDevice.h` | EspUsbDevice | **実装済み(実機検証済み: peer `usb_device_keyboard_output`)** | あり | 複合 HID デバイス(keyboard + consumer + mouse)。keyboard = boot 6KRO、consumer = 16bit usage × 1、相対軸 → mouse report(int8 飽和 + 繰り越し、Pan は非対応で破棄)。各 report は変化時のみ送信 + busy 時は次 update で再送。LED output report 受信 = lock 正本になれる(kana 含む)。`connected()` = ホストに mount 中 |
| `TextOutputAdapter` | `ESP32KeyBridgeSerial.h` | なし(Arduino `Print` のみ) | **実装済み(実機検証待ち)** | なし | 文字対応出力。文字ストリーム(`writeText()`)だけを消費して `Print`(Serial / Serial1 / SoftwareSerial 等)へ UTF-8 で出す。文字源は `typeText()` 由来と、`convertLayout()` を設定した入力の押下由来(刻印デコード。Enter→改行 / Tab→タブは core が付与)。**出力側にレイアウトは持たない**(HID バーコード → 文字は入力側 convertLayout で決まる) |
| `LogOutputAdapter` | `ESP32KeyBridgeSerial.h` | なし(Arduino `Print` のみ) | **実装済み(実機検証待ち)** | なし | デバッグ監視。押下集合の変化(`KEYS ...`)・タイプ文字(`TEXT ...`)・相対軸(`AXIS ...`)を人間可読で `Print` へ。実出力と併設 or 単独でブリングアップ |
| `EspBleHidOutputAdapter` | `ESP32KeyBridgeEspBle.h` | EspBle 1.2.0+ | **実装済み(実機検証済み: peer `ble_hid_output`)** | あり | BLE HID peripheral(HID over GATT)。複合 HID(keyboard + consumer + mouse)で、report の形式は USB Device 出力と同一(keyboard = boot 6KRO、consumer = 16bit usage × 1、mouse = ボタン 8 + X/Y/Wheel。Pan は非対応で破棄)。各 report は変化時のみ送信 + 送れなかったときは次 update で再送。LED output report 受信 = lock 正本になれる(kana 含む)。`connected()` = ホストが接続・暗号化済みで input report を subscribe 中 |
| `EspBleClassicHidOutputAdapter` | `ESP32KeyBridgeEspBleClassic.h` | EspBle 1.3.0+ | **実装済み(実機検証済み: peer `classic_hid_output`)** | あり | Bluetooth Classic HID Device(**無印 ESP32 のみ**)。BLE 出力と同じ keyboard 6KRO + consumer + mouse report。Classic 固有の SDP record 上限(Report Descriptor + identity 文字列で 214 byte)内に 3 profile を合成し、超過時は `bluetooth.begin()` が失敗。接続済み HID Host は 1 台 |

## PC からどう見えるか(USB デバイス構成)

USB Device 出力は 1 種類で、常に**複合 HID デバイス**として PC に見えます。

| 出力アダプタ | PC から見える構成 | 送る report |
|---|---|---|
| `EspUsbDeviceHidOutputAdapter` | 複合 HID デバイス(keyboard + consumer + mouse) | keyboard(boot 6KRO)/ consumer(16bit usage × 1)/ mouse(ボタン 8 + X/Y/Wheel、int8 飽和 + 繰り越し。Pan は boot mouse report に無く破棄) |

規則:

- **常に全部入りで見せる**方針です。使わない interface は report を送らないだけで USB 的に無害なので、キーボード単体版は用意しません(古い BIOS 向けの最小構成が必要なら実キーボードを直結すればよい、という割り切り)。keyboard interface は boot protocol 対応なので BIOS/UEFI でも動作します。将来 interface の無効化オプションが要る場合はコンストラクタ引数(既定値付き)で非破壊に追加できます。
- **interface 構成は `usbDevice.begin()` 時に確定**し、以後変更できません(追加・削除には再 enumerate = 抜き挿し相当が必要)。設定にマウスやメディアキーを後から足しても、descriptor は最初から揃っているので再列挙は起きません。
- VID / PID / 製品名などのデバイス素性は `EspUsbDeviceConfig` で(スタックはスケッチ所有)。
- **LED output report を受信**し、接続中は lock 状態の正本(の連鎖の候補)になります([DATA_MODEL.ja.md](DATA_MODEL.ja.md) の Lock 状態)。
- rollover(32 キー)report builder は core にありますが、v1 の USB Device 出力は boot keyboard を基本とします(NKRO descriptor は将来のアダプタ拡張)。

### BLE HID 出力から見た構成

`EspBleHidOutputAdapter` も同じ考え方で、常に keyboard + consumer + mouse を 1 つの HID service に載せます(HID over GATT)。report の中身は USB Device 出力と同一なので、出力を差し替えても、両方同時に登録しても、キーの見え方は変わりません。相違点は 2 つだけです。

- HID service の構成は `ble.begin()` で確定します(USB の `usbDevice.begin()` に対応)。したがって出力アダプタは `ble.begin()` より先に構築します。
- `connected()` は「mount 済み」ではなく「接続 + 暗号化済み + input report を subscribe 済み」です。HOGP ではホストが subscribe するまで notification を出せないためで、ペアリング直後の一瞬は false になります。

### Bluetooth Classic HID 出力から見た構成

`EspBleClassicHidOutputAdapter` も report の中身は同じで、keyboard + consumer + mouse を 1 台の Classic HID Device にします。構成は `bluetooth.begin()` で SDP に登録されるため、アダプタを先に構築します。BLE と違い、Report Descriptor と `EspBleClassicHidProfileConfig` の identity 文字列が 1 個の SDP record を共有し、合計 **214 byte** が上限です。既定文字列の 3 profile は収まりますが、長いカスタム文字列は `begin()` を失敗させます。

Classic は address ベースで 1 Host と接続し、HOGP の GATT subscription はありません。したがって `connected()` は profile を構成済みかつ Classic HID Host が接続済み、という意味です。

## update() 頻度と取りこぼし耐性

サンプリングの時間精度はアダプタが担保するのが原則です([CORE_DESIGN.ja.md](CORE_DESIGN.ja.md) の「時間・並行性の境界」)。`update()` の呼び出し間隔が影響するのは「反映(変換・出力)の遅れ」だけで、入力自体の取りこぼしは起きません — 例外は `GpioKeyInputAdapter` です。

| アダプタ | サンプリング機構 | loop が長時間ブロックしたとき |
|---|---|---|
| `GpioKeyInputAdapter` | `update()` 内で読む(**loop レート依存**) | **ブロック中より短い押下は取りこぼす**。ペダル・ボタンのような押下の長い入力専用の割り切り |
| `GpioMatrixInputAdapter` | 専用タスク 1kHz(スキャン・デバウンス・ゴースト判定込み。初回 `update()` で起動) | 取りこぼさない(押下状態はタスク側で確定済み。反映が遅れるだけ) |
| `RotaryEncoderInputAdapter` | PCNT ペリフェラルのハードウェア計数 | 取りこぼさない(カウンタが溜めている。反映が遅れるだけ) |
| `EspUsbHostKeyboardInputAdapter` / `EspUsbHostMouseInputAdapter` / `EspUsbHostGamepadInputAdapter` / `EspUsbHostMidiInputAdapter` | EspUsbHost のタスクからコールバック受け(critical section 越しの共有状態) | 取りこぼさない(状態スナップショット保持。マウスの移動量は合算。MIDI は Note On/Off 状態を保持) |
| `EspBleHidHostKeyboardInputAdapter` / `EspBleHidHostMouseInputAdapter` / `EspBleHidHostGamepadInputAdapter` | EspBle のイベントキュー(配送は `ble.update()` のコンテキスト) | 取りこぼさない(スタックタスクがキューに積む。反映が遅れるだけ)。ただし **`ble.update()` を呼ばないと何も届かない** |
| `EspBleClassicHidHostKeyboardInputAdapter` / `EspBleClassicHidHostMouseInputAdapter` | EspBleClassic のイベントキュー(配送は `bluetooth.update()` のコンテキスト) | 取りこぼさない(状態 / 相対量をアダプタが保持)。ただし **`bluetooth.update()` を呼ばないと何も届かない** |
| `ManualInputAdapter` | スケッチが直接 `press()`/`release()` | スケッチ次第(呼んだものはそのまま残る) |

出力側(`EspUsbDeviceHidOutputAdapter` 等)は状態スナップショット方式なので、`update()` が遅れても「最後の状態」が送られるだけで壊れません(タイピング・マクロの打鍵速度は update 間隔に比例して遅くなります)。

## ロータリーエンコーダ: 直交デコードとデテント

`RotaryEncoderInputAdapter` は A 相・B 相の**両方**にカウントを設定し、さらに立上り・立下りの**両エッジ**を数える完全な x4 直交デコード(quadrature ×4)です。仕組みと、なぜ「1 ノッチ = 出力 1」になるかを説明します。

### x4 直交デコード(両相・両エッジ)

PCNT を 2 チャンネル使い、互いの相を参照させます。

- チャンネル A: エッジ入力 = A 相、レベル入力 = B 相
- チャンネル B: エッジ入力 = B 相、レベル入力 = A 相

各チャンネルは**両エッジ**で ±1 し、相手相のレベルで方向(増加/減少)を決めます。結果、A の両エッジ + B の両エッジ = **1 直交周期あたり 4 カウント**。回転方向は A/B どちらの相が先行するかで符号が決まります(片相・片エッジだけだと方向が取れず、分解能も 1/4 になる)。

グリッチフィルタ(1µs)で機械接点のチャタリングを除きます。割り込みは使わず、カウンタはハードウェア(PCNT)が常時進めるので、`update()` を高頻度に呼ばなくても回転は取りこぼしません(反映が遅れるだけ)。

### デテント(ノッチ)への畳み込み

x4 なので生カウントは「回した分 × 4」で増えます。ご指摘のとおり **1 ノッチで複数カウント**出ます。これを `pulsesPerDetent_`(既定 4)で割ってデテント数に正規化します。

- EC11 など一般的なクリック(検知)付きエンコーダは、**1 デテント(ノッチ)= 1 直交周期 = 4 カウント**。→ 4 ÷ 4 = **1 デテント/ノッチ**。`mapToKeys` は 1 タップ、`mapToAxis` は ±1。
- 割った**余りはアキュムレータに持ち越す**ので、途中で切り捨てられず**累積ズレ(ドリフト)は起きません**。`update()` の間隔が空いて複数ノッチ分溜まっても、まとめて正しいデテント数に畳まれます。

### 他の型・逆回転への調整

「1 ノッチあたりのパルス数」はエンコーダの型で違うので、実機で回して合わせます。

- 1 ノッチで **2 出る** → デテントがハーフ周期に来るタイプ。`setPulsesPerDetent(2)`。
- 想定より多い/少ない → データシートの detents/revolution と cycles(pulses)/revolution の比を確認して `setPulsesPerDetent(n)`。
- **逆回転**している → `mapToKeys(cw, ccw)` の引数を入れ替えるか、`mapToAxis` なら `config.setAxisScale(axis, -1)` で反転(NaturalScroll の例と同じ)。

### 台数の上限

1 アダプタ = 1 エンコーダ = 1 PCNT ユニット。チップの PCNT ユニット数まで並べられます(ESP32-S3 は 4 ユニット)。`SOC_PCNT_SUPPORTED` を持たないチップではこのアダプタはコンパイルされません。

## 追加依存ライブラリ

core(`ESP32KeyBridge.h`)は依存ゼロ(純粋 C++)です。アダプタヘッダは**ヘッダのみ**なので、include したアダプタの分だけ依存ライブラリが必要になります(include しなければ不要)。

| アダプタヘッダ | 依存ライブラリ | 入手 | sketch.yaml での指定例 |
|---|---|---|---|
| `ESP32KeyBridgeEspUsbHost.h` | [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost) 2.6.0 以上(`onKeyboardState`、report-ID/NKRO キーボードへの LED 送信、および 2.6.0 の `addDeviceDisconnectedListener()` / `addMidiMessageListener()` = アダプタが単一スロット callback を占有しないための multi-listener API) | Arduino Library Manager | `- EspUsbHost` |
| `ESP32KeyBridgeEspUsbDevice.h` | [EspUsbDevice](https://github.com/tanakamasayuki/EspUsbDevice) 2.0.0 以上(`ledState()`) | Arduino Library Manager | `- EspUsbDevice` |
| `ESP32KeyBridgeEspBle.h` | [EspBle](https://github.com/tanakamasayuki/EspBle) 1.2.0 以上 | Arduino Library Manager | `- EspBle` |
| `ESP32KeyBridgeEspBleClassic.h` | EspBle 1.3.0 以上(**無印 ESP32 + arduino-esp32 3.2.0 以上のみ**) | Arduino Library Manager | `- EspBle (1.3.0)` |
| `ESP32KeyBridgeGpio.h` | なし(Arduino GPIO のみ) | - | - |
| `ESP32KeyBridgeSerial.h` | なし(Arduino `Print` のみ) | - | - |

- ボードサポートは arduino-esp32(examples は 3.3.10 の profile を同梱。ESP32-P4 の USB Device は HS 固定などの制約は [SwapCtrlCapsLock の README](../examples/practical/SwapCtrlCapsLock/README.ja.md) を参照)。
- リポジトリを隣に checkout して開発する場合は、sketch.yaml の `libraries:` に `- dir: ../../../../EspUsbHost` のようなディレクトリ参照も使えます。

## 共通規則(全アダプタ)

- スタック(EspUsbHost / EspUsbDevice 等)は**スケッチが所有**し、スタック自身の config で起動する。アダプタはコンストラクタで参照を受けるだけで **`begin()` を持たない**(購読は初回 `update()`。USB device の interface 登録のみアダプタ構築時)。
- 時間・割り込み・タスクはアダプタ内部に閉じ、`update()` の呼び出しコンテキストへスレッドセーフに引き渡す([CORE_DESIGN.ja.md](CORE_DESIGN.ja.md) の「時間・並行性の境界」)。
- BLE / USB の**購読は multi-listener API(`add*Listener()`)で行う**ため、スケッチ側も `onKeyboardState` / `onMouse` / `onDeviceDisconnected` 等を使えます。Classic HID Host は EspBle 1.3.0 時点で単一スロット callback のため、Classic keyboard / mouse 入力アダプタは対応する `onKeyboardState()` / `onMouse()` を所有します。
  - **キーボード LED の output report** (`onOutputReport`)も単一スロットなので、各 HID 出力アダプタが所有する。代わりに `ledState()` をポーリングする。
- **コールバックの実行コンテキストはライブラリごとに違う**。EspUsbHost / EspUsbDevice は自前のタスクから呼ぶので、アダプタは critical section で `update()` のコンテキストへ渡す。EspBle / EspBleClassic は各 `update()` を呼んだコンテキストから配送するので、**`ble.update()` / `bluetooth.update()` を `bridge.update()` と同じコンテキストで先に呼ぶ**。
- 出力アダプタは表現できるものだけを出し、他は黙って捨てる(virtual キーは出力に出ない)。
