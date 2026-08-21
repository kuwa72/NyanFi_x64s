/**
 * @file gui/rename_dialog.h
 * @brief 一括リネームのダイアログ (wx 依存)
 *
 * @details モード選択 (正規表現置換/連番付与/大文字小文字変換) → プレビュー
 * (変更前後の一覧を常に表示、衝突・変化なしの行が分かる) → 確認 → 実行、を
 * 1つのダイアログにまとめてある (gui/grep_dialog.h と同じ「薄い wxDialog を
 * 関数で公開する」作り)。ロジックは gui/rename.h (wx 非依存) に分離してある。
 */
#ifndef NYANFI_GUI_RENAME_DIALOG_H
#define NYANFI_GUI_RENAME_DIALOG_H

#include <vector>

#include <wx/wx.h>

#include "gui/rename.h"

namespace rename_dialog {

/**
 * @brief 一括リネームのダイアログを表示し、プレビュー・確認・実行までを行う
 * @param parent 親ウィンドウ
 * @param dir 対象の親ディレクトリ (両ペイン共通で、対象は同一ディレクトリ内に限る)
 * @param targets リネーム対象 (マーク済み、無ければカーソル位置の1件。
 *        gui/main_frame.cpp 側で組み立てる)
 * @return true 1件以上実際に名前を変更した (呼び出し側で一覧の再読み込みが必要)
 */
bool Run(wxWindow *parent, const UnicodeString &dir, const std::vector<rename_core::RenameTarget> &targets);

}  // namespace rename_dialog

#endif  // NYANFI_GUI_RENAME_DIALOG_H
