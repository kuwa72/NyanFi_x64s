/**
 * @file gui/mask_dialog.h
 * @brief マスク/マッチ選択のダイアログ (wx 依存)
 *
 * @details VCL 版の該当は `src/MaskSelDlg.cpp` (`TMaskSelectDlg`。
 *          `MaskSelect` と `MatchSelect` で共用し、履歴の読み書きまで持つ)。
 *          ここでは履歴の永続化を除いた入力部分だけを wx で再構成したもの
 *          (gui/grep_dialog.h と同じ作り)。`Mask` はワイルドカード
 *          (`;` 区切り複数可)、`Match` は指定文字列 (`;` 区切り複数可、
 *          `/～/` は正規表現。`ptn_match_str` の仕様のまま)
 */
#ifndef NYANFI_GUI_MASK_DIALOG_H
#define NYANFI_GUI_MASK_DIALOG_H

#include <wx/wx.h>

#include "usr_str.h"

namespace mask_dialog {

/// どちらの選択か (VCL の `TMaskSelectDlg::CmdName` に対応)
enum class Mode {
	Mask,   //!< マスク選択 (ワイルドカード)
	Match,  //!< マッチ選択 (指定文字列)
};

/**
 * @brief マスク/マッチ選択のダイアログを表示し、入力を受け付ける
 * @param parent 親ウィンドウ
 * @param mode マスク選択かマッチ選択か (タイトルとヒントが変わる)
 * @param[out] pattern_out 入力された文字列 (戻り値が true のときのみ有効)
 * @param initial 初期値 (空なら `*`)
 * @return true OK で閉じた (pattern_out が有効)。キャンセルなら false
 */
bool Run(wxWindow *parent, Mode mode, UnicodeString &pattern_out,
         const UnicodeString &initial = EmptyStr);

}  // namespace mask_dialog

#endif  // NYANFI_GUI_MASK_DIALOG_H
