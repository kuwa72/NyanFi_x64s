/**
 * @file gui/net_share_dialog.h
 * @brief ネットワーク共有ダイアログ (wx 依存)
 *
 * @details VCL 実測:
 * - `src/ShareDlg.cpp` (`TNetShareDlg`) / `src/ShareDlg.h` / `src/ShareDlg.dfm`
 * - 呼び出しは `src/MainFrm.cpp:25862-25892`、コマンドは
 *   `src/usr_cmdlist.cpp:221` の `ShareList`
 *
 * 入力検証・絞り込み・並べ替え・整形は `gui/net_share.h`、一覧の取得と
 * 接続判断の OS 依存部分は `gui/misc_ops.h` が担当する。
 *
 * 未移植 (未実装扱い):
 * - リモート `NetShareEnum` / `WNetAddConnection3W` (.status に明記)
 * - VCL と同じパンくず操作、ライブラリ/検索設定/ディレクトリ選択モード
 */
#ifndef NYANFI_GUI_NET_SHARE_DIALOG_H
#define NYANFI_GUI_NET_SHARE_DIALOG_H

#include <vector>

#include <wx/wx.h>

#include "gui/net_share.h"

namespace net_share_dialog {

/// 呼び出し側が集めた共有情報。
struct Context {
	UnicodeString computer;
	std::vector<net_share::ShareItem> shares;
	UnicodeString warning;
};

/**
 * @brief 共有ダイアログを表示し、選択された UNC パスを返す
 * @param parent 親ウィンドウ
 * @param context 初期コンピュータ名と取得済み共有
 * @param[out] selected_path_out OK で選択した UNC パス
 * @return true で OK を閉じた場合
 */
bool Run(wxWindow *parent, const Context &context, UnicodeString &selected_path_out);

}  // namespace net_share_dialog

#endif  // NYANFI_GUI_NET_SHARE_DIALOG_H
