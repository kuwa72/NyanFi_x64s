/**
 * @file gui/app_dialog.h
 * @brief アプリケーション一覧・ランチャーダイアログ (wx 依存)
 *
 * @details VCL 版の該当は `src/AppDlg.cpp` (`TAppListDlg`。起動中アプリ一覧+
 *          ランチャー)。対応コマンドは `AppList`
 *          (`src/MainFrm.cpp:13499` の AO/LO/LI/FA/FL/FI/FZ/AS。パラメータ
 *          解決は `gui/f_batch7_ops.h` の `ParseAppListOpts` が既に持ち、
 *          表示構成の解決は `gui/app_list.h` の `ResolveAppView` が持つ)。
 *          ここでは一覧とランチャーの表示・選択・起動だけを wx で再構成した
 *          もの (gui/sync_dialog.h と同じ作り)。
 *          判断部分は wx 非依存の `gui/app_list.h` が持ち、ここでは表示と
 *          入力だけを受け持つ。
 *          Win32 `EnumWindows` による実列挙は実機APIのため薄い層
 *          (`EnumerateApps`) に隔離し、純粋部 (`app_list.h`) のみテストする。
 *          未移植 (未実装扱い。`gui/app_list.h` の説明を参照):
 *          - DWM サムネイル・UWP 解決・PEB コマンドライン取得
 *          - ウィンドウ実操作 (最小化/最大化/閉じる/強制終了) は確認+警告の
 *            簡略版 (実操作は行わない)
 *          - OptDlg (4729行・設定全体のため対象外)、FtpDlg/GitView
 *            (外部連携のため対象外)
 */
#ifndef NYANFI_GUI_APP_DIALOG_H
#define NYANFI_GUI_APP_DIALOG_H

#include <vector>
#include <wx/wx.h>

#include "gui/app_list.h"
#include "gui/f_batch7_ops.h"

namespace app_dialog {

//---------------------------------------------------------------------------
// ダイアログの結果 (VCL の JumpFileName/JumpPathName/LaunchFileName 等に相当)
//---------------------------------------------------------------------------
struct AppResult {
	UnicodeString launch_file;  //!< 実行するランチャー項目 (LaunchFileName)
	UnicodeString jump_file;    //!< 実行ファイル位置へ移動 (JumpFileName)
	UnicodeString jump_path;    //!< ランチャーディレクトリへ移動 (JumpPathName)
	bool minimize_app = false;  //!< NyanFi を最小化 (mrRetry 相当)
	bool switch_to_nyan = false;//!< NyanFi 自体への切り替え (isNyan 相当)
};

/**
 * @brief 起動中ウィンドウを列挙する (Win32 実機APIの薄い層)
 * @details Windows では `EnumWindows` で取得し `app_list::ShouldListWindow`
 *          で絞る。非 Windows では空を返す (CI/単体テスト用)。
 * @param exc_text 除外テキスト (`ExcAppText` の `;` 区切り)
 */
std::vector<app_list::AppEntry> EnumerateApps(const UnicodeString &exc_text);

/**
 * @brief アプリケーション一覧・ランチャーを表示する
 * @param parent 親ウィンドウ
 * @param opts AppList パラメータの解決結果 (AO/LO/LI/FA/FL/FI/FZ/AS)
 * @param apps 起動中アプリ一覧 (EnumerateApps の結果。空でも表示可)
 * @param launch_dir ランチャーのトップディレクトリ (.lnk/.url を走査)
 * @param[out] out 選択結果 (戻り値が true のときだけ有効)
 * @return true OK で閉じた (out が有効)。キャンセルなら false
 */
bool Run(wxWindow *parent, const f_batch7_ops::AppListOpts &opts,
         const std::vector<app_list::AppEntry> &apps, const UnicodeString &launch_dir,
         AppResult &out);

}  // namespace app_dialog

#endif  // NYANFI_GUI_APP_DIALOG_H
