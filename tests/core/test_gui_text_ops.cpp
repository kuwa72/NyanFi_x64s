/**
 * @file tests/core/test_gui_text_ops.cpp
 * @brief gui/text_ops.cpp のテスト
 */
#include "doctest/doctest.h"

#include <string>

#include "gui/text_ops.h"
#include "temp_dir.h"
#include "usr_file_ex.h"

using nyanfi_test::TempDir;

namespace {

void write_bytes(const UnicodeString &path, const std::string &data)
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
	                         FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	DWORD written = 0;
	if (!data.empty()) ::WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, NULL);
	::CloseHandle(h);
}

std::string read_bytes(const UnicodeString &path)
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
	                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return std::string();
	std::string out;
	char buf[4096];
	DWORD n = 0;
	while (::ReadFile(h, buf, sizeof(buf), &n, NULL) && n > 0) out.append(buf, n);
	::CloseHandle(h);
	return out;
}

}  // namespace

TEST_CASE("CountLines: 全行数と空白行を数える")
{
	const std::vector<UnicodeString> lines = {_T("abc"), EmptyStr, _T("   "), _T("\t"),
	                                          _T("def")};
	const text_ops::LineStats st = text_ops::CountLines(lines);
	CHECK(st.total == 5);
	CHECK(st.blank == 3);   // 空 / 空白のみ / タブのみ
	CHECK(st.non_blank == 2);
}

TEST_CASE("CountLines: 空のリスト")
{
	const text_ops::LineStats st = text_ops::CountLines({});
	CHECK(st.total == 0);
	CHECK(st.blank == 0);
	CHECK(st.non_blank == 0);
}

TEST_CASE("JoinTextFiles: 順番どおりに結合する")
{
	TempDir tmp;
	write_bytes(tmp.file(_T("a.txt")), "one\r\ntwo\r\n");
	write_bytes(tmp.file(_T("b.txt")), "three\r\n");

	const text_ops::JoinResult r = text_ops::JoinTextFiles(
		{tmp.file(_T("a.txt")), tmp.file(_T("b.txt"))}, tmp.file(_T("out.txt")));

	CHECK(r.joined == 2);
	CHECK(r.failures.empty());
	CHECK(read_bytes(tmp.file(_T("out.txt"))) == "one\r\ntwo\r\nthree\r\n");
}

TEST_CASE("JoinTextFiles: 出力先が既にあれば何もしない")
{
	// 規約: 上書きを既定にしない
	TempDir tmp;
	write_bytes(tmp.file(_T("a.txt")), "x\r\n");
	write_bytes(tmp.file(_T("out.txt")), "KEEP");

	const text_ops::JoinResult r =
		text_ops::JoinTextFiles({tmp.file(_T("a.txt"))}, tmp.file(_T("out.txt")));

	CHECK(r.joined == 0);
	CHECK(r.failures.size() == 1);
	CHECK(read_bytes(tmp.file(_T("out.txt"))) == "KEEP");  // 触っていない
}

TEST_CASE("JoinTextFiles: 読めないファイルは理由つきで飛ばす")
{
	TempDir tmp;
	write_bytes(tmp.file(_T("a.txt")), "ok\r\n");

	const text_ops::JoinResult r = text_ops::JoinTextFiles(
		{tmp.file(_T("a.txt")), tmp.file(_T("nosuch.txt"))}, tmp.file(_T("out.txt")));

	CHECK(r.joined == 1);
	REQUIRE(r.failures.size() == 1);
	CHECK(ContainsText(r.failures[0], _T("nosuch.txt")));
}

TEST_CASE("ConvertEncoding: Shift_JIS を UTF-8 にする")
{
	TempDir tmp;
	// "あ" を Shift_JIS で (0x82 0xA0)
	write_bytes(tmp.file(_T("sjis.txt")), "\x82\xA0\r\n");

	UnicodeString error;
	REQUIRE(text_ops::ConvertEncoding(tmp.file(_T("sjis.txt")), CP_UTF8, false, error));
	CHECK(error.IsEmpty());

	// UTF-8 の "あ" は E3 81 82
	const std::string got = read_bytes(tmp.file(_T("sjis.txt")));
	CHECK(got.substr(0, 3) == "\xE3\x81\x82");
}

TEST_CASE("ConvertEncoding: BOM を付ける")
{
	TempDir tmp;
	write_bytes(tmp.file(_T("a.txt")), "abc\r\n");

	UnicodeString error;
	REQUIRE(text_ops::ConvertEncoding(tmp.file(_T("a.txt")), CP_UTF8, true, error));

	const std::string got = read_bytes(tmp.file(_T("a.txt")));
	CHECK(got.substr(0, 3) == "\xEF\xBB\xBF");
}

TEST_CASE("ConvertEncoding: バイナリは変換しない")
{
	// 壊すので触らない
	TempDir tmp;
	std::string bin("\x00\x01\x02\x03\x00\xFF", 6);
	write_bytes(tmp.file(_T("bin.dat")), bin);

	UnicodeString error;
	CHECK_FALSE(text_ops::ConvertEncoding(tmp.file(_T("bin.dat")), CP_UTF8, false, error));
	CHECK(ContainsText(error, _T("バイナリ")));
	CHECK(read_bytes(tmp.file(_T("bin.dat"))) == bin);  // 触っていない
}

TEST_CASE("ConvertEncoding: 存在しないファイル")
{
	UnicodeString error;
	CHECK_FALSE(text_ops::ConvertEncoding(_T("C:\\nosuch\\none.txt"), CP_UTF8, false, error));
	CHECK_FALSE(error.IsEmpty());
}
