/** @file tests/core/test_gui_hist.cpp */
#include "doctest/doctest.h"

#include "gui/hist.h"
#include "UIniFile.h"
#include "temp_dir.h"

using nyanfi_test::TempDir;

TEST_CASE("hist: DirHistory の実在パラメータをモードへ変換する")
{
	hist::Mode mode = hist::Mode::Current;
	REQUIRE(hist::ParseMode(_T("GA"), mode));
	CHECK(mode == hist::Mode::All);
	REQUIRE(hist::ParseMode(_T("GS"), mode));
	CHECK(mode == hist::Mode::All);
	REQUIRE(hist::ParseMode(_T("FM"), mode));
	CHECK(mode == hist::Mode::Search);
	REQUIRE(hist::ParseMode(_T("RD"), mode));
	CHECK(mode == hist::Mode::Recent);
	CHECK_FALSE(hist::ParseMode(_T("AC"), mode));
	CHECK_FALSE(hist::ParseMode(_T("GC"), mode));
	CHECK_FALSE(hist::ParseMode(_T("XX"), mode));
}

TEST_CASE("hist: 検索モードだけフィルタを使う")
{
	hist::Entry a;
	a.path = _T("C:\\Alpha\\One");
	hist::Entry b;
	b.path = _T("C:\\Beta\\Two");
	hist::State state;
	state.mode = hist::Mode::Search;
	state.entries = {a, b};
	state.filter = _T("alpha");

	const hist::View view = hist::BuildView(state, false);
	REQUIRE(view.visible.size() == 1);
	CHECK(view.visible[0] == 0);
	CHECK(hist::MatchesFilter(a, _T("alpha"), false));
	CHECK_FALSE(hist::MatchesFilter(a, _T("alpha"), true));
	CHECK(ContainsText(hist::ModeTitle(hist::Mode::Search, 1, 2), _T("1/2")));
}

TEST_CASE("hist: 表示行の削除とコピーは元の項目を指す")
{
	hist::State state;
	state.mode = hist::Mode::Search;
	state.entries.resize(3);
	for (int i = 0; i < 3; ++i) state.entries[static_cast<std::size_t>(i)].path = UnicodeString().sprintf(_T("D%d\\"), i);
	state.filter = _T("D1");
	const hist::View view = hist::BuildView(state, false);
	REQUIRE(view.visible.size() == 1);
	const std::vector<UnicodeString> lines = hist::CopyLines(state, view);
	REQUIRE(lines.size() == 1);
	CHECK(lines[0] == UnicodeString(_T("D1\\")));
	hist::RemoveVisible(state.entries, view.visible);
	REQUIRE(state.entries.size() == 2);
	CHECK(state.entries[0].path == UnicodeString(_T("D0\\")));
	CHECK(state.entries[1].path == UnicodeString(_T("D2\\")));
}

TEST_CASE("hist: ディレクトリ追加は区切り文字と重複を正規化する")
{
	std::vector<hist::Entry> entries;
	hist::Entry old;
	old.path = _T("C:\\Foo");
	entries.push_back(old);
	hist::MergeDirectories(entries, {_T("C:\\Foo\\"), _T("C:\\Bar"), EmptyStr, _T("C:\\Bar\\")});
	REQUIRE(entries.size() == 2);
	CHECK(entries[0].path == UnicodeString(_T("C:\\Foo\\")));
	CHECK(get_csv_item(entries[0].record, 0) == UnicodeString(_T("C:\\Foo\\")));
	CHECK(get_csv_item(entries[0].record, 1) == UnicodeString(_T("0")));
	CHECK(entries[1].path == UnicodeString(_T("C:\\Bar\\")));
	CHECK(get_csv_item(entries[1].record, 0) == UnicodeString(_T("C:\\Bar\\")));
	CHECK(get_csv_item(entries[1].record, 1) == UnicodeString(_T("0")));
}

TEST_CASE("hist: 操作ボタンの有効条件")
{
	CHECK(hist::CanClearAll(hist::Mode::Current, 1));
	CHECK_FALSE(hist::CanClearAll(hist::Mode::Stack, 1));
	CHECK(hist::CanClearFiltered(hist::Mode::Search, 1, 2));
	CHECK_FALSE(hist::CanClearFiltered(hist::Mode::Search, 2, 2));
	CHECK(hist::CanCopy(1));
	CHECK_FALSE(hist::CanCopy(0));
	CHECK(hist::CanProperty(0));
	CHECK_FALSE(hist::CanProperty(-1));
	CHECK(StartsText(_T("1"), hist::DisplayWithAccelerator(
		hist::Entry{_T("C:\\X\\"), EmptyStr, EmptyStr, false}, 0)));
}

TEST_CASE("hist: 全体履歴と設定を wx 専用 ini に往復する")
{
	TempDir tmp;
	UsrIniFile ini(tmp.file(_T("hist-dialog.ini")));
	hist::Store store;
	hist::Entry e;
	e.path = _T("C:\\Memo\\日本語\\");
	e.record = _T("\"C:\\Memo\\日本語\\\",0");
	store.MutableEntries().push_back(e);
	store.SaveToIni(ini);
	hist::Preferences prefs;
	prefs.migemo = true;
	hist::SavePreferences(ini, prefs);
	REQUIRE(ini.UpdateFile());

	UsrIniFile reread(tmp.file(_T("hist-dialog.ini")));
	hist::Store got;
	got.LoadFromIni(reread);
	REQUIRE(got.Entries().size() == 1);
	CHECK(got.Entries()[0].path == e.path);
	CHECK(got.Entries()[0].record == e.record);
	hist::Preferences got_prefs;
	hist::LoadPreferences(reread, got_prefs);
	CHECK(got_prefs.migemo);
}
