# Phase 0 報告: VCL 依存の除去とツールチェインの OSS 化

issue #1 の Phase 0「シム雛形 + CMake を用意し、フォーム非依存のファイルを VCL 無しでビルドする」の実施記録。
目的は「VCL 無しでビルドが通る行数」を実数で出し、シムに必要な API を洗い出すこと。

- 対象ブランチ: `port/phase0`
- 期間: 2026-08-20
- 対象は Win64 のみ (クロスプラットフォーム化は目的外)

## 1. 確定した方針

| 項目 | 決定 | 理由 |
|---|---|---|
| 本番ツールチェイン | clang-cl + Windows SDK + CMake + Ninja | issue #1 の方針どおり。無償入手可・GitHub ホストランナー標準搭載 |
| ローカル検証ツールチェイン | **mingw-w64 (GCC 16.2)** | Windows 実機なしでコンパイルでき、生成した .exe を WSL interop でそのまま実行できるため doctest が回る。Phase 0 の反復速度が CI 待ちに律速されない |
| ソース文字コード | CP932 → **BOM 無し UTF-8** に一括変換 | `scripts/convert_to_utf8.py`。230 ファイルすべて往復検証済み |
| narrow リテラルの実行時文字コード | **CP932** (`-fexec-charset=CP932`) | C++Builder と同じ意味論を保つため。詳細は §4 |
| 新規コードの配置 | トップレベル `CMakeLists.txt` + `compat/` | 将来こちらが本流になる前提 |
| 既存ソースの変更 | **しない** (src/ は文字コード変換以外ノータッチ) | `vcl_shim.h` を強制インクルード (`-include` / `/FI`) して C++Builder の暗黙 `vcl.h` を再現する方式を採った |

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

Phase 0 の対象に残したのは以下の 15 ファイル。

<!-- TODO: ビルド結果の表 (scripts/probe.sh の出力) -->

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

**問題**: 本番ターゲットの clang-cl は UTF-8 以外の実行時文字コードを扱えない。clang-cl へ移る前に、非 ASCII を含む narrow リテラルを wide (`_T(...)`) へ機械変換する必要がある。

`scripts/scan_narrow_literals.py` による実測:

| 範囲 | 件数 |
|---|---|
| src 全体 | **1,944 箇所 / 70 ファイル** (全 230 ファイル中) |
| Phase 0 対象 15 ファイル | 712 箇所 |
| 最多 | `usr_cmdlist.cpp` 583 (コマンド表の日本語説明)、`MainFrm.cpp` 259、`Global.cpp` 195、`OptDlg.cpp` 166 |

変換自体は機械的 (リテラルを `_T()` で包む) で、`UnicodeString(const char*)` を経由していた箇所が `const wchar_t*` になるだけなので意味論の変化はない。Phase 1 の作業項目とする。

<!-- TODO: §5 ビルド結果 / §6 テスト / §7 残課題 / §8 次のアクション -->
