/**
 * @file gui/pre_same_dialog.h
 * @brief 同名時処理の事前指定ダイアログ (wx 依存)
 *
 * @details VCL 版の `src/PreSameDlg.cpp` (`TPreSameNemeDlg`) を移植する。
 *          判断は wx 非依存の `gui/pre_same.h` が担当する。
 */
#ifndef NYANFI_GUI_PRE_SAME_DIALOG_H
#define NYANFI_GUI_PRE_SAME_DIALOG_H

#include <wx/wx.h>

#include "gui/pre_same.h"

namespace pre_same_dialog {

/// @param mode 初期表示。OK で閉じたときだけ書き換わる
/// @return OK で閉じたなら true
bool Run(wxWindow *parent, pre_same::Mode &mode);

}  // namespace pre_same_dialog

#endif  // NYANFI_GUI_PRE_SAME_DIALOG_H
