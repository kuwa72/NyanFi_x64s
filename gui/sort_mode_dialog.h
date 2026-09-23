/**
 * @file gui/sort_mode_dialog.h
 * @brief ソート設定ダイアログ (wx 依存)
 *
 * @details VCL の `src/SrtModDlg.cpp` (`TSortModeDlg`) を wx で再構成したもの。
 *          VCL の接続は `src/MainFrm.cpp:26360-26365` (ファイル一覧) と
 *          `src/MainFrm.cpp:35871-35872` (画像ビューア) で実測した。
 *          判断は wx 非依存の `gui/sort_mode.h`、ここでは入力と表示だけを持つ。
 *
 *          未移植 (未実装扱い):
 *          - 2段ソートの実比較、論理/自然順/優先拡張子リストの完全実装
 *          - 更新日時アクセラレータと同一キー即時終了のフック
 *          - 結果リストの場所順 (`SortDlg_L`) と画像ビューア一覧の並べ替え
 *          - VCL のキーボード専用即時終了 (F/E/D/S/A/U) は MainFrame の
 *            コマンドパラメータ処理で受け付ける
 */
#ifndef NYANFI_GUI_SORT_MODE_DIALOG_H
#define NYANFI_GUI_SORT_MODE_DIALOG_H

#include <wx/wx.h>

#include "gui/sort_mode.h"

namespace sort_mode_dialog {

/**
 * @brief ソート設定を入力する
 * @param parent 親ウィンドウ
 * @param[in,out] options 初期値。OK 時に正規化済み設定を返す
 * @param show_dir_options ファイル一覧のときだけディレクトリ設定を表示するか
 * @return true OK。キャンセルなら false
 */
bool Run(wxWindow *parent, sort_mode::Options &options, bool show_dir_options = true);

}  // namespace sort_mode_dialog

#endif  // NYANFI_GUI_SORT_MODE_DIALOG_H
