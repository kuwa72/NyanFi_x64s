/**
 * @file gui/new_file_dialog.h
 * @brief 新規ファイル作成ダイアログ (wx 依存)
 *
 * @details VCL 版の `src/NewDlg.cpp` (`TNewFileDlg`) の入力画面を移植する。
 *          判断は wx 非依存の `gui/new_file.h` が担当する。
 *
 *          未移植 (未実装扱い):
 *          - テンプレートの小アイコン付き owner draw
 *          - NewFileExeCmd のユーザー設定に基づく複数コマンドの内部記法
 *          - VCL のユーザー編集メニュー、位置・設定の保存
 */
#ifndef NYANFI_GUI_NEW_FILE_DIALOG_H
#define NYANFI_GUI_NEW_FILE_DIALOG_H

#include <vector>

#include <wx/wx.h>

#include "gui/new_file.h"

namespace new_file_dialog {

struct Options {
	UnicodeString template_path;
	UnicodeString name;
	UnicodeString post_command;
	std::vector<UnicodeString> template_history;
	UnicodeString default_directory;
};

/// @return OK で閉じたなら true
bool Run(wxWindow *parent, Options &options);

}  // namespace new_file_dialog

#endif  // NYANFI_GUI_NEW_FILE_DIALOG_H
