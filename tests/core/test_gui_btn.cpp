/** @file tests/core/test_gui_btn.cpp */
#include "doctest/doctest.h"

#include "gui/btn.h"
#include "UIniFile.h"
#include "temp_dir.h"

using nyanfi_test::TempDir;

TEST_CASE("btn: 3項目 CSV を往復する")
{
	const btn::Item in{_T("戻る"), _T("BackDirHist"), _T("back.ico")};
	const UnicodeString record = btn::FormatItem(in);
	const btn::Item out = btn::ParseItem(record);
	CHECK(out.caption == in.caption);
	CHECK(out.command == in.command);
	CHECK(out.icon == in.icon);
	CHECK(btn::IsSeparator(btn::ParseItem(_T("-,,"))));
	CHECK(btn::CanAdd(in));
	CHECK_FALSE(btn::CanAdd(btn::Item{}));
}

TEST_CASE("btn: コマンド一覧は実在名とエイリアスを重複させない")
{
	const std::vector<UnicodeString> choices = btn::CommandChoices(
		btn::Mode::FileList, {_T("ReloadList"), _T("ReloadList"), _T("OpenStandard")}, {_T("$tool")});
	REQUIRE(choices.size() == 3);
	CHECK(choices[0] == UnicodeString(_T("ReloadList")));
	CHECK(choices[1] == UnicodeString(_T("OpenStandard")));
	CHECK(choices[2] == UnicodeString(_T("$tool")));
	CHECK(btn::WithPathParameter(_T("OpenByWin"), _T("C:\\a.txt")) == UnicodeString(_T("OpenByWin_\"C:\\a.txt\"")));
}

TEST_CASE("btn: 上下移動と 1 始まりの実行番号を解決する")
{
	std::vector<btn::Item> items(3);
	items[0].caption = _T("a");
	items[1].caption = _T("b");
	items[2].caption = _T("c");
	CHECK(btn::Move(items, 0, 1));
	CHECK(items[0].caption == UnicodeString(_T("b")));
	CHECK(items[1].caption == UnicodeString(_T("a")));
	CHECK_FALSE(btn::Move(items, 0, -1));
	CHECK(btn::ResolveIndex(_T("2"), 3) == 1);
	CHECK(btn::ResolveIndex(_T("4"), 3) == -1);
	CHECK(btn::ResolveIndex(_T("x"), 3) == -1);
}

TEST_CASE("btn: ボタン一覧を wx 専用 ini に往復する")
{
	TempDir tmp;
	UsrIniFile ini(tmp.file(_T("buttons.ini")));
	btn::Store store;
	btn::Item item{_T("上"), _T("ToParent"), _T("up.ico")};
	store.MutableItems().push_back(item);
	store.SaveToIni(ini);
	REQUIRE(ini.UpdateFile());
	UsrIniFile reread(tmp.file(_T("buttons.ini")));
	btn::Store got;
	got.LoadFromIni(reread);
	REQUIRE(got.Items().size() == 1);
	CHECK(got.Items()[0].caption == item.caption);
	CHECK(got.Items()[0].command == item.command);
	CHECK(got.Items()[0].icon == item.icon);
}
