/**
 * @file gui/color_settings.h
 * @brief 配色ダイアログの判断ロジック (wx 非依存の純粋ロジック)
 *
 * @details VCL 版の該当は `src/ColDlg.cpp` (`TColorDlg`)。
 *          配色項目の一覧は `FormCreate` の `ColorListBox->Items->Text`
 *          (37項目の `key=caption` 形式) を実測。無効化できるのは
 *          `DisableColActionUpdate` の
 *          `contained_wd_i("fgSelItem|bdrLine|Indent2|bdrFold|bdrFixed|fgPair")`
 *          に一致する6項目だけ (`DisableColActionExecute` が
 *          `Values[key] = IntToStr(col_None)` を書くのを実測)。
 *          色の読み書きは `RefColBtnClick` の `ToIntDef` / `IntToStr` を実測
 *          (参照の既定は `clBlack`、スポイトの既定は `col_None`)。
 *          色値は VCL の `TColor` の整数値のまま扱う (RGB は 0x00BBGGRR、
 *          無効は `clNone = 0x1FFFFFFF`。`compat/graphics.h` を実測)。
 *
 *          未移植 (未実装扱い。落とさない):
 *          - スポイト (`BeginSpuit`/`EndSpuit`。画面色の採取)
 *          - スウォッチパネル (`UsrSwatchPanel`)
 *          - 配色のインポート/エクスポート (INI の [Color] セクション読み書き。
 *            `ReadSection`/`AssignSection`+`UpdateFile` のファイル入出力は
 *            呼び出し側の責務)
 *          - 全体オプションへの反映 (`SetOptColor`。`col_*` グローバルへの
 *            書き戻しと `ColorList` の更新)
 *          - ビューアへの適用 (`ObjViewer->SetColor`/`Repaint`。wx ビューアに
 *            配色機構が無いため状態保持のみ)
 */
#ifndef NYANFI_GUI_COLOR_SETTINGS_H
#define NYANFI_GUI_COLOR_SETTINGS_H

#include <vector>

#include "usr_str.h"

namespace color_settings {

// 無効値 (`Graphics::clNone`。`compat/graphics.h` を実測。`src/Global.h` の
// 巨大ヘッダは直接読まず値を複写する)
constexpr int kDisabledColor = 0x1FFFFFFF;

//---------------------------------------------------------------------------
// 配色項目 (TColorDlg::FormCreate の ColorListBox->Items->Text を実測)
//---------------------------------------------------------------------------
struct ColorItem {
	UnicodeString key;      //!< ColBufList のキー (Values[] の名前)
	UnicodeString caption;  //!< 一覧の表示名
};

/// 37項目の一覧
const std::vector<ColorItem> &ColorItems();

/// キーで項目を引く。無ければ nullptr (ColBufList->Values[] と同じく大小無視)
const ColorItem *FindItem(const UnicodeString &key);

//---------------------------------------------------------------------------
// 色値の読み書き (RefColBtnClick の ToIntDef / IntToStr を実測)
//---------------------------------------------------------------------------

/// 文字列を色値にする。空・非数値は def (VCL の ToIntDef と同じ)
int ParseColorValue(const UnicodeString &s, int def);

/// 色値を文字列にする (VCL の IntToStr と同じ)
UnicodeString FormatColorValue(int color);

/// 無効値 (col_None)
int DisabledColor();

//---------------------------------------------------------------------------
// 無効化 (DisableColActionExecute / DisableColActionUpdate を実測)
//---------------------------------------------------------------------------

/// 無効化できる項目か (6項目だけ true)
bool CanDisable(const UnicodeString &key);

//---------------------------------------------------------------------------
// ダイアログの入出力
//---------------------------------------------------------------------------
struct ColorEntry {
	UnicodeString key;  //!< 配色キー
	int color = 0;      //!< 色値 (kDisabledColor なら無効)
};

/// ColorItems() 順に並べ、不足は def で補い、余分は落とす
std::vector<ColorEntry> EnsureEntries(const std::vector<ColorEntry> &current, int def);

/// key の項目を探す。無ければ nullptr
const ColorEntry *FindEntry(const std::vector<ColorEntry> &entries,
                            const UnicodeString &key);

/// key の色を変える。項目が無ければ何もしない
void SetEntry(std::vector<ColorEntry> &entries, const UnicodeString &key, int color);

/// key を無効化する。対象外 (CanDisable が false) なら false を返す
bool DisableEntry(std::vector<ColorEntry> &entries, const UnicodeString &key);

}  // namespace color_settings

#endif  // NYANFI_GUI_COLOR_SETTINGS_H
