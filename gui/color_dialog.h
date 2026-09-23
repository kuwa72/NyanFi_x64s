/**
 * @file gui/color_dialog.h
 * @brief 配色ダイアログ (wx 依存)
 *
 * @details VCL 版の該当は `src/ColDlg.cpp` (`TColorDlg`。テキストビューアの
 *          配色37項目の一覧・色の参照・無効化)。判断部分は wx 非依存の
 *          `gui/color_settings.h` が持ち、ここでは一覧の表示・色の選択・
 *          無効化だけを wx で再構成したもの (gui/sync_dialog.h と同じ作り)。
 *          スポイト・スウォッチ・INI のインポート/エクスポート・全体への反映
 *          は未移植のため扱わない (未実装扱い。`gui/color_settings.h` 参照)
 */
#ifndef NYANFI_GUI_COLOR_DIALOG_H
#define NYANFI_GUI_COLOR_DIALOG_H

#include <vector>
#include <wx/wx.h>

#include "gui/color_settings.h"

namespace color_dialog {

/**
 * @brief 配色ダイアログを表示し、配色の編集を受け付ける
 * @param parent 親ウィンドウ
 * @param[in,out] entries 配色 (OK 時に確定内容で置き換わる。
 *        不足項目は無効値で補われる)
 * @return true OK で閉じた (entries が有効)。キャンセルなら false
 */
bool Run(wxWindow *parent, std::vector<color_settings::ColorEntry> &entries);

}  // namespace color_dialog

#endif  // NYANFI_GUI_COLOR_DIALOG_H
