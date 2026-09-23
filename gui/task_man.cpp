/**
 * @file gui/task_man.cpp
 * @brief gui/task_man.h の実装
 */
#include "gui/task_man.h"

#include <algorithm>

namespace task_man {

std::vector<TaskRow> BuildRows(const State &state)
{
	std::vector<TaskRow> rows;
	rows.reserve(state.paused.size() + static_cast<std::size_t>(std::max(0, state.reserved_count)));
	for (std::size_t i = 0; i < state.paused.size(); ++i) {
		TaskRow row;
		row.number = static_cast<int>(i) + 1;
		row.command = _T("(実スレッド未移植)");
		row.detail = _T("コピー/移動の実スレッドと進捗更新は未実装扱い");
		row.paused = state.paused[i];
		row.cancel_requested = i < state.cancel_requested.size() && state.cancel_requested[i];
		rows.push_back(row);
	}
	for (int i = 0; i < state.reserved_count; ++i) {
		TaskRow row;
		row.number = static_cast<int>(state.paused.size()) + i + 1;
		row.command = _T("(予約タスク未移植)");
		row.detail = _T("TaskReserveList と StartReserve は未実装扱い");
		row.reserved = true;
		row.reserved_suspended = state.suspended;
		rows.push_back(row);
	}
	return rows;
}

int BusyCount(const State &state)
{
	return static_cast<int>(state.paused.size());
}

int PausedCount(const State &state)
{
	return static_cast<int>(std::count(state.paused.begin(), state.paused.end(), true));
}

UnicodeString StatusText(const TaskRow &row)
{
	if (row.reserved) return row.reserved_suspended ? UnicodeString(_T("保留中"))
	                                               : UnicodeString(_T("待機中"));
	// src/TaskDlg.cpp:146-149 の優先順位を保つ。
	if (row.paused) return UnicodeString(_T("一旦停止中"));
	if (row.cancel_requested) return UnicodeString(_T("中断要求中"));
	if (row.preparing) return UnicodeString(_T("準備中"));
	if (row.fast) return UnicodeString(_T("高速実行中"));
	return UnicodeString(_T("実行中"));
}

Actions ResolveActions(const State &state, int selected, int reserved_count,
                       bool has_empty_slot, bool has_force_empty_slot)
{
	Actions actions;
	actions.cancel_all = f_misc_ops::CanCancelTasks(BusyCount(state));
	// Suspend は予約状態自体の切替なので、選択行が無くても利用可能。
	actions.suspend = true;

	const int active = BusyCount(state);
	if (selected >= 0 && selected < active) {
		const bool paused = selected < static_cast<int>(state.paused.size()) && state.paused[static_cast<std::size_t>(selected)];
		const bool cancelled = selected < static_cast<int>(state.cancel_requested.size()) &&
		                       state.cancel_requested[static_cast<std::size_t>(selected)];
		actions.cancel = !cancelled;
		actions.pause = !paused;
		actions.restart = paused;
		return actions;
	}

	if (selected >= active && selected < active + std::max(0, reserved_count)) {
		actions.start = state.suspended && has_empty_slot;
		actions.ext_start = has_force_empty_slot;
	}
	return actions;
}

bool PauseSelected(State &state, int selected)
{
	if (selected < 0 || selected >= BusyCount(state)) return false;
	std::vector<bool>::reference paused = state.paused[static_cast<std::size_t>(selected)];
	if (paused) return false;
	paused = true;
	return true;
}

bool RestartSelected(State &state, int selected)
{
	if (selected < 0 || selected >= BusyCount(state)) return false;
	std::vector<bool>::reference paused = state.paused[static_cast<std::size_t>(selected)];
	if (!paused) return false;
	paused = false;
	return true;
}

bool RequestCancel(State &state, int selected)
{
	if (selected < 0 || selected >= BusyCount(state)) return false;
	if (state.cancel_requested.size() < state.paused.size())
		state.cancel_requested.resize(state.paused.size(), false);
	std::vector<bool>::reference cancelled = state.cancel_requested[static_cast<std::size_t>(selected)];
	if (cancelled) return false;
	cancelled = true;
	return true;
}

void RequestCancelAll(State &state)
{
	if (state.cancel_requested.size() < state.paused.size())
		state.cancel_requested.resize(state.paused.size(), false);
	std::fill(state.cancel_requested.begin(), state.cancel_requested.end(), true);
}

void PauseAll(State &state, const UnicodeString &param)
{
	const bool target = f_misc_ops::DecidePauseAllTarget(
		f_misc_ops::AnyPaused(state.paused), param);
	std::fill(state.paused.begin(), state.paused.end(), target);
}

bool ToggleSuspend(State &state, const UnicodeString &param)
{
	state.suspended = f_misc_ops::ToggleFlagValue(state.suspended, param);
	return state.suspended;
}

UnicodeString Summary(const State &state)
{
	return f_misc_ops::FormatTaskSummary(BusyCount(state), PausedCount(state));
}

}  // namespace task_man
