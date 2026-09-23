/**
 * @file tests/core/test_gui_cre_dirs.cpp
 * @brief gui/cre_dirs.h の純関数テスト
 */
#include "doctest/doctest.h"

#include "gui/cre_dirs.h"

TEST_CASE("cre_dirs: 連番は開始値の桁数を保つ")
{
	const std::vector<UnicodeString> in = {_T("a"), _T("b")};
	const std::vector<UnicodeString> out = cre_dirs::AddSerial(in, 7, 2, true, 3);
	REQUIRE(out.size() == 2);
	CHECK(out[0] == UnicodeString(_T("007a")));
	CHECK(out[1] == UnicodeString(_T("009b")));

	const std::vector<UnicodeString> post = cre_dirs::AddSerial(in, 1, 1, false);
	CHECK(post[0] == UnicodeString(_T("a1")));
	CHECK(post[1] == UnicodeString(_T("b2")));
}

TEST_CASE("cre_dirs: 連番を付加できる条件")
{
	CHECK(cre_dirs::CanAddSerial(_T("1"), 1));
	CHECK_FALSE(cre_dirs::CanAddSerial(EmptyStr, 1));
	CHECK_FALSE(cre_dirs::CanAddSerial(_T("1"), 0));
}

TEST_CASE("cre_dirs: 文字列を前後へ付加する")
{
	const std::vector<UnicodeString> in = {_T("src"), _T("doc")};
	const std::vector<UnicodeString> out = cre_dirs::AddText(in, _T("x_"), true);
	CHECK(out[0] == UnicodeString(_T("x_src")));
	CHECK(out[1] == UnicodeString(_T("x_doc")));
}

TEST_CASE("cre_dirs: 日付書式の増分単位を VCL と同じ順序で決める")
{
	CHECK(cre_dirs::ResolveDateUnit(_T("yyyy/mm/dd")) == cre_dirs::DateUnit::Day);
	CHECK(cre_dirs::ResolveDateUnit(_T("yyyy/mm")) == cre_dirs::DateUnit::Month);
	CHECK(cre_dirs::ResolveDateUnit(_T("yyyy")) == cre_dirs::DateUnit::Year);
}

TEST_CASE("cre_dirs: 日付を各行へ付加し FormatDateTime の結果を使う")
{
	const std::vector<UnicodeString> in = {_T("a"), _T("b")};
	std::vector<UnicodeString> out;
	UnicodeString error;
	REQUIRE(cre_dirs::AddDate(in, TDateTime(2024, 1, 31), _T("yyyy/mm/dd"), true, out, error));
	REQUIRE(out.size() == 2);
	CHECK(out[0] == UnicodeString(_T("2024/01/31a")));
	// 2行目は VCL 同样に日++;
	CHECK(out[1] == UnicodeString(_T("2024/02/01b")));
	CHECK(error.IsEmpty());
}

TEST_CASE("cre_dirs: 空行を除いた作成対象を数える")
{
	const std::vector<UnicodeString> in = {_T("a"), EmptyStr, _T("  "), _T("x\\y")};
	const cre_dirs::Validation v = cre_dirs::ValidateEntries(in);
	CHECK(v.total == 4);
	CHECK(v.creatable == 2);
	CHECK(v.blank == 2);
	CHECK(v.valid);
	CHECK(cre_dirs::CreatableEntries(in).size() == 2);
}
