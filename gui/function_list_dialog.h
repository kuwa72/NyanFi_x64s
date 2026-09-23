/**
 * @file gui/function_list_dialog.h
 * @brief 関数・ユーザー定義文字列・マーク行一覧ダイアログ (wx 依存)
 *
 * @details VCL の呼び出しを実測した行:
 *          - `src/MainFrm.cpp:33040-33045` が3つのモードを振り分け
 *          - `src/MainFrm.cpp:33600-33637` が `FF`/`FZ` と FileEdit 要求を処理
 *          - `src/TxtViewer.cpp:5273-5275` が UserDefList へのユーザー定義文字列設定
 *
 *          一覧の判断は `gui/function_list.h`、wx の表示と入力だけを中心にする。
 *          未移植 (未実装扱い): Migemo辞書、OwnerDraw の構文強調、DFM/ソース切替。
 */
#ifndef NYANFI_GUI_FUNCTION_LIST_DIALOG_H
#define NYANFI_GUI_FUNCTION_LIST_DIALOG_H

#include <wx/wx.h>

#include "gui/function_list.h"

namespace function_list_dialog {

/// Run の結果。
struct Result {
	int line_no = -1;             //!< 選択した元の行 (0始まり)
	bool request_edit = false;    //!< Alt+E でエディタ編集を要求
	UnicodeString user_pattern;   //!< ユーザー定義文字列
	bool name_only = false;
	bool link = true;
	bool regex = false;
};

/**
 * @brief 関数/ユーザー定義/マーク行一覧を表示する。
 * @param parent 親ウィンドウ
 * @param source テキストビューアのスナップショット
 * @param options モード・フィルタ等の初期値
 * @param[out] out OK 時だけ有効
 * @return OK で閉じたなら true
 */
bool Run(wxWindow *parent, const function_list::Source &source,
         const function_list::Options &options, Result &out);

}  // namespace function_list_dialog

#endif  // NYANFI_GUI_FUNCTION_LIST_DIALOG_H
