/**
 * @file gui/pack_dialog.h
 * @brief アーカイブ作成設定ダイアログ (wx 依存)
 *
 * @details VCL の `src/PackDlg.cpp` (`TPackArcDlg`) の DFM 相当の入力項目を
 *          wx で再構成したもの。接続先は `src/MainFrm.cpp:23339-23361`。
 *          設定解決は wx 非依存の `gui/pack_settings.h`。
 *
 *          未移植 (未実装扱い):
 *          - 実際の圧縮/パスワード/追加スイッチ/自己解凍の実行
 *          - 同名書庫への追加・削除後の再作成
 *          - ディレクトリごとの圧縮
 */
#ifndef NYANFI_GUI_PACK_DIALOG_H
#define NYANFI_GUI_PACK_DIALOG_H

#include <wx/wx.h>

#include "gui/pack_settings.h"

namespace pack_dialog {

/// VCL MainFrm.cpp:23339 の PackArcDlg に渡す初期状態
struct Context {
	UnicodeString default_name;
	pack_settings::Availability availability;
	bool per_directory_available = false;
};

/// ダイアログを表示して設定を受け取る
bool Run(wxWindow *parent, pack_settings::Options &options, const Context &context);

}  // namespace pack_dialog

#endif  // NYANFI_GUI_PACK_DIALOG_H
