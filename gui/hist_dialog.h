/**
 * @file gui/hist_dialog.h
 * @brief ディレクトリ履歴/スタック一覧ダイアログ (wx 依存)
 *
 * @details VCL の `TDirHistoryDlg` (`src/HistDlg.h:24-97`,
 *          `src/HistDlg.dfm:1-169`) を wx の薄い入力層に再構成したもの。
 *          実処理は `hist::State` と MainFrame が持ち、ここでは一覧/検索/
 *          コピー/削除Confirmation/プロパティ表示の UI だけを担当する。
 *
 *          未移植 (未実装扱い): Windows Recent シェル列挙、Migemo 辞書、
 *          UNC/登録名の表示変換、ワークリスト実読込、位置保存。
 *          スタック項目の一覧からの実削除/取り出し (wx は表示と移動だけ)。
 */
#ifndef NYANFI_GUI_HIST_DIALOG_H
#define NYANFI_GUI_HIST_DIALOG_H

#include <wx/wx.h>

#include "gui/hist.h"

namespace hist_dialog {

/**
 * @brief 履歴ダイアログを表示する。
 * @param state 初期状態。検索/削除/追加結果を inout で返す。
 * @param result OK と操作結果
 * @return 選択して OK したとき true。キャンセルや選択なしの終了は false。
 *         state の変更は MainFrame が結果を見て永続化する。
 */
bool Run(wxWindow *parent, hist::State &state, hist::Result &result);

}  // namespace hist_dialog

#endif  // NYANFI_GUI_HIST_DIALOG_H
