# 要件

> **scope について:** この文書は実装設計の履歴です。リポジトリ内の記述だけでは scope になりません。ChatGPT 上で明示的に合意されていない未実装項目は、以下で取り消し線を付けたとおり scope 外です。

## ChatGPT で明示的に追加された scope

- **Bluetooth 接続先 hotkey 切替(実装済み)**: Scroll Lock を既定 modifier として保持し、上段数字 `1` から stack / hardware の最大対応 slot までで、接続する paired Bluetooth device を選択する。`Scroll Lock + 0` で現在選択中 slot の pairing mode を開始する。Scroll Lock LED report に対応する physical keyboard では LED 点滅で切替 / pairing 状態を示し、完了後に本来の lock 状態を復元する。LED 非搭載・report 非対応 keyboard でも機能本体は動作する。
- **後続の locally hosted web interface**: keyboard layout 切替と macro を設定する。

実装状態と、まだ合意されていない詳細は [SCOPE.md](SCOPE.md) を正本とします。

`ESP32KeyBridge` は、ESP32 シリーズを用いて様々な入力デバイスから取得したキーボード系入力を共通形式で処理し、任意の出力デバイスへ変換するための Arduino 向けライブラリです。

## 目的

本ライブラリは、個々のデバイス制御ではなく「入力 adapter」「入力ごとの処理」「入力統合」「全体変換」「出力 adapter」を分離し、ESP32 シリーズで共通に利用できるキーボード入力変換エンジンを提供します。

core は特定の入出力方式、設定形式、保存先、設定 UI に依存しません。USB Host / USB Device は入出力 adapter の一種であり、USB adapter の実装では `EspUsbHost` や `EspUsbDevice` を利用できます。

代表的なユースケースは、USB HID keyboard、GPIO matrix、barcode reader などの入力を受け取り、キーマップ、レイヤー、キー入れ替え、入力統合を適用して USB HID keyboard や BLE HID などへ出力する構成です。~~WebSerial からの設定変更や USB HID + CDC 複合デバイスのリファレンス実装~~は scope 外です。

## 対応プラットフォーム

- ESP32-S3
- ~~ESP32 / S2 / C3 / C6 / H2 / P4 および将来の variant~~ — scope 外・非サポート

正式サポートと検証対象は ESP32-S3 のみです。

## ライブラリの役割

- 入力デバイスからキーイベントを取得する。
- 入力ごとの表現を共通キーイベント(種別 + 値)へ変換する。
- キーコード、修飾キー、レイヤー、マクロなどを変換する。
- 物理 keyboard の刻印 layout と host OS の layout 設定が異なる構成で、記号を含めて刻印どおりに入力できるようにする(レイアウト変換。例: US 刻印 keyboard を日本語設定の host で使う)。
- barcode reader、serial、macro などの文字由来入力を、出力先 layout に応じた打鍵列として送信する。
- Caps Lock / Num Lock などの lock 状態を「正本 = 解釈する側」の原則で扱い、host からの LED report を全入力デバイスへ転送する。正本不在時はブリッジが終端ホストとして管理する。
- マウス(ボタン + 相対移動・ホイール)を入力・変換・出力する。
- 変換済みイベントを出力デバイスへ送信する。
- C++ コードで構築した設定、または外部で読み込んだ設定 object を適用し、キーマップやレイヤーを切り替える。

ハードウェア依存部分は adapter 層へ分離します。

## 中間データモデル

キー・ボタンの同一性は「種別 + 値」で表し(keyboard / consumer / mouse_button / virtual)、HID 由来の種別は Usage ID と数値恒等にします。トラフィックは「継続(状態系: 押下集合)」と「単発(イベント系: 文字・キー操作指示・相対値)」の 2 大分類で扱います。パススルーとリマップは変換テーブル不要で、レイアウト変換と文字列打鍵だけが変換表を必要とします。詳細は [DATA_MODEL.ja.md](DATA_MODEL.ja.md) を参照してください。

## 入力インターフェース

GPIO:

- 単体スイッチ
- 行列キーボード
- キーパッド
- ロータリーエンコーダ
- フットスイッチ
- ~~抵抗ラダー式キー~~ — scope 外

USB Host:

- USB HID Keyboard
- USB Numeric Keypad
- USB Barcode Scanner
- USB Consumer Control
- USB Gamepad

Bluetooth:

- BLE Keyboard
- BLE Consumer Device
- BLE Remote
- Bluetooth Classic HID Keyboard
- ~~Bluetooth Classic HID Consumer Device 入力~~ — scope 外

~~UART / Serial イベント入力(scope 外):~~

- ~~外部マイコンからのキーイベント~~
- ~~独自プロトコル~~
- ~~デバッグ入力~~

~~I2C 入力(scope 外):~~

- ~~GPIO Expander~~
- ~~キーパッドユニット~~
- ~~タッチ入力デバイス~~

~~SPI 入力(scope 外):~~

- ~~GPIO Expander~~
- ~~キーボードコントローラ~~

~~ネットワーク入力(scope 外):~~

- ~~ESP-NOW~~
- ~~TCP~~
- ~~UDP~~
- ~~WebSocket~~

~~赤外線入力(scope 外):~~

- ~~IR Remote~~
- ~~学習リモコン~~
- ~~Consumer Control~~

## 出力インターフェース

USB Device:

- HID Keyboard
- Consumer Control
- Mouse
- ~~CDC Serial for reference configuration examples~~ — scope 外

Bluetooth:

- BLE HID Keyboard
- BLE HID Consumer Control
- Bluetooth Classic HID Keyboard

UART:

- ~~シリアルイベント出力~~ — scope 外(実装済みの文字・ログ出力とは別)
- デバッグ出力

~~ネットワーク出力(scope 外):~~

- ~~ESP-NOW~~
- ~~TCP~~
- ~~UDP~~
- ~~MQTT~~

~~GPIO 出力(scope 外):~~

- ~~LED~~
- ~~ブザー~~
- ~~リレー~~
- ~~外部マイコン制御~~

## キー変換機能

- キーコード変換(1 段 remap、種別またぎ可)
- キー入れ替え、キー無効化
- モーメンタリレイヤー(virtual キートリガ、press 時確定)
- マクロ実行(文字列タイプ macro)。~~キーシーケンス macro~~ は scope 外
- キーボードのレイアウト変換((キー, Shift) 変換表 + Shift 抑制・合成、on/off 切替付き)
- キーボードレイアウト記述(`en_us` / `ja_jp` を最初に、基本的な言語を同梱)とユーザー定義表
- lock 状態管理(正本 = アクティブな lock 報告出力、正本不在時は終端ホストモード、全入力へ LED 転送)
- Consumer Key 変換
- マウスボタン変換、相対軸の反転・倍率
- 独自キー定義(virtual 種別)

~~ワンショット・トグル・tap/hold などの core 機能~~は scope 外です。必要なら利用側アダプタが論理押下として合成します。

## 設定方針

キーマップやレイヤーは、スケッチ内で決め打ちする使い方と、外部から読み込んだ設定 object として適用する使い方の両方を許容します。

core は設定の取得方法、転送手段、保存先、ファイル形式、UI を規定しません。~~WebSerial、CDC serial、NVS、LittleFS のリファレンス実装~~は scope 外です。

設定対象:

- キーマップ
- レイヤー
- マクロ
- Bluetooth 情報
- USB VID / PID / product / manufacturer / serial などの USB 情報
- デバイス名
- 入力 adapter / 出力 adapter の有効化
- 永続設定

~~リファレンス実装の設定転送案(scope 外):~~

- ~~USB CDC + WebSerial~~
- ~~UART~~
- ~~BLE~~
- ~~TCP~~
- ~~独自プロトコル~~

~~USB HID + CDC 複合デバイスによる WebSerial 設定画面 / serial CLI~~は scope 外です。

## 永続化

core は永続化を必須にしません。必要なアプリケーションや example が、任意の保存先へ設定を保存できるものとします。

~~保存先候補(scope 外):~~

- ~~NVS~~
- ~~LittleFS~~
- ~~SPIFFS~~
- ~~SD card~~

~~NVS / LittleFS / SPIFFS / SD card を使う永続設定リファレンス実装~~は scope 外です。

## 非機能要件

- Arduino から利用可能にする。
- MIT ライセンスで提供する。
- SoC 依存部を最小化する。
- core は移植可能な純粋 C++ とし、Arduino API・ESP-IDF・FreeRTOS などプラットフォーム固有機能に依存しない。プラットフォーム依存は adapter 層(実質の HAL)に閉じ込める。
- core は時刻取得・動的メモリ確保をせず、host PC 上でコンパイル・テストできる。
- 入力 adapter、入力ごとの処理、入力統合、全体変換、出力 adapter を独立したモジュールとして構成する。
- 複数入力の同時利用を可能にする。
- 複数出力の同時利用を可能にする。
- core の依存を小さくし、複雑な利用例や設定 UI は examples / adapter 側で拡張できる設計にする。

## 想定利用例

- USB keyboard から USB keyboard への変換。
- USB keyboard から BLE keyboard への変換。
- BLE keyboard から USB keyboard への変換。
- GPIO key matrix から USB keyboard への変換。
- GPIO key matrix から BLE keyboard への変換。
- ~~USB keyboard から ESP-NOW への変換。~~
- ~~ESP-NOW から USB keyboard への変換。~~
- ~~Serial イベント入力から USB keyboard への変換。~~
- ~~IR remote から Consumer Control への変換。~~
- USB barcode reader から serial 出力への変換。
- US 刻印 keyboard を日本語設定の Windows へ接続し、記号を含めて刻印どおりに入力する(レイアウト変換)。
- serial で受け取った文字列を、host の layout 設定に合わせた打鍵として送信する。
- 複数の入力デバイスと USB keyboard + mouse 複合デバイス出力を組み合わせる。
- ~~WebSerial 設定画面で keymap を変更し、再ビルドなしで HID 出力へ反映する。~~
- USB hub 経由の複数 keyboard を統合し、片方の Shift / Ctrl をもう片方の keyboard 入力にも効かせる。

## 位置付け

本ライブラリは QMK のようなキーボードファームウェアそのものを目指すものではありません。ESP32 シリーズ上で動作する汎用的な「キーボード入力、統合、変換、出力エンジン」として位置付けます。

主対象は keyboard / consumer control などの HID 系入力です。実装済みの mouse / pointer / wheel も扱います。~~将来の event domain 拡張~~は scope 外です。SoC ごとの機能差は各 adapter 実装で吸収します。
