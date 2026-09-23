/**
 * @file gui/diff_dialog.h
 * @brief ディレクトリ比較のダイアログ (wx 依存)
 *
 * @details VCL 版の該当は `src/DiffDlg.cpp` (`TDiffDirDlg`。
 *          比較元/先の表示・対象/除外マスク・サブディレクトリ・
 *          除外ディレクトリマスクを持つ)。このうち入力部分だけを wx で
 *          再構成したもの (gui/dupl_dialog.h と同じ作り)。
 *          判断部分は wx 非依存の `gui/compare.h`
 *          (`compare::DiffDirOptions` 等) が持ち、ここでは表示と
 *          入力だけを受け持つ。確定時の正規化 (空の対象マスクは `*.*`) も
 *          `compare::NormalizeDiffIncMask` 経由で行う。
 *          未移植 (未実装扱い):
 *          - 履歴の永続化 (`DiffIncMaskHistory` 等のコンボ履歴の読み書き。
 *            現在値は呼び出し側が ini に残す)
 *          - 除外欄のグレー表示の色づけ (`get_WinColor`。有効/無効の切り替えは行う)
 */
#ifndef NYANFI_GUI_DIFF_DIALOG_H
#define NYANFI_GUI_DIFF_DIALOG_H

#include <wx/wx.h>

#include "gui/compare.h"

namespace diff_dialog {

/**
 * @brief ディレクトリ比較のダイアログを表示し、条件の入力を受け付ける
 * @param parent 親ウィンドウ
 * @param src_dir 比較元ディレクトリ (表示のみ)
 * @param dst_dir 比較先ディレクトリ (表示のみ)
 * @param[in,out] opt_inout 条件 (初期表示に使い、OK 時に確定内容で置き換わる。
 *        `case_sensitive` は表題の切り替えにだけ使い、変更しない)
 * @return true OK で閉じた (opt_inout が有効)。キャンセルなら false
 */
bool Run(wxWindow *parent, const UnicodeString &src_dir, const UnicodeString &dst_dir,
         compare::DiffDirOptions &opt_inout);

}  // namespace diff_dialog

#endif  // NYANFI_GUI_DIFF_DIALOG_H
