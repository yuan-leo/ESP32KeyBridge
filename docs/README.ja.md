# ドキュメント案内

[English](README.md)

どの文書を、どの順で読むかの案内です。設計文書は日本語のみ(`*.ja.md`)で、README・リリースチェックリスト・example は日英併記です。

## まずここから

| やりたいこと | 読む文書 |
| --- | --- |
| ライブラリが何をするものか知り、動くスケッチを見る | [../README.ja.md](../README.ja.md) |
| 自分のハードウェア向けのスケッチを探す | [../examples/README.ja.md](../examples/README.ja.md) — すべてそのまま書き込める完結したスケッチ |
| 使うアダプタを選び、できること・できないことを知る | [ADAPTERS.ja.md](ADAPTERS.ja.md) |
| キーモデルがなぜこの形なのかを知る | [DATA_MODEL.ja.md](DATA_MODEL.ja.md) |
| 自作アダプタを書く | `src/ESP32KeyBridge.h` の `InputAdapter` / `OutputAdapter` のコメント → `TouchButtons` example → [CORE_DESIGN.ja.md](CORE_DESIGN.ja.md) |
| 現在地と残作業を知る | [DEVELOPMENT_PLAN.ja.md](DEVELOPMENT_PLAN.ja.md) と [ADAPTERS.ja.md](ADAPTERS.ja.md) の状況列 |
| なぜそう設計したのかを知る | [DECISIONS.ja.md](DECISIONS.ja.md) |

## 文書一覧

**設計(全体像を掴むならこの順)**

1. [REQUIREMENTS.ja.md](REQUIREMENTS.ja.md) — 何のためのライブラリで、どこまでの機器を対象にするか。
2. [USE_CASES.ja.md](USE_CASES.ja.md) — 設計検証に使った具体的なシナリオ。各ユースケースが確定させた規則も記録している(例: 入力の切断時はその入力のキーをクリアする → 電池切れで修飾キーが stuck しない)。
3. [DATA_MODEL.ja.md](DATA_MODEL.ja.md) — 中間表現。種別 + 値のキー同一性、継続 / 単発の 2 大分類、lock 状態の正本、レイアウト変換。
4. [CORE_DESIGN.ja.md](CORE_DESIGN.ja.md) — core / adapter / example の境界、依存の規則、時間・並行性の境界(どのライブラリのコールバックがどのコンテキストで走るか)。
5. [ADAPTERS.ja.md](ADAPTERS.ja.md) — 全アダプタの挙動・実装状況・依存・PC から見える USB 構成・`update()` が遅れたときの耐性。
6. [CONFIGURATION.ja.md](CONFIGURATION.ja.md) — 設定モデルのメモ。保存先や設定 UI を core の外に置く理由。
7. [DECISIONS.ja.md](DECISIONS.ja.md) — 設計決定の台帳。採らなかった案と、v1 では見送った案も含む。
8. [API_SKETCHES.ja.md](API_SKETCHES.ja.md) — 実装前の API 案。記録として残しているだけで、正本は `src/ESP32KeyBridge.h`。

**プロセス**

- [DEVELOPMENT_PLAN.ja.md](DEVELOPMENT_PLAN.ja.md) — 実装順、現在地、残作業。
- [RELEASE_CHECKLIST.ja.md](RELEASE_CHECKLIST.ja.md) / [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md) — リリース前の確認項目と、共通リリース workflow の動き。
- [../tests/README.ja.md](../tests/README.ja.md) — テスト構成(`unit/`、`examples_compile/`、`single/`、`peer/`、`manual/`)と実行方法。
- [../tests/TEST_PLAN.ja.md](../tests/TEST_PLAN.ja.md) — カバレッジ表。何をどこで確認していて、何が未着手か。

## 記述の約束

- 文書中のコード例では namespace(`esp32keybridge::`)を省略しない(`using` に頼らない)。
- 文書は**実装済みのこと**を書く。未実装は現在形で書かず「予定」と明示する。
- `ADAPTERS.ja.md` は「実装済み(実機検証待ち)」と「実装済み(実機検証済み)」を区別する。リリース可否の判断で状況列をそのまま信用できるようにするため。
