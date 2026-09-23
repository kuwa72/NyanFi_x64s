/**
 * @file gui/distribution_dialog.h
 * @brief 振り分けダイアログ (wx 依存)
 *
 * @details VCL `src/DistrDlg.cpp` の実測コメント:
 *          - `src/MainFrm.cpp:16369-16419` が `DistributionDlg` を開く
 *          - `src/DistrDlg.cpp:208-248` が登録ファイルの読込/保存
 *          - `src/DistrDlg.cpp:278-407` がプレビュー更新
 *
 *          登録規則・マスク照合・出力先は wx 非依存の
 *          `gui/distribution.h` にあり、ここは wx の入力・一覧・ファイル選択を
 *          受け持つ。
 *
 *          未移植 (未実装扱い):
 *          - VCL のOwnerDraw、進捗バー、リストファイル右クリック編集、
 *            ドラッグ並べ替え
 *          - \DT/\TS/\XT/\Z の日時・連番書式
 */
#ifndef NYANFI_GUI_DISTRIBUTION_DIALOG_H
#define NYANFI_GUI_DISTRIBUTION_DIALOG_H

#include <vector>
#include <wx/wx.h>

#include "gui/distribution.h"

namespace distribution_dialog {

/// Run が返す確定結果。
struct Result {
	std::vector<distribution::Rule> rules;
	distribution::Options options;
	distribution::Preview preview;
};

/**
 * @brief 振り分けダイアログを表示する。
 * @param parent 親ウィンドウ
 * @param items 現在のファイル/ディレクトリ一覧
 * @param initial_rules 既存の登録規則
 * @param initial_options 既存の同名処理・宛先設定
 * @param[out] out OK 時に確定結果。キャンセルなら変更しない
 * @return OK で閉じたなら true
 */
bool Run(wxWindow *parent, const std::vector<distribution::Input> &items,
         const std::vector<distribution::Rule> &initial_rules,
         const distribution::Options &initial_options, Result &out);

}  // namespace distribution_dialog

#endif  // NYANFI_GUI_DISTRIBUTION_DIALOG_H
