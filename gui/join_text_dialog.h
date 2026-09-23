/**
 * @file gui/join_text_dialog.h
 * @brief テキストファイル結合ダイアログ (wx 依存)
 *
 * @details VCL 版の `src/JoinDlg.cpp` (`TJoinTextDlg`) の入力画面を移植する。
 *          選択肢と有効条件は wx 非依存の `gui/join_text.h`、実処理は
 *          `gui/text_ops.h` が担当する。
 *
 *          未移植 (未実装扱い):
 *          - テンプレートによる連結とテンプレートの外部編集
 *          - VCL のユーザー編集メニュー、ドラッグ並べ替え、位置・設定の保存
 */
#ifndef NYANFI_GUI_JOIN_TEXT_DIALOG_H
#define NYANFI_GUI_JOIN_TEXT_DIALOG_H

#include <vector>

#include <wx/wx.h>

#include "gui/join_text.h"

namespace join_text_dialog {

struct Options {
	std::vector<UnicodeString> sources;
	UnicodeString output_name;
	UnicodeString output_code;
	UnicodeString line_break = _T("\r\n");
	UnicodeString template_path;
	bool with_bom = false;
};

/// @return OK で閉じたなら true。テンプレートが指定された場合は未移植の警告を出し false
bool Run(wxWindow *parent, Options &options);

}  // namespace join_text_dialog

#endif  // NYANFI_GUI_JOIN_TEXT_DIALOG_H
