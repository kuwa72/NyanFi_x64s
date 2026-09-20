/**
 * @file gui/find_dialog.h
 * @brief ファイル名検索のダイアログ (wx 依存)
 *
 * @details VCL 版の該当は `src/FindDlg.cpp` (`TFindFileDlg`。ファイル名検索・
 *          ディレクトリ名検索・両方の3コマンドで共用)。VCL 版はマスク・検索語
 *          (正規表現/AND/大小文字)・日付・サイズ・属性に加えて、拡張子別の
 *          拡張条件 (Exif・動画・画像・テキスト内容等) を持つ巨大なダイアログだが、
 *          拡張条件部 (`check_file_ex()`) は未移植のため扱わない
 *          (未実装扱い。検索自体は落とさない)。ここでは基本条件部だけを
 *          `find_files::Query` (wx 非依存) に取り、薄い wxDialog で入力する
 *          (gui/grep_dialog.h と同じ作り)。
 */
#ifndef NYANFI_GUI_FIND_DIALOG_H
#define NYANFI_GUI_FIND_DIALOG_H

#include <wx/wx.h>

#include "gui/find_files.h"

namespace find_dialog {

/**
 * @brief ファイル名検索のダイアログを表示し、検索条件の入力を受け付ける
 * @param parent 親ウィンドウ
 * @param initial_target 対象の初期値 (呼び出し元のコマンドで決まる。
 *        ダイアログ内でもファイル名/ディレクトリ名/両方を切り替えられる)
 * @param initial_mask マスクの初期値 (現在のペインのマスク等)
 * @param[out] query_out 入力された条件 (戻り値が true のときのみ有効)
 * @return true OK で閉じた (query_out が有効)。キャンセルなら false
 */
bool Run(wxWindow *parent, find_files::Target initial_target,
         const UnicodeString &initial_mask, find_files::Query &query_out);

}  // namespace find_dialog

#endif  // NYANFI_GUI_FIND_DIALOG_H
