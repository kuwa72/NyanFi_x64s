/** @file tests/core/test_gui_inp_ex.cpp */
#include "doctest/doctest.h"

#include "gui/inp_ex.h"

TEST_CASE("inp_ex: モードごとの入力欄と履歴キーを決める")
{
	CHECK(inp_ex::UsesCombo(inp_ex::Mode::CreateDir));
	CHECK(inp_ex::UsesCombo(inp_ex::Mode::FindTag));
	CHECK_FALSE(inp_ex::UsesCombo(inp_ex::Mode::CreateTestFile));
	CHECK(inp_ex::Prompt(inp_ex::Mode::CreateDir) == UnicodeString(_T("名前")));
	CHECK(inp_ex::Prompt(inp_ex::Mode::FindTag) == UnicodeString(_T("タグ")));
	CHECK(inp_ex::Prompt(inp_ex::Mode::CreateTestFile) == UnicodeString(_T("ファイル名")));
	CHECK(inp_ex::HistorySection(inp_ex::Mode::CreateDir) == UnicodeString(_T("CreateDirHistory")));
	CHECK(inp_ex::HistorySection(inp_ex::Mode::NewTextFile) == UnicodeString(_T("NewTextHistory")));
	CHECK(inp_ex::HistorySection(inp_ex::Mode::FindTag).IsEmpty());
	CHECK(ContainsText(inp_ex::Title(inp_ex::Mode::CreateDir, _T("C:\\Foo")), _T("C:\\Foo")));
	CHECK(ContainsText(inp_ex::Hint(inp_ex::Mode::FindTag), _T("AND")));
}

TEST_CASE("inp_ex: 16進アドレスを VCL と同じ位置へ 0x を付ける")
{
	inp_ex::Values v;
	v.mode = inp_ex::Mode::JumpAddress;
	v.value = _T("  +A  ");
	v.hexadecimal = true;
	inp_ex::NormalizeOnClose(v);
	CHECK(v.value == UnicodeString(_T("+0xA")));

	v.value = _T("0x10");
	inp_ex::NormalizeOnClose(v);
	CHECK(v.value == UnicodeString(_T("0x10")));
	v.hexadecimal = false;
	v.value = _T("  10  ");
	inp_ex::NormalizeOnClose(v);
	CHECK(v.value == UnicodeString(_T("10")));
}

TEST_CASE("inp_ex: ディレクトリ名の文字数を VCL の境界で判定する")
{
	const inp_ex::LengthStatus ok = inp_ex::MeasureCreateDir(_T("C:\\Base\\"), _T("Name"));
	CHECK(ok.path_ok);
	CHECK(ok.name_ok);
	CHECK(ok.path_length == 12);

	inp_ex::Values v;
	v.mode = inp_ex::Mode::CreateDir;
	v.path_name = _T("C:\\Base\\");
	v.value = EmptyStr;
	UnicodeString error;
	CHECK_FALSE(inp_ex::Validate(v, error));
	CHECK_FALSE(error.IsEmpty());
	v.value = _T("Name");
	CHECK(inp_ex::Validate(v, error));
	CHECK(error.IsEmpty());
}

TEST_CASE("inp_ex: テストファイルと追加タグの入力検証")
{
	inp_ex::Values v;
	v.mode = inp_ex::Mode::CreateTestFile;
	v.value = _T("test.dat");
	v.test_size = _T("1M");
	v.test_count = 1;
	UnicodeString error;
	CHECK(inp_ex::Validate(v, error));
	v.test_count = 0;
	CHECK_FALSE(inp_ex::Validate(v, error));
	v.test_count = 1;
	v.mode = inp_ex::Mode::AddTag;
	v.value = EmptyStr;
	CHECK_FALSE(inp_ex::Validate(v, error));
}

TEST_CASE("inp_ex: 保存文字コードの並びを UserMdl と同じにする")
{
	const std::vector<UnicodeString> names = inp_ex::CodePageNames();
	REQUIRE(names.size() == 7);
	CHECK(names[0] == UnicodeString(_T("Shift_JIS")));
	CHECK(names[1] == UnicodeString(_T("UTF-8")));
	CHECK(names[2] == UnicodeString(_T("UTF-8N")));
	CHECK(names[6] == UnicodeString(_T("ISO-2022-JP")));
}
