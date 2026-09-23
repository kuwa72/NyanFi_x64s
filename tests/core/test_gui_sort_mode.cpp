/**
 * @file tests/core/test_gui_sort_mode.cpp
 * @brief gui/sort_mode のソート設定・コマンドパラメータのテスト
 */
#include "doctest/doctest.h"

#include "gui/sort_mode.h"

TEST_CASE("sort_mode: VCL のモード添字を正規化する")
{
	CHECK(sort_mode::FromIndex(0) == sort_mode::Mode::Name);
	CHECK(sort_mode::FromIndex(4) == sort_mode::Mode::Attribute);
	CHECK(sort_mode::FromIndex(5) == sort_mode::Mode::None);
	CHECK(sort_mode::FromIndex(-1) == sort_mode::Mode::Name);
	CHECK(sort_mode::FromIndex(99) == sort_mode::Mode::None);
	CHECK(sort_mode::ToIndex(sort_mode::Mode::Size) == 3);
	CHECK(sort_mode::DirectoryFromIndex(5) == sort_mode::DirectoryMode::Mixed);
	CHECK(sort_mode::DirectoryFromIndex(6) == sort_mode::DirectoryMode::Icon);
}

TEST_CASE("sort_mode: 第2ソートモードの有効・無効")
{
	// VCL PrimeComboBoxClick: 名前順のときは「なし」だけ、他の主方式は同じ番号だけ無効
	CHECK(sort_mode::IsSubModeEnabled(0, 5));
	CHECK_FALSE(sort_mode::IsSubModeEnabled(0, 0));
	CHECK_FALSE(sort_mode::IsSubModeEnabled(1, 1));
	CHECK(sort_mode::IsSubModeEnabled(1, 5));
	CHECK(sort_mode::NormalizeSubMode(0, 0) == 5);
	CHECK(sort_mode::NormalizeSubMode(2, 2) == 5);
	CHECK(sort_mode::NormalizeSubMode(2, 4) == 4);
}

TEST_CASE("sort_mode: IV/IA は方向だけを反転する")
{
	sort_mode::Options o;
	o.mode = sort_mode::Mode::Date;
	o.descending_name = true;
	o.descending_old = false;
	o.descending_small = true;
	o.descending_attribute = false;

	auto r = sort_mode::ApplyParam(o, _T("IV"));
	CHECK(r.recognized);
	CHECK(r.changed);
	CHECK(r.options.mode == sort_mode::Mode::Date);
	// IV は現在のモード (Date) の方向だけ反転する
	CHECK(r.options.descending_name);
	CHECK(r.options.descending_old);
	CHECK(r.options.descending_small);
	CHECK_FALSE(r.options.descending_attribute);

	r = sort_mode::ApplyParam(o, _T("IA"));
	CHECK(r.recognized);
	CHECK_FALSE(r.options.descending_name);
	CHECK(r.options.descending_old);
	CHECK_FALSE(r.options.descending_small);
	CHECK(r.options.descending_attribute);
}

TEST_CASE("sort_mode: 2文字・1文字・ディレクトリの SortDlg パラメータ")
{
	sort_mode::Options o;
	o.mode = sort_mode::Mode::Name;
	o.dir_mode = sort_mode::DirectoryMode::SameAsFile;

	auto r = sort_mode::ApplyParam(o, _T("FE"));
	CHECK(r.recognized);
	CHECK(r.options.mode == sort_mode::Mode::Extension);
	r = sort_mode::ApplyParam(r.options, _T("FE"));
	CHECK(r.options.mode == sort_mode::Mode::Name);

	r = sort_mode::ApplyParam(o, _T("S"));
	CHECK(r.options.mode == sort_mode::Mode::Size);
	r = sort_mode::ApplyParam(o, _T("U"));
	CHECK(r.options.mode == sort_mode::Mode::None);

	r = sort_mode::ApplyParam(o, _T("XD"));
	CHECK(r.options.dir_mode == sort_mode::DirectoryMode::Date);
	r = sort_mode::ApplyParam(o, _T("XNX"));
	CHECK(r.options.dir_mode == sort_mode::DirectoryMode::Mixed);
	r = sort_mode::ApplyParam(r.options, _T("XNX"));
	CHECK(r.options.dir_mode == sort_mode::DirectoryMode::SameAsFile);
	r = sort_mode::ApplyParam(o, _T("XNI"));
	CHECK(r.options.dir_mode == sort_mode::DirectoryMode::Icon);

	CHECK_FALSE(sort_mode::ApplyParam(o, _T("ZZ")).recognized);
}

TEST_CASE("sort_mode: 設定から FilePane の設定へ変換する")
{
	sort_mode::Options o;
	o.mode = sort_mode::Mode::Size;
	o.dir_mode = sort_mode::DirectoryMode::Mixed;
	o.descending_small = true;
	const sort_mode::PaneSettings p = sort_mode::ToPaneSettings(o);
	CHECK(p.key == SortKey::Size);
	CHECK(p.descending);
	CHECK_FALSE(p.dirs_first);

	o.mode = sort_mode::Mode::None;
	o.dir_mode = sort_mode::DirectoryMode::SameAsFile;
	const sort_mode::PaneSettings none = sort_mode::ToPaneSettings(o);
	CHECK(none.key == SortKey::Name);
	CHECK_FALSE(none.descending);
	CHECK(none.dirs_first);
}
