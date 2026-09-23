/**
 * @file gui/backup_dialog.h
 * @brief バックアップ設定ダイアログ (wx 依存)
 *
 * @details VCL の `src/BakDlg.cpp` (`TBackupDlg`) の DFM 相当の項目を wx で
 *          再構成したもの。接続先は `src/MainFrm.cpp:13715-13755`。
 *          設定の CSV/日付条件/同期先解決は wx 非依存の
 *          `gui/backup_settings.h`。
 *
 *          未移植 (未実装扱い):
 *          - 実バックアップタスクの実行
 *          - マスク入力欄の履歴永続化
 *          - コマンドファイルの実保存 (ボタン押下時は警告だけ)
 */
#ifndef NYANFI_GUI_BACKUP_DIALOG_H
#define NYANFI_GUI_BACKUP_DIALOG_H

#include <vector>
#include <wx/wx.h>

#include "gui/backup_settings.h"

namespace backup_dialog {

struct Context {
	UnicodeString source_dir;
	UnicodeString dest_dir;
	std::vector<sync_dirs::SyncEntry> sync_settings;
};

/// ダイアログを表示する。setup list/選択位置は保存・削除操作で更新される
bool Run(wxWindow *parent, const Context &context, backup_settings::Options &options,
         std::vector<backup_settings::Setup> &setups, int &selected_index);

}  // namespace backup_dialog

#endif  // NYANFI_GUI_BACKUP_DIALOG_H
