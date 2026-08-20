# Phase 0 報告: VCL 依存の除去とツールチェインの OSS 化

issue #1 の Phase 0「シム雛形 + CMake を用意し、フォーム非依存のファイルを VCL 無しでビルドする」の実施記録。
目的は「VCL 無しでビルドが通る行数」を実数で出し、シムに必要な API を洗い出すこと。

- 対象ブランチ: `port/phase0`
- 期間: 2026-08-20
- 対象は Win64 のみ (クロスプラットフォーム化は目的外)

## 1. 確定した方針

| 項目 | 決定 | 理由 |
|---|---|---|
| 基準ツールチェイン | **MSYS2 UCRT64 (mingw-w64 GCC) + CMake + Ninja** | 完全に無償・パーミッシブで、Windows SDK にも Embarcadero にも依存しない。理由の実測根拠は §1.1 |
| Linux ホストでのクロスビルド | mingw-w64 (GCC 16.2) | 同じ mingw-w64 ターゲットなのでソースと設定が共通。CI の Linux ジョブで常時確認する |
| ローカルでのテスト実行 | WSL interop | mingw が生成した .exe をそのまま実行できるため、Windows 実機なしで doctest が回る |
| clang-cl | **採用しない** | §1.1 のとおり、narrow リテラル 1,944箇所の機械変換が前提条件になる |
| ソース文字コード | CP932 → **BOM 無し UTF-8** に一括変換 | `scripts/convert_to_utf8.py`。230 ファイルすべて往復検証済み |
| narrow リテラルの実行時文字コード | **CP932** (`-fexec-charset=CP932`) | C++Builder と同じ意味論を保つため。詳細は §4 |
| 新規コードの配置 | トップレベル `CMakeLists.txt` + `compat/` | 将来こちらが本流になる前提 |
| 既存ソースの変更 | 最小限 (文字コード変換 + C++Builder 拡張 5箇所) | `vcl_shim.h` を強制インクルード (`-include` / `/FI`) して C++Builder の暗黙 `vcl.h` を再現し、呼び出し側の書き換えを避けた。5箇所の詳細は §8.3 |

### 1.1 ツールチェイン選定の実測根拠

issue #1 は本番ターゲットを clang-cl + Windows SDK としていたが、Phase 0 の実測を踏まえて
**mingw-w64 (GCC) を基準に変更した**。根拠:

| 実測 | 内容 |
|---|---|
| `clang++ -fexec-charset=CP932` | **`error: invalid value 'CP932'`**。clang は UTF-8 以外の実行時文字コードを一切扱えない。clang-cl でも llvm-mingw でも同じ。つまり clang 系を採ると、非 ASCII の narrow リテラル **1,944箇所** を wide へ機械変換するのが着手の前提条件になる (§4) |
| `(DWORD)obj` のポインタ縮小キャスト | GCC は `-fpermissive` で降格できるが clang には同等のオプションが無く、**16箇所**の修正が前提条件になる (§5) |
| Windows SDK | clang-cl は SDK 必須。SDK は proprietary で EULA が付き、ホストも Windows に限られる。mingw-w64 なら Linux ホストからクロスビルドでき、CI を `ubuntu-latest` で回せる |
| CRT | 現行の mingw-w64 ツールチェインが生成したバイナリの import テーブルは `api-ms-win-crt-*.dll`、すなわち **UCRT**。MSYS2 UCRT64 と同じ CRT を既に使っている |

つまり「無償・CI・脱ロックイン」という目的に対して、mingw-w64 の方が前提条件が少なく、
かつ Phase 0 の 14,088 行はその条件で既に通っている。clang-cl は、上記2つの機械変換を
済ませた後の**任意の追加ターゲット**として扱う。

## 2. 実測: 文字コードとフォーム依存

### 文字コード

| 対象 | ファイル数 | 変換前 | 変換後 |
|---|---|---|---|
| `src/*.cpp` `src/*.h` | 230 | 全て CP932 (純 ASCII は 0) | BOM 無し UTF-8 |
| `src/*.dfm` | 77 | 全て ASCII (日本語は `#12496` 形式のエスケープ) | 変換不要 |

BOM は付けていない。clang-cl も BCC64 (clang ベース) もソースを UTF-8 と仮定するため、`/source-charset` の指定は不要。

### Phase 0 の対象ファイル選定

issue #1 は「フォーム非依存のコードが 23,780 行 / 26 ファイル」としていたが、実測すると **3 ファイルは VCL に依存していた**。

| ファイル | 実際の依存 | 判定 |
|---|---|---|
| `usr_scrpanel.cpp` (818行) | `Vcl.CheckLst.hpp` / `Vcl.Grids.hpp` | Phase 0 の対象外 |
| `usr_tag.cpp` (670行) | `Vcl.CheckLst.hpp` (`TCheckListBox` を実際に操作) | Phase 0 の対象外 |
| `imgv_thread.cpp` (782行) | `MainFrm.h` / `Global.h` / `HistFrm.h` | Phase 0 の対象外 |
| `usr_swatch.cpp` / `usr_highlight.cpp` / `UIniFile.cpp` | `usr_scale.h` 経由で `Vcl.Grids.hpp` | Phase 0 の対象外 |
| `check_thread` / `grep_thread` / `thumb_thread` / `task_thread` / `usr_excmd` | `Global.h` / `MainFrm.h` などの GUI グローバル | Phase 0 の対象外 |

`usr_tag.h` は `TCheckListBox*` を引数の型として参照するだけなので、前方宣言のみの転送ヘッダ (`compat/include/Vcl.CheckLst.hpp`) で `usr_cmdlist.cpp` からの取り込みは通る。

Phase 0 の対象に残したのは以下の 15 ファイル (計 14,088 行)。結果は §8。

## 3. シムに必要だった API (実測)

対象 15 ファイル + 到達するヘッダ計 30 ファイル (17,535 行) を走査した結果。

| 分類 | 実測値 |
|---|---|
| `UnicodeString` | 1,671 (`.IsEmpty()` 235 / `.Length()` 116 / `.ToIntDef()` 61 / `.Insert()` 39 / `.SubString()` 37 / `.Pos()` 31 / `.Delete()` 31) |
| `TStringList` | 243 (`->Add()` 152 / `->Strings[]` 84 / `->Count` 75 / `->Values[]` 37 / `->Text` 32 / `->Objects[]` 23) |
| `TColor` | 88 / `TStringDynArray` 85 / `TFileStream` 86 |
| `EmptyStr` | 195 |
| `SameText` / `SameStr` | 179 / 93 |
| `TStream` 操作 | `->Seek()` 135 / `->ReadBuffer()` 80 |
| `TRegEx` | 38 (`Match` 18 / `Replace` 12 / `IsMatch` 8) |
| `TStyleManager::ActiveStyle` | 17 |
| `Graphics::TBitmap` | 26 |
| `TDateTime` | 33 |
| `TEncoding` | 10 |
| `TSearchRec` + `FindFirst`/`FindNext`/`FindClose` | 8 + 42 |
| `TRegistry` | 4 |
| `TJSONValue` 系 | 10 |

### issue #1 の記載との差分

- **「Delphi の `Set<>` 0件」は誤り**。`file_filter.h` の `Set<FilterOpt, foIsGrep, foExcludeTag>` と `usr_key.cpp` の `TShiftState` で実際に使われている (`s << ssShift` / `s.Contains(ssShift)`)。`compat/set.h` に `std::bitset` ベースのシムを用意した
- `TStringList` の呼び出しは **括弧なしのプロパティ形式** (`lst->Count`、`lst->Values[k].ToIntDef(0)`、`lst->ValueFromIndex[i] = v`) が支配的で、素の C++ ではメソッドに置き換えられない。呼び出し側を書き換えない方針を貫くため、`compat/property.h` にプロパティプロキシを用意し、算出プロパティは `UnicodeString` を継承した書き戻し可能な参照型 (`TStrings::StringRef`) で返す設計にした

## 4. narrow リテラルと clang-cl の非互換 (要対応)

C++Builder は narrow リテラルを CP932 として扱い、`UnicodeString(const char*)` は CP_ACP で変換していた。書庫 DLL (`unrar64.dll` など) が返す char バッファも同じ経路を通るため、**`UnicodeString(const char*)` を UTF-8 解釈に変えると実行時データが壊れる**。したがって RTL と同じ CP_ACP 意味論を維持し、リテラル側を `-fexec-charset=CP932` で合わせた (mingw-w64 で動作確認済み)。

**基準ツールチェイン (mingw-w64 GCC) ではこの変換は不要**。`-fexec-charset=CP932` が使えるため、C++Builder と同じ意味論のままビルドできる。

一方 **clang 系 (clang-cl / llvm-mingw) に移る場合は必須の前提条件**になる。`clang++ -fexec-charset=CP932` は `error: invalid value 'CP932'` で拒否されるため (実測)、非 ASCII を含む narrow リテラルを wide (`_T(...)`) へ機械変換しない限りビルドできない。これが clang-cl を基準から外した最大の理由 (§1.1)。

`scripts/scan_narrow_literals.py` による実測:

| 範囲 | 件数 |
|---|---|
| src 全体 | **1,944 箇所 / 70 ファイル** (全 230 ファイル中) |
| Phase 0 対象 15 ファイル | 712 箇所 |
| 最多 | `usr_cmdlist.cpp` 583 (コマンド表の日本語説明)、`MainFrm.cpp` 259、`Global.cpp` 195、`OptDlg.cpp` 166 |

変換自体は機械的 (リテラルを `_T()` で包む) で、`UnicodeString(const char*)` を経由していた箇所が `const wchar_t*` になるだけなので意味論の変化はない。**clang 系を追加ターゲットにする判断をした時点での作業項目**とする。

## 5. シムでは回避できないブロッカー: C++Builder 独自拡張

issue #1 は「Delphi 固有の厄介な機能への依存は薄い」としていたが、**C++Builder のコンパイラ拡張**への依存は残っていた。これらは互換シム (ライブラリ) では吸収できず、**clang-cl でも同様に通らない**ため、ソースの機械変換が必要になる。

| 拡張 | 出現数 | Phase 0 での影響 | 対処方針 |
|---|---|---|---|
| `__property` (プロパティ構文) | **31箇所 / 17ファイル** | `usr_shell.h:150` と `usr_mmfile.h:41` の 2箇所が 4ファイルをブロック | どちらも添字プロパティ (`Items[int Index] = {read=Get, write=Put}`)。`compat/property.h` のプロキシへ機械変換できる |
| `try { } __finally { }` | **14箇所 / 6ファイル** (`__try` は 0件) | `usr_file_inf.cpp` の 3箇所 | RAII ガードへの置き換え。機械的だが、例外経路の確認は必要 |

内訳 (`__property`): task_thread.h 6 / thumb_thread.h 5 / MainFrm.h 5 / usr_scrpanel.h 2 / 他 13ファイル各1
内訳 (`__finally`): usr_shell.cpp 7 / usr_file_inf.cpp 3 / usr_highlight.cpp・task_thread.cpp・Global.cpp・CalcDlg.cpp 各1

合計 45箇所と小さいため、Phase 1 の最初の作業として独立したコミットで処理するのが妥当。

その他、ソース変更が必要なもの:

| 事象 | 出現数 | 内容 |
|---|---|---|
| `(DWORD)obj` / `(int)obj` | 16箇所 | `TStringList` の `Objects` スロットに 32bit 値を詰めて取り出す書き方。C++ としては ill-formed。GCC は `-fpermissive` で降格できるので基準ツールチェインでは通る。clang 系に移るなら `(DWORD)(DWORD_PTR)obj` へ要修正 |
| 非 ASCII の narrow リテラル | 1,944箇所 | §4 のとおり。基準ツールチェインでは不要、clang 系では必須 |

## 6. 作成した互換シム

`compat/` 配下。すべて新規で、`src/` は 1 行も変更していない (文字コード変換を除く)。

| ヘッダ | 内容 |
|---|---|
| `config.h` | Windows ヘッダ、Delphi スカラ型別名、A/W マクロの調整 |
| `win_headers.h` | シェル / COM / WIC ヘッダ (C++Builder が Vcl.* 経由で取り込んでいた分) |
| `ustring.h` | `UnicodeString` (1始まり)、`AnsiStringT<CodePage>`、`DynamicArray<T>` |
| `sysutils.h` / `datetime.h` / `math.h` | SysUtils / StrUtils の自由関数、`TDateTime`、`TSearchRec` と FindFirst 系 |
| `classes.h` | `TObject` / `TStrings` / `TStringList` / `TList` |
| `streams.h` / `encoding.h` | `TStream` 系、`TEncoding` |
| `property.h` | `__property` 相当のプロパティプロキシ |
| `set.h` | Delphi の集合型 `Set<T,min,max>` |
| `exception.h` | `Exception` 階層 |
| `regex.h` | `TRegEx` (std::wregex バックエンド) |
| `json.h` | `System.JSON` 相当 (手書きパーサ) |
| `graphics.h` | `TColor` / `TCanvas` / `TBitmap` (GDI DIB) / `TStyleManager` |
| `registry.h` | `TRegistry` |
| `cominterface.h` | `TComInterface<T>` (COM スマートポインタ) |
| `application.h` | `Application->Active` / `ProcessMessages()` / `ExeName`、`HInstance` |
| `controls.h` | `TShiftState` など入力系の型 |
| `vcl_forward.h` / `gui_stubs.h` | GUI コントロール (詳細は §7) |
| `mingw_patch.h` | ローカル検証用 mingw-w64 と Windows SDK の差分埋め |

`#include <System.JSON.hpp>` のような RAD 形式の include をそのまま通すため、転送ヘッダ (`System.*.hpp` / `Vcl.*.hpp` / `utilcls.h` / `RestartManager.h`) も用意した。

### wide printf の `%s` (CRT ではなく mingw の stdio 由来)

mingw-w64 の wide printf 実装は `%s` を **マルチバイト文字列**として解釈する (wide は `%ls` / `%S`)。
既存コードは `%s` に `wchar_t*` を渡す前提なので、`UnicodeString::sprintf` / `cat_sprintf` の中で
書式を事前スキャンし、長さ修飾子なしの `%s` → `%ls`、`%c` → `%lc` に書き換えてから
`vswprintf` に渡している。

これは **msvcrt か UCRT かとは無関係** (現行ツールチェインは既に UCRT をリンクしているが挙動は同じ)。
`__USE_MINGW_ANSI_STDIO` を定義しても変わらないことも実測済み。書き換え後の `%ls` は UCRT でも
MSVC でも同じ意味なので、この対処はツールチェインを変えても外す必要がない。

### 正規表現: PCRE2 は Phase 1 で急がなくてよい

issue #1 は正規表現 366箇所を PCRE2 へ置き換える計画だが、**src 全体の `TRegEx` パターン (150以上の異なるリテラル) を実測したところ、PCRE 固有の構文は 1件も使われていなかった**。名前付きグループ `(?<name>)`、先読み・後読み `(?=` `(?!` `(?<=` `(?<!`、`\p{...}`、atomic / possessive group、`\A` `\z` はいずれも不使用。`TRegExOptions` も `roIgnoreCase` / `roMultiLine` / `roCompiled` の3つだけ。

したがって Phase 0 は std::wregex バックエンドで足りており、PCRE2 への移行は「構文の互換性」ではなく **性能と `\w` の Unicode 解釈の忠実度** の問題に絞られる。優先度は下げてよい。

## 7. GUI 依存部の扱い

ロジック層のファイルには、GUI コントロールをポインタで受け取って数個のメンバに触るだけの関数が混ざっている (`usr_color.cpp` の `set_EditColor(TEdit*)`、`usr_str.cpp` の `get_WidthInPanel(TPanel*)`、`usr_cmdlist.cpp` の `TComboBox` 操作など)。

Phase 0 ではこれらを **宣言だけのスタブ** (`compat/gui_stubs.h`) で通している。データメンバは宣言するが、メンバ関数の定義は書かない。したがって:

- コンパイルは通る
- 実際に呼ぶと **リンクエラーになる** (実装が無いことが静かに隠れない)
- Phase 2 で wxWidgets のコントロールに置き換える対象がそのまま一覧になる

このため「通った行数」は 2 段階で報告する必要がある (§8)。

## 8. ビルド結果

### 8.1 コンパイル・リンク

**対象 15 ファイル (14,088 行) すべてが VCL 無しでコンパイルとリンクを通った。**

| ファイル | 行数 | 結果 |
|---|---|---|
| `usr_str.cpp` | 3,314 | PASS |
| `usr_file_inf.cpp` | 2,690 | PASS |
| `usr_file_ex.cpp` | 1,371 | PASS |
| `usr_cmdlist.cpp` | 1,273 | PASS |
| `usr_arc.cpp` | 1,194 | PASS |
| `usr_exif.cpp` | 1,089 | PASS |
| `htmconv.cpp` | 980 | PASS |
| `usr_wic.cpp` | 527 | PASS |
| `file_filter.cpp` | 387 | PASS |
| `usr_color.cpp` | 345 | PASS |
| `usr_id3.cpp` | 320 | PASS |
| `usr_key.cpp` | 278 | PASS |
| `usr_mmfile.cpp` | 119 | PASS |
| `usr_xd2tx.cpp` | 109 | PASS |
| `usr_migemo.cpp` | 92 | PASS |
| **合計** | **14,088** | **15/15** |

成果物: `libnyanfi_core.a` (7.8MB, 15 オブジェクト) / `libnyanfi_compat.a` (9.9MB)

再現手順:

```
brew install cmake ninja mingw-w64      # 未導入なら
./scripts/build.sh                      # cmake + ninja + ctest
./scripts/probe.sh                      # ファイル単位の通過状況の表
```

### 8.2 テスト

doctest。mingw-w64 が生成した .exe を WSL interop でそのまま実行している。

| テスト | ケース数 | アサーション数 | 結果 |
|---|---|---|---|
| `compat_tests` (シム自体) | 155 | 619 | 全パス |
| `core_tests` (既存ロジックの回帰) | 157 | 555 | 全パス |
| **合計** | **312** | **1,174** | **全パス** |

`core_tests` の対象は `usr_str.cpp` (108ケース) / `usr_color.cpp` (13) / `file_filter.cpp` (14) / `htmconv.cpp` (11) / `usr_key.cpp` (11)。

### 8.3 到達のために src へ入れた変更 (5箇所)

「src 無変更」の方針は、対象15ファイル中4ファイル (4,210行) を C++Builder 独自拡張がブロックしていたため、最小限だけ破った。いずれも **標準 C++ のみ**で書き、`#ifdef` による分岐を入れていないので C++Builder 12.1 のビルドも通る想定。

| ファイル | 変更 |
|---|---|
| `usr_file_inf.cpp` | `try { } __finally { }` 3箇所を RAII (`scope_exit`) に置換。ヘルパは同ファイル内の無名 namespace に置いた |
| `usr_shell.h` | `__property drop_target_rec *Items[int Index]` を添字プロキシクラスに置換 (呼び出し形 `TargetList->Items[i]` は不変) |
| `usr_mmfile.h` | `__property BYTE Bytes[unsigned int Index]` を同様に置換 |

**未検証**: この 5箇所が C++Builder 12.1 (BCC64) で実際に通るかは、この環境に BCC64 が無いため確認できていない。標準 C++ の範囲で書いてあるため通る見込みだが、**RAD Studio 側でのビルド確認が必要**。

### 8.4 検証できていないこと (無言のスキップを避けるため明記)

1. **MSYS2 UCRT64 (Windows ホスト) でのビルド**。基準ツールチェインだが、この環境は Linux ホストなので直接は未検証。同じ mingw-w64 GCC + UCRT ターゲットであり、CI (`.github/workflows/port-ci.yml`) の `windows-ucrt64` ジョブで確認する
2. **clang-cl でのビルド**。基準から外したため未実施。§4 の narrow リテラル 1,944箇所と §5 の `(DWORD)obj` 16箇所を機械変換すれば追加ターゲットにできる
3. **C++Builder 12.1 での再ビルド**。§8.3 のとおり
4. **GUI 依存関数の動作**。`compat/gui_stubs.h` は宣言のみで実装が無く、呼ぶとリンクエラーになる。`core_tests` では、同一オブジェクトファイル内の未使用関数がリンクを要求する分だけ `tests/core/test_link_stubs.cpp` が `std::abort()` するダミー定義を与えている (5シンボル: `TWinControl::LockDrawing` / `UnlockDrawing`、`TControl::Perform`、`TDirect2DCanvas` のコンストラクタと `Supported()`)
5. **外部 DLL を要する経路**。`usr_arc.cpp` (書庫 DLL) / `usr_migemo.cpp` (migemo.dll) / `usr_xd2tx.cpp` (xd2txlib.dll) はコンパイル・リンクは通るが、実行時の動作は未確認
6. **`TMultiReadExclusiveWriteSynchronizer` の再入**。Delphi 版は同一スレッドの再入を許すが、シムは SRWLOCK なので再入不可。呼び出し側 (`usr_tag.cpp` / `Global.cpp`) が再入していないかは Phase 1 で確認が必要
7. **`TList` の終端解放**。派生クラスの `Notify` はデストラクタからは呼ばれない (C++ では基底デストラクタ実行時に vtable が巻き戻る)。`usr_shell.cpp` の最終 `delete` で `drop_target_rec` が解放されずリークする。クラッシュはしない

### 8.5 テストで判明した既存実装の挙動 (直していない)

回帰テストは「現状の挙動を固定する」ことが目的なので、以下は修正せず、テストで固定した上で記録する。

| 箇所 | 内容 |
|---|---|
| `HtmConv::Convert` | `HtmDelBlkCls` / `HtmDelBlkId` が既定の空文字列のとき、`class` (または `id`) 属性を持たない `DIV` / `SECTION` / `TABLE` / `UL` などが丸ごと削除される。`SplitString("", ";")` が 1要素 `[""]` を返し、属性なしの `GetTagAtr()` の戻り値 `""` と一致するため |
| `get_size_str_T` | 単位の繰り上げが `>` 判定なので、ちょうど 1GB は `"1024 MB"` と表示される |
| `remove_text` | `ReplaceText` を使っており、最初の1件ではなく大小文字を無視して全件置換する |
| `get_AlNumChar` | 仮想キーコードと ASCII を区別しないため、`VK_F1` (0x70) が `'p'` と衝突して `"p"` を返す |
| `is_RuledLine("")` | 空文字列に対して 1 (罫線とみなす) を返す |

## 9. 次のアクション (Phase 1 の入口)

基準ツールチェインを mingw-w64 に変更したため、優先順位が変わった。

1. **CI を動かす**。`.github/workflows/port-ci.yml` を追加済み。`windows-ucrt64` (基準環境でのビルド + テスト + 成果物) と `linux-cross` (Linux からのクロスビルド確認)、タグ push でのリリースまで書いてある。あとは push して実際に回すだけ
2. **ロジック層の残りを移植する**。`usr_tag.cpp` / `usr_scrpanel.cpp` / `usr_swatch.cpp` / `usr_highlight.cpp` / `UIniFile.cpp` は `Vcl.Grids.hpp` / `Vcl.CheckLst.hpp` 経由の依存を切り離せば通る見込み
3. **回帰テストを増やす**。現状 `usr_file_ex.cpp` と `usr_exif.cpp` は未カバー
4. **`Global.cpp` / `usr_shell.cpp` に着手する**。ここに `IShellFolder` 21 / `IContextMenu` 3 のシェル統合が入っている。`__uuidof` が 2箇所あり、GCC では使えないので `IID_IXxx` 定数に置き換える必要がある (Global.cpp)
5. **Phase 2 (wxWidgets)**。ここからがアプリとして起動するための本体作業。wxWidgets 3.3 は mingw-w64 UCRT64 でビルドできる

### 保留 (clang 系を追加ターゲットにする判断をした場合のみ)

- 非 ASCII の narrow リテラル 1,944箇所を `_T(...)` へ機械変換 (§4)
- `(DWORD)obj` 16箇所を `(DWORD)(DWORD_PTR)obj` へ修正 (§5)
