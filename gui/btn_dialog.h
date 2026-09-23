/**
 * @file gui/btn_dialog.h
 * @brief ツールバーボタン設定ダイアログ (wx 依存)
 *
 * @details VCL の `TToolBtnDlg` (`src/BtnDlg.h:23-88`,
 *          `src/BtnDlg.dfm:1-227`) の CSV 編集・並び替え UI を移植したもの。
 *          実保存先は wx 専用 ini で、MainFrame が `btn::Store` を受け渡す。
 *
 *          未移植 (未実装扱い): ドラッグ&ドロップ、シェルアイコン描画、
 *          コマンド/アイコンの VCL 参照ダイアログ、ファイル編集、
 *          ExtMenu/ExtTool エイリアスの解決。
 */
#ifndef NYANFI_GUI_BTN_DIALOG_H
#define NYANFI_GUI_BTN_DIALOG_H

#include <wx/wx.h>

#include "gui/btn.h"

namespace btn_dialog {

/// 編集が OK なら items を更新して true。キャンセルなら false。
bool Run(wxWindow *parent, btn::Mode mode,
         const std::vector<UnicodeString> &commands,
         std::vector<btn::Item> &items, int initial_index = -1);

}  // namespace btn_dialog

#endif  // NYANFI_GUI_BTN_DIALOG_H
