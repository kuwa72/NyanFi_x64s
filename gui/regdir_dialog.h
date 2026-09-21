/**
 * @file gui/regdir_dialog.h
 * @brief 登録ディレクトリダイアログ (wx 依存)
 *
 * @details VCL 版の該当は `src/DirDlg.cpp` (`TRegDirDlg` の通常モード。
 *          一覧の表示・キーでのジャンプ・追加モード・使用後の先頭移動)。
 *          判断部分は wx 非依存の `gui/regdir.h` が持ち、ここでは一覧の表示・
 *          フィルタ・選択・追加・削除だけを wx で再構成したもの
 *          (gui/dupl_dialog.h と同じ作り)。
 *          項目の編集・上下移動・環境変数表示・特殊フォルダ合成一覧は
 *          未移植のため扱わない (未実装扱い。`gui/regdir.h` の説明を参照)
 */
#ifndef NYANFI_GUI_REGDIR_DIALOG_H
#define NYANFI_GUI_REGDIR_DIALOG_H

#include <vector>
#include <wx/wx.h>

#include "gui/regdir.h"

namespace regdir_dialog {

/**
 * @brief 登録ディレクトリダイアログを表示し、移動先の選択を受け付ける
 * @param parent 親ウィンドウ
 * @param[in,out] items 登録内容 (追加・削除・使用後の先頭移動で変わる。
 *        戻り値が true のとき呼び出し側で ini へ保存すること)
 * @param current_dir 現在のディレクトリ (追加時のパス・初期カーソル用)
 * @param[out] selected_out 選ばれた項目の添字 (戻り値が true のときのみ有効。
 *        先頭移動後の添字)
 * @return true OK で閉じた (selected_out が有効)。キャンセルなら false
 */
bool Run(wxWindow *parent, std::vector<regdir::RegDirItem> &items,
         const UnicodeString &current_dir, int &selected_out);

}  // namespace regdir_dialog

#endif  // NYANFI_GUI_REGDIR_DIALOG_H
