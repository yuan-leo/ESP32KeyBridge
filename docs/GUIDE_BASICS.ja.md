# ESP32KeyBridge 入門ガイド

> English: [GUIDE_BASICS.md](GUIDE_BASICS.md)

ESP32KeyBridge を初めて使う人が、**何が起きているのか**を理解するための資料です。用語はすべてこの文書内で説明します。

実際のコードは各 example にあります。この文書は概念に集中し、対応する example へのリンクを示します。

このライブラリは USB Host / USB Device / BLE / GPIO という**性質の違う複数の入出力**を 1 つのキーモデルへ束ねるものなので、5 章と 6 章でインタフェースごとの特徴と制約をまとめます。

---

## 1. ESP32KeyBridge とは

### 1.1 間に挟む装置を作るライブラリ

ESP32KeyBridge は、ESP32 を**キーボードと、それを読む機器の間に挟む**ためのライブラリです。

```text
[USB キーボード] --(USB)--> [ESP32] --(USB)--> [PC]
                             ↑ここで変換する
```

PC から見えるのは普通のキーボードです。**PC にドライバも常駐ソフトも要らず、キーボード側の設定も変えません。** 「CapsLock を Ctrl にしたい」「US 配列を日本語設定の PC で刻印どおり打ちたい」「Bluetooth キーボードを BIOS 画面で使いたい」といった要求を、どちらの機器にも手を入れずに解決するのがこのライブラリの立ち位置です。

挟む相手は USB に限りません。入力は USB / BLE / GPIO のいずれでもよく、出力も USB / BLE / シリアルのいずれでもよく、しかも**入力を複数まとめて、出力を複数へ同時に**出せます。

| 入力になるもの | 出力になるもの |
|---|---|
| USB キーボード・マウス・ゲームパッド・MIDI | USB HID デバイス(PC から見て普通のキーボード) |
| BLE キーボード・マウス・ゲームパッド | BLE HID デバイス(PC から見て Bluetooth キーボード) |
| GPIO の単独スイッチ、キーマトリクス、ロータリーエンコーダ | シリアル(文字列として書き出す・デバッグログ) |
| 自作アダプタ(タッチ、I2C エキスパンダ、ネットワーク…) | 自作アダプタ |

### 1.2 3 つの層

コードを読む前に、3 つの層があることを押さえてください。

| 層 | 担当 | 実体 |
|---|---|---|
| **入力アダプタ** | ハードウェアから読み、**共通のキーモデルへ正規化する** | `EspUsbHostKeyboardInputAdapter` など |
| **core(ブリッジ)** | 統合・変換。ハードウェアを一切知らない | `ESP32KeyBridge` クラス |
| **出力アダプタ** | 変換結果を、自分が表現できる形で送り出す | `EspUsbDeviceHidOutputAdapter` など |

core は **Arduino にも ESP-IDF にもハードウェアにも依存しない**移植可能な C++ です。時刻を読まず、動的確保をせず、例外も RTTI も使いません。プラットフォーム依存はすべてアダプタ層にあります。

この分離には実利があります。変換ロジック全体を**ホスト PC 上の unit test で固定できる**こと、そして新しいハードウェアへの対応が「アダプタを 1 つ書く」で済むことです。

### 1.3 大原則 1 — すべては `update()` から動く

**`begin()` はありません。** 登録も設定も状態を置くだけで、実際に何かが起きるのは `loop()` の中で呼ぶ `bridge.update()` からです。

```cpp
void loop() {
  bridge.update();  // 入力の読み取り・変換・出力はすべてここから走る
  delay(1);
}
```

1 回の `update()` で次の全部が起きます。

1. 全入力アダプタの `update()` を呼ぶ
2. 各入力の押下集合を読み、前回との差分(押した/離した)を取る
3. 変換を適用して出力の押下集合を作る
4. lock 状態を更新する
5. 全出力アダプタへ書き出す

したがって `addInput()` / `addOutput()`、`applyConfig()`、アダプタの起動に**順序の制約はありません**。どの順で書いても、最初の `update()` から全部が動き始めます。

裏を返せば、**`update()` を呼び忘れると何も起きません**。

### 1.4 大原則 2 — キーは「種別 + 値」

このライブラリの中では、キーもボタンも 1 つの型 `Key` で表されます。中身は **(種別, 値)** の組です。

| 種別 (`KeyKind`) | 値の意味 | 例 |
|---|---|---|
| `Keyboard` | HID keyboard page(0x07)の Usage ID **と数値が同じ** | A=0x04、Enter=0x28、LeftCtrl=0xE0 |
| `Consumer` | HID consumer page(0x0C)の Usage ID と数値が同じ | 音量 Up=0x00E9、Play/Pause=0x00CD |
| `MouseButton` | ボタン番号(1 始まり) | 左=1、右=2、中=3 |
| `Virtual` | **ブリッジ内部専用**。出力へは決して出ない | レイヤーのトリガ、マクロのトリガ |

スケッチでは enum を使うので、`Key` を意識せずに書けます。

```cpp
esp32keybridge::KeyboardUsage::CapsLock   // キーボードのキー
esp32keybridge::ConsumerUsage::VolumeIncrement  // メディアキー
esp32keybridge::MouseUsage::Middle        // マウスの中ボタン
esp32keybridge::VirtualUsage::V1          // 内部専用キー
```

ここが効いてくるのは**種別をまたぐリマップ**です。「F13 を音量アップに」「ゲームパッドのボタンを Escape に」「マウスのサイドボタンをキーに」は、どれも**テーブル 1 行**であって特別扱いではありません。

```cpp
config.global.remap(esp32keybridge::KeyboardUsage::F13,
                    esp32keybridge::ConsumerUsage::VolumeIncrement);
```

もう 1 つ重要な性質があります。**`Keyboard` 種別の値は「文字」ではなく「キーの物理的な位置」です。** JIS キーボードでも US キーボードでも、A の位置のキーは同じ 0x04 を送ります。刻印をどう解釈するかは PC 側の設定の仕事です。だから「US 配列を日本語設定の PC に挿すと記号がずれる」という現象が起きるわけで、それを解決するのが 3.4 節のレイアウト変換です。

### 1.5 大原則 3 — キーは押した瞬間に確定する

キーは**押下時に一度だけ**変換され、その結果を離すまで保持します。押している最中にレイヤーが変わっても、`applyConfig()` で設定を入れ替えても、**押したものと違うキーが release されることはありません。**

これは stuck key(押しっぱなしになるキー)を構造的に防ぐための規則です。もし release のたびに変換をやり直すと、「Fn を押しながら H を押す → 左矢印が押される → 先に Fn を離す → H を離す → 左矢印は押されたまま」という事故が起きます。

同じ理由で、**入力が切断されたらその入力のキーは全部解放されます**。BLE キーボードの電池が切れても、修飾キーが押されたままになりません。

### 1.6 スケッチの 3 段構成

どの example も次の 3 段でできています。

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
  // 1) ハードウェアを起動する(スタックはアダプタではなくスケッチの所有物)
  usbHost.begin();
  usbDevice.begin();

  // 2) ブリッジを配線する
  bridge.addInput(keyboard);
  bridge.addOutput(pc);

  // 3) 設定を組んで適用する
  esp32keybridge::ESP32KeyBridgeConfig config;
  config.global.remap(esp32keybridge::KeyboardUsage::CapsLock,
                      esp32keybridge::KeyboardUsage::LeftCtrl);
  config.global.remap(esp32keybridge::KeyboardUsage::LeftCtrl,
                      esp32keybridge::KeyboardUsage::CapsLock);
  bridge.applyConfig(config);
}

void loop() {
  bridge.update();
  delay(1);
}
```

覚えておくべき所有関係が 1 つあります。**USB や BLE のスタック(`EspUsbHost` / `EspUsbDevice` / `EspBle`)はスケッチが所有し、スケッチが起動します。** アダプタはその参照を受け取るだけで、自分では `begin()` を持ちません。ポート選択・VID/PID・BLE のデバイス名やセキュリティは、アダプタではなく**スタック側の config** で指定します。

---

## 2. データの流れ

### 2.1 パイプライン

```text
物理入力
  → 入力アダプタ       キー・文字・相対値を生成。入力ごとの押下集合を持つ
  → 入力ごとの変換     per-input remap / 無効化、(opt-in)レイアウト変換
  → 統合               押下集合は和集合、単発イベントは到着順で合流
  → 全体変換           global remap / レイヤー / マクロ / 無効化
  → 出力の縁           文字→打鍵の展開、相対値の合算
  → 出力アダプタ       表現できるものを report にして送信

逆方向:
  出力アダプタ(lock 報告)→ 内部 LockState → 全入力アダプタ(LED 等へ)
```

### 2.2 流れるものは 2 種類しかない

中間表現を流れるものは、次の 2 つに分類されます。**新しい入力や出力を考えるときは、まずこの分類を決めれば処理規則が機械的に決まります。**

| | **継続(状態系)** | **単発(イベント系)** |
|---|---|---|
| 意味 | 「押している間」に意味がある | 発生した瞬間がすべて。1 回消費して終わり |
| 表現 | 押下集合(スナップショット) | 順序付きの列(キュー) |
| 統合 | 和集合 | 到着順に合流 |
| 切断時 | クリアして全 release | 未消化分を捨てる |
| 例 | キーボードのキー、修飾キー、メディアキー、マウスボタン | 文字、マウスの移動・ホイール |

注意が要るのは 2 つです。

- **メディアキー(Consumer)は継続系です。** 音量キーのように押しっぱなしで効くか、Play/Pause のようにエッジで効くかは**PC 側の解釈**であって、ブリッジは関知しません(Enter と CapsLock の違いと同じ態度です)。
- **マウスはボタンが継続系、移動とホイールが単発系(相対値)です。**

### 2.3 統合 — 和集合と、切断の扱い

複数の入力を登録すると、押下集合は**和集合**になります。

- あるキーの release が成立するのは、**そのキーを持っている全入力が離したとき**です
- ある入力が切断されたら、**その入力の分だけ**が解放されます

「USB キーボード + GPIO の追加ボタン」を 1 つのキーボードとして PC に見せる(`MergeKeyboards`)のはこの性質です。

上限は次のとおりで、いずれも固定長です(core は動的確保をしないため)。

| 対象 | 上限 |
|---|---|
| 入力アダプタ | 8 |
| 出力アダプタ | 4 |
| 同時押下(統合後の押下集合) | 32 キー |
| 保持中の押下(入力ごとの追跡) | 64 |

### 2.4 per-input と global の違い

変換には**入力ごと**のものと**統合後**のものがあります。意味が違います。

| | per-input(`InputConfig`) | global(`config.global`) |
|---|---|---|
| 効く範囲 | 束ねた入力から来たキーだけ | どの入力から来ても |
| 使うもの | remap / 無効化 / **レイアウト変換** | remap / 無効化 |
| 典型例 | 「このキーボードだけ US 刻印として扱う」 | 「全体で CapsLock を Ctrl に」 |

レイアウト変換が per-input なのは、**刻印は入力デバイスの属性**だからです。

```cpp
esp32keybridge::ESP32KeyBridgeConfig config;
esp32keybridge::InputConfig &scanner = config.addInputConfig();
scanner.convertLayout(esp32keybridge::KeyboardLayout::enUs());  // この入力は US 刻印
bridge.addInput(barcode, scanner);   // 束ねる
bridge.addInput(keyboard);           // こちらは per-input 設定なし
```

`addInput()` が受け取るのはハンドルの同一性だけで、**中身が効くのは `applyConfig()` のとき**です。したがって `addInputConfig()` の中身を書くのは `addInput()` の前でも後でも構いません。

### 2.5 出力は「表現できるものだけ」を出す

出力アダプタは、自分が表現できるものを出し、**それ以外は黙って捨てます**。

- `Virtual` キーは出力へ出ません(パイプライン内で消費されるのが原則)
- マウスに対応しない出力へ相対値を渡しても、何も起きません
- 文字ストリームは、文字対応の出力(シリアル)だけが受け取ります。HID 出力は打鍵に展開された結果のほうを見ます

「エラーにならず黙って落ちる」のは意図的です。同じ設定のまま出力だけ差し替えられるようにするためで、そのぶん**意図した出力が出ないときは、まず出力アダプタが表現できる範囲を確認**してください(6 章)。

---

## 3. 変換編

設定はすべて `ESP32KeyBridgeConfig` を組み立てて `applyConfig()` に渡します。設定オブジェクトは**値のコピーが取られる**ので、渡したあとにローカル変数が消えても問題ありません。

### 3.1 リマップと無効化

```cpp
config.global.remap(from, to);
config.global.disable(key);
```

**リマップは 1 段の lookup で、連鎖しません。** `CapsLock→LeftCtrl` と `LeftCtrl→CapsLock` の 2 行を書けば、それは「入れ替え」として成立します(連鎖したら無限ループか、片方向だけになってしまいます)。

上限は remap 32 件、無効化 32 件です(per-input も同じ枠を各自持ちます)。

### 3.2 レイヤー — Fn キーの作り方

**物理的な Fn キーは HID に現れません。** キーボードの内部で処理されてしまうので、観測も共有もリマップもできません。これは原理的な制約です。

そこでブリッジでは「HID に飛んでくる何かのキーを `Virtual` キーへ remap して消費し、それを押している間だけ置換表を重ねる」という形で Fn 相当を作ります。

```cpp
constexpr esp32keybridge::Key kFn = esp32keybridge::virtualKey(1);

config.global.remap(esp32keybridge::KeyboardUsage::CapsLock, kFn);  // CapsLock を Fn に
esp32keybridge::LayerConfig &nav = config.addLayer(kFn);
nav.remap(esp32keybridge::KeyboardUsage::H, esp32keybridge::KeyboardUsage::Left);
nav.remap(esp32keybridge::KeyboardUsage::J, esp32keybridge::KeyboardUsage::Down);
nav.remap(esp32keybridge::KeyboardUsage::K, esp32keybridge::KeyboardUsage::Up);
nav.remap(esp32keybridge::KeyboardUsage::L, esp32keybridge::KeyboardUsage::Right);
```

- **トリガキー自身は消費され、出力には出ません**
- レイヤーは統合後に効くので、既定で**全入力に効きます**(分割キーボードやフットペダルの Fn が成立するのはこのため)
- 1.5 節のとおり、置換結果は press 時に確定します。**レイヤーを解除してから離しても、離れるのは矢印キーのほうです**
- 上限はレイヤー 4 つ、1 レイヤーにつき remap 16 件

自作キーボードなら、そもそもアダプタの割り当てで直接 `Virtual` キーを産めます。

なお **tap/hold(短押しで元のキー、長押しで Fn)は現バージョンの対象外**です。時間依存の判定になり、core が時刻を持たない方針と衝突するためです。

### 3.3 文字列 — マクロと `typeText()`

キーではなく**文字**を流す経路が別にあります。

```cpp
config.textMacro(esp32keybridge::VirtualUsage::V2, "user@example.com");  // マクロ
bridge.typeText("Hello, world!\n");                                      // 直接投入
```

文字は**文字のまま出力の縁まで運ばれ**、そこで `config.hostLayout`(PC 側のレイアウト)を使って打鍵列へ展開されます。この分離のおかげで、**マクロの定義が出力先のレイアウトに依存しません**。同じマクロが US 設定の PC でも日本語設定の PC でも正しく打てます。

展開時には次の面倒が自動で処理されます。

- 必要な Shift の合成(`@` を打つのに Shift が要るかはレイアウト次第)
- **CapsLock 補正**(内部シャドウの lock 状態を見て打ち分ける)
- **ユーザーが押している修飾キーの一時退避と復元**(文字 1 個分の枠はアトミックで、間に他が割り込みません)
- 数字は keypad ではなく主キー列へ展開(NumLock に依存しない)

打鍵できない文字(かな漢字、絵文字、そのレイアウトに無い記号)は**読み捨てられ、`textEncodeFailCount()` に加算**されます。読み捨てになった文字も、文字対応の出力(シリアル)へは届きます。

制御文字は 5 つだけ意味を持ちます。

| 文字 | 打鍵 |
|---|---|
| 0x09 TAB | Tab |
| 0x0A LF / 0x0D CR | Enter(CRLF は Enter 1 回) |
| 0x08 BS | Backspace |
| 0x1B ESC | Escape |

**エスケープの解釈はしません。** `"\n"` を実際の改行にするのはコンパイラの仕事で、ブリッジは実際の制御文字だけを見ます。

#### バックプレッシャ

文字キューは 64 文字の固定長です。`typeText()` は**あふれても捨てずに止まり、消費したバイト数を返します**。長い文字列は次のように流します。

```cpp
const char *p = text;
while (*p != '\0') {
  p += bridge.typeText(p);
  bridge.update();   // ここで打鍵が進み、キューが空く
}
```

`typeAvailable()` で空きを問い合わせられるので、`Serial.available()` と組み合わせた形も書けます(`SerialTextTyper`)。マクロや `typeChar()` のように再開できない経路だけは、あふれたら捨てて `textOverflowCount()` に加算します。

上限はマクロ 4 件、1 マクロ 31 文字です。

### 3.4 レイアウト変換 — US 刻印を日本語設定の PC で

1.4 節のとおり、HID が運ぶのは文字ではなくキーの位置です。US 刻印のキーボードを日本語設定の PC に挿すと、英数字は合っているのに記号だけがずれます。

レイアウト変換は、**(キー, Shift 状態) → (キー, Shift 状態)** の変換です。usage 単体の置換では足りません — US の Shift+2 は `@` ですが、ja_jp では 0x2F の単打だからです。

```cpp
esp32keybridge::InputConfig &kb = config.addInputConfig();
kb.convertLayout(esp32keybridge::KeyboardLayout::enUs());   // 入力の「刻印」
config.hostLayout = esp32keybridge::KeyboardLayout::jaJp(); // PC の「解釈」
config.layoutConversionToggle = esp32keybridge::KeyboardUsage::F12;  // 切替キー
bridge.addInput(keyboard, kb);
```

押さえるべき性質です。

- **英字・数字・矢印・修飾キーなどの恒等キーは素通し**します。変換対象は記号系だけ(US⇄JA で約 15 キー)
- **Shift は抑制も合成もされます。** 物理 Shift を押していても出力で外す、押していなくても出力で押す。押している間は維持されるので、長押しやリピートは保たれます
- **修飾コンボ(Ctrl / Alt / GUI 押下中)は既定で変換しません。** ショートカットは位置ベースで解釈されることが多いためです。通常の Dvorak preset は Dvorak の文字どおりに変換し、`_qwmod` variant は物理 QWERTY 位置へ戻します
- **on/off の切替手段は事実上必須です。** BIOS / UEFI / リカバリ環境は別の解釈をするので、変換したままだと逆に化けます。`layoutConversionToggle` にキーを割り当ててください
- preset は 20 種です。`en_us`、`ja_jp`、生成 14 locale と、Dvorak 4 variant (`dvorak` / `dvorak_qwmod` / `dvorak_iu` / `dvorak_iu_qwmod`)。取得は `KeyboardLayout::byName("dvorak_iu")`

方向によって実用度が違います。**US 刻印 → ja_jp ホストは ASCII を 100% 被覆**しますが、**ja_jp 刻印 → en_us ホストは `¥` などが打てず欠けが出ます**。

複数キーボードで刻印が混在していても、per-input 変換 → 和集合統合の順なので成立します。個別に PC へ挿す方式では OS のレイアウトが 1 つしかないため原理的に解決できません。

### 3.5 相対軸 — スケールと反転

マウスの移動・ホイール・横ホイールとエンコーダの回転は、軸 (`Axis::X` / `Y` / `Wheel` / `Pan`) の相対値として流れます。

```cpp
config.setAxisScale(esp32keybridge::Axis::Wheel, -1);  // ナチュラルスクロール
config.setAxisScale(esp32keybridge::Axis::X, 2);       // 2 倍速
```

負で反転、絶対値が整数倍率です。**ポインタ加速のような曲線変換は対象外**です(直線的な変換だけを core に置く方針)。

相対値は入力をまたいで合算され、出力側で report の値域(int8)に飽和させ、**あふれた分は次の report へ繰り越します**。速く動かしても総移動量がずれません。

### 3.6 関連する example

| example | 内容 |
|---|---|
| [SwapCtrlCapsLock](../examples/practical/SwapCtrlCapsLock/) | 2 行の remap で入れ替え。**P4 の USB ポート事情の説明はここが基準** |
| [MediaKeys](../examples/practical/MediaKeys/) | 種別をまたぐ remap(F13 → 音量) |
| [NavLayer](../examples/practical/NavLayer/) | CapsLock 長押しで HJKL が矢印になるレイヤー |
| [LayerKeypad](../examples/practical/LayerKeypad/) | 自作キーパッドで 1 キー 2 機能 |
| [UsKeyboardOnJapanesePc](../examples/practical/UsKeyboardOnJapanesePc/) | レイアウト変換 |
| [SerialTextTyper](../examples/practical/SerialTextTyper/) | `typeText()` とバックプレッシャ |
| [FootSwitch](../examples/practical/FootSwitch/) | 定型文マクロとメディア操作 |
| [NaturalScroll](../examples/practical/NaturalScroll/) | 軸の反転とボタン入れ替え |

---

## 4. lock 状態と LED

CapsLock・NumLock・ScrollLock の状態は、**キーボードではなく PC が持っています**。キーボードは「押した」と伝えるだけで、点灯の指示は PC から LED report として返ってきます。だから間に挟まる装置には、この逆方向の流れを中継する責任があります。

### 4.1 正本は 1 つ

| 状況 | 正本 | ブリッジの振る舞い |
|---|---|---|
| lock を報告できる出力(USB Device / BLE HID)が接続中 | **その PC** | 中継のみ。自分では toggle しない |
| そういう出力が無い(シリアル出力だけ、USB 未接続) | **ブリッジ自身** | 終端ホストとして Caps/Num/Scroll を自分で toggle する |

外部の正本が現れたら最初の report で上書きされ(外部の勝ち)、消えたら最終値から引き継ぎます。

lock を報告できる出力が複数登録された場合、**最初に登録されたものが正本**です。他方の PC とのズレは関知しません(禁止はしませんが整合も保証しません)。lock 報告出力は 1 つを推奨します。

### 4.2 入力の LED へ流す

ブリッジは内部に lock 状態のシャドウを 1 つ持ち、変化したら**全入力アダプタへ通知**します。あとは各アダプタが自分のやり方で反映します(USB キーボードへは LED report、BLE キーボードへは output report、自作キーボードならピン出力)。

**後から接続された入力にも、接続時点の状態が通知されます。** PC は変化したときしか report を送らないので、ブリッジが保持していないと「後から挿したキーボードの CapsLock LED だけ消えている」という状態になるためです。

かな LED (`kana`) だけは扱いが違い、**ブリッジは中継するだけで自分では toggle しません**。運べるかどうかはトランスポート次第です(4.3 節)。

なお **ズレたホストへ toggle キーを注入して合わせる**ような能動同期は行いません。

### 4.3 トランスポートごとの対応

| 経路 | num / caps / scroll | kana |
|---|---|---|
| USB Device 出力 → ブリッジ | ✅ | ✅ |
| BLE HID 出力 → ブリッジ | ✅ | ✅ |
| ブリッジ → USB Host のキーボード LED | ✅ | ❌(EspUsbHost に転送手段がない) |
| ブリッジ → BLE HID Host のキーボード LED | ✅ | ✅ |

---

## 5. 入力アダプタ編 — インタフェースごとの特徴と制約

ここからがインタフェース別の話です。**どのアダプタを使っても core から見た形は同じ**(`update()` と `keys()` だけ)ですが、その裏側の事情はまったく違います。

### 5.1 共通の約束

全アダプタに共通する規則が 3 つあります。

1. **スタックはスケッチが所有する。** アダプタはコンストラクタで参照を受けるだけで `begin()` を持ちません。購読は最初の `update()` で行われます(USB Device の interface 登録だけは例外で、アダプタの構築時)
2. **購読は multi-listener API (`add*Listener()`) で行う。** アダプタは単一スロットの `on*()` を一切奪いません。**スケッチ側は `onKeyboardState` / `onMouse` / `onDeviceDisconnected` などをこれまでどおり自由に使えます。** アダプタは自分が取った listener を破棄時に外します
   - 例外は**キーボード LED の output report** (`onOutputReport`) です。3 ライブラリとも「LED は event ではなく state」という設計で単一スロットのままなので、**出力アダプタ**がこのスロットを取ります。スケッチ側は代わりに `keyboard.ledState()` / `ble.hidKeyboard().ledState()` をポーリングしてください(値はコールバックの有無にかかわらず更新されます)
3. **時間・割り込み・タスクはアダプタの内側に閉じる。** core は単一コンテキストで動きます

### 5.2 コールバックが走るコンテキストはライブラリごとに違う

これが**このライブラリで最も間違えやすい点**です。

| スタック | コールバックの実行元 | アダプタの対処 | スケッチの義務 |
|---|---|---|---|
| **EspUsbHost / EspUsbDevice** | スタック自身のタスク | critical section 越しに `update()` のコンテキストへ渡す | なし |
| **EspBle** | **`EspBle::update()` を呼んだコンテキスト** | 同期不要 | **`ble.update()` を `bridge.update()` と同じコンテキストで呼ぶ** |

つまり BLE を使う場合、`loop()` はこう書きます。

```cpp
void loop() {
  ble.update();     // ここで BLE のイベントが配送される
  bridge.update();  // 直後に呼ぶので、届いたばかりの report が今回の周期に載る
  delay(1);
}
```

**`ble.update()` を呼ばないと BLE 入力は何も届きません。** 順序も `ble.update()` が先です。後ろに置くと、届いた入力の反映が常に 1 周期遅れます。

### 5.3 `update()` が遅れたときの取りこぼし

サンプリングの時間精度は**アダプタが担保する**のが原則です。`update()` の間隔が影響するのは「反映の遅れ」だけで、入力自体の取りこぼしは起きません — **例外が 1 つだけ**あります。

| アダプタ | サンプリング機構 | loop が長時間ブロックしたとき |
|---|---|---|
| `GpioKeyInputAdapter` | **`update()` の中で読む** | ⚠ **ブロック中より短い押下は取りこぼす** |
| `GpioMatrixInputAdapter` | 専用タスク 1kHz | 取りこぼさない |
| `RotaryEncoderInputAdapter` | PCNT ペリフェラルのハードウェア計数 | 取りこぼさない |
| USB Host 系 | EspUsbHost のタスクからコールバック | 取りこぼさない(状態スナップショット保持) |
| BLE HID Host 系 | EspBle のイベントキュー | 取りこぼさない(ただし `ble.update()` が要る) |

`GpioKeyInputAdapter` の割り切りは「ペダル・パネルボタンのような**押下時間の長い入力専用**」です。短いタップを確実に拾いたいなら `GpioMatrixInputAdapter`(1 行 1 列の 1 キーでも使えます)を選んでください。

### 5.4 USB Host 入力

ヘッダ: `ESP32KeyBridgeEspUsbHost.h` / 依存: [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost) 2.6.0 以上

| アダプタ | 台数 | できること |
|---|---|---|
| `EspUsbHostKeyboardInputAdapter` | 最大 4 台を合算 | キーボード + そのメディアキー |
| `EspUsbHostMouseInputAdapter` | 最大 4 台を合算 | ボタン・移動・ホイール・横ホイール |
| `EspUsbHostGamepadInputAdapter` | 最大 4 台を合算 | ボタンとハット/十字の写像 |
| `EspUsbHostMidiInputAdapter` | 最大 4 台 | MIDI ノートの写像 |

**キーボード**の状態は `onKeyboardState` の **256 ビットのスナップショット**として受け取ります。boot / report-ID boot / NKRO の違いも、修飾キー単独の変化も、この 1 つの形に吸収されているので、**どんなキーボードでも同じ経路**で処理されます。boot のロールオーバーエラーコード(usage 0x01〜0x03)は除外されます。メディアキーは同時 8 usage まで。

**マウス**の presence は「最初の report が来たとき」に判定されます(列挙を問い合わせる API が無いため、挿しただけではまだ存在として認識されません)。横ホイール(AC Pan)は boot mouse report には無いので 0 になります。

**ゲームパッド**は、登録した写像だけが出力になります。ボタン番号はコントローラごとに違うので、**まず `LogOutputAdapter` を繋いでどの番号が来るか観察**してから決めてください。

```cpp
gamepad.mapButton(1, esp32keybridge::KeyboardUsage::Space);
gamepad.mapHat(esp32keybridge::KeyboardUsage::Up,
               esp32keybridge::KeyboardUsage::Down,
               esp32keybridge::KeyboardUsage::Left,
               esp32keybridge::KeyboardUsage::Right);  // 斜めは 2 キー同時
```

対応範囲はボタンとハットです。~~アナログスティックとトリガ~~は scope 外です。

**MIDI** は Note On(velocity > 0)で押下、Note Off(または velocity 0 の Note On)で解放です。既定では全チャンネルを聞き、`setChannel(ch)` で 1 ch に絞れます。**Control Change やピッチベンドは無視**されます。

### 5.5 BLE HID Host 入力

ヘッダ: `ESP32KeyBridgeEspBle.h` / 依存: [EspBle](https://github.com/tanakamasayuki/EspBle) 1.2.0 以上

| アダプタ | 接続数 | できること |
|---|---|---|
| `EspBleHidHostKeyboardInputAdapter` | 最大 4 接続を合算 | キーボード + そのメディアキー |
| `EspBleHidHostMouseInputAdapter` | 最大 4 接続を合算 | ボタン・移動・ホイール |
| `EspBleHidHostGamepadInputAdapter` | 最大 4 接続を合算 | ボタンとハット/十字の写像 |

キーボードの読み取り経路は USB Host 側と**同じ 256 ビットスナップショット**です。ゲームパッドの写像とハットのデコードも core の `GamepadKeyMap` を共有しているので、**USB とまったく同じ挙動**になります。

BLE 固有の事情が 4 つあります。

**1. 接続の手続きはスケッチが書く。** アダプタは「繋がった後の HID report を消費する」だけです。scan → connect → pair → `hidHost().discover()` の連鎖はスケッチ側にあります。名前やアドレスで相手を絞りたいときに自由が利くようにするためで、アダプタはブリッジへ先に登録しておいて構いません(接続が無くても害はありません)。

```cpp
ble.scanner().onResult([](const EspBleScanResult &result) {
  if (result.connectable && result.advertisesService("1812")) {  // HID Service
    ble.scanner().stop();
    ble.connect(result);
  }
});
ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
  if (event.success) {
    ble.hidHost().discover(event.connection.id);  // 暗号化の後で discover
  }
});
ble.setAutoReconnect(true);
ble.hidHost().setAutoRediscover(true);
ble.scanner().start();
```

**2. 暗号化は事実上必須。** 市販のキーボードは暗号化されていないリンクへ HID report を出しません。`bleConfig.security.enabled = true` と `bonding = true` を設定し、**`discover()` はペアリング成功後に呼びます**。暗号化前に HID の attribute へ触ると ATT のエラーになります。

**3. メディアキーは 1 接続あたり 1 usage。** HOGP の consumer report が同時に 1 usage しか運べないためです(USB Host 側は 8 usage)。同時に 2 つのメディアキーを押しても、届くのは 1 つです。

**4. かな LED を転送できる。** これは USB Host 側にはない利点です(4.3 節)。

#### 5.5.1 Bluetooth Classic HID Host 入力

ヘッダ: `ESP32KeyBridgeEspBleClassic.h` / 依存: EspBle 1.3.0 以上 / **無印 ESP32 のみ**

`EspBleClassicHidHostKeyboardInputAdapter` と `EspBleClassicHidHostMouseInputAdapter` は、Classic HID Host が Report Descriptor から復号した keyboard / mouse report を BLE 版と同じ KeySet・相対軸へ写像します。report の中身は互換ですが、接続の周辺は別物です。

- Classic HID Host は **1 接続**。keyboard + mouse 複合デバイスなら両アダプタで同じ接続を読める
- BLE の service advertisement (`0x1812`)を scan する代わりに、inquiry 等で得た **Bluetooth address を `hidHost().connect(address)` へ渡す**
- `bluetooth.begin()` と `bluetooth.hidHost().begin()` の両方が必要。切断後の再接続もスケッチが行う
- EspBle 1.3.0 の Classic Host が標準復号するのは keyboard / mouse。consumer / gamepad は raw report には届くが、このアダプタでは扱わない
- callback は単一スロットなので、keyboard アダプタが `onKeyboardState()`、mouse アダプタが `onMouse()` を所有する
- `bluetooth.update()` を `bridge.update()` と同じコンテキストで先に呼ぶ

```cpp
EspBleClassic bluetooth;
EspBleClassicHidHostKeyboardInputAdapter keyboard(bluetooth);

bluetooth.begin();
bluetooth.hidHost().begin();
bluetooth.hidHost().connect("00:11:22:33:44:55");

void loop() { bluetooth.update(); bridge.update(); }
```

Bluetooth Classic の BR/EDR 無線を持つ ESP32 SoC は**無印 ESP32 だけ**です。S3 / C3 / C6 / H2 / P4 では BLE アダプタを使います。

同じ無印 ESP32 で BLE と Classic を両方 `begin()` する dual-host mode は EspBle では実験扱いで、flash も大きく使います。`ClassicKeyboardToBle` は既定 app partition の約 97% なので、製品固有機能を足す場合は app の大きい partition scheme が必要です。

### 5.6 GPIO 入力

ヘッダ: `ESP32KeyBridgeGpio.h` / 依存: なし(Arduino GPIO と、エンコーダは PCNT)

#### `GpioKeyInputAdapter` — 単独スイッチ

```cpp
keys.addKey(4, esp32keybridge::KeyboardUsage::F13);           // 既定 activeLow + pullUp
keys.addKey(5, esp32keybridge::ConsumerUsage::PlayPause);
keys.setDebounceMillis(5);  // 既定 5ms
```

ピンの設定は登録後**最初の `update()`** で行われます。デバウンス込みで `update()` の中で読むので、5.3 節のとおり **loop レート依存**です。フットスイッチやパネルボタン向けの割り切りです。

#### `GpioMatrixInputAdapter` — キーマトリクス

最大 16 行 × 16 列。**配線とキーマップを分けて書く**のが特徴です。

```cpp
matrix.setRowPins({4, 5, 6, 7});      // 行 = スキャン出力
matrix.setColPins({15, 16, 17});      // 列 = プルアップ入力
matrix.setKeys({                       // 行優先。コードの見た目 = 物理配置
  esp32keybridge::KeyboardUsage::Keypad7, esp32keybridge::KeyboardUsage::Keypad8, esp32keybridge::KeyboardUsage::Keypad9,
  esp32keybridge::KeyboardUsage::Keypad4, esp32keybridge::KeyboardUsage::Keypad5, esp32keybridge::KeyboardUsage::Keypad6,
  esp32keybridge::KeyboardUsage::Keypad1, esp32keybridge::KeyboardUsage::Keypad2, esp32keybridge::KeyboardUsage::Keypad3,
  esp32keybridge::KeyboardUsage::Keypad0, esp32keybridge::Key(),                  esp32keybridge::KeyboardUsage::KeypadEnter,
});
```

座標や添字は出てきません。歯抜けは無効キー `Key()` を置きます。設定は**最初の `update()` より前に**済ませてください。

スキャン・デバウンス・ゴースト判定は**専用タスクで 1kHz** で走ります。loop が詰まっても短いタップを取りこぼしません。

**ダイオードは任意です。** 無い場合、ゴーストブロックが働きます — 2×2 の四角を完成させてしまう新しい押下は保留されます。四角の 4 隅はどれも同じに読めるので本物と幻を区別できず、**間違ったキーを出すくらいなら保留する**という判断です。結果として**ダイオード無しでは確実な同時押しは 2 キーまで**になります。`setHasDiodes(true)` を指定すれば制限は外れます。

#### `RotaryEncoderInputAdapter` — ロータリーエンコーダ

PCNT ペリフェラルによる**完全な x4 直交デコード**(A 相 B 相の両方、立上り立下りの両エッジ)です。割り込みを使わず、カウンタはハードウェアが常時進めるので、`update()` の頻度に関係なく回転を取りこぼしません。

```cpp
encoder.mapToKeys(esp32keybridge::ConsumerUsage::VolumeIncrement,   // 時計回り
                  esp32keybridge::ConsumerUsage::VolumeDecrement);  // 反時計回り
// または
encoder.mapToAxis(esp32keybridge::Axis::Wheel);   // 1 ノッチ = ±1
encoder.setPulsesPerDetent(4);  // 既定 4 (EC11 など一般的なクリック付き)
```

x4 なので生カウントは「回した分 × 4」になり、それを `pulsesPerDetent` で割ってノッチ数に正規化します。**割った余りは持ち越すのでドリフトしません。**

- 1 ノッチで 2 進む型 → `setPulsesPerDetent(2)`
- **逆回転する** → `mapToKeys()` の引数を入れ替えるか、`mapToAxis()` なら `config.setAxisScale(axis, -1)`
- **1 アダプタ = 1 エンコーダ = 1 PCNT ユニット**(ESP32-S3 は 4 ユニット)。`SOC_PCNT_SUPPORTED` が無いチップではコンパイルされません
- 押し込みスイッチは別扱いで、`GpioKeyInputAdapter` のキーとして登録します

### 5.7 自作の入力アダプタ

入力アダプタの実体は **`update()` と `keys()` だけ**です。I2C エキスパンダ、センサ、ネットワーク入力を書くのも 1 ページ程度で済みます。

```cpp
class MyInput : public esp32keybridge::InputAdapter {
public:
  void update() override { /* ハードを読んで keys_ を更新する */ }
  const esp32keybridge::KeySet &keys() const override { return keys_; }
  // 任意: connected() / setLockState() / takeAxisDelta()
private:
  esp32keybridge::KeySet keys_;
};
```

`keys()` が返すのは**論理押下**であって、物理スイッチとの 1:1 対応は要求されません。トグル(踏むたびに Shift の press/release を交互に出す)、ワンショット、エンコーダのタップ合成などは、アダプタが押下を合成して表現します。

切断を検出したら `connected()` を `false` にしてください。**押下集合のクリアは core がやります。**

実例は [TouchButtons](../examples/practical/TouchButtons/)(ESP32-S3 内蔵の静電容量タッチ)、最小のリファレンスは `ManualInputAdapter` です。

### 5.8 入力アダプタ早見表

| アダプタ | 依存 | 台数 | 取りこぼし | 固有の制約 |
|---|---|---|---|---|
| `EspUsbHostKeyboardInputAdapter` | EspUsbHost | 4 | しない | メディアキー同時 8。kana LED は送れない |
| `EspUsbHostMouseInputAdapter` | EspUsbHost | 4 | しない | presence は最初の report から |
| `EspUsbHostGamepadInputAdapter` | EspUsbHost | 4 | しない | アナログ軸は scope 外 |
| `EspUsbHostMidiInputAdapter` | EspUsbHost | 4 | しない | Note On/Off のみ |
| `EspBleHidHostKeyboardInputAdapter` | EspBle | 4 接続 | しない* | メディアキーは 1 接続 1 usage。暗号化が事実上必須 |
| `EspBleHidHostMouseInputAdapter` | EspBle | 4 接続 | しない* | 横ホイールなし |
| `EspBleHidHostGamepadInputAdapter` | EspBle | 4 接続 | しない* | アナログ軸は scope 外 |
| `EspBleClassicHidHostKeyboardInputAdapter` | EspBle 1.3.0 | 1 接続 | しない** | 無印 ESP32 のみ。consumer 入力は未復号 |
| `EspBleClassicHidHostMouseInputAdapter` | EspBle 1.3.0 | 1 接続 | しない** | 無印 ESP32 のみ。横ホイールなし |
| `GpioKeyInputAdapter` | なし | ピン数まで | ⚠ **する** | 押下の長い入力専用 |
| `GpioMatrixInputAdapter` | なし | 16×16 | しない | ダイオード無しは同時 2 キーまで |
| `RotaryEncoderInputAdapter` | PCNT | 1/アダプタ | しない | PCNT ユニット数まで |

\* `ble.update()` を呼んでいる限り。

\** `bluetooth.update()` を呼んでいる限り。

---

## 6. 出力アダプタ編

### 6.1 USB Device HID 出力

ヘッダ: `ESP32KeyBridgeEspUsbDevice.h` / 依存: [EspUsbDevice](https://github.com/tanakamasayuki/EspUsbDevice) 2.0.0 以上

PC からは**常に複合 HID デバイス(keyboard + consumer + mouse)**として見えます。

| report | 形式 |
|---|---|
| keyboard | boot 6KRO(修飾 1 バイト + キー 6 個) |
| consumer | 16 ビット usage × 1 |
| mouse | ボタン 8 + X / Y / Wheel(int8 飽和 + 繰り越し。**Pan は非対応で破棄**) |

**キーボード単体版は用意していません。** 使わない interface は report を送らないだけで USB 的に無害だからです。keyboard interface は boot protocol に対応しているので、BIOS / UEFI でも動きます。

制約が 2 つあります。

- **interface 構成は `usbDevice.begin()` で確定**し、以後変更できません。したがって**出力アダプタは `usbDevice.begin()` より先に構築**してください(構築時に interface を登録するため)。設定にマウスやメディアキーを後から足しても、descriptor は最初から揃っているので再列挙は起きません
- **同時に送れるメディアキーは 1 つ**です(consumer report が 16 ビット usage を 1 つしか持たないため)

各 report は変化したときだけ送られ、busy で送れなかったときは次の `update()` で再送されます。VID / PID / 製品名は `EspUsbDeviceConfig` で指定します(スタック側の設定)。

**LED output report を受信するので、この出力は lock の正本になれます**(4 章)。

### 6.2 BLE HID 出力

ヘッダ: `ESP32KeyBridgeEspBle.h` / 依存: EspBle 1.3.0 以上

考え方は USB Device 出力と同じで、keyboard + consumer + mouse を 1 つの HID service に載せます。**report の中身は USB Device 出力と同一**なので、出力を差し替えても、両方同時に登録しても、キーの見え方は変わりません。

相違点は 2 つだけです。

- HID service の構成は **`ble.begin()` で確定**します。したがって出力アダプタは `ble.begin()` より先に構築します(USB の `usbDevice.begin()` に対応)
- **`connected()` の意味が違います。** USB の「mount 済み」に対し、BLE は「接続 + 暗号化済み + input report を subscribe 済み」です。HOGP ではホストが subscribe するまで notification を出せないので、**ペアリング直後の一瞬は `false` になります**

アドバタイズはスケッチが管理します。切断後に再開したいなら明示的に書きます。

```cpp
ble.advertising().setName(bleConfig.deviceName);
ble.advertising().start();
ble.onDisconnected([](const EspBleConnection &) { ble.advertising().start(); });
```

こちらも LED output report を受け取るので、lock の正本になれます(かな含む)。

#### 6.2.1 キーボードから bond 済み BLE 接続先を切り替える

physical keyboard 入力を `BluetoothHotkeyInputAdapter` で包み、BLE 出力と並べて
`EspBleHidTargetController` を置きます。

```cpp
constexpr char bluetoothName[] = "KeyBridge Keyboard";
EspUsbHostKeyboardInputAdapter rawKeyboard(usbHost);
EspBleHidTargetController targets(ble, bluetoothName);
BluetoothHotkeyInputAdapter keyboard(rawKeyboard, targets);
EspBleHidOutputAdapter output(ble);

bridge.addInput(keyboard);
bridge.addOutput(output);

// ble.begin() 後、最初に選択されている slot 1 を advertise する。
targets.applySelectedSlotName();
ble.advertising().start();
```

Scroll Lock を押しながら上段 `1`、`2`、`3` を押すと、その bond slot へ再接続します。
Scroll Lock + `0` は選択中 slot の bond を削除・入れ替えて通常の pairing advertising を始めます。
それ以外の Scroll Lock chord は素通しし、Scroll Lock 単独 tap も通常の tap として出力します。
状態表示では Scroll Lock LED だけを一時的に上書きし、Caps / Num / Kana と本来の Scroll Lock
状態を保存して終了後に復元します。切断・再接続時の advertising は
`EspBleHidTargetController` が所有するため、無条件に advertising を始める
`onDisconnected(... advertising().start())` callback は併用しません。
選択 slot は `KeyBridge Keyboard 1`、`2`、`3` として advertise されます。
host が古い名前を cache している場合は、device の削除と再 pairing まで更新されないことがあります。

#### 6.2.2 Bluetooth Classic HID 出力

ヘッダ: `ESP32KeyBridgeEspBleClassic.h` / 依存: EspBle 1.3.0 以上 / **無印 ESP32 のみ**

`EspBleClassicHidOutputAdapter` は BLE 版と同じ keyboard 6KRO + consumer + mouse report を Bluetooth Classic HID Device として送ります。`EspBleClassic` をスケッチが所有し、アダプタを `bluetooth.begin()` より先に構築する点も同型です。

Classic 固有の違いは、Report Descriptor と `EspBleClassicHidProfileConfig` の名前・説明・provider 文字列が 1 個の SDP record を共有し、合計 **214 byte** までであることです。既定文字列なら 3 profile は収まります。長い文字列で超過すると `bluetooth.begin()` が失敗します。

Classic HID Host は 1 台で、BLE の advertising / GATT subscription はありません。Host 側の Bluetooth 設定から pairing します。相手が BLE HID を受け付けるなら、対応 SoC が広く省電力な BLE を優先してください。

### 6.3 シリアル出力

ヘッダ: `ESP32KeyBridgeSerial.h` / 依存: なし(Arduino の `Print` のみ)

#### `TextOutputAdapter` — 文字として書き出す

**HID report ではなく文字を出す**出力です。文字ストリーム(`writeText()`)だけを消費し、UTF-8 で `Print`(Serial / Serial1 / SoftwareSerial…)へ書きます。

文字の源は 2 つです。

1. `typeText()` / `typeChar()` / マクロで投入した文字
2. **`convertLayout()` を設定した入力の押下**(刻印でデコードした文字。Enter → 改行、Tab → タブは core が付与)

2 番目が効くのが `BarcodeToText` です。USB HID のバーコードリーダーは「キーボードとして文字を打つ」機器なので、その打鍵を刻印レイアウトでデコードすれば文字列に戻せます。

**この出力自身はレイアウトを持ちません。** 何の文字になるかは入力側の `convertLayout()` の 1 箇所で決まります。

#### `LogOutputAdapter` — デバッグ監視

押下集合の変化 (`KEYS ...`)、タイプされた文字 (`TEXT ...`)、相対軸 (`AXIS ...`) を人間が読める形で出します。実出力と**併設**して何が出ているか見る、あるいは単独で入力ハードのブリングアップに使います。**lock の正本にはなりません。**

ゲームパッドのボタン番号調べ、マトリクスの配線確認、エンコーダの向き確認は、まずこれを繋ぐのが近道です。

### 6.4 出力アダプタ早見表

| アダプタ | キー | メディアキー | マウス | 文字 | lock 正本 |
|---|---|---|---|---|---|
| `EspUsbDeviceHidOutputAdapter` | 6KRO | 1 usage | ボタン 8 + X/Y/Wheel | 打鍵に展開 | ✅ |
| `EspBleHidOutputAdapter` | 6KRO | 1 usage | ボタン 8 + X/Y/Wheel | 打鍵に展開 | ✅ |
| `EspBleClassicHidOutputAdapter` | 6KRO | 1 usage | ボタン 8 + X/Y/Wheel | 打鍵に展開 | ✅ |
| `TextOutputAdapter` | — | — | — | ✅ そのまま | — |
| `LogOutputAdapter` | 表示のみ | 表示のみ | 表示のみ | 表示のみ | — |

出力側は**状態スナップショット方式**なので、`update()` が遅れても「最後の状態」が送られるだけで壊れません(タイピングやマクロの打鍵速度が `update()` 間隔に比例して遅くなるだけです)。

---

## 7. チップとポートの選び方

正式にサポートする target は **ESP32-S3 のみ**です。

| 機能 | ESP32-S3 |
|---|---|
| GPIO キー / マトリクス | ✅ |
| ロータリーエンコーダ(PCNT) | ✅ |
| USB Host 入力 | ✅ |
| USB Device 出力 | ✅ |
| BLE HID 入出力 | ✅ |
| Bluetooth Classic HID | — |

S3 の native USB controller は 1 つです。`UsbKeyboardToBle` または `BleKeyboardToUsb` のように USB を一方の役割で使い、反対側を BLE にします。USB-to-USB bridge と他の ESP32 variant は support scope 外です。

USB Host 側にはもう 1 つ電源の話があります。**キーボードへバス電源を供給できる基板**(USB-A のホストコネクタを持つもの、または USB-OTG ケーブル + 外部 5V)が必要です。

依存ライブラリは**include したアダプタの分だけ**増えます。core ヘッダ (`ESP32KeyBridge.h`) の依存はゼロです。

| ヘッダ | 依存 |
|---|---|
| `ESP32KeyBridge.h` | なし |
| `ESP32KeyBridgeEspUsbHost.h` | EspUsbHost 2.6.0 以上 |
| `ESP32KeyBridgeEspUsbDevice.h` | EspUsbDevice 2.0.0 以上 |
| `ESP32KeyBridgeEspBle.h` | EspBle 1.3.0 以上 |
| `ESP32KeyBridgeEspBleClassic.h` | EspBle 1.3.0 以上(無印 ESP32 + arduino-esp32 3.2.0 以上) |
| `ESP32KeyBridgeGpio.h` | なし |
| `ESP32KeyBridgeSerial.h` | なし |

---

## 8. うまく動かないときに見るところ

| 症状 | 疑うところ |
|---|---|
| **何も起きない** | `loop()` で `bridge.update()` を呼んでいるか |
| **BLE 入力が何も届かない** | `ble.update()` を `bridge.update()` と同じコンテキストで、かつ**先に**呼んでいるか(5.2 節) |
| **BLE 出力が送られない** | `connected()` は「接続 + 暗号化 + subscribe 済み」。ペアリング直後の一瞬は `false`(6.2 節) |
| **BLE キーボードから report が来ない** | `security.enabled = true` と、**ペアリング成功後**の `hidHost().discover()`(5.5 節) |
| **USB 出力の interface が足りない** | 出力アダプタを `usbDevice.begin()` / `ble.begin()` より**先に構築**しているか(6.1 / 6.2 節) |
| **短いボタン押下を取りこぼす** | `GpioKeyInputAdapter` は loop レート依存。`GpioMatrixInputAdapter` へ(5.3 節) |
| **マトリクスで 3 キー目が入らない** | ダイオード無しのゴーストブロック。`setHasDiodes(true)` かダイオード追加(5.6 節) |
| **エンコーダが逆・進みすぎる** | `mapToKeys()` の引数入れ替え、`setAxisScale(axis, -1)`、`setPulsesPerDetent()`(5.6 節) |
| **ゲームパッドのボタンが効かない** | 写像したものだけが出る。`LogOutputAdapter` で番号を確認(5.4 節) |
| **記号だけずれる** | レイアウト変換の刻印 (`convertLayout`) と `hostLayout` の組み合わせ(3.4 節) |
| **BIOS で記号が化ける** | レイアウト変換が効いたまま。`layoutConversionToggle` で切る(3.4 節) |
| **マクロの一部が打てない** | そのホストレイアウトに無い文字。`textEncodeFailCount()` を見る(3.3 節) |
| **長い文字列が途中で切れる** | `typeText()` の戻り値で再開しているか。`textOverflowCount()` を見る(3.3 節) |
| **メディアキーが 1 つしか効かない** | consumer report は 16 ビット usage を 1 つだけ運ぶ。仕様どおり(6.1 節) |
| **CapsLock LED が点かない** | lock 報告できる出力が接続しているか。無ければブリッジが終端ホストとして自分で toggle する(4 章) |
| **キーが押しっぱなしになる** | ブリッジ経由では構造的に起きない。`mergedOverflow()` / `outputOverflow()` を確認(あふれたキーは出力に出ないので stuck にもならない) |

数え上げ用の照会 API が揃っているので、まずこれらを表示してください。

```cpp
bridge.textOverflowCount();      // キューが埋まって捨てた文字
bridge.textEncodeFailCount();    // ホストレイアウトで打てなかった文字
bridge.layoutConvertFailCount(); // 変換できなかった押下
bridge.mergedOverflow();         // 統合の押下集合があふれた
bridge.outputOverflow();         // 変換後の押下集合があふれた
bridge.lockAuthorityPresent();   // 外部の lock 正本がいるか
```

---

## 9. 原理的にできないこと

仕様で解決できない事柄です。回避策があるものは併記します。

| # | 内容 | 補足 |
|---|---|---|
| 1 | **打てない文字がある** | 出力先レイアウトに存在しない文字は HID では生成不能(en_us ホストへの `¥` など) |
| 2 | **ショートカットの意味論を判別できない** | アプリのキー束縛が文字ベースか位置ベースかは観測できない。レイアウト変換はどちらに倒しても壊れるケースが残る |
| 3 | **修飾状態はホストに 1 つしかない** | 矛盾する Shift 要求の同時押し(`@` と `"` を同時に保持など)は表現不能 |
| 4 | **ホストの見えない状態がある** | IME のオン/オフやフォーカス中のアプリは観測不能。文字系機能は IME オフが前提 |
| 5 | **物理 Fn キーは扱えない** | キーボード内部で処理され HID に現れない。代替は 3.2 節のレイヤー |

次のリポジトリ由来の案は、合意済み scope に含まれません。

- ~~ゲームパッドのアナログスティック・トリガ~~(対応範囲はボタンとハット)
- ~~tap/hold~~
- ~~USB Device 出力の NKRO~~(対応範囲は boot 6KRO)
- ~~マウスの横ホイール(Pan)の出力~~

---

## 10. 次に読むもの

| やりたいこと | 読む文書 |
|---|---|
| 自分のハードウェアで動くスケッチを探す | [examples/README.ja.md](../examples/README.ja.md) — すべてそのまま書き込める |
| アダプタごとの詳細と実機検証状況 | [ADAPTERS.ja.md](ADAPTERS.ja.md) |
| キーモデルがなぜこの形なのか | [DATA_MODEL.ja.md](DATA_MODEL.ja.md) |
| core / アダプタの境界と並行性 | [CORE_DESIGN.ja.md](CORE_DESIGN.ja.md) |
| 自作アダプタを書く | `src/ESP32KeyBridge.h` の `InputAdapter` / `OutputAdapter` のコメント → [TouchButtons](../examples/practical/TouchButtons/) → [CORE_DESIGN.ja.md](CORE_DESIGN.ja.md) |
| なぜそう設計したのか | [DECISIONS.ja.md](DECISIONS.ja.md) |
| 文書全体の案内 | [README.ja.md](README.ja.md) |
