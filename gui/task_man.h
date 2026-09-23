/**
 * @file gui/task_man.h
 * @brief タスクマネージャダイアログの wx 非依存な判断ロジック
 *
 * @details VCL の実測元は `src/TaskDlg.cpp` (`TTaskManDlg`) と
 *          `src/TaskDlg.dfm`。実測した呼び出し位置/行番号は以下のとおり。
 *          - `src/TaskDlg.cpp:99-194` `Timer1Timer` が TaskThread と
 *            TaskReserveList を行へ変換し、状態/進捗/経過時間を更新する
 *          - `src/TaskDlg.cpp:292-317` `CancelTaskActionExecute/Update`
 *          - `src/TaskDlg.cpp:335-359` `PauseAction`/`RestartAction`
 *          - `src/TaskDlg.cpp:364-404` `Suspend`/`Start`/`ExtStart`
 *          - VCL からの呼び出しは `src/MainFrm.cpp:26801-26804`
 *            (`TaskManActionExecute` → `TTaskManDlg::ShowModal`)
 *          - コマンド表は `src/usr_cmdlist.cpp:302` の `FL:TaskMan`
 *
 *          wx 版が受け取るのは MainFrame が保持する `task_paused_` と
 *          cancel/suspend 状態だけで、実 TTaskThread は渡さない。状態操作の
 *          共通規則 (AnyPaused/PauseAllCaption/SetToggleAction 等) は既存
 *          `gui/f_misc_ops.h` を再利用する。
 *
 *          未移植 (未実装扱い):
 *          - TTaskThread の実体、コピー/移動の実行、進捗/速度/残り時間の
 *            250ms タイマー更新、TaskGrid の空スロット/キーボード移動
 *          - TaskReserveList の実データ、`StartReserve` による予約開始
 *          - 中断要求を実スレッドへ伝えること (ここでは状態フラグだけ)
 *          - `NotShowNoTask` による<TaskManActionUpdate>の非表示条件
 */
#ifndef NYANFI_GUI_TASK_MAN_H
#define NYANFI_GUI_TASK_MAN_H

#include <vector>

#include "gui/f_misc_ops.h"

namespace task_man {

/// MainFrame から渡す実タスク状態スナップショット。
struct State {
	std::vector<bool> paused;          //!< task_paused_ 相当
	std::vector<bool> cancel_requested; //!< 実スレッド未移植の中断要求状態
	int reserved_count = 0;             //!< TaskReserveList の件数 (現状 0)
	bool suspended = false;             //!< RsvSuspended 相当
};

/// ダイアログの1行。VCL の TaskGrid の列に対応する。
struct TaskRow {
	int number = 0;                    //!< # 列 (実行中タスクは1始まり)
	UnicodeString command;
	UnicodeString detail;
	int remaining = 0;                 //!< 残り項目数
	int progress = -1;                 //!< 0..100 (不明は -1)
	int elapsed_ms = 0;                //!< 経過時間
	int remaining_ms = -1;             //!< 残り時間 (不明は -1)
	int speed_bytes_per_sec = 0;
	bool paused = false;
	bool cancel_requested = false;
	bool preparing = false;
	bool fast = false;
	bool reserved = false;
	bool reserved_suspended = false;
};

/// ボタンの有効/無効判定結果。
struct Actions {
	bool cancel = false;
	bool cancel_all = false;
	bool pause = false;
	bool restart = false;
	bool suspend = false;
	bool start = false;
	bool ext_start = false;
};

/// State から一覧行を作る。実スレッドのメタデータがない分は明示的に未実装扱い。
std::vector<TaskRow> BuildRows(const State &state);

/// VCL の get_BusyTaskCount 相当。这里は pause/cancel を持つ行数を数える。
int BusyCount(const State &state);

/// pause 中の行数。
int PausedCount(const State &state);

/// VCL の `TaskPause ? ... : TaskCancel ? ...` の状態文字列。
UnicodeString StatusText(const TaskRow &row);

/// 選択行からボタンの有効性を決める。予約開始には空きスロット情報が必要。
Actions ResolveActions(const State &state, int selected, int reserved_count,
                       bool has_empty_slot, bool has_force_empty_slot);

/// 選択中タスクへ状態だけ pause/cancel を設定する。予約行は操作しない。
bool PauseSelected(State &state, int selected);
bool RestartSelected(State &state, int selected);
bool RequestCancel(State &state, int selected);

/// 全タスクへ状態だけ中断要求を立てる (実スレッドは未移植)。
void RequestCancelAll(State &state);

/// f_misc_ops::AnyPaused/DecidePauseAllTarget を使う全タスク pause/resume。
void PauseAll(State &state, const UnicodeString &param);

/// f_misc_ops::ToggleFlagValue を使う予約保留/解除。実予約開始は未実装。
bool ToggleSuspend(State &state, const UnicodeString &param);

/// ダイアログ下部の要約 (件数表示)。
UnicodeString Summary(const State &state);

}  // namespace task_man

#endif  // NYANFI_GUI_TASK_MAN_H
