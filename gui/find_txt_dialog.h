/**
 * @file gui/find_txt_dialog.h
 * @brief テキストビューア内検索ダイアログ (wx 依存)
 *
 * VCL の `TFindTextDlg` は `src/FindTxtDlg.cpp:25-319`。実呼び出しは
 * `src/MainFrm.cpp:19167-19171`、テキストビューア側のコマンド可用性判定は
 * `src/TxtViewer.cpp:5396-5399`。オプションの正規化/行検索は gui/find_txt.h。
 *
 * 未移植 (未実装扱い): Migemo 辞書検索、バイト列検索、強調描画、履歴/ini。
 */
#ifndef NYANFI_GUI_FIND_TXT_DIALOG_H
#define NYANFI_GUI_FIND_TXT_DIALOG_H

#include <wx/wx.h>

#include "gui/find_txt.h"

namespace find_txt_dialog {

/// 検索条件を入力する。OK なら options を確定内容で更新して true。
bool Run(wxWindow *parent, bool binary, find_txt::Options &options);

}  // namespace find_txt_dialog

#endif  // NYANFI_GUI_FIND_TXT_DIALOG_H
