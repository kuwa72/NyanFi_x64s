/**
 * @file gui/grep_dialog.h
 * @brief ファイル内容検索 (grep) のダイアログ (wx 依存)
 *
 * @details 検索条件の入力 → wxProgressDialog を出しながら gui/grep.h
 * (wx 非依存のロジック層) で走査 → 結果一覧から選んで開く、の3段を
 * 1つの関数にまとめてある (gui/file_info_panel.h の ShowFileInfoDialog と
 * 同じ「薄い wxDialog を関数で公開する」作り)。
 */
#ifndef NYANFI_GUI_GREP_DIALOG_H
#define NYANFI_GUI_GREP_DIALOG_H

#include <wx/wx.h>

#include "gui/grep.h"

namespace grep_dialog {

/**
 * @brief grep のダイアログを表示し、検索条件の入力から結果の選択までを行う
 * @param parent 親ウィンドウ
 * @param dir 検索対象ディレクトリ (現在のペインのディレクトリ)
 * @param initial_mask 初期表示するファイル名マスク (現在のペインのマスク)
 * @param[out] selected 結果一覧から選ばれたマッチ (戻り値が true のときのみ有効)
 * @return true 結果を選んで「開く」を選択した (selected が有効)。
 *         入力をキャンセルした・一致が無かった・選ばずに閉じた場合は false
 */
bool Run(wxWindow *parent, const UnicodeString &dir, const UnicodeString &initial_mask,
         grep_core::GrepMatch &selected);

}  // namespace grep_dialog

#endif  // NYANFI_GUI_GREP_DIALOG_H
