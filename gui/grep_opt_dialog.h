/**
 * @file gui/grep_opt_dialog.h
 * @brief GREP 拡張設定ダイアログ (wx 依存)
 *
 * VCL の `TGrepExOptDlg` は `src/GrepOptDlg.cpp:23-215`、実呼び出しは
 * `src/MainFrm.cpp:30891-30900`。判断とサンプル生成は gui/grep_opt.h へ
 * 分離し、ここは wx の入力界面だけ担当する。
 *
 * 未移植 (未実装扱い): GREP の実出力、置換/ログ実行、VCL の OwnerDraw/
 * ドラッグ＆ドロップ/ini 位置保存。usr_cmdlist.cpp に GrepOption コマンドは
 * ないため、MainFrame には存在しないコマンド名を新たに作らない。
 */
#ifndef NYANFI_GUI_GREP_OPT_DIALOG_H
#define NYANFI_GUI_GREP_OPT_DIALOG_H

#include <wx/wx.h>

#include "gui/grep_opt.h"

namespace grep_opt_dialog {

/// 設定を入力する。OK なら options を確定内容で更新して true。
bool Run(wxWindow *parent, grep_opt::Options &options, bool replace_mode);

}  // namespace grep_opt_dialog

#endif  // NYANFI_GUI_GREP_OPT_DIALOG_H
