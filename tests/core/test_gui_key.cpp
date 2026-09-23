/** @file tests/core/test_gui_key.cpp */
#include "doctest/doctest.h"

#include "gui/key.h"
#include "UIniFile.h"
#include "temp_dir.h"

using nyanfi_test::TempDir;

TEST_CASE("key: usr_cmdlist のレコードと割り当てを解釈する")
{
	UnicodeString mode, command, description;
	REQUIRE(key::ParseCommandRecord(_T("F:ReloadList=再読み込み"), mode, command, description));
	CHECK(mode == UnicodeString(_T("F")));
	CHECK(command == UnicodeString(_T("ReloadList")));
	CHECK(description == UnicodeString(_T("再読み込み")));
	key::Entry entry;
	REQUIRE(key::ParseAssignment(_T("F5=ReloadList"), _T("再読み込み"), entry));
	CHECK(entry.key == UnicodeString(_T("F5")));
	CHECK(entry.command == UnicodeString(_T("ReloadList")));
	CHECK_FALSE(key::ParseAssignment(_T("F5="), EmptyStr, entry));
}

TEST_CASE("key: + と ~ を VCL 相当の表示へ整形する")
{
	CHECK(key::FormatKeyForDisplay(_T("Ctrl+Shift+K")) == UnicodeString(_T("Ctrl + Shift + K")));
	CHECK(key::FormatKeyForDisplay(_T("Ctrl+K~D")) == UnicodeString(_T("Ctrl + K ~ D")));
	CHECK(key::TabLabel(key::Tab::File) == UnicodeString(_T("ファイラー")));
	CHECK(key::TabLabel(key::Tab::Search) == UnicodeString(_T("INC.サーチ")));
}

TEST_CASE("key: フィルタと安定ソート")
{
	std::vector<key::Entry> rows = {
		{_T("B"), _T("Beta"), _T("説明B")},
		{_T("A"), _T("Alpha"), _T("説明A")},
		{_T("C"), _T("Gamma"), _T("説明A")},
	};
	key::SortEntries(rows, key::SortMode::Key);
	CHECK(rows[0].key == UnicodeString(_T("A")));
	CHECK(rows[1].key == UnicodeString(_T("B")));
	key::SortEntries(rows, key::SortMode::Description);
	CHECK(rows[0].description == UnicodeString(_T("説明A")));
	CHECK(rows[0].key == UnicodeString(_T("A")));  // stable_sort
	const std::vector<key::Entry> filtered = key::FilterAndSort(rows, _T("gamma"), key::SortMode::Key);
	REQUIRE(filtered.size() == 1);
	CHECK(filtered[0].command == UnicodeString(_T("Gamma")));
}

TEST_CASE("key: 未登録コマンド表示はキーを作らずに実在の行を返す")
{
	const std::vector<key::Entry> assignments = {{_T("F5"), _T("ReloadList"), _T("再読み込み")}};
	const std::vector<key::Entry> commands = {
		{EmptyStr, _T("ReloadList"), _T("再読み込み")},
		{EmptyStr, _T("OpenStandard"), _T("標準で開く")},
	};
	const auto hidden = key::BuildRows(assignments, commands, false);
	REQUIRE(hidden.size() == 1);
	CHECK(hidden[0].key == UnicodeString(_T("F5")));
	const auto shown = key::BuildRows(assignments, commands, true);
	REQUIRE(shown.size() == 2);
	CHECK(shown[1].key.IsEmpty());
}

TEST_CASE("key: コピー形式を TAB 区切りにする")
{
	const std::vector<key::Entry> rows = {{_T("F5"), _T("ReloadList"), _T("再読み込み")}};
	CHECK(key::FormatList(rows) == UnicodeString(_T("キー\tコマンド\t説明\r\nF5\tReloadList\t再読み込み")));
}

TEST_CASE("key: 表示設定を wx 専用 ini に往復する")
{
	TempDir tmp;
	UsrIniFile ini(tmp.file(_T("key-dialog.ini")));
	key::StateStore state;
	state.tab = key::Tab::Text;
	state.sort_mode = key::SortMode::Description;
	state.show_all = true;
	state.migemo = true;
	state.confirm_execute = true;
	state.filter = _T("tag");
	state.SaveToIni(ini);
	REQUIRE(ini.UpdateFile());
	UsrIniFile reread(tmp.file(_T("key-dialog.ini")));
	key::StateStore got;
	got.LoadFromIni(reread);
	CHECK(got.tab == key::Tab::Text);
	CHECK(got.sort_mode == key::SortMode::Description);
	CHECK(got.show_all);
	CHECK(got.migemo);
	CHECK(got.confirm_execute);
	CHECK(got.filter == UnicodeString(_T("tag")));
}
