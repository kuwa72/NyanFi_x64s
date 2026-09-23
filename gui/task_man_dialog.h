/**
 * @file gui/task_man_dialog.h
 * @brief タスクマネージャダイアログ (wx 依存)
 *
 * @details VCL の `TTaskManDlg` は `src/TaskDlg.cpp:21-404`、
 *          UI は `src/TaskDlg.dfm:1-249`。実呼び出しは
 *          `src/MainFrm.cpp:26801-26804`、コマンド表は
 *          `src/usr_cmdlist.cpp:302` の `FL:TaskMan`。
 *          状態と操作可否は wx 非依存の `gui/task_man.h`、実スレッド操作は
 *          未移植 (未実装扱い) として警告する。
 */
#ifndef NYANFI_GUI_TASK_MAN_DIALOG_H
#define NYANFI_GUI_TASK_MAN_DIALOG_H

#include <wx/wx.h>

#include "gui/task_man.h"

namespace task_man_dialog {

/**
 * @brief タスクマネージャを表示する。
 * @param parent 親ウィンドウ
 * @param[in,out] state MainFrame の pause/cancel/suspend 状態
 * @return true で OK/閉じる。状態操作は VCL と同じく即時反映される。
 *
 * @details TTaskThread/TaskReserveList は渡さず、state のスナップショットだけを
 *          表示する。コピー/移動の実処理、進捗タイマー、予約開始、
 *          NotShowNoTask による非表示判定は未移植 (未実装扱い)。
 */
bool Run(wxWindow *parent, task_man::State &state);

}  // namespace task_man_dialog

#endif  // NYANFI_GUI_TASK_MAN_DIALOG_H
