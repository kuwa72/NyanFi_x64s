/**
 * @file tests/core/test_gui_join_text.cpp
 * @brief gui/join_text.h (TJoinTextDlg の判断ロジック) のテスト
 */
#include "doctest/doctest.h"

#include "gui/join_text.h"

TEST_CASE("join_text: 出力文字コードの選択肢")
{
	CHECK(join_text::EncodingCount() == 6);
	CHECK(join_text::EncodingName(0) == _T("自動(先頭のコードに統一)"));
	CHECK(join_text::EncodingName(1) == _T("Shift_JIS"));
	CHECK(join_text::EncodingName(5) == _T("UTF-16"));
	CHECK(join_text::EncodingIndex(_T("UTF-8")) == 4);
	CHECK(join_text::EncodingIndex(_T("unknown")) == 0);
	CHECK(join_text::EncodingName(-1) == join_text::EncodingName(0));
	CHECK(join_text::EncodingName(99) == join_text::EncodingName(0));
}

TEST_CASE("join_text: 出力文字コードからコードページを決める")
{
	CHECK(join_text::CodePageFor(_T("自動(先頭のコードに統一)")) == 0);
	CHECK(join_text::CodePageFor(_T("Shift_JIS")) == 932);
	CHECK(join_text::CodePageFor(_T("ISO-2022-JP")) == 50220);
	CHECK(join_text::CodePageFor(_T("EUC-JP")) == 20932);
	CHECK(join_text::CodePageFor(_T("UTF-8")) == 65001);
	CHECK(join_text::CodePageFor(_T("UTF-16")) == 1200);
	CHECK(join_text::CodePageFor(_T("unknown")) == 0);
}

TEST_CASE("join_text: UTF 系だけ BOM を選べる")
{
	CHECK(join_text::BomAvailable(_T("UTF-8")) == true);
	CHECK(join_text::BomAvailable(_T("UTF-16")) == true);
	CHECK(join_text::BomAvailable(_T("Shift_JIS")) == false);
	CHECK(join_text::BomAvailable(_T("自動(先頭のコードに統一)")) == false);
}

TEST_CASE("join_text: 改行と開始ボタン条件")
{
	CHECK(join_text::LineBreakFor(0) == _T("\r\n"));
	CHECK(join_text::LineBreakFor(1) == _T("\n"));
	CHECK(join_text::LineBreakFor(2) == _T("\r"));
	CHECK(join_text::LineBreakFor(99) == _T("\r\n"));
	const std::vector<UnicodeString> none;
	const std::vector<UnicodeString> one = {_T("a.txt")};
	CHECK(join_text::CanSubmit(none.size(), _T("out.txt")) == false);
	CHECK(join_text::CanSubmit(one.size(), EmptyStr) == false);
	CHECK(join_text::CanSubmit(one.size(), _T("out.txt")) == true);
}

TEST_CASE("join_text: 結合順の移動と削除")
{
	std::vector<UnicodeString> files = {_T("a.txt"), _T("b.txt"), _T("c.txt")};
	CHECK(join_text::MoveSource(files, 1, -1) == 0);
	CHECK(files == std::vector<UnicodeString>{_T("b.txt"), _T("a.txt"), _T("c.txt")});
	CHECK(join_text::MoveSource(files, 1, 1) == 2);
	CHECK(files == std::vector<UnicodeString>{_T("b.txt"), _T("c.txt"), _T("a.txt")});
	CHECK(join_text::MoveSource(files, 0, -1) == -1);
	CHECK(join_text::MoveSource(files, 2, 1) == -1);
	CHECK(join_text::CanMoveSource(files.size(), 0, -1) == false);
	CHECK(join_text::CanMoveSource(files.size(), 1, 1) == true);
	CHECK(join_text::CanDeleteSource(files.size(), 2) == true);
	CHECK(join_text::CanDeleteSource(files.size(), 3) == false);
	CHECK(join_text::RemoveSource(files, 1) == _T("c.txt"));
	CHECK(files == std::vector<UnicodeString>{_T("b.txt"), _T("a.txt")});
	CHECK(join_text::RemoveSource(files, 9).IsEmpty());
}

TEST_CASE("join_text: 出力先パス")
{
	CHECK(join_text::OutputPath(_T("C:\\in"), _T("out.txt")) == _T("C:\\in\\out.txt"));
	CHECK(join_text::OutputPath(_T("C:\\in\\"), _T("out.txt")) == _T("C:\\in\\out.txt"));
}
