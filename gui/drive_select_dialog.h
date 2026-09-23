/**
 * @file gui/drive_select_dialog.h
 * @brief ドライブ一覧/選択ダイアログ (wx 依存)
 *
 * @details VCL の `TSelDriveDlg` は `src/DriveDlg.cpp:23-552`、
 *          UI は `src/DriveDlg.dfm:1-310`。実呼び出しは
 *          `src/MainFrm.cpp:16855-16866` (`DriveListActionExecute`) で、
 *          コマンド表は `src/usr_cmdlist.cpp:81` の `F:DriveList`。
 *          wx 版の判断は `gui/drive_select.h`、Windows の列挙/容量取得は
 *          この dialog の薄い層に置く。
 *
 *          未移植 (未実装扱い): Global.cpp のアイコン/バス情報/仮想ドライブ/
 *          ejectable 完全検出、Eject/Tray/Property/Explorer、NetConnect/
 *          NetDisconnect、VolumeLabel、DriveGraph/popup、列幅・位置 ini、
 *          MainFrm.cpp:1940 からの定期更新の VCL 互換再現。ボタンは黙って
 *          消さず警告を表示する。
 */
#ifndef NYANFI_GUI_DRIVE_SELECT_DIALOG_H
#define NYANFI_GUI_DRIVE_SELECT_DIALOG_H

#include <vector>
#include <wx/wx.h>

#include "gui/drive_select.h"

namespace drive_select_dialog {

/// 実ドライブを列挙する。既存 core の get_available_drive_list/get_drive_type
/// を使い、論理ドライブと空き容量を Windows API で薄く採取する。
std::vector<drive_select::DriveInfo> EnumerateDrives();

struct Context {
	UnicodeString current_path;             //!< 現在のディレクトリ (初期選択)
	drive_select::Options options;          //!< 初期オプション
};

struct Result {
	UnicodeString path;                     //!< 選択した移動先
	int selected = -1;                      //!< 選択した一覧の添字
	drive_select::Options options;          //!< OK 時に確定したオプション
};

/// ダイアログを表示する。選択して OK した場合のみ true。
bool Run(wxWindow *parent, const std::vector<drive_select::DriveInfo> &drives,
         const Context &context, Result &result);

}  // namespace drive_select_dialog

#endif  // NYANFI_GUI_DRIVE_SELECT_DIALOG_H
