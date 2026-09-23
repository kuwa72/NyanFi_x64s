/**
 * @file tests/core/test_gui_edit_hist.cpp
 * @brief gui/edit_hist.h の表示・検索・削除・設定判定の回帰テスト
 *
 * VCL の実測根拠は gui/edit_hist.h 冒頭の対応表を参照。
 */
#include "doctest/doctest.h"

#include "gui/edit_hist.h"
#include "temp_dir.h"

using nyanfi_test::TempDir;

TEST_CASE("edit_hist: 編集履歴の表示モードで件数と順序を保つ")
{
	history::HistoryList h(10);
	h.Add(_T("C:\\Work\\old.txt"));
	h.Add(_T("C:\\Work\\sub\\new.txt"));
	h.Add(_T("C:\\Other\\other.txt"));

	edit_hist::Context all;
	all.current_path = _T("C:\\Work\\");
	all.mode = edit_hist::Mode::All;
	const auto all_rows = edit_hist::BuildEntries(h, all);
	REQUIRE(all_rows.size() == 3);
	CHECK(all_rows[0].path == UnicodeString(_T("C:\\Other\\other.txt")));
	CHECK(all_rows[1].path == UnicodeString(_T("C:\\Work\\sub\\new.txt")));
	CHECK(all_rows[2].path == UnicodeString(_T("C:\\Work\\old.txt")));
	CHECK(all_rows[1].name == UnicodeString(_T("new.txt")));
	CHECK(all_rows[1].location == UnicodeString(_T("C:\\Work\\sub\\")));

	all.mode = edit_hist::Mode::CurrentPath;
	const auto below_rows = edit_hist::BuildEntries(h, all);
	REQUIRE(below_rows.size() == 2);
	CHECK(below_rows[0].path == UnicodeString(_T("C:\\Work\\sub\\new.txt")));
	CHECK(below_rows[1].path == UnicodeString(_T("C:\\Work\\old.txt")));

	all.mode = edit_hist::Mode::CurrentDirectory;
	const auto same_rows = edit_hist::BuildEntries(h, all);
	REQUIRE(same_rows.size() == 1);
	CHECK(same_rows[0].path == UnicodeString(_T("C:\\Work\\old.txt")));
	CHECK(edit_hist::ModeTitle(edit_hist::Mode::All) == UnicodeString(_T("最近編集したファイル一覧")));
}

TEST_CASE("edit_hist: 検索はファイル名だけを対象に通常は大小無視の正規表現を使う")
{
	history::HistoryList h(10);
	h.Add(_T("C:\\Other\\Alpha.TXT"));
	h.Add(_T("C:\\Work\\alpha.md"));
	h.Add(_T("C:\\Work\\Beta.txt"));

	edit_hist::Context context;
	context.current_path = _T("C:\\Work\\");
	context.filter = _T("alpha");
	context.migemo = false;
	const auto literal = edit_hist::BuildEntries(h, context);
	REQUIRE(literal.size() == 2);
	CHECK(literal[0].path == UnicodeString(_T("C:\\Work\\alpha.md")));
	CHECK(literal[1].path == UnicodeString(_T("C:\\Other\\Alpha.TXT")));

	context.filter = _T("^alpha\\.(txt|md)$");
	context.migemo = true;
	const auto regex = edit_hist::BuildEntries(h, context);
	REQUIRE(regex.size() == 2);
	CHECK(regex[0].path == UnicodeString(_T("C:\\Work\\alpha.md")));
	CHECK(regex[1].path == UnicodeString(_T("C:\\Other\\Alpha.TXT")));

	context.filter = _T("C:\\\\Other");
	context.migemo = false;
	CHECK(edit_hist::BuildEntries(h, context).empty());
}

TEST_CASE("edit_hist: 項目削除は既存MRUの大小無視削除を使う")
{
	history::HistoryList h(10);
	h.Add(_T("C:\\A.txt"));
	h.Add(_T("C:\\B.txt"));
	h.Add(_T("C:\\A.txt"));

	REQUIRE(edit_hist::RemoveEntry(h, _T("c:\\a.txt")));
	CHECK(h.Entries().size() == 1);
	CHECK(h.Entries()[0] == UnicodeString(_T("C:\\B.txt")));
	CHECK_FALSE(edit_hist::RemoveEntry(h, _T("C:\\missing.txt")));
}

TEST_CASE("edit_hist: 表示しないパスの設定で該当するディレクトリを整理する")
{
	history::HistoryList h(10);
	h.Add(_T("C:\\Temp\\old.txt"));
	h.Add(_T("C:\\Project\\keep.txt"));
	h.Add(_T("D:\\TEMP\\old.txt"));

	CHECK(edit_hist::ApplyExcludedPaths(h, _T("C:\\Temp;Project")) == 2);
	REQUIRE(h.Entries().size() == 1);
	CHECK(h.Entries()[0] == UnicodeString(_T("D:\\TEMP\\old.txt")));
	CHECK(edit_hist::ApplyExcludedPaths(h, EmptyStr) == 0);
}

TEST_CASE("edit_hist: VCLのパラメータと設定iniを往復する")
{
	edit_hist::Request request = edit_hist::ParseRequest(_T("FF;AC"));
	CHECK(request.focus_filter);
	CHECK(request.clear_all);
	CHECK_FALSE(edit_hist::ParseRequest(_T("FF")).clear_all);

	TempDir tmp;
	UsrIniFile ini(tmp.file(_T("edit-hist.ini")));
	edit_hist::Preferences saved;
	saved.mode = edit_hist::Mode::CurrentDirectory;
	saved.migemo = true;
	saved.status_bar = false;
	saved.filter_width = 275;
	saved.excluded_paths = _T("C:\\Temp;D:\\Cache");
	edit_hist::SavePreferences(ini, saved);
	REQUIRE(ini.UpdateFile());

	UsrIniFile reread(tmp.file(_T("edit-hist.ini")));
	edit_hist::Preferences loaded;
	edit_hist::LoadPreferences(reread, loaded);
	CHECK(loaded.mode == edit_hist::Mode::CurrentDirectory);
	CHECK(loaded.migemo);
	CHECK_FALSE(loaded.status_bar);
	CHECK(loaded.filter_width == 275);
	CHECK(loaded.excluded_paths == UnicodeString(_T("C:\\Temp;D:\\Cache")));
}

TEST_CASE("edit_hist: 操作ボタンの有効条件と件数がVCL相当になる")
{
	CHECK(edit_hist::CanDelete(0, 1));
	CHECK_FALSE(edit_hist::CanDelete(-1, 1));
	CHECK(edit_hist::CanClearAll(1));
	CHECK_FALSE(edit_hist::CanClearAll(0));
	CHECK(edit_hist::StatusText(2, 5, 1) == UnicodeString(_T("表示: 2/5  選択: 1")));
	CHECK(edit_hist::ModeIndex(edit_hist::Mode::CurrentPath) == 1);
	CHECK(edit_hist::ModeFromIndex(2) == edit_hist::Mode::CurrentDirectory);
	CHECK(edit_hist::ModeFromIndex(99) == edit_hist::Mode::All);
}
