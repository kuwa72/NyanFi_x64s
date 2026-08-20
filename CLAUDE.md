# NyanFi_x64s の作業規約

キーボード操作主体の Windows 専用2画面ファイラ。もともと C++Builder 12.1 / VCL 依存で、
[issue #1](https://github.com/kuwa72/NyanFi_x64s/issues/1) に沿って OSS ツールチェインへ
移植中。**実施記録と実測値は [docs/port/phase0-report.md](docs/port/phase0-report.md) が正**。
作業を始める前にそれを読むこと。

## ビルドと検証

```
./scripts/build.sh          # cmake + ninja + ctest (Linux からのクロスビルド)
./scripts/build.sh --clean  # build ディレクトリを作り直す
./scripts/probe.sh          # src のファイル単位の通過状況の表
./scripts/probe.sh usr_str  # 個別ファイルのエラー全出力
```

- 基準ツールチェインは **MSYS2 UCRT64 (mingw-w64 GCC)**。Linux ホストからは
  `cmake/toolchain-mingw-w64.cmake` でクロスビルドする
- mingw が作った `.exe` は **WSL interop でそのまま実行できる**。テストは実 Win32 API を叩く
- 並行して別のビルドが走っている可能性があるので、`BUILD_DIR=/tmp/... ./scripts/build.sh`
  で出力先を分けると衝突しない
- GUI (Phase 2) は `-DNYANFI_BUILD_GUI=ON -DWX_CONFIG=<prefix>/bin/wx-config`。
  wxWidgets は `scripts/build_wx.sh` で用意する

## 絶対に守る規約

### 1. 非 ASCII の文字列リテラルは必ず `_T("...")` で包む

narrow リテラルのままだと実行時の ANSI コードページ依存になり、**英語版 Windows
(GitHub の CI ランナー) で文字化けする**。実際に CI で13件のテストが落ちた。

```
python3 scripts/convert_narrow_literals.py --check <paths>   # 対象を確認
python3 scripts/convert_narrow_literals.py <paths>           # 変換
```

doctest のマクロ (`TEST_CASE` / `SUBCASE` / `MESSAGE`) の引数は `const char*` 固定なので
包まない (ツールが除外する)。

**CI が機械チェックする** (`scripts/check_literals.py`)。手元でも同じコマンドで確認できる:
```
python3 scripts/check_literals.py
```
対象はビルドに入っているファイルだけ。未移植の GUI ファイル (MainFrm.cpp など) には
未変換のリテラルが 1,200 箇所以上残っているが、それは Phase 3 の作業。

### 2. `compat/ustring.h` の UnicodeString のオーバーロード集合を削らない

C++Builder の `UnicodeString` は数値・文字・文字列のすべてから **暗黙変換** できる。
この集合を崩すと**コンパイルは通るのに静かに壊れる**。実際に2回壊した (報告書 §11)。

- `explicit` を付けると `val_str = v_ui;` が `UnicodeString(wchar_t)` に落ちて
  「整数値のコードポイント1文字」になる
- `UnicodeString(char)` を消すと `get_tkn_r(s, ',')` の `','` が `"44"` になる
- `int` だけにすると `unsigned int` の実引数が曖昧になる

### 3. `src/` の変更は最小限。C++Builder のビルドも壊さない

`vcl_shim.h` を強制インクルードして C++Builder の暗黙 `vcl.h` を再現しているので、
呼び出し側を書き換えずに済むことが多い。やむを得ず変更する場合:

- **標準 C++ のみで書く**。`#ifdef` でツールチェインを分岐させない
  (C++Builder 12.1 でもそのまま通るようにするため)
- C++Builder 独自拡張の置き換え例: `src/usr_shell.h` の `TItemsProperty`
  (`__property` → 添字プロキシ)、`src/usr_file_inf.cpp` の `scope_exit`
  (`try/__finally` → RAII)
- **BCC64 での再ビルドは未検証** (この環境に BCC64 が無い)。src を触ったら報告に明記する

### 4. GUI スタブは「宣言のみ」を貫く

`compat/gui_stubs.h` の GUI コントロールは、データメンバは宣言するがメンバ関数の定義を
書かない。**呼ぶとリンクエラーになる**ので実装漏れが静かに隠れない。
`gui/vcl_gui_bridge.cpp` の「未移植 (呼ばれたら落とす)」の一覧が、GUI 移植の残作業表に
なっている。

注意: `--gc-sections` は未定義参照を消してくれない (GNU ld のセクション回収はシンボル
解決の後に走る)。未使用の関数からの参照でもリンクエラーになる。

### 5. 環境差で壊れやすい箇所を知っておく

CI (英語版 Windows + MSYS2) と手元 (日本語 Windows + brew の mingw) で実際に差が出た箇所。
新しいコードを足すときはここを疑う。

| 箇所 | 差 |
|---|---|
| ANSI コードページ | CI は 1252、手元は 932。narrow リテラルと `AnsiString` の往復、半角換算幅が影響を受ける (規約1、`tests/locale_guard.h`) |
| wxWidgets のバージョン | CI は MSYS2 の 3.2、手元は自前ビルドの 3.3.3。3.3 の API は `wxCHECK_VERSION` でガードする |
| windres へのインクルードパス | MSYS2 では `-I` が効かず `wx/msw/wx.rc` を見つけられなかった。リソースの取り込みは **CMake の `find_file` で絶対パスに解決**してから `configure_file` で埋める |
| mingw ヘッダと Windows SDK | mingw の `winternl.h` は `FILE_RENAME_INFORMATION` を定義する (SDK は定義しない)。`RestartManager.h` は mingw に無い。この種の差は `compat/mingw_patch.h` と `compat/include/RestartManager.h` に隔離する |

### 6. 既存実装のバグは直さず記録する

回帰テストの目的は「現在の挙動を固定する」こと。おかしいと思っても直さず、
報告書 §8.5 / §12 に追記する (これまでに13件記録)。移植と挙動変更を混ぜない。

### 7. ロケール依存の検証は `tests/locale_guard.h` で切り分ける

`AnsiString` 往復や半角換算幅は ACP=932 前提。`NYANFI_REQUIRE_ACP_932()` を使い、
スキップした事実を doctest の出力に残す (黙って消さない)。

## 破壊的な機能を足すとき

ユーザーのファイルを操作するコードは、テストが通っただけでは足りない。実際に
`gui/file_ops.cpp` には「テストが1件も無い経路」に無限再帰の穴があった (ディレクトリを
自分自身の配下にコピーするとディスクを埋め尽くす)。

- 削除は**ゴミ箱送りのみ**。完全削除は実装しない
- 上書きを既定にしない。既存があればスキップして件数を報告する
- 破壊的操作の前に必ず確認ダイアログ (件数・宛先・対象名)
- **自分自身や配下を対象にする操作を弾く** (`file_ops::IsSameOrInside`)
- テストは一時ディレクトリの中だけで行う (`tests/temp_dir.h`)
- **危険な経路は書いた人以外がコードを読む**

## コードスタイル

- インデントは**タブ**。コメントは日本語。ヘッダは Doxygen (`@brief` / `@param` / `@return`)
- 区切りは `//---------------------------------------------------------------------------`
- 既存ファイルに合わせる。`src/` は元の書き方を尊重する
- シムに API を足すときは、**先に `src/` の実呼び出し箇所を grep して**、そこで
  期待されている挙動に合わせる。推測で決めた点はコメントと報告に残す

## ディレクトリ

| 場所 | 内容 |
|---|---|
| `src/` | 既存の実装 (C++Builder 版と共用)。変更は最小限 |
| `compat/` | VCL 互換シム。`vcl_shim.h` が傘ヘッダで強制インクルードされる |
| `gui/` | wxWidgets 版の GUI (Phase 2 の骨格) |
| `tests/compat/` | シム自体のテスト / `tests/core/` は既存ロジックの回帰テスト |
| `cmake/phase0_sources.cmake` | シムだけでビルドが通る src ファイルの一覧 |
| `docs/port/` | 移植の実施記録 |
| `scripts/` | ビルド・変換・調査のスクリプト (繰り返す作業はここに足す) |

## コミットと PR

- コミットは論理単位で分ける。機械的な変換 (文字コード / リテラル) は独立したコミットに
- **PR はレビュー可能な量に保つ**。機械的な大量差分は別 PR に分離し、本体 PR の diff から
  外す (例: #2 は文字コード変換のみ、#3 が本体)
- PR の説明には「特に見てほしいところ」と「流し読みでよい機械的変更」を書き分ける
- 未検証事項は必ず明記する (無言のスキップをしない)
- **サブエージェントが並行作業している間は `git add -A` を使わない**。作業途中の
  ファイルを巻き込んでコミットしてしまう (実際にやってしまい、履歴を直した)。
  自分が触ったファイルを明示して `git add <path>...` する
