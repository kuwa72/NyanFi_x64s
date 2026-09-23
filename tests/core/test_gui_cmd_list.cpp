/**
 * @file tests/core/test_gui_cmd_list.cpp
 * @brief gui/cmd_list.h の判断ロジックの回帰テスト
 */
#include "doctest/doctest.h"

#include "gui/cmd_list.h"

using namespace cmd_list;

TEST_CASE("cmd_list: コマンドファイル名の正規化と判定")
{
	CHECK(IsCommandFile(_T("run.NBT")) == true);
	CHECK(IsCommandFile(_T("run.txt")) == false);
	CHECK(NormalizeCommandPath(_T("@\"C:\\NyanFi\\run.nbt\"")) == _T("C:\\NyanFi\\run.nbt"));
	CHECK(NormalizeCommandPath(_T("\"C:\\NyanFi\\run.nbt\"")) == _T("C:\\NyanFi\\run.nbt"));
	CHECK(MakeExecutionCommand(_T("C:\\NyanFi\\run.nbt")) == _T("ExeCommands_\"@C:\\NyanFi\\run.nbt\""));
	CHECK(MakeEditCommand(_T("C:\\NyanFi\\run.nbt")) == _T("FileEdit_\"C:\\NyanFi\\run.nbt\""));
}

TEST_CASE("cmd_list: ファイル一覧を正規表現・あいまい・通常検索で絞る")
{
	const std::vector<Entry> all = {
		{_T("C:\\NyanFi\\alpha.nbt"), _T("alpha.nbt"), _T(";alpha"), 10, 2},
		{_T("C:\\NyanFi\\beta.nbt"), _T("beta.nbt"), _T(";beta"), 20, 0},
		{_T("C:\\NyanFi\\gamma.nbt"), _T("gamma.nbt"), _T(";gamma"), 30, 4},
	};
	const auto exact = FilterEntries(all, _T("beta"), {false, false, false});
	CHECK(exact.size() == 1);
	CHECK(exact[0].name == _T("beta.nbt"));

	const auto fuzzy = FilterEntries(all, _T("gma"), {true, false, false});
	CHECK(fuzzy.size() == 1);
	CHECK(fuzzy[0].name == _T("gamma.nbt"));

	const auto rx = FilterEntries(all, _T("^(alpha|gamma)"), {false, true, true});
	CHECK(rx.size() == 2);
}

TEST_CASE("cmd_list: 選択位置と自動実行条件")
{
	const std::vector<Entry> all = {
		{_T("C:\\NyanFi\\a.nbt"), _T("a.nbt"), _T(""), 0, 0},
		{_T("C:\\NyanFi\\b.nbt"), _T("b.nbt"), _T(""), 0, 0},
	};
	CHECK(FindSelected(all, _T("C:\\NyanFi\\b.nbt")) == 1);
	CHECK(FindSelected(all, _T("missing.nbt")) == -1);
	CHECK(CompareNatural(all[0], all[1]) < 0);
	CHECK(ShouldAutoExecute(/*select_only=*/false, /*confirm_execute=*/true,
	                        /*filter_focused=*/true, /*filter_empty=*/false,
	                        /*visible_count=*/1) == true);
	CHECK(ShouldAutoExecute(/*select_only=*/true, /*confirm_execute=*/true,
	                        /*filter_focused=*/true, /*filter_empty=*/false,
	                        /*visible_count=*/1) == false);
	CHECK(ShouldAutoExecute(/*select_only=*/false, /*confirm_execute=*/true,
	                        /*filter_focused=*/false, /*filter_empty=*/false,
	                        /*visible_count=*/1) == false);
}
