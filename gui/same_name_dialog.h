/**
 * @file gui/same_name_dialog.h
 * @brief 同名ファイル処理ダイアログ (wx 依存)
 *
 * @details VCL の `src/SameDlg.cpp` (`TSameNameDlg`) の5モード・全件適用・
 *          手動改名・情報欄を wx で再構成したもの。判断は
 *          wx 非依存の `gui/same_name.h`。
 *
 *          VCL の実測した接続先は MainFrm.cpp:3999-4019（タスクコピー）、
 *          9847-9867（解凍コピー）、37948-37979（ダウンロード）、
 *          38094-38142（アップロード）。wx 側では MainFrame の Copy/Move
 *          の同名衝突プレイスホルダから利用する。
 *
 *          未移植 (未実装扱い):
 *          - TTaskThread/FTP/Git の実コピー
 *          - VCL の情報欄 OwnerDraw（色・経路の特殊描画）
 *          - 衝突ごとの逐次再表示
 */
#ifndef NYANFI_GUI_SAME_NAME_DIALOG_H
#define NYANFI_GUI_SAME_NAME_DIALOG_H

#include <wx/wx.h>

#include "gui/same_name.h"

namespace same_name_dialog {

/// ダイアログの入力モードを受け取る
bool Run(wxWindow *parent, const same_name::Context &context, same_name::Options &options);

}  // namespace same_name_dialog

#endif  // NYANFI_GUI_SAME_NAME_DIALOG_H
