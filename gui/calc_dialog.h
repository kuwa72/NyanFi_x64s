/**
 * @file gui/calc_dialog.h
 * @brief 電卓ダイアログ (wx 依存)
 *
 * @details VCL 版の `src/CalcDlg.cpp` (`TCalculator`) の入力欄・履歴・角度
 *          切替・NOW/HEX/DEC/NOT/AC だけを使う。式評価は wx 非依存の
 *          `gui/calc.h` が行う。
 *
 *          VCL 呼び出し位置 (grep 実測): `src/MainFrm.cpp:14039-14064`、
 *          `src/UserMdl.cpp:1200-1207`。未移植 (未実装扱い): DEFINE の
 *          ini 編集、位置/履歴の ini 永続化、浮動小数点例外の VCL マスク、
 *          入力欄の VCL 的特殊メニュー。
 */
#ifndef NYANFI_GUI_CALC_DIALOG_H
#define NYANFI_GUI_CALC_DIALOG_H

#include <vector>

#include <wx/wx.h>

#include "gui/calc.h"

namespace calc_dialog {

/// Run に渡す前回状態。OK で履歴と角度モードを返す。
struct Context {
	UnicodeString initial_line;
	std::vector<UnicodeString> history;
	calc::AngleMode angle_mode = calc::AngleMode::Deg;
	int output_digits = 18;
	bool comma_grouping = false;
};

/// 電卓を表示する。キャンセルなら false で context は変更しない。
bool Run(wxWindow *parent, Context &context);

}  // namespace calc_dialog

#endif  // NYANFI_GUI_CALC_DIALOG_H
