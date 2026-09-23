/**
 * @file tests/core/test_gui_file_ext.cpp
 * @brief gui/file_ext.cpp の集計入力・並べ替え・出力書式テスト
 */
#include "doctest/doctest.h"

#include <string>

#include "gui/file_ext.h"
#include "temp_dir.h"
#include "usr_file_ex.h"

using nyanfi_test::TempDir;

namespace {

void mkfile(const UnicodeString &path, std::size_t bytes)
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
	                         FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	if (bytes > 0) {
		const std::string data(bytes, 'x');
		DWORD written = 0;
		::WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, NULL);
	}
	::CloseHandle(h);
}

}  // namespace

TEST_CASE("file_ext::Collect: 拡張子ごとの件数・容量・ファイルパスを集める")
{
	TempDir tmp;
	mkfile(tmp.file(_T("a.txt")), 10);
	mkfile(tmp.file(_T("b.TXT")), 20);
	mkfile(tmp.file(_T("README")), 5);
	::CreateDirectoryW(tmp.file(_T("sub")).c_str(), NULL);
	mkfile(tmp.file(_T("sub\\c.dat")), 30);

	bool truncated = false;
	const file_ext::Summary summary =
		file_ext::Collect(tmp.path, true, false, false, truncated);

	CHECK(summary.total_count == 4);
	CHECK(summary.total_bytes == 65);
	CHECK_FALSE(truncated);
	REQUIRE(summary.extensions.size() == 3);

	const file_ext::Entry *txt = nullptr;
	for (const file_ext::Entry &entry : summary.extensions) {
		if (entry.extension == UnicodeString(_T("txt"))) txt = &entry;
	}
	REQUIRE(txt != nullptr);
	CHECK(txt->count == 2);
	CHECK(txt->bytes == 30);
	CHECK(txt->files.size() == 2);
	const bool has_a = ContainsText(txt->files[0], _T("a.txt"))
		|| ContainsText(txt->files[1], _T("a.txt"));
	CHECK(has_a);
}

TEST_CASE("file_ext::Sort: 同数・同容量なら拡張子名の昇順で安定する")
{
	TempDir tmp;
	mkfile(tmp.file(_T("z.txt")), 10);
	mkfile(tmp.file(_T("a.dat")), 10);

	bool truncated = false;
	file_ext::Summary summary = file_ext::Collect(tmp.path, false, false, false, truncated);
	file_ext::SortExtensions(summary, file_ext::ExtensionSort::Count, true);
	REQUIRE(summary.extensions.size() == 2);
	CHECK(summary.extensions[0].extension == UnicodeString(_T("dat")));
	CHECK(summary.extensions[1].extension == UnicodeString(_T("txt")));
}

TEST_CASE("file_ext::FormatReport: CSV/TSV と一覧形式を返す")
{
	TempDir tmp;
	mkfile(tmp.file(_T("a.txt")), 10);
	mkfile(tmp.file(_T("b.txt")), 30);
	bool truncated = false;
	file_ext::Summary summary = file_ext::Collect(tmp.path, false, false, false, truncated);
	file_ext::SortExtensions(summary, file_ext::ExtensionSort::Extension, true);

	const UnicodeString csv = file_ext::FormatReport(summary, file_ext::OutputFormat::Csv, _T("C:\\root"));
	CHECK(ContainsText(csv, _T("\"拡張子\",\"ファイル数\",\"合計サイズ\",\"平均サイズ\"")));
	CHECK(ContainsText(csv, _T("\".txt\",2,40,20")));

	const UnicodeString tsv = file_ext::FormatReport(summary, file_ext::OutputFormat::Tsv, _T("C:\\root"));
	CHECK(ContainsText(tsv, _T("拡張子\tファイル数\t合計サイズ\t平均サイズ")));
	CHECK(ContainsText(tsv, _T(".txt\t2\t40\t20")));

	const UnicodeString text = file_ext::FormatReport(summary, file_ext::OutputFormat::Text, _T("C:\\root"));
	CHECK(ContainsText(text, _T("C:\\root")));
	CHECK(ContainsText(text, _T("txt")));
}

TEST_CASE("file_ext::BuildMask: 拡張子をファイラのマスクへ変換する")
{
	CHECK(file_ext::BuildMask(_T("txt")) == UnicodeString(_T("*.txt")));
	CHECK(file_ext::BuildMask(_T(".TXT")) == UnicodeString(_T("*.TXT")));
	CHECK(file_ext::BuildMask(_T("(none)")) == UnicodeString(_T("*")));
}

TEST_CASE("file_ext::ResolveTargetPath: CP はカーソル位置のディレクトリを使う")
{
	UnicodeString error;
	CHECK(file_ext::ResolveTargetPath(_T("CP"), _T("C:\\current"), _T("C:\\cursor\\sub"),
	                                  true, false, error)
	      == UnicodeString(_T("C:\\cursor\\sub")));
	CHECK(error.IsEmpty());

	CHECK(file_ext::ResolveTargetPath(_T("CP"), _T("C:\\current"), _T("C:\\current\\.."),
	                                  true, true, error)
	      == UnicodeString(_T("C:\\current")));

	CHECK(file_ext::ResolveTargetPath(_T("CP"), _T("C:\\current"), _T("C:\\cursor\\file.txt"),
	                                  false, false, error).IsEmpty());
	CHECK_FALSE(error.IsEmpty());

	CHECK(file_ext::ResolveTargetPath(EmptyStr, _T("C:\\current"), _T("C:\\cursor\\file.txt"),
	                                  false, false, error)
	      == UnicodeString(_T("C:\\current")));
}
