# Single-Board Tests

ESP32 1台で実行するテストの置き場です。

- `classic_hid_adapters`: 指定した無印 ESP32 1 台で EspBle 1.3.0 の Classic stack / HID Device / HID Host を起動し、`EspBleClassicHidOutputAdapter` の keyboard + consumer + mouse が 214 byte の SDP 上限内で登録されること、keyboard / mouse 入力アダプタを bridge に登録して未接続状態を安全に処理できることを確認します。無線 report 往復には Classic 対応 peer がもう 1 台必要なので、この single test の範囲外です。

core の実機ランタイム確認は、ホスト実行の unit テストと同じスケッチをプロファイル切り替えで実機に流します(スケッチの二重管理をしない):

```sh
# 単体 S3 を TEST_SERIAL_PORT_ESP32S3 のポートに接続してから
uv run --env-file .env pytest unit/core_smoke --profile esp32s3
```

`tests/unit/core_smoke/core_smoke.ino` が core 全域の約 60 アサーション(キーモデル、統合、変換・レイヤー、lock、タイピング、レイアウト変換、report builder)を `setup()` で実行し、`TEST done N/N` で判定します。**host と実機で分岐しない完全に同一のコード**です:

- `esp32s3` プロファイルは examples と同じ `USBMode=default`。`Serial` はボードの外付け USB-serial(UART0)に出ます(native USB-OTG CDC ではない)。
- テストがブリッジ(約 7.3KB)と config(約 5.6KB)をローカル変数に取るため、スケッチは `getArduinoLoopTaskStackSize()`(24KB を返す)を定義します。arduino-esp32 が loop task のスタックを決めるために呼ぶ weak フックの上書きで、host core では呼ばれない関数になるだけです。
- 結果サマリは `loop()` で繰り返し送出します。テストランナーはボードが起動して `setup()` を走らせた後に読み始めるため、起動時の 1 回きりの出力はこの隙間で失われます(この harness で実証済み。固定 delay で当てにいっても隙間の長さは環境依存)。ランナーは最初に見えた行でマッチして切断します。host では `setup()` の出力を即マッチするので `loop()` は無害です。アサーション失敗時はボードが panic して `loop()` に到達しない = サマリが出ない = テスト失敗、という意図どおりの挙動になります。

single 用の S3 は**必要なときだけ接続**する前提です。将来、実機でしか確認できない runtime 項目(アダプタのタスク挙動など)が出てきたら、このディレクトリに専用テストを追加します。

Classic smoke は無印 ESP32 を指定して実行します。

```sh
TEST_SERIAL_PORT_ESP32=/run/board-identify/by-id/esp32-d0wd-v3-ece334703a84 \
  uv run pytest single/classic_hid_adapters
```
