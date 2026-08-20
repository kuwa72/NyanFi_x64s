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
| `core_tests` (既存ロジックの回帰) | 281 | 884 | 全パス |
| **合計** | **436** | **1,503** | **全パス** |

`core_tests` の対象は `usr_str.cpp` (108ケース) / `usr_file_ex.cpp` (87) / `usr_exif.cpp` (34) / `file_filter.cpp` (14) / `usr_color.cpp` (13) / `htmconv.cpp` (11) / `usr_key.cpp` (11)。

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

## 10. CI (GitHub Actions) と、それが見つけた欠陥

`.github/workflows/port-ci.yml` を追加し、実際に回した結果を記録する。

| ジョブ | 内容 | 結果 |
|---|---|---|
| `windows-ucrt64` | MSYS2 UCRT64 でビルド + `ctest` + 成果物アップロード。リンクした CRT も記録する | 成功 |
| `linux-cross` | Linux ホストからのクロスビルド確認 | 成功 |
| `release` | `v*` タグで成果物をまとめて GitHub Release を作成 | タグ待ち |

CI で記録した `core_tests.exe` のインポート DLL は `api-ms-win-crt-*.dll` (UCRT) と
`KERNEL32` / `USER32` / `GDI32` / `SHLWAPI` のみ。mingw のランタイム DLL には依存しない。

### CI が実際に見つけた欠陥 (ローカル検証では出なかったもの)

GitHub のランナーは **英語版 Windows (ACP=1252)** で、開発環境 (日本語 Windows) との差が
そのまま欠陥として出た。これが CI を用意した最大の収穫。

| 回 | 症状 | 原因 | 対応 |
|---|---|---|---|
| 1回目 | 13 アサーション失敗 | 日本語の narrow リテラルが CP932 バイトで埋め込まれ、`UnicodeString(const char*)` が CP_ACP で変換するため文字化け | src 713箇所 + テスト29箇所を `_T()` で wide 化 (§4)。**NyanFi がロケール非依存になった** |
| 2回目 | 8 アサーション失敗 | `AnsiString` 往復と半角換算幅が本質的に ACP=932 依存 | 実装は変えず、テストをロケール条件付きに (`tests/locale_guard.h`)。§10.1 に既存の制限として記録 |

### 10.1 既存の制限: 半角換算幅が ANSI コードページに依存する

`str_len_half` (usr_str.cpp:1765) は `AnsiString` に変換したバイト長で全角/半角を判定している。
ACP=932 なら全角1文字=2バイト=2半角で正しいが、**ACP≠932 の Windows では全角文字が
1 と数えられ、ファイルペインの列幅がずれる**。C++Builder 版から引き継いだ挙動で、
移植で持ち込んだものではない。

恒久対策の候補 (未実施。挙動変更になるため本移植の範囲外とした):
- 幅の判定を East Asian Width ベースに置き換える
- 実行ファイルのマニフェストに `activeCodePage` を指定してプロセスの ACP を固定する (Windows 11 / Server 2025 以降)

## 11. シムの設計で踏んだ罠: UnicodeString のオーバーロード集合

C++Builder の `UnicodeString` は数値・文字・文字列のいずれからも **暗黙変換** できる。
このオーバーロード集合を正確に再現しないと、**コンパイルは通るのに静かに壊れる**。
実際に 3 段階で問題が連鎖した。記録として残す。

| 段階 | 契約 | 起きたこと |
|---|---|---|
| 1 | `explicit UnicodeString(int)` | `val_str = v_ui;` (usr_exif.cpp に 9箇所、usr_str.cpp に 4箇所) が `UnicodeString(wchar_t)` 経由の暗黙変換に落ち、10進表記ではなく **そのコードポイント1文字** になった。`Exif_GetImgSize` が常に 0×0 を返していた |
| 2 | `explicit` を外した | 今度は `char` リテラル (`get_tkn_r(s, ',')` のような慣用句) が `char`→`int` の **昇格** で `UnicodeString(int)` に解決され、`','` が `"44"` になった。回帰テスト 41ケースが検出 |
| 3 | `UnicodeString(char)` を追加 | `char` に完全一致するオーバーロードができ、両方解決 |

現在の契約 (いずれも非 explicit):

```
UnicodeString(wchar_t)                      1文字
UnicodeString(char)                         1文字 (CP_ACP)
UnicodeString(int / unsigned int / long /
              unsigned long / long long /
              unsigned long long)           10進表記
UnicodeString(double)                       FloatToStr 相当
```

型ごとに用意しているのは曖昧さを避けるため。`int` だけにすると `unsigned int` の実引数が
`int` と `wchar_t` の双方に変換可能で曖昧になる。**この集合を削ってはいけない。**

いずれも回帰テストが検出した。逆に言えば、シムの意味論はテストなしでは担保できない。

## 12. テストで判明した既存実装の問題 (直していない)

§8.5 に加えて、`usr_file_ex.cpp` / `usr_exif.cpp` のテストを書く過程で判明したもの。

| 箇所 | 内容 |
|---|---|
| `get_dirs` (usr_file_ex.h) | 宣言と Doxygen コメントがあるが **実装が src/ のどこにも無い**。呼ぶとリンクエラーになる |
| `delete_Dirs` | 「サブディレクトリを含めたディレクトリの削除」と書かれているが、ファイルは削除せず無視する。そのため木の中にファイルが1つでも残っていると削除が失敗し、上位まで連鎖して木全体が消えない |
| `chk_cre_dir` | 新規作成時のみ末尾 `\` 付きで返し、既存ディレクトリのときは引数をそのまま返す (末尾 `\` の有無が不定) |
| `EXIF_format_inf` の ISO フォールバック | `get_tkn_r(lst->Values["NK:2"], ',')` は「最初の区切りより後ろ全部」を返すため、`"100,200,320"` から最右トークンではなく `"200,320"` が ISO 値として採用される |

## 13. Phase 2 の骨格 (wxWidgets)

「移植済みのロジック層だけでファイラとして動くか」を確かめるための最小の GUI を
`gui/` に置いた。**`MainFrm.cpp` (38,502行) の置き換えではない。**

| 項目 | 内容 |
|---|---|
| 規模 | 約 1,900 行 (`file_pane` / `file_item` / `main_frame` / `key_map` / `settings` / `vcl_gui_bridge` / `nyanfi_app`) |
| wxWidgets | 3.3.3。`scripts/build_wx.sh` が同じツールチェインでクロスビルドする |
| 成果物 | `nyanfi.exe` (静的リンク、依存は Windows システム DLL と UCRT のみ) |
| 実装済みの操作 | カーソル移動 / ディレクトリ移動 / ペイン切替 / マーク / 再読込 / 並べ替え / マスク絞り込み / 設定の永続化 / キー割り当て表示 / 終了 |
| CI | MSYS2 UCRT64 でビルドし、**起動を5秒維持することまで確認**している |

### 作り直さず、移植済みのコードを使っている

| GUI 側 | 使っている既存コード |
|---|---|
| ファイル列挙・整列 | シムの `FindFirst`/`FindNext`、`StrCmpLogicalW` による自然順 |
| 属性・サイズ表示 | `get_file_attr_str()`、`get_size_str_B()` / `_G()` |
| キー名の生成 | `get_KeyStr(WORD, TShiftState)` (usr_key.cpp) |
| カーソルキーの割り当て | `get_CsrKeyCmd()` (usr_cmdlist.cpp) |
| コマンド表 | `set_CmdList()` (usr_cmdlist.cpp) |

入力は VCL 版と同じ3段 (キー名 → コマンド名 → 実行)。割り当て表も ini と同じ
「キー名=コマンド名」形式なので、実際のキー割り当てを後から流し込める。

### ライト/ダークモード

配色は `wxSystemSettings::GetColour()` から取り、`wxApp::MSWEnableDarkMode()` を呼ぶ
(wx 3.3 以降。3.2 でもビルドできるようバージョンガードしてある)。本フォークの存在理由
だった VCL Styles のライト/ダーク切替は、これで置き換えられる。

### 13.1 GUI 移植でつまずいた点 (記録)

| 事象 | 原因と対処 |
|---|---|
| 起動直後に終了コード 57、出力なし | **wx は MSW で `wx/msw/wx.rc` のリンクが必須**。無いと COMCTL32 v6 のマニフェストが埋め込まれず `wxApp::OnInit` が失敗する。GUI サブシステムのため何も表示されない。`gui/nyanfi.rc` で取り込み、診断用に `-DNYANFI_GUI_CONSOLE=ON` でコンソール版も作れるようにした |
| GUI スタブの未定義参照 | `--gc-sections` では解決しない。**GNU ld のセクション回収はシンボル解決の後**に走るため、呼んでいない関数からの参照でもリンクエラーになる。`gui/vcl_gui_bridge.cpp` に「無害化できるので実装する」と「未移植なので呼ばれたら落とす」に分けて定義した |

`gui/vcl_gui_bridge.cpp` の後者の一覧が、GUI 移植の残作業表になっている。現時点では
`TControl::Perform` と `TDirect2DCanvas` の2件。

### 13.2 残りの規模

| 層 | 行数 | 状態 |
|---|---|---|
| ロジック層 | 17,858 | **移植済み** (21ファイル、テスト 478ケース) |
| GUI 骨格 (新規) | 約 900 | **動作する** |
| `Global.cpp` | 16,550 | 未着手。シェル統合 (`IShellFolder` 21 / `IContextMenu` 3) と GUI グローバルの塊 |
| `MainFrm.cpp` | 38,502 | 未着手 |
| その他のフォーム 76個 + DFM 4,114コンポーネント | 約 78,000 | 未着手 |

コードベース全体 151,010 行に対して、ビルドできているのは 12%。Phase 2 を「常用できる
NyanFi」まで進めるには、`Global.cpp` の GUI 依存の切り離しが次の関門になる。

### 13.3 ini との接続 (実測)

「VCL 版の ini をそのまま流し込める」という設計上の主張を実証した。形式は推測ではなく
`src/OptDlg.cpp` の `InpKeyBtnClick` / `ExpKeyBtnClick` から実測したもの。

| 項目 | 実測結果 |
|---|---|
| セクション | `KeyFuncList` |
| 形式 | `<モード文字>:<キー名>=<コマンド名>` (TStringList の Name=Value) |
| モード文字 | `ScrModeIdStr = "FSVIL"` の1文字 (F=ファイルペイン / S=検索 / V=ビューア / I=画像 / L=リスト) |
| 修飾 | `SELECT+` (選択操作用、`Global.cpp` の `KeyStr_SELECT`)、`~` 区切りの2ストロークキー (`Ctrl+K~D` など) |

Phase 2 の骨格はモード `F` のみを読み、`SELECT+` と2ストロークキーは **読み飛ばす**
(対応する仕組みが無いため、黙って誤動作させるより何もしない方を選んだ)。

**ユーザーの ini は書き換えない。** `UsrIniFile::UpdateFile()` は読み込んだ全セクションを
書き直す実装で、コメントや並び順が失われる。そのためウィンドウ位置とペインのディレクトリは
別ファイル `<exe名>_wx.ini` に保存し、キー割り当ては本物の ini から読み取り専用で読む。

### 13.4 並べ替えの意味論 (実測)

`comp_NaturalOrder` / `comp_AscendOrder` (usr_str.cpp) は **ファイル一覧の並べ替えには
使われていない**。実際の呼び出し箇所は `GenInfDlg.cpp` / `HistDlg.cpp` / `TxtViewer.cpp` /
`ShareDlg.cpp` の行単位の `TStringList` だった。

ファイル一覧の並べ替えは `Global.cpp` の `SortComp_Name` / `_Ext` / `_Time` / `_Size` /
`_Attr` (未移植、GUI グローバル依存) が型付きのフィールドを直接比較し、同値のときだけ
`StrCmpLogicalW` で自然順のタイブレークをしている。`gui/file_item.cpp` の
`CompareFileItems` はこの意味論に合わせた。

### 13.5 CI が見つけた環境差 (GUI 編)

ローカル (日本語 Windows / 自前ビルドの静的 wx 3.3.3) では出ず、CI (英語版 Windows /
MSYS2 の共有ライブラリ wx 3.2.11) で出た失敗。

| 症状 | 真の原因 |
|---|---|
| `wx.rc が見つかりません (探索先: )` | **CMake on Windows は `#!/bin/sh` を実行できない**。`wx-config` の応答が全て空になり、空を黙って受け入れていたため誤った症状として現れた。シェル経由で呼び、空の結果を拒否するようにした |
| `__imp__ZN8wxObject...` の未定義参照 | グローバルな `-static` が、共有ライブラリ版 wx のインポートライブラリを使えなくしていた。`-static` はテスト実行ファイルだけに限定した |
| (GUI 起動確認で必要だったこと) | 共有ライブラリ版 wx を使う場合、`/ucrt64/bin` が PATH に無いと DLL を読めない。起動確認は msys2 シェルの中で行う |

### 13.6 ファイル操作とファイルを開く機能

Phase 2 の骨格に、ファイラとして最低限必要な操作を足した。キー割り当ては
`src/Global.cpp` の既定 (`KeyFuncList`) を実測して合わせている。

| キー | コマンド | 内容 | VCL 既定との一致 |
|---|---|---|---|
| `C` | `Copy` | 反対ペインへコピー | 一致 (`F:C=Copy`) |
| `M` | `Move` | 反対ペインへ移動 | 一致 (`F:M=Move`) |
| `D` | `Delete` | **ゴミ箱へ送る** | 一致 (`F:D=Delete`) |
| `K` | `CreateDir` | ディレクトリ作成 | 一致 (`F:K=CreateDir`) |
| `R` | `RenameDlg` | 名前の変更 | 一致 (`F:R=RenameDlg`) |
| `Enter` | `OpenStandard` | ディレクトリなら入る / ファイルは関連付けで開く | 一致 (`F:Enter=OpenStandard`) |
| `Ctrl+Enter` | `OpenByApp` | アプリケーションを選んで開く | 一致 (`F:Ctrl+Enter=OpenByApp`) |
| `Alt+Enter` | `PropertyDlg` | ファイル情報の表示 | **推測** (VCL に既定キーが無い) |

二画面ファイラの慣習である F5〜F8 ではなく単キーなのは、NyanFi の実際の既定がそうだった
ため (`F:F5=ReloadList`)。

### 13.7 破壊的操作の安全策

ユーザーのファイルを操作するため、以下を方針として決めた。

| 方針 | 理由 |
|---|---|
| **削除はゴミ箱送りのみ。完全削除は実装しない** | `SHFileOperationW` + `FOF_ALLOWUNDO`。`Global.cpp` の `delete_File(use_trash=true)` と同じフラグ。移植済みの `delete_Dir` / `delete_Dirs` は完全削除で、しかも後者はファイルを削除しない不具合がある (§12) ので使わない |
| **既存ファイルを上書きしない** | `copy_File` / `move_File` は Win32 の仕様で黙って上書きするため、呼ぶ前に存在を確認してスキップする |
| **全ての破壊的操作の前に確認ダイアログ** | 件数・宛先・対象名 (最大8件) を表示する |
| **結果を必ず報告** | 成功 / スキップ / 失敗の件数を表示。失敗は理由付きで一覧する |
| **実行可能ファイルを開く前に確認** | `test_ExeExt` で判定。意図しない実行は事故になる |

#### 統合時に見つけて直した穴

`CopyItems` に **自分自身の配下へのコピーを弾く防御が無かった**。左ペインで `C:\work` を
選び、右ペインが `C:\work\sub` にある状態でコピーすると、作ったコピーを再び走査して
**無限再帰でディスクを埋め尽くす**。同一ディレクトリへのコピーもスタックオーバーフローに
なる。

`IsSameOrInside()` を追加して両方を弾き、移動側にも同じガードを入れた
(Win32 も拒否するが、意味の分かるメッセージにするため)。パス区切りを見て前方一致を
判定するので `C:\work\a` と `C:\work\ab` を誤検出しない。

**この経路にはテストが存在しなかった。** 破壊的操作は「テストが通っている」だけでは
不十分という例として記録する。

### 13.8 移植が不完全なまま動かしている箇所

GUI から呼ぶために、リンクを通すための実装を置いた箇所がある
(`gui/vcl_gui_bridge.cpp` / `gui/usr_file_inf_link_shim.cpp` / `gui/usr_shell_fmt_shim.cpp`)。
方針は「無害化して実装」と「呼ばれたら落とす」の2分類。

| シンボル | 扱い | 影響 |
|---|---|---|
| `TWinControl::LockDrawing` / `UnlockDrawing` | 何もしない | 再描画抑止は最適化なので影響なし |
| `TDirect2DCanvas::Supported()` | `false` を返す | D2D 経路に入らなくなる (意図通り) |
| `TControl::Perform` / `TDirect2DCanvas` の構築 / `UserShell::get_PropInf` / `get_Duration` / `TMetafile::LoadFromFile` | 例外を投げる | 呼ばれたら落ちる。GUI 移植の残作業表そのもの |

**追記 (port/phase2): `LoadUsrMsg` は移植済み。** メッセージ文字列テーブルと
Abort系関数 (`UserAbort`/`SysErrAbort`/`LastErrAbort`/`TextAbort`/`SkipAbort`/
`CancelAbort`/`EmptyAbort`) は GUI に依存しないため `src/usr_msg.cpp` のまま
`cmake/phase0_sources.cmake` に追加してビルド対象にした。`gui/usr_file_inf_link_shim.cpp`
の簡易実装 (メッセージIDを含むだけの文字列) は削除し、本物の日本語文言に差し替えた
(`tests/core/test_usr_msg.cpp` で回帰テスト済み)。
一方 `msgbox_ERR`/`msgbox_WARN`/`msgbox_OK`/`msgbox_Y_N`/`msgbox_Y_N_C`/
`msgbox_Retry`/`msgbox_Sure`/`msgbox_SureAll` (メッセージボックス表示そのもの) は
VCL の `CreateMessageDialog`/`TForm::ShowModal`/`TCheckBox` の実インスタンス化に
依存しており、ヘッドレスな compat シムでは「宣言のみ」にしかできない。これらを
`usr_msg.cpp` と同じ翻訳単位に残すと、GNU ld がオブジェクトファイル単位で取り込む
ため (規約5と同じ事情)、`LoadUsrMsg` 目的でオブジェクトファイルが取り込まれた瞬間に
未解決参照でビルドが落ちてしまう。そのため `src/usr_msg_dlg.cpp` に分離し、
`cmake/phase0_sources.cmake` には載せていない (`src/NyanFi.cbproj` には
`CppCompile` として追加済みなので C++Builder 側の挙動は変えていないが、
BCC64 での再ビルドは未検証)。現状 `gui/` からこれらの関数を呼ぶ経路は無い
(Phase 3 で MainFrm.cpp 等を移植する際に、wx (wxMessageDialog 等) で実装するか
判断すること)。

`get_AppInf` / `get_duration` (実行ファイルのバージョン情報・メディアの再生時間) と
`get_MetafileInf` (.wmf/.emf) は、未移植の `usr_SH` や `TMetafile` に依存するため
**ファイル情報の表示から除外**している。
