/**
 * @file tests/core/test_gui_function_list.cpp
 * @brief gui/function_list.h の判断ロジックの回帰テスト
 */
#include "doctest/doctest.h"

#include "gui/function_list.h"

using namespace function_list;

TEST_CASE("function_list: モードとコマンドパラメータを解決する")
{
	CHECK(NormalizeMode(-1) == Mode::Function);
	CHECK(NormalizeMode(0) == Mode::Function);
	CHECK(NormalizeMode(1) == Mode::UserDefined);
	CHECK(NormalizeMode(2) == Mode::MarkLine);
	CHECK(NormalizeMode(99) == Mode::Function);
	CHECK(ParseCommandOptions(_T("FF")).to_filter == true);
	CHECK(ParseCommandOptions(_T("FZ")).fuzzy == true);
	CHECK(ParseCommandOptions(_T("FF;FZ")).to_filter == true);
	CHECK(ParseCommandOptions(_T("FF;FZ")).fuzzy == true);
	CHECK(IsAvailable(Mode::MarkLine, true, false) == false);
	CHECK(IsAvailable(Mode::MarkLine, true, true) == true);
	CHECK(IsAvailable(Mode::Function, false, true) == false);
}

TEST_CASE("function_list: 関数・ユーザ定義・マーク行の一覧を作る")
{
	Source src;
	src.file_name = _T("sample.cpp");
	src.lines = {_T("int first()"), _T("  return 1;"), _T("void second(int x)")};
	src.function_pattern = _T("^[A-Za-z_].*\\(.*\\)");
	src.name_pattern = _T("^[A-Za-z_][A-Za-z0-9_]*");
	src.current_line = 1;
	src.marks = {0, 2};

	const auto funcs = BuildEntries(src, Mode::Function);
	CHECK(funcs.size() == 2);
	CHECK(funcs[0].line_no == 0);
	CHECK(funcs[1].line_no == 2);

	src.user_pattern = _T("return");
	const auto users = BuildEntries(src, Mode::UserDefined);
	CHECK(users.size() == 1);
	CHECK(users[0].line_no == 1);

	const auto marks = BuildEntries(src, Mode::MarkLine);
	CHECK(marks.size() == 2);
	CHECK(marks[1].text == _T("void second(int x)"));
}

TEST_CASE("function_list: フィルタと名前の取り出し")
{
	std::vector<Entry> entries = {
		{_T("AlphaFunction();"), 4},
		{_T("BetaFunction();"), 9},
		{_T("Gamma();"), 12},
	};
	const FilterOptions fuzzy{true, false, false};
	const auto filtered = FilterEntries(entries, _T("lpha"), fuzzy);
	CHECK(filtered.size() == 1);
	CHECK(filtered[0].line_no == 4);

	const FilterOptions regex{false, true, true};
	const auto rx = FilterEntries(entries, _T("^(Alpha|Beta)"), regex);
	CHECK(rx.size() == 2);

	CHECK(NameOnlyText(entries[0].text, true, _T("^[A-Za-z_][A-Za-z0-9_]*")) == _T("AlphaFunction"));
	CHECK(SelectNearest(entries, 8) == 0);
	CHECK(SelectNearest(entries, -1) == 0);
}
