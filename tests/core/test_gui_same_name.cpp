/**
 * @file tests/core/test_gui_same_name.cpp
 * @brief gui/same_name の同名ファイル処理のテスト
 */
#include "doctest/doctest.h"

#include "gui/same_name.h"

TEST_CASE("same_name: 同一パスでは上書き等を手動改名へ切り替える")
{
	CHECK(same_name::NormalizeMode(same_name::Mode::Overwrite, false) == same_name::Mode::Overwrite);
	CHECK(same_name::NormalizeMode(same_name::Mode::Overwrite, true) == same_name::Mode::ManualRename);
	CHECK(same_name::IsModeEnabled(same_name::Mode::Overwrite, true) == false);
	CHECK(same_name::IsModeEnabled(same_name::Mode::AutoRename, true));
}

TEST_CASE("same_name: 全件適用と手動改名の排他を正規化する")
{
	same_name::Context c;
	c.destination = _T("D:\\dst\\old.txt");
	c.initial_name = _T("new.txt");
	same_name::Options o;
	o.mode = same_name::Mode::ManualRename;
	o.copy_all = true;
	o.rename_name = _T("");
	const auto n = same_name::NormalizeOptions(c, o);
	CHECK(n.mode == same_name::Mode::AutoRename);
	CHECK_FALSE(n.copy_all);
	CHECK(n.rename_name == UnicodeString(_T("new.txt")));
}

TEST_CASE("same_name: 最新だけコピーする判定")
{
	CHECK(same_name::ShouldCopy(same_name::Mode::Overwrite, 1, 1, 2, 2));
	CHECK(same_name::ShouldCopy(same_name::Mode::KeepNewer, 1, 20, 2, 10));
	CHECK_FALSE(same_name::ShouldCopy(same_name::Mode::KeepNewer, 1, 10, 2, 10));
	CHECK_FALSE(same_name::ShouldCopy(same_name::Mode::Skip, 1, 20, 2, 10));
	CHECK(same_name::ShouldCopy(same_name::Mode::AutoRename, 1, 1, 2, 2));
}

TEST_CASE("same_name: 自動改名名を入力済み名避けて作る")
{
	int calls = 0;
	const UnicodeString result = same_name::MakeAutoRenamePath(
		_T("C:\\src\\photo.jpg"), _T("D:\\dst"), false,
		[&calls](const UnicodeString &path) {
			++calls;
			return path == UnicodeString(_T("D:\\dst\\photo_1.jpg"));
		});
	CHECK(result == UnicodeString(_T("D:\\dst\\photo_2.jpg")));
	CHECK(calls == 2);
}

TEST_CASE("same_name: 比較情報の表示文字列")
{
	CHECK(same_name::SizeSummary(10, 10) == UnicodeString(_T("サイズ: 同じ")));
	CHECK(same_name::SizeSummary(10, 20).Pos(_T("大きい")) > 0);
	CHECK(same_name::TimeSummary(10, 10) == UnicodeString(_T("タイム: 同じ")));
	CHECK(same_name::TimeSummary(20, 10).Pos(_T("古い")) > 0);
}
