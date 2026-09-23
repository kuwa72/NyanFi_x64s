/**
 * @file tests/core/test_gui_task_man.cpp
 * @brief gui/task_man.h の wx 非依存判断ロジックの回帰テスト
 *
 * VCL の実測元:
 *   - src/TaskDlg.cpp:99-194 (Timer1Timer のタスク一覧/状態表示)
 *   - src/TaskDlg.cpp:292-404 (中止・停止・再開・予約開始の各 Action)
 *   - src/MainFrm.cpp:26801-26804 (TaskMan の TTaskManDlg 呼び出し)
 *   - src/usr_cmdlist.cpp:302 (FL:TaskMan)
 *
 * TTaskThread の実体（コピー/移動、進捗タイマー）はまだ無いため、
 * ここでは MainFrame が保持する pause/cancel 状態と表示上の操作可否だけを固定する。
 */
#include "doctest/doctest.h"

#include "gui/task_man.h"

using namespace task_man;

TEST_CASE("BuildRows: MainFrame の pause/cancel 状態を VCL の行に落とす")
{
	State state;
	state.paused = {false, true, false};
	state.cancel_requested = {false, false, true};
	const std::vector<TaskRow> rows = BuildRows(state);
	REQUIRE(rows.size() == 3);
	CHECK(rows[0].number == 1);
	CHECK(rows[0].paused == false);
	CHECK(rows[0].cancel_requested == false);
	CHECK(rows[1].paused == true);
	CHECK(rows[2].cancel_requested == true);
	CHECK(rows[0].command == UnicodeString(_T("(実スレッド未移植)")));
	CHECK(ContainsText(rows[0].detail, _T("未実装扱い")));
}

TEST_CASE("StatusText: VCL の状態表示の優先順位を再現する")
{
	TaskRow row;
	row.paused = true;
	row.cancel_requested = true;
	CHECK(StatusText(row) == UnicodeString(_T("一旦停止中")));
	row.paused = false;
	CHECK(StatusText(row) == UnicodeString(_T("中断要求中")));
	row.cancel_requested = false;
	row.preparing = true;
	CHECK(StatusText(row) == UnicodeString(_T("準備中")));
	row.preparing = false;
	row.fast = true;
	CHECK(StatusText(row) == UnicodeString(_T("高速実行中")));
	row.fast = false;
	CHECK(StatusText(row) == UnicodeString(_T("実行中")));
}

TEST_CASE("ResolveActions: 選択行と状態から各ボタンの有効性を決める")
{
	State state;
	state.paused = {false, true};
	state.cancel_requested = {false, false};
	const Actions running = ResolveActions(state, 0, 0, false, false);
	CHECK(running.cancel);
	CHECK(running.pause);
	CHECK_FALSE(running.restart);
	CHECK_FALSE(running.start);
	CHECK_FALSE(running.ext_start);

	const Actions paused = ResolveActions(state, 1, 0, false, false);
	CHECK(paused.cancel);
	CHECK_FALSE(paused.pause);
	CHECK(paused.restart);

	state.cancel_requested[0] = true;
	CHECK_FALSE(ResolveActions(state, 0, 0, false, false).cancel);
	state.paused = {};
	state.cancel_requested = {};
	CHECK_FALSE(ResolveActions(state, -1, 0, false, false).cancel);
	CHECK_FALSE(ResolveActions(state, 0, 0, false, false).cancel_all);
}

TEST_CASE("ResolveActions: 予約行は空きスロットと保留状態がない限り開始不可")
{
	State state;
	state.reserved_count = 2;
	state.suspended = true;
	const Actions enabled = ResolveActions(state, 0, 2, true, true);
	CHECK(enabled.start);
	CHECK(enabled.ext_start);

	const Actions no_slot = ResolveActions(state, 0, 2, false, false);
	CHECK_FALSE(no_slot.start);
	CHECK_FALSE(no_slot.ext_start);

	state.suspended = false;
	CHECK_FALSE(ResolveActions(state, 0, 2, true, true).start);
	CHECK_FALSE(ResolveActions(state, 2, 2, true, true).start);
}

TEST_CASE("Pause/Restart/Cancel: 選択中タスクの状態だけを更新する")
{
	State state;
	state.paused = {false, false};
	state.cancel_requested = {false, false};
	CHECK(PauseSelected(state, 0));
	CHECK(state.paused[0]);
	CHECK(RestartSelected(state, 0));
	CHECK_FALSE(RestartSelected(state, 0));
	CHECK_FALSE(state.paused[0]);
	CHECK(RequestCancel(state, 0));
	CHECK(state.cancel_requested[0]);
	CHECK_FALSE(RequestCancel(state, 0));
	CHECK_FALSE(PauseSelected(state, 2));
	CHECK_FALSE(RestartSelected(state, -1));
	CHECK(RequestCancel(state, 1));
}

TEST_CASE("PauseAll: f_misc_ops の any/反転規則を再利用する")
{
	State state;
	state.paused = {false, false};
	PauseAll(state, EmptyStr);
	CHECK(state.paused[0]);
	CHECK(state.paused[1]);
	PauseAll(state, _T("OFF"));
	CHECK_FALSE(state.paused[0]);
	CHECK_FALSE(state.paused[1]);
	PauseAll(state, _T("ON"));
	CHECK(state.paused[0]);
	CHECK(state.paused[1]);
}

TEST_CASE("RequestCancelAll: 実タスクを触らず全行に中断要求だけ立てる")
{
	State state;
	state.paused = {false, true};
	state.cancel_requested = {false, false};
	RequestCancelAll(state);
	CHECK(state.cancel_requested[0]);
	CHECK(state.cancel_requested[1]);
	CHECK(state.paused[0] == false);
	CHECK(state.paused[1] == true);
}

TEST_CASE("ToggleSuspend/Summary: 予約保留と件数表示")
{
	State state;
	state.paused = {false, true};
	state.cancel_requested = {};
	CHECK_FALSE(state.suspended);
	ToggleSuspend(state, EmptyStr);
	CHECK(state.suspended);
	CHECK_FALSE(ToggleSuspend(state, _T("OFF")));
	CHECK_FALSE(state.suspended);
	CHECK(ContainsText(Summary(state), _T("2")));
	CHECK(Summary(State{}) == UnicodeString(_T("実行中のタスクはありません")));
}
