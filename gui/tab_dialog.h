/**
 * @file gui/tab_dialog.h
 * @brief タブの設定ダイアログ (wx 依存)
 *
 * @details VCL 版の該当は `src/TabDlg.cpp` (`TTabSetDlg`。タブ一覧の1行の
 *          うちキャプション・アイコン・左右のホーム・ワークリスト指定を
 *          編集する)。判断部分 (`TabList` CSV との変換) は wx 非依存の
 *          `gui/tab_settings.h` が持ち、ここでは入力 UI だけを wx で
 *          再構成したもの (gui/dupl_dialog.h と同じ作り)。
 *          アイコンのプレビュー (`usr_SH->draw_SmallIcon`) は未移植のため
 *          扱わない (未実装扱い)
 */
#ifndef NYANFI_GUI_TAB_DIALOG_H
#define NYANFI_GUI_TAB_DIALOG_H

#include <wx/wx.h>

#include "gui/tab_settings.h"

namespace tab_dialog {

/**
 * @brief タブの設定ダイアログを表示し、設定の入力を受け付ける
 * @param parent 親ウィンドウ
 * @param[in,out] settings 編集する設定 (戻り値が true のときだけ書き換わる)
 * @param current0 左の現在のディレクトリ (「現在のディレクトリを設定」用)
 * @param current1 右の現在のディレクトリ (同上)
 * @param group_title タブグループ名 (空でなければタイトルに付ける。
 *        VCL の `TabGroupName` の末尾要素に相当)
 * @return true OK で閉じた (settings が有効)。キャンセルなら false
 */
bool Run(wxWindow *parent, tab_settings::TabSettings &settings,
         const UnicodeString &current0, const UnicodeString &current1,
         const UnicodeString &group_title = EmptyStr);

}  // namespace tab_dialog

#endif  // NYANFI_GUI_TAB_DIALOG_H
