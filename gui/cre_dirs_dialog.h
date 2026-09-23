/**
 * @file gui/cre_dirs_dialog.h
 * @brief 階層的ディレクトリ作成ダイアログ (wx 依存)
 *
 * VCL の `TCreateDirsDlg` は `src/CreDirsDlg.cpp:22-318`、実呼び出しは
 * `src/MainFrm.cpp:15665-15697`。リスト変換/判定は gui/cre_dirs.h、実作成は
 * 既存 file_ops2::CreateDirs に委ねる。
 *
 * 未移植 (未実装扱い): get_SubDirs/禁止文字変換、リスト保存/読込、進捗表示、
 * VCL の ini 位置/Action/ポップアップメニュー。
 */
#ifndef NYANFI_GUI_CRE_DIRS_DIALOG_H
#define NYANFI_GUI_CRE_DIRS_DIALOG_H

#include <wx/wx.h>

#include "gui/cre_dirs.h"

namespace cre_dirs_dialog {

/// 作成リストと各入力条件を受け取る。OK なら state を更新して true。
bool Run(wxWindow *parent, cre_dirs::DialogState &state);

}  // namespace cre_dirs_dialog

#endif  // NYANFI_GUI_CRE_DIRS_DIALOG_H
