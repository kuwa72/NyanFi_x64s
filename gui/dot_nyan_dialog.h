/**
 * @file gui/dot_nyan_dialog.h
 * @brief .nyanfi 設定ダイアログ (wx 依存)
 *
 * @details VCL の `src/DotDlg.cpp` (`TDotNyanDlg`) のうち、テキスト入力と
 *          _ORDER/属性>_切替だけを表示する。設定の解析・保存は wx 非依存の
 *          `gui/dot_nyan.*`、実体の適用は `MainFrame` 側。
 *
 *          VCL 呼び出し位置 (grep 実測): `src/MainFrm.cpp:16792-16810`、
 *          DFM は `src/DotDlg.dfm:1-554`。未移植 (未実装扱い): 配色
 *          chooser/spuit、音声参照・テスト再生、画像参照、コマンドファイル選択、
 *          継承探索、削除、位置/履歴の ini 保存。
 */
#ifndef NYANFI_GUI_DOT_NYAN_DIALOG_H
#define NYANFI_GUI_DOT_NYAN_DIALOG_H

#include <wx/wx.h>

#include "gui/dot_nyan.h"

namespace dot_nyan_dialog {

/// .nyanfi の入力画面を表示する。OK で options を更新、取消なら false。
bool Run(wxWindow *parent, const UnicodeString &config_path, dot_nyan::Options &options);

}  // namespace dot_nyan_dialog

#endif  // NYANFI_GUI_DOT_NYAN_DIALOG_H
