/**
 * @file gui/dupl_dialog.h
 * @brief 重複ファイル検索のダイアログ (wx 依存)
 *
 * @details VCL 版の該当は `src/DuplDlg.cpp` (`TFindDuplDlg`。ハッシュ算法・
 *          最大サイズ・左右比較・サブディレクトリ・シンボリックリンク除外・
 *          リンク解決の選択肢を持つ)。このうち移植済みの条件
 *          (判定方法・サブディレクトリ) と、既存のロジック層
 *          (`find_files::DuplicateOptions`) で扱えるマスクだけを wx で
 *          再構成したもの (gui/grep_dialog.h と同じ作り)。
 *          ハッシュ算法の選択・最大サイズ・左右比較等は未移植のため扱わない
 *          (未実装扱い。検索自体は落とさない)
 */
#ifndef NYANFI_GUI_DUPL_DIALOG_H
#define NYANFI_GUI_DUPL_DIALOG_H

#include <wx/wx.h>

#include "gui/find_files.h"

namespace dupl_dialog {

/**
 * @brief 重複ファイル検索のダイアログを表示し、条件の入力を受け付ける
 * @param parent 親ウィンドウ
 * @param[out] opt_out 入力された条件 (戻り値が true のときのみ有効)
 * @return true OK で閉じた (opt_out が有効)。キャンセルなら false
 */
bool Run(wxWindow *parent, find_files::DuplicateOptions &opt_out);

}  // namespace dupl_dialog

#endif  // NYANFI_GUI_DUPL_DIALOG_H
