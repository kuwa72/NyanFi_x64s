/**
 * @file gui/inp_ex_dialog.h
 * @brief 拡張入力ダイアログ (wx 依存)
 *
 * @details VCL の `TInputExDlg` (`src/InpExDlg.h:39-103`,
 *          `src/InpExDlg.dfm:1-302`) のモード表示と入力欄を wx で再構成。
 *          判定・正規化は `gui/inp_ex.h`、実処理は MainFrame が担当する。
 *
 *          未移植 (未実装扱い): 特殊編集ポップアップ、IME、ヘルプ、
 *          入力履歴の VCL 互換保存先、FunctionKey/ClipPaste の専用 Action。
 */
#ifndef NYANFI_GUI_INP_EX_DIALOG_H
#define NYANFI_GUI_INP_EX_DIALOG_H

#include <wx/wx.h>

#include "gui/inp_ex.h"

namespace input_ex_dialog {

/// OK なら values を確定内容で更新して true。キャンセルなら false。
bool Run(wxWindow *parent, inp_ex::Values &values);

}  // namespace input_ex_dialog

#endif  // NYANFI_GUI_INP_EX_DIALOG_H
