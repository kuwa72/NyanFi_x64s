/**
 * @file gui/exe_cmd_dialog.h
 * @brief 外部コマンド実行ダイアログ (wx 依存)
 *
 * @details VCL 版の `src/ExeDlg.cpp` (`TExeCmdDlg`) の入力画面を移植する。
 *          判断は wx 非依存の `gui/exe_cmd.h` が担当する。
 *
 *          未移植 (未実装扱い):
 *          - UAC の ForcedElevation フラグを指定する UAC ダイアログ専用動作
 *          - VCL のユーザー編集メニュー、位置・設定の保存
 */
#ifndef NYANFI_GUI_EXE_CMD_DIALOG_H
#define NYANFI_GUI_EXE_CMD_DIALOG_H

#include <vector>

#include <wx/wx.h>

#include "gui/exe_cmd.h"

namespace exe_cmd_dialog {

/// @return OK で閉じたなら true
bool Run(wxWindow *parent, exe_cmd::Options &options, const exe_cmd::CommandSeed &seed);

}  // namespace exe_cmd_dialog

#endif  // NYANFI_GUI_EXE_CMD_DIALOG_H
