# SettingsPortal

[English](README.md)

ESP32-S3 が host する設定 website の Phase 2 bring-up です。WPA2 SoftAP を
起動し、保存済み layout 設定と bridge の live status を表示する responsive
な offline page を配信します。

この Phase は read-only です。設定書込み endpoint、captive portal、有線
keyboard からの page 操作はまだ含みません。

## 動かし方

1. ESP32-S3 へ sketch を書き込み、115200 baud の serial console を開く。
2. phone を Wi-Fi `KeyBridge-Setup`、password `keybridge` へ接続する。
3. `http://192.168.4.1/` または serial console に表示された address を開く。

この credential は開発確認用で、sketch 内に明記されています。実運用前に
変更してください。page は外部 CDN 等を使わず、firmware だけで動きます。

実 keyboard sketch へ統合するときは keyboard 処理を優先するため、
`bridge.update()`（必要ならその前の `ble.update()`）の後で
`settingsWeb.update()` を呼びます。
