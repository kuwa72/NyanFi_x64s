/**
 * @file gui/cmd_list_dialog.h
 * @brief コマンドファイル一覧ダイアログ (wx 依存)
 *
 * @details VCL 側の実測:
 *          - `src/MainFrm.cpp:14426-14432` が実コマンド `CmdFileList` から開く
 *          - `src/OptDlg.cpp:2615-2633,3693-3697,4155-4160` と
 *            `src/BtnDlg.cpp:209-211` は設定画面からの選択補助として同じ
 *            ダイアログを `ShowToSelect` で使う
 *
 *          GUI には独立した新規コマンドを作らず、実在する `CmdFileList` から
 *          開く。設定画面 (OptDlg) 自体が未移植のため、その3経路は現時点では
 *          到達不能 (未実装扱い) であり、`CmdFileList` のみを配線する。
 *
 *          ファイル列挙とフィルタの判断は `gui/cmd_list.h`、wx のグリッド、
 *          プレビュー、入力だけを中心にする。
 *
 *          未移植 (未実装扱い):
 *          - VCL の TStringGrid OwnerDraw/列幅/ソートヘッダ
 *          - 詳細なファイル情報・参照元の内部設定追随
 *          - 改名/削除後の設定一覧の一括更新と ExeCommandsCore 実行
 */
#ifndef NYANFI_GUI_CMD_LIST_DIALOG_H
#define NYANFI_GUI_CMD_LIST_DIALOG_H

#include <vector>
#include <wx/wx.h>

#include "gui/cmd_list.h"

namespace cmd_list_dialog {

/**
 * @brief 実行ファイル配下の .nbt を列挙する。
 * @details VCL `src/CmdListDlg.cpp:192-217` と同じ再帰検索を使う。実体から
 *          サイズ/日時/先頭行の説明を取得する。
 */
std::vector<cmd_list::Entry> Enumerate(const UnicodeString &exe_path);

/**
 * @brief コマンドファイル一覧を表示する。
 * @param parent 親ウィンドウ
 * @param entries 候補一覧
 * @param options フィルタ焦点・選択専用等の初期値
 * @param[out] out OK/操作時に有効
 * @return 実行/編集/選択の操作が確定した場合 true
 */
bool Run(wxWindow *parent, const std::vector<cmd_list::Entry> &entries,
         const cmd_list::Options &options, cmd_list::Result &out);

}  // namespace cmd_list_dialog

#endif  // NYANFI_GUI_CMD_LIST_DIALOG_H
