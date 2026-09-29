# SettingsPortal

[English](README.md)

ESP32-S3 が host する設定 website の bring-up です。WPA2 SoftAP を起動し、
保存済み layout 設定の編集と bridge の live status 表示ができる responsive
な offline page を配信します。F1-F24 を trigger にする4個の text macro も
編集できます。保存内容は厳密に検証し、二重化 NVS へ書いて
読戻し確認を済ませてから bridge へ適用します。

この単体 bring-up sketch は keyboard を接続しないため、navigation endpoint
は unavailable を返します。Phase 4 の有線 keyboard navigation adapter は
`UsbKeyboardToBle` example で接続しています。Phase 6 の captive portal
discovery は有効で、Wi-Fi 接続後に phone が local 設定 page を提示または
自動表示します。

## 動かし方

1. ESP32-S3 へ sketch を書き込み、115200 baud の serial console を開く。
2. phone を Wi-Fi `KeyBridge-Setup`、password `keybridge` へ接続する。
3. phone の設定 page prompt を開く。表示されない場合は
   `http://192.168.4.1/` または serial console の address を開く。

page と API は firmware に組み込まれ、internet 上の asset を使いません。
`GET /api/v1/navigation` は keyboard capture lease の更新と navigation action
の取得に使います。

`accessPoint.captivePortal = true` の場合、wildcard DNS は要求された host 名を
ESP32-S3 へ解決します。phone の接続確認に使われる未知の非 API GET path は
固定 SoftAP address へ redirect します。未知の `/api/` path と GET 以外の
request は 404 のままです。captive discovery は WPA2 の代替ではなく、
application authentication も追加しません。

この credential は開発確認用で、sketch 内に明記されています。実運用前に
変更してください。page は外部 CDN 等を使わず、firmware だけで動きます。

実 keyboard sketch へ統合するときは keyboard 処理を優先するため、
`bridge.update()`（必要ならその前の `ble.update()`）の後で
`settingsWeb.update()` を呼びます。
