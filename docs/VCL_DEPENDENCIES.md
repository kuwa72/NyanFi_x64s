# VCL 依存の現状監査

Issue #1「VCL 依存の除去とツールチェインの OSS 化」のための、実測ベースの監査記録。

- 測定日: 2026-09-24
- 対象 commit: `84237f7c5f53f8dcf22b7094316a70087f30f5c9` (`origin/main`)
- 作業ツリー: `/tmp/wt-i1`
- 対象ターゲット: Win64 (x86_64)。Linux ホストから mingw-w64 でクロスコンパイルする。

## 結論

1. 現在の `nyanfi.exe` は Embarcadero の VCL/RTL ライブラリを読み込んでいない。VCL 由来の型・関数名を C++ の互換層 (`compat/`) と Win32 API で補い、wxWidgets の静的ライブラリをリンクしている。
2. ただし「VCL 依存ゼロ」ではない。`UnicodeString`、`TStringList`、`TForm`、`TControl`、`TWinControl`、`Graphics::*` などの API 境界が compat に残り、`Global.cpp` / `UserFunc.cpp` / `usr_shell.cpp` / `MainFrm.cpp` と 77 個の DFM は現在の wx ターゲットに入っていない。
3. 静的リンクの単位がオブジェクトファイルであるため、GUI から直接呼ばない関数も、同一 `.cpp` にある VCL 風シンボルがリンク時に必要になる。`gui/vcl_gui_bridge.cpp` と `gui/usr_file_inf_link_shim.cpp` はそのリンク抜けを埋める橋渡しであり、完全な GUI 移植ではない。
4. Linux からの Release コアビルド、GUI クロスビルド、ctest、文字列・コマンド検査は実測で通る。Windows UCRT64 上の実行と CI は、この文書の作成時点では未確認であり、PR の CI を正とする。

## 用語の判定

| 用語 | ここでの意味 |
|---|---|
| 移植済み | VCL ランタイムを使わず、現在のターゲットでコンパイル・リンクできる。compat の型名を使うことは残る。 |
| 互換シム | `compat/` の C++ 実装。VCL のソースや DLL を読み込まず、API の形だけを再現している。 |
| 部分移植 | 純粋なロジックはリンクできるが、同じ翻訳単位に未移植の GUI/COM 参照がある。 |
| 未移植 | CMake の現行ターゲットに入っていない、または実 GUI の経路を代替できない。 |
| Win32 直接 | VCL ではないが Windows 固有の API/DLL に依存する。OSS 化とは Windows ネイティブを廃止することを意味しない。 |

## 実測ビルドとリンク

### ビルド対象

`CMakeLists.txt` は `cmake/phase0_sources.cmake` を取り込む。2026-09-24 現在の `scripts/probe.sh` 実測は **22/22 ファイル、18,031/18,031 行が構文チェック PASS** だった。`nyanfi_core` はこの 22 個の `src/*.cpp` に加えて、次の 2 個のシムを含む。

- `gui/usr_shell_fmt_shim.cpp`
- `gui/usr_file_inf_link_shim.cpp`

一方、wx 実行ファイルの `add_executable` は `gui/CMakeLists.txt` にある 48 個の C++ ソースと `nyanfi.rc` を使い、`nyanfi_gui_core`、`nyanfi_core`、`nyanfi_compat` をリンクする。

### 最終実行ファイルに実際に取り込まれた archive member

静的アーカイブの `ar t` と、最終リンクに `-Wl,-Map` を付けた Ninja のリンクマップを突き合わせた。マップは `/tmp` での測定用であり、リポジトリには追加していない。

| アーカイブ | アーカイブ内の member 数 | `nyanfi.exe` に引き込まれた member 数 | 引き込まれなかった member |
|---|---:|---:|---|
| `libnyanfi_core.a` | 24 (22 `src` + 2 GUI シム) | **16** (14 `src` + 2 シム) | `file_filter`, `usr_color`, `usr_migemo`, `usr_mmfile`, `usr_scale`, `usr_swatch`, `usr_scrpanel`, `usr_highlight` |
| `libnyanfi_compat.a` | 15 | **11** | `math`, `ioutils`, `json`, `iduri` |
| `libnyanfi_gui_core.a` | 70 | **68** | `git_ftp`, `tab_settings` |

`nyanfi.exe` に引き込まれた `src` は次の 14 ファイルである。

```text
src/htmconv.cpp
src/usr_arc.cpp
src/usr_cmdlist.cpp
src/usr_exif.cpp
src/usr_file_ex.cpp
src/usr_file_inf.cpp
src/usr_id3.cpp
src/usr_key.cpp
src/usr_msg.cpp
src/usr_str.cpp
src/usr_wic.cpp
src/usr_xd2tx.cpp
src/usr_tag.cpp
src/UIniFile.cpp
```

これは「`CMakeLists.txt` にソース一覧がある」ことと、「その `.obj` が最終実行ファイルまで取り込まれる」ことを区別するための数字である。archive だけで未使用のファイルは、wx GUI からまだ使われていないが、core テストや将来の画面から使われる可能性がある。

`phase0_sources.cmake` のコメントには「21 ファイル」と書かれた箇所があるが、実際のリストと `scripts/probe.sh` は 22 ファイルである。本監査的数字は後者を採用する。

### 成果物と外部依存

Release GUI の成果物は `PE32+ executable (GUI) x86-64` で、測定時のサイズは約 32.6 MB だった。`objdump -p` の import には Windows のシステム DLL と `api-ms-win-crt-*.dll` はあるが、VCL/RTL の DLL 名はない。`-static` は GCC ランタイムと wx の静的ライブラリに効くが、Windows の UCRT とシステム DLL は import のまま残る。

## VCL 由来コードの分類

| 対象 | 用途 | 移植可否 | 障害要因 | 移行メモ |
|---|---|---|---|---|
| `compat/ustring.h`, `classes.h`, `streams.h`, `encoding.h`, `regex.h` | `UnicodeString`、`TStringList`、ファイル I/O、文字コード、正規表現 | **移植済み（リンク可）** | VCL の暗黙変換・プロパティ構文・1 始まりの添字を標準 C++ で再現する作業 | wx 側では compat と wx 文字列の変換境界を明示する。当面は core の内部 API として残せる。 |
| `compat/sysutils.h`, `datetime.h`, `application.h`, `registry.h`, `netencoding.h` | パス/ファイル属性/日付/レジストリ/URL と `Application` | **移植済み（Windows 限定）** | `GetEnvironmentVariable`、レジストリ、Win32 メッセージなどの直接呼び出し | VCL ではなく Win32 の責務として整理する。将来 OS を増やすならこの層を置き換える。 |
| `compat/graphics.h`, `graphics.cpp` | `TColor`、24bpp `TBitmap`、GDI の `TCanvas`、`TFont` | **部分移植** | `TGraphic`/`TIcon`/`TPicture`、描画メソッド、`SetHandle` などは宣言のみ。WIC 用の 24bpp DIB と一部 GDI だけが実体 | wx の `wxBitmap`/`wxDC` を主実装にし、core で必要な最小画像データだけ compat に残す。 |
| `src/usr_str.cpp` | 文字列、正規表現、JSON/CSV、文字幅計算のコア | **部分移植でリンク可** | `get_WidthInPanel(TPanel*)` が `TDirect2DCanvas` を参照。`Global.cpp` から増える描画 API は未実装 | wx の DC へ幅計算を移し、`TDirect2DCanvas` 経路を削除する。 |
| `src/UIniFile.cpp` | ini 読み書き、グリッド幅、フォーム位置の永続化 | **部分移植でリンク可** | `TForm` と `UserFunc::adjust_form_pos(TForm*)`。wx 版は別の `<exe名>_wx.ini` を使う | wx の `wxConfig` または専用設定モデルへ移行し、`LoadFormPos` 系を分離する。 |
| `src/usr_file_ex.cpp` | コピー、移動、作成、パス/環境変数の長いパス対応 | **移植済み（Windows 限定）** | `TRegistry` と `StrCmpLogicalW` 等の Win32 依存。VCL 実行時依存はない | 破壊操作の wx 側ラッパーは既存 core を再利用する。 |
| `src/usr_exif.cpp`, `src/usr_id3.cpp` | EXIF/ID3 の解析 | **移植済み（リンク可）** | `TFileStream` と `TStringList` の compat 依存。画像保存側は `Graphics` compat に依存 | 解析は core の回帰テストで固定し、描画・保存だけを wx に分ける。 |
| `src/usr_wic.cpp` | WIC による画像ロード/サイズ/変換 | **部分移植でリンク可** | WIC COM、レジストリ、24bpp `TBitmap`。graphics の一部の API は宣言のみ | wx image への変換を実装するか、core の WIC API をそのまま契約にする。 |
| `src/htmconv.cpp` | HTML から plain text への変換 | **移植済み（リンク可）** | `TStringList`/compat の文字列 API。wx 依存はない | 現在の wx GUI から使う純粋な core として維持する。 |
| `src/usr_xd2tx.cpp` | 任意 `xd2txlib.dll` のテキスト抽出 | **移植済み（リンク可、実行時 DLL は未検証）** | 実行時に `xd2txlib.dll` を `LoadLibrary` する。VCL 依存ではない | DLL を必須にしないフォールバックを実装するか、機能を任意依存として明示する。 |
| `src/usr_arc.cpp` | 7-Zip 系/書庫 DLL の列挙・処理 | **部分移植（リンク可、実行時 DLL は未検証）** | `7-zip64.dll`、`unlha64.dll`、`cab64.dll`、`tar64.dll`、`unrar64j.dll`、`uniso64.dll` を動的ロード。wx 側のアーカイブ UI は別 | DLL の有無を UI の状態として持ち、ない場合の明示的エラーを実装する。 |
| `src/usr_cmdlist.cpp` | VCL 版と同じコマンド表とパラメータ表 | **部分移植** | `set_CmdList` は移植済みだが、`get_PrmList` は `TComboBox` を直接触る | コマンド表を wx 非依存データとして維持し、コンボボックス更新だけを wx adapter に移す。 |
| `src/usr_key.cpp` | キーボード一覧、ショートカット、キーイベント送出 | **部分移植** | `TComboBox`、`TControl::Perform`、`LockDrawing`。現在の wx GUI はイベントを直接処理 | キー文字列生成だけ core に残し、送信・描画抑止は wx のイベント/DC API に移す。 |
| `src/usr_tag.cpp` | タグファイルの読み書き・検索 | **部分移植でリンク可** | `TCanvas::TextWidth/TextOutW` など GDI 型の compat 依存 | データ部分は既存 core を維持し、タグ描画は wx DC に移す。 |
| `src/usr_file_inf.cpp` | 画像/音声/PE/文書などのファイル情報 | **部分移植** | `UserShell::get_PropInf`、`UserShell::get_Duration`、`TMetafile::LoadFromFile`。未使用関数も同一 `.obj` なのでリンク時に参照される | 現在の wx GUI は実行ファイル詳細・一部の duration・metafile を対象外にしている。対象機能を実装するか、UI 上で明示的に除外する。 |
| `file_filter.cpp`, `usr_color.cpp`, `usr_migemo.cpp`, `usr_mmfile.cpp`, `usr_scale.cpp`, `usr_swatch.cpp`, `usr_scrpanel.cpp`, `usr_highlight.cpp` | フィルター、色、Migemo、memory map、DPI、色見本、スクロールパネル、強調表示 | **core には移植済みだが今回の `nyanfi.exe` では未引き込み** | GUI control/canvas 関連の API は compat の宣言または GDI 実装。`usr_migemo.cpp` は `migemo.dll` を動的ロードする | 将来 wx画面から使う機能ごとに、core のデータ部分と wx の描画/操作を分離して取り込む。現時点では未引き込みである理由を docs に残す。 |
| `gui/usr_shell_fmt_shim.cpp` | `usr_shell.cpp` の `get_PropTitle`/`add_PropLine` 等の文字列整形 | **移植済み（コピー）** | 元ファイルには `IShellFolder`/`IContextMenu` 等の GUI/COM 処理が同居し、全体をまだリンクできない | 明確发表评论を分離する。`usr_shell.cpp` の該当部分を移植したら、この shim を一本化する。 |
| `gui/usr_file_inf_link_shim.cpp` | `usr_file_inf.cpp` のリンク抜け、`LoadUsrMsg` と abort 系の境界 | **部分移植** | `usr_SH` は `NULL` 固定。`UserShell` と `TMetafile` の未移植メソッドは例外。未移植の `msgbox_*` は別ファイル | 現在の GUI が呼ばない経路を fail-fast で残すのは意図的。`UserShell` を実装したら定義を削除する。 |
| `gui/vcl_gui_bridge.cpp` | 静的リンクで必要になる GUI 風シンボルの橋渡し | **部分移植（意図的な失敗も含む）** | `TControl::Perform`、Direct2D、フォーム位置補正は wx 実装がない。宣言のみの関数と no-op が混在 | 下の「橋渡しの責務」を優先して解消する。静的な no-op を不用意に増やさない。 |
| `src/Global.cpp` | 共有グローバル状態、表示設定、シェル/タグ/タスクの統合 | **未リンク（構文チェックのみ PASS）** | 2026-09-24 の `nm -u` 実測で未定義 760 シンボル。`msgbox_*`、リスト/キャンバス/クリップボード、`UserShell`、`TImageList` 等を含む | データだけ、Win32 shell、描画、ダイアログの順に分離する。現 CMake には追加しない。 |
| `src/UserFunc.cpp` | 汎用 Win32/UI/日付/クリップボード処理 | **未リンク（構文チェックのみ PASS）** | `nm -u` は 194 シンボル。`TForm::ShowModal`、`TControl::Perform`、`TClipboard`、`UserShell` 等 | Win32 へ直接移せる関数と wx ダイアログが必要に関数を分離する。 |
| `src/usr_shell.cpp` | Shell namespace、アイコン、Drag & Drop、コンテキストメニュー、`IShellFolder` | **未リンク（構文チェックのみ PASS）** | `nm -u` は 241 シンボル。COM シェル、OLE clipboard、`TIcon` の宣言のみ API が障害 | 現在の wx GUI は一部を Win32 COM 直呼で置き換えている。必要機能を絞って `UserShell` API を再定義する。 |
| `src/MainFrm.cpp` と 77 個の DFM | 本体のメインフォーム、ファイル操作 UX、フォーム状態 | **未リンク** | `scripts/probe_phase3.sh MainFrm` は `MainFrm.h:32` の `IdBaseComponent.hpp` 不在で停止。DFM/フォーム生成も VCL 前提 | wx へ書き直す対象。データ/ロジックを先に `nyanfi_gui_core` へ移してから、フォーム単位で置換する。 |
| `src/usr_msg_dlg.cpp` | `msgbox_ERR/WARN/OK/Y_N/...` の VCL メッセージダイアログ | **未移植** | 実測 52 error lines。`CreateMessageDialog`、`TForm::ShowModal`、`TMsgDlgButtons`、`Screen::MessageFont` 等がない | 現 GUI は各 dialog で `wxMessageBox` 等を直接呼ぶ。共通 API が必要になったら wx 実装を一つに集約する。 |
| `compat/zip.h` (`TZipFile`) | ZIP 内の画像抽出と更新アーカイブ展開 | **宣言のみ（未実装）** | `Open`、`Extract`、`FileName[i]` 等の実体がなく、2 つの `src` 呼び出しがリンク不能 | ZIP ライブラリを持ち込むのは「更新/サムネイル」機能を戻す段階まで保留できる。 |

## Global/UserFunc 由来の依存

`src/Global.cpp` と `src/UserFunc.cpp` は `CMakeLists.txt` の `nyanfi_core`、`nyanfi_gui_core`、`nyanfi` のいずれにもソース一覧として列挙されていない。wx 側がそれらを直接リンクしているのではない。

- `gui/clone_name.cpp`、`gui/file_ops2.cpp`、`gui/work_list.cpp` などは、`Global.cpp` / `MainFrm.cpp` の書式や操作手順を wx 非依存の関数として書き直している。
- `gui/settings.cpp`、`gui/key_map.cpp` は VCL の設定・キー処理を別設計で持っており、元の `Global.cpp` のグローバル状態を引き継いでいない。
- `UIniFile.cpp` から参照される `UserFunc::adjust_form_pos(TForm*)` だけは、リンク上の未定義シンボルとして `vcl_gui_bridge.cpp` が fail-fast 定義している。
- `Global.cpp` 本体は構文チェックを通過するが、`nm -u` で 760 個の未定義シンボルがあり、現行 wx バイナリには入れていない。

`nm -u` の個数は VCL だけの数ではなく、Win32/C++ 標準・プロジェクト関数を含む object 全体の未定義シンボル数である。表に挙げた `TForm`、`TControl`、`TClipboard`、`UserShell` などの例が VCL/GUI 境界の主な証拠である。

したがって、GUI の関数に VCL の世界が完全に消えたわけではないが、`Global` / `UserFunc` のソースを直接引きずってリンクするのではなく、 필요한規則だけを `nyanfi_gui_core` に分離している状態である。

## 橋渡しの責務 (`vcl_gui_bridge.cpp`)

`vcl_gui_bridge.cpp` は wx の画面を VCL の画面に見せかけるファイルではない。静的アーカイブのオブジェクト単位リンクで必要になったシンボルに対して、次の 2 種類だけを提供している。

| シンボル/経路 | 現在の実装 | 実測した由来 | 本来の移行先 |
|---|---|---|---|
| `TWinControl::LockDrawing` / `UnlockDrawing` | no-op | `src/usr_key.cpp:42-46` | wx の再描画抑制 API、または呼び出し自体を wx のイベント処理へ移す |
| `TDirect2DCanvas::Supported()` | `false` | `src/usr_str.cpp:1746-1747` | wx DC/GDI 経路へ統一。Direct2D が不要なら分岐を削除 |
| `TDirect2DCanvas::TDirect2DCanvas` | `not_ported` で例外 | `usr_str.cpp` の D2D 分岐 | wx canvas/DC の実装 |
| `TControl::Perform` | `not_ported` で例外 | `src/usr_key.cpp:118-121` | wx の key event 生成・配送。`WM_*` を一律に wx へ写すとは限らない |
| `adjust_form_pos(TForm*)` | `not_ported` で例外 | `src/UserFunc.cpp:67`、`UIniFile.cpp:564` | wx window の monitor bounds による位置補正 |
| `not_ported` | `wxLogError` の後に `std::logic_error` | bridge 内の fail-fast 共通処理 | 実装または未実装であることの明示とテスト |

`gui/usr_file_inf_link_shim.cpp` は別の境界である。そこでは `LoadUsrMsg`/`UserAbort` などの実体を `src/usr_msg.cpp` から使い、`usr_SH` を `NULL` にして、未使用の `UserShell::get_PropInf`・`get_Duration`・`TMetafile::LoadFromFile` を例外にしている。`usr_msg_dlg.cpp` の VCL メッセージボックスを同じ `.obj` に入れると、`LoadUsrMsg` だけを使う場合でも dialog の未定義シンボルを解決する必要が生じるため、意図的に `phase0_sources.cmake` から外している。

`--gc-sections` はシンボル解決の後に実行される。故に、関数単位に未使用でも、同じ `.obj` の別関数から VCL 風シンボルを参照する余地がある。

## 現在の wx GUI が実際に避けている経路

`gui/file_info.cpp` と `gui/main_frame.cpp` のコメント・実装から、次を実測した。

- `.exe` の詳細 version/company 情報は `UserShell::get_PropInf` に依存するため「実行可能ファイルです (詳細情報は未対応)」だけを表示する。
- 再生時間は `get_duration` を試し、Shell property 経路が失敗したら `??:??:??.?? E` として記録する。ただし `get_duration` の fallback は `usr_SH->get_Duration` を呼び、現行 wx ビルドでは `usr_file_inf_link_shim.cpp` が `usr_SH` を `NULL` 固定している。対応済み拡張子以外では null 参照の可能性があり、GUI 側の `catch` では防げない。
- `.wmf/.emf` の metafile情報、`lnk` 解決、context menu、icon rendering は `usr_SH`/VCL graphics の未移植部分であり、wx 側で未対応として表示する。
- 現在の `Settings`/window position は VCL の `UIniFile` と同じ書式を全面的に継承せず、wx 用の別 ini に保存する。`KeyFuncList` だけ読み取り専用の互換入力になっている。

これらは「移植完了」を意味せず、wx GUI の境界で意図的に拒否している範囲である。

## 主な障害要因

1. **GUI コントロールとフォームのモデル**: `compat/gui_stubs.h` はデータメンバだけを提供し、`Perform`、`Focused`、`SetFocus`、コントロール collection、dialog execution などを宣言のみにする。wx の widget/event/ownership に一対一で写せるとは限らない。
2. **Shell/COM 統合**: `usr_shell.cpp` は `IShellFolder`、`IContextMenu`、`IDataObject`、OLE clipboard、shell icon を直接扱う。wx には同じ高レベル API がないため、必要な機能を絞って再定義する必要がある。
3. **`Global` の共有状態**: `Global.h` は image collection、task、tag、scrpanel、shell を一括 include する。`Global.cpp` の構文が通ることは、760 個の未定義シンボルが解決したことや wx で動くことを意味しない。
4. **メッセージダイアログ**: `usr_msg_dlg.cpp` は VCL の modal dialog API に直接依存する。既存の wx 実装は dialog ごとの散在実装であり、共通 `msgbox_*` 契約にはなっていない。
5. **Graphics/metafile/zip**: WIC/GDI の小さな実体は compat にあるが、`TIcon`、`TPicture`、metafile、zip は宣言のみである。画像・書庫・更新機能を戻すまで、リンク切れ・実行時未対応として扱う必要がある。
6. **外部 DLL**: archive、migemo、xd2tx の DLL は動的ロードであり、リンク成功だけでは機能の可用性を保証しない。実測したのはコンパイルと静的 link までである。
7. **文字コード/ロケール**: CMake は `-finput-charset=UTF-8` と `-fexec-charset=CP932` を明示している。VCL 実行時依存の除去とは別問題だが、`str_len_half` の ACP 依存は英語 Windows で表示幅が変わり得る。

## 残タスクの優先順

| 優先度 | 残タスク | 完了条件 |
|---|---|---|
| P0 | `vcl_gui_bridge.cpp` の `TControl::Perform`、D2D、`adjust_form_pos` を、wx で必要か判定する | 必要なら wx 実装と単体/統合テスト、不要なら呼び出しと宣言のみを一緒に削除。`nyanfi.exe` と core_tests のリンクを再確認 |
| P0 | `UserShell` の必要機能を切り出す | `get_PropInf`、duration、`lnk`/icon、context menu の採用機能を実装し、`usr_file_inf_link_shim.cpp` の fail-fast を減らす |
| P0 | `msgbox_*` の境界を決める | 共通 wx API を採用するか、wx 側 dialog に集約し、`usr_msg_dlg.cpp` を CMake に入れる/外す条件を明記 |
| P0 | `Global.cpp` のデータだけを `nyanfi_gui_core` の小さな所有状態へ分離 | Global の巨大な archive を一挙に link せず、状態・ini・key table ごとにテスト付きで移行 |
| P1 | `UserFunc.cpp` / `usr_shell.cpp` の Win32-only 関数を wx から直接使える形にする | COM/clipboard/icon の呼び出し元を明記し、未使用 object の未定義シンボルを減らす |
| P1 | `MainFrm.cpp` と DFM を機能群単位で wx へ移植 | `IdBaseComponent.hpp` を含む VCL header 依存と DFM を段階的に置換し、起動・主要操作を CI で確認する |
| P1 | Graphics/metafile/zip の実装 | `TIcon`/`TMetafile`/`TZipFile` の呼び出しを実測し、未採用なら target から外す。採用なら wx/GDI/zip library の契約をテストする |
| P2 | archive/migemo/xd2tx など外部 DLL の実行時確認 | DLL 不在・読込失敗・関数欠落の UI 表現とテストを追加する |
| P2 | FTP/Git、更新、特殊 dialog などの未移植機能 | `docs/port/phase3-plan.md` の延期範囲に従い、個別に採否を決める |

## 実施したコード改善

**コード変更なし。** `usr_shell_fmt_shim.cpp` の撤去は、`src/usr_shell.cpp` の COM/GUI 依存を先に解決しない限り `usr_file_inf.cpp` の link を壊すため安全ではない。`usr_file_inf_link_shim.cpp` と `vcl_gui_bridge.cpp` も、それぞれ異なる未移植シンボルの責務を持しており、重複定義をまとめるには動作契約の判断が必要である。よって docs のみを追加する。

## 再現コマンド

```text
cmake -S /tmp/wt-i1 -B /tmp/core-i1 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DNYANFI_BUILD_TESTS=ON -DNYANFI_BUILD_GUI=OFF \
  -DCMAKE_TOOLCHAIN_FILE=/tmp/wt-i1/cmake/toolchain-mingw-w64.cmake
cmake --build /tmp/core-i1 -j4
ctest --test-dir /tmp/core-i1 --output-on-failure

cmake -S /tmp/wt-i1 -B /tmp/gui-i1 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DNYANFI_BUILD_GUI=ON -DNYANFI_BUILD_TESTS=OFF \
  -DCMAKE_TOOLCHAIN_FILE=/tmp/wt-i1/cmake/toolchain-mingw-w64.cmake \
  -DWX_CONFIG=$HOME/opt/wx-3.3.3-mingw64/bin/wx-config
cmake --build /tmp/gui-i1 --target nyanfi -j4

scripts/probe.sh
python3 scripts/check_commands.py
python3 scripts/check_literals.py
```

実測結果は `ctest` 2/2 PASS、`scripts/probe.sh` 22/22 PASS、`check_commands.py` は VCL のコマンド表 464 個のうち GUI が 405 個を使用（独自 1 個、欠損 0）、`check_literals.py` は 388 files / 0 箇所であった。Windows UCRT64 の実機・CI 結果は PR 作成後に `gh pr checks` で確認する。
