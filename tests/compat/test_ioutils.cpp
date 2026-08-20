/**
 * @file tests/compat/test_ioutils.cpp
 * @brief TDirectory / TFile / TSearchOption 互換シムの単体テスト
 */
#include "doctest/doctest.h"

#include <cstring>
#include <memory>

#include "compat/encoding.h"
#include "compat/ioutils.h"
#include "compat/streams.h"
#include "compat/sysutils.h"

namespace {

/// テスト用に一時ディレクトリ構造を作る
/// base/
///   a.migemo-dict
///   b.txt
///   sub/
///     c.migemo-dict
struct TempDirFixture {
	UnicodeString base = _T("nyanfi_test_ioutils_dir");
	UnicodeString sub;

	TempDirFixture()
	{
		sub = IncludeTrailingPathDelimiter(base) + _T("sub");
		ForceDirectories(sub);

		WriteFile(IncludeTrailingPathDelimiter(base) + _T("a.migemo-dict"), _T("A"));
		WriteFile(IncludeTrailingPathDelimiter(base) + _T("b.txt"), _T("B"));
		WriteFile(IncludeTrailingPathDelimiter(sub) + _T("c.migemo-dict"), _T("C"));
	}

	~TempDirFixture()
	{
		::DeleteFileW((IncludeTrailingPathDelimiter(base) + _T("a.migemo-dict")).c_str());
		::DeleteFileW((IncludeTrailingPathDelimiter(base) + _T("b.txt")).c_str());
		::DeleteFileW((IncludeTrailingPathDelimiter(sub) + _T("c.migemo-dict")).c_str());
		RemoveDir(sub);
		RemoveDir(base);
	}

	static void WriteFile(const UnicodeString &path, const UnicodeString &text)
	{
		std::unique_ptr<TFileStream> fs(new TFileStream(path, fmCreate));
		AnsiString a(text);
		if (a.Length() > 0) fs->WriteBuffer(a.c_str(), a.Length());
	}
};

}  // namespace

//===========================================================================
// TSearchOption
//===========================================================================
TEST_CASE("TSearchOption: 非スコープ enum なので修飾アクセスもそのまま使える (実測: usr_migemo.cpp)")
{
	TSearchOption opt = TSearchOption::soAllDirectories;
	CHECK(opt == soAllDirectories);

	TSearchOption opt2 = soTopDirectoryOnly;
	CHECK(opt2 == TSearchOption::soTopDirectoryOnly);
}

//===========================================================================
// TDirectory::GetFiles
//===========================================================================
TEST_CASE("TDirectory::GetFiles: soTopDirectoryOnly はサブディレクトリを見ない")
{
	TempDirFixture fx;

	TStringDynArray files = TDirectory::GetFiles(fx.base, _T("*.migemo-dict"), soTopDirectoryOnly);
	REQUIRE(files.Length == 1);
	CHECK(ExtractFileName(files[0]) == UnicodeString(_T("a.migemo-dict")));
}

TEST_CASE("TDirectory::GetFiles: soAllDirectories はサブディレクトリも再帰的に見る (実測: usr_migemo.cpp)")
{
	TempDirFixture fx;

	TSearchOption opt = TSearchOption::soAllDirectories;
	TStringDynArray files = TDirectory::GetFiles(fx.base, _T("*.migemo-dict"), opt);
	CHECK(files.Length == 2);

	bool foundA = false, foundC = false;
	for (int i = 0; i < files.Length; ++i) {
		const UnicodeString name = ExtractFileName(files[i]);
		if (name == UnicodeString(_T("a.migemo-dict"))) foundA = true;
		if (name == UnicodeString(_T("c.migemo-dict"))) foundC = true;
	}
	CHECK(foundA);
	CHECK(foundC);
}

TEST_CASE("TDirectory::GetFiles: パターンに一致しないファイルは含まれない")
{
	TempDirFixture fx;

	TStringDynArray files = TDirectory::GetFiles(fx.base, _T("*.migemo-dict"), soTopDirectoryOnly);
	for (int i = 0; i < files.Length; ++i) CHECK_FALSE(ExtractFileName(files[i]) == UnicodeString(_T("b.txt")));
}

TEST_CASE("TDirectory::Exists")
{
	TempDirFixture fx;
	CHECK(TDirectory::Exists(fx.base));
	CHECK_FALSE(TDirectory::Exists(_T("nyanfi_test_ioutils_dir_nope")));
}

//===========================================================================
// TFile::AppendAllText
//===========================================================================
TEST_CASE("TFile::AppendAllText: ファイルが無ければ新規作成し、あれば末尾に追記する")
{
	const UnicodeString path = _T("nyanfi_test_ioutils_append.tmp");
	::DeleteFileW(path.c_str());

	TFile::AppendAllText(path, _T("hello"), TEncoding::UTF8);
	TFile::AppendAllText(path, _T(", world"), TEncoding::UTF8);

	// "hello, world" は ASCII のみなので UTF-8 でもバイト列はそのまま比較できる
	std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
	ms->LoadFromFile(path);
	REQUIRE(ms->Size == 12);
	CHECK(std::memcmp(ms->Memory, "hello, world", 12) == 0);

	::DeleteFileW(path.c_str());
}

TEST_CASE("TFile::AppendAllText: BOM を書かない")
{
	const UnicodeString path = _T("nyanfi_test_ioutils_append_nobom.tmp");
	::DeleteFileW(path.c_str());

	TFile::AppendAllText(path, _T("x"), TEncoding::UTF8);

	std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
	ms->LoadFromFile(path);
	REQUIRE(ms->Size == 1);  // BOM (3 バイト) が付いていれば Size は 4 になるはず
	const BYTE *bp = (const BYTE *)ms->Memory;
	CHECK(bp[0] == 'x');

	::DeleteFileW(path.c_str());
}
