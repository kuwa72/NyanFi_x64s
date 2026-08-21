/**
 * @file tests/core/test_gui_dir_info.cpp
 * @brief gui/dir_info.cpp のテスト
 */
#include "doctest/doctest.h"

#include <string>

#include "gui/dir_info.h"
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

void mkdir_(const UnicodeString &path) { ::CreateDirectoryW(path.c_str(), NULL); }

}  // namespace

TEST_CASE("CalcDirSize: 再帰的に数える")
{
	TempDir tmp;
	mkfile(tmp.file(_T("a.txt")), 100);
	mkdir_(tmp.file(_T("sub")));
	mkfile(tmp.file(_T("sub\\b.txt")), 200);
	mkdir_(tmp.file(_T("sub\\deep")));
	mkfile(tmp.file(_T("sub\\deep\\c.txt")), 300);

	const dir_info::DirSize r = dir_info::CalcDirSize(tmp.path, false, false);
	CHECK(r.bytes == 600);
	CHECK(r.files == 3);
	CHECK(r.dirs == 2);
	CHECK_FALSE(r.truncated);
}

TEST_CASE("CalcDirSize: 空のディレクトリ")
{
	TempDir tmp;
	const dir_info::DirSize r = dir_info::CalcDirSize(tmp.path, false, false);
	CHECK(r.bytes == 0);
	CHECK(r.files == 0);
	CHECK(r.dirs == 0);
}

TEST_CASE("CalcDirSize: 隠しファイルは表示設定に従う")
{
	// 画面に出ていないものを数えると数字が合わない
	TempDir tmp;
	mkfile(tmp.file(_T("visible.txt")), 10);
	mkfile(tmp.file(_T("hidden.txt")), 90);
	::SetFileAttributesW(tmp.file(_T("hidden.txt")).c_str(), FILE_ATTRIBUTE_HIDDEN);

	CHECK(dir_info::CalcDirSize(tmp.path, false, false).bytes == 10);
	CHECK(dir_info::CalcDirSize(tmp.path, true, false).bytes == 100);
}

TEST_CASE("CalcExtStats: 拡張子ごとに数え、件数の多い順に並べる")
{
	TempDir tmp;
	mkfile(tmp.file(_T("a.txt")), 10);
	mkfile(tmp.file(_T("b.txt")), 20);
	mkfile(tmp.file(_T("c.dat")), 30);

	bool truncated = false;
	const auto r = dir_info::CalcExtStats(tmp.path, false, false, false, truncated);

	REQUIRE(r.size() == 2);
	CHECK(r[0].ext == UnicodeString(_T("txt")));
	CHECK(r[0].count == 2);
	CHECK(r[0].bytes == 30);
	CHECK(r[1].ext == UnicodeString(_T("dat")));
	CHECK(r[1].count == 1);
}

TEST_CASE("CalcExtStats: 拡張子は小文字にまとめる")
{
	TempDir tmp;
	mkfile(tmp.file(_T("a.TXT")), 1);
	mkfile(tmp.file(_T("b.txt")), 1);

	bool truncated = false;
	const auto r = dir_info::CalcExtStats(tmp.path, false, false, false, truncated);
	REQUIRE(r.size() == 1);
	CHECK(r[0].count == 2);
}

TEST_CASE("CalcExtStats: 拡張子が無いものは (なし) にまとめる")
{
	TempDir tmp;
	mkfile(tmp.file(_T("README")), 1);

	bool truncated = false;
	const auto r = dir_info::CalcExtStats(tmp.path, false, false, false, truncated);
	REQUIRE(r.size() == 1);
	CHECK(r[0].ext == UnicodeString(_T("(なし)")));
}

TEST_CASE("CalcExtStats: recursive でサブディレクトリも見る")
{
	TempDir tmp;
	mkfile(tmp.file(_T("a.txt")), 1);
	mkdir_(tmp.file(_T("sub")));
	mkfile(tmp.file(_T("sub\\b.txt")), 1);

	bool truncated = false;
	CHECK(dir_info::CalcExtStats(tmp.path, false, false, false, truncated)[0].count == 1);
	CHECK(dir_info::CalcExtStats(tmp.path, true, false, false, truncated)[0].count == 2);
}

TEST_CASE("BuildTree: ディレクトリだけを深さ付きで並べる")
{
	TempDir tmp;
	mkdir_(tmp.file(_T("b_dir")));
	mkdir_(tmp.file(_T("a_dir")));
	mkdir_(tmp.file(_T("a_dir\\inner")));
	mkfile(tmp.file(_T("file.txt")), 1);  // ファイルは出ない

	bool truncated = false;
	const auto t = dir_info::BuildTree(tmp.path, 5, false, false, truncated);

	REQUIRE(t.size() == 3);
	// 名前順 (FindFirst の順はファイルシステム任せなので並べ直している)
	CHECK(t[0].name == UnicodeString(_T("a_dir")));
	CHECK(t[0].depth == 0);
	CHECK(t[1].name == UnicodeString(_T("inner")));
	CHECK(t[1].depth == 1);
	CHECK(t[2].name == UnicodeString(_T("b_dir")));
	CHECK(t[2].depth == 0);
}

TEST_CASE("BuildTree: 深さの上限で打ち切る")
{
	TempDir tmp;
	mkdir_(tmp.file(_T("d1")));
	mkdir_(tmp.file(_T("d1\\d2")));
	mkdir_(tmp.file(_T("d1\\d2\\d3")));

	bool truncated = false;
	CHECK(dir_info::BuildTree(tmp.path, 0, false, false, truncated).size() == 1);
	CHECK(dir_info::BuildTree(tmp.path, 1, false, false, truncated).size() == 2);
	CHECK(dir_info::BuildTree(tmp.path, 5, false, false, truncated).size() == 3);
}
