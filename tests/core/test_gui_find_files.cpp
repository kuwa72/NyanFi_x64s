/**
 * @file tests/core/test_gui_find_files.cpp
 * @brief gui/find_files.cpp のテスト
 */
#include "doctest/doctest.h"

#include "gui/find_files.h"
#include "temp_dir.h"

using nyanfi_test::TempDir;

namespace {

void mkfile(const UnicodeString &path)
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
	                         FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	::CloseHandle(h);
}

void mkdir_(const UnicodeString &path) { ::CreateDirectoryW(path.c_str(), NULL); }

bool contains(const std::vector<FileItem> &v, const UnicodeString &name)
{
	for (const FileItem &it : v) {
		if (SameText(it.name, name)) return true;
	}
	return false;
}

}  // namespace

//===========================================================================
// MatchesMask
//===========================================================================

TEST_CASE("MatchesMask: 空のマスクは常に一致する")
{
	CHECK(find_files::MatchesMask(_T("anything.txt"), EmptyStr));
}

TEST_CASE("MatchesMask: * と ?")
{
	CHECK(find_files::MatchesMask(_T("a.txt"), _T("*.txt")));
	CHECK(find_files::MatchesMask(_T("a.txt"), _T("*")));
	CHECK(find_files::MatchesMask(_T("a.txt"), _T("?.txt")));
	CHECK_FALSE(find_files::MatchesMask(_T("ab.txt"), _T("?.txt")));
	CHECK_FALSE(find_files::MatchesMask(_T("a.dat"), _T("*.txt")));
}

TEST_CASE("MatchesMask: 大文字小文字を区別しない")
{
	CHECK(find_files::MatchesMask(_T("A.TXT"), _T("*.txt")));
	CHECK(find_files::MatchesMask(_T("a.txt"), _T("*.TXT")));
}

TEST_CASE("MatchesMask: セミコロン区切りで複数指定")
{
	CHECK(find_files::MatchesMask(_T("a.txt"), _T("*.dat;*.txt")));
	CHECK(find_files::MatchesMask(_T("a.dat"), _T("*.dat;*.txt")));
	CHECK_FALSE(find_files::MatchesMask(_T("a.bin"), _T("*.dat;*.txt")));
	// 空白が入っていても効く
	CHECK(find_files::MatchesMask(_T("a.txt"), _T("*.dat ; *.txt")));
}

TEST_CASE("MatchesMask: * が途中にある")
{
	CHECK(find_files::MatchesMask(_T("report_2026.txt"), _T("report*.txt")));
	CHECK(find_files::MatchesMask(_T("report.txt"), _T("report*.txt")));
	CHECK(find_files::MatchesMask(_T("abcdef"), _T("a*f")));
	CHECK_FALSE(find_files::MatchesMask(_T("abcdeg"), _T("a*f")));
}

//===========================================================================
// Search
//===========================================================================

TEST_CASE("Search: 再帰的にファイルを探す")
{
	TempDir tmp;
	mkfile(tmp.file(_T("top.txt")));
	mkdir_(tmp.file(_T("sub")));
	mkfile(tmp.file(_T("sub\\inner.txt")));
	mkfile(tmp.file(_T("sub\\other.dat")));

	find_files::Query q;
	q.mask = _T("*.txt");
	const find_files::Result r = find_files::Search(tmp.path, q);

	CHECK(r.items.size() == 2);
	CHECK(contains(r.items, _T("top.txt")));
	CHECK(contains(r.items, _T("inner.txt")));
	CHECK_FALSE(contains(r.items, _T("other.dat")));
	CHECK_FALSE(r.truncated_scan);
	CHECK_FALSE(r.truncated_hits);
}

TEST_CASE("Search: full_path が入る (別ディレクトリの項目が混ざるため)")
{
	TempDir tmp;
	mkdir_(tmp.file(_T("sub")));
	mkfile(tmp.file(_T("sub\\x.txt")));

	find_files::Query q;
	const find_files::Result r = find_files::Search(tmp.path, q);

	REQUIRE(r.items.size() == 1);
	CHECK_FALSE(r.items[0].full_path.IsEmpty());
	CHECK(EndsText(_T("sub\\x.txt"), r.items[0].full_path));
}

TEST_CASE("Search: recursive を切ると直下だけ")
{
	TempDir tmp;
	mkfile(tmp.file(_T("top.txt")));
	mkdir_(tmp.file(_T("sub")));
	mkfile(tmp.file(_T("sub\\inner.txt")));

	find_files::Query q;
	q.recursive = false;
	const find_files::Result r = find_files::Search(tmp.path, q);

	CHECK(r.items.size() == 1);
	CHECK(contains(r.items, _T("top.txt")));
}

TEST_CASE("Search: ディレクトリだけ / 両方")
{
	TempDir tmp;
	mkfile(tmp.file(_T("a.txt")));
	mkdir_(tmp.file(_T("adir")));

	find_files::Query q;
	q.target = find_files::Target::Directories;
	const find_files::Result d = find_files::Search(tmp.path, q);
	CHECK(d.items.size() == 1);
	CHECK(contains(d.items, _T("adir")));

	q.target = find_files::Target::Both;
	CHECK(find_files::Search(tmp.path, q).items.size() == 2);
}

TEST_CASE("Search: 隠しファイルは設定に従う")
{
	TempDir tmp;
	mkfile(tmp.file(_T("visible.txt")));
	mkfile(tmp.file(_T("hidden.txt")));
	::SetFileAttributesW(tmp.file(_T("hidden.txt")).c_str(), FILE_ATTRIBUTE_HIDDEN);

	find_files::Query q;
	CHECK(find_files::Search(tmp.path, q).items.size() == 1);
	q.show_hidden = true;
	CHECK(find_files::Search(tmp.path, q).items.size() == 2);
}

TEST_CASE("Search: 見つからなければ空 (エラーにしない)")
{
	TempDir tmp;
	mkfile(tmp.file(_T("a.dat")));

	find_files::Query q;
	q.mask = _T("*.nosuch");
	const find_files::Result r = find_files::Search(tmp.path, q);
	CHECK(r.items.empty());
	CHECK(r.scanned == 1);  // 走査はしている
}

TEST_CASE("Search: 存在しないディレクトリでも落ちない")
{
	find_files::Query q;
	const find_files::Result r = find_files::Search(_T("C:\\nosuch_dir_xyz"), q);
	CHECK(r.items.empty());
	CHECK(r.scanned == 0);
}

//===========================================================================
// FindDuplicates
//===========================================================================

namespace {

void mkfile_data(const UnicodeString &path, const char *data)
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
	                         FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	DWORD written = 0;
	::WriteFile(h, data, static_cast<DWORD>(::strlen(data)), &written, NULL);
	::CloseHandle(h);
}

}  // namespace

TEST_CASE("FindDuplicates: 内容が同じものを見つける")
{
	TempDir tmp;
	mkdir_(tmp.file(_T("sub")));
	mkfile_data(tmp.file(_T("a.txt")), "same content");
	mkfile_data(tmp.file(_T("sub\\b.txt")), "same content");   // 名前は違うが中身が同じ
	mkfile_data(tmp.file(_T("c.txt")), "different");

	const auto r = find_files::FindDuplicates(tmp.path, find_files::DuplicateBy::Content,
	                                           false, false);
	CHECK(r.groups == 1);
	CHECK(r.items.size() == 2);
	CHECK(contains(r.items, _T("a.txt")));
	CHECK(contains(r.items, _T("b.txt")));
	CHECK_FALSE(contains(r.items, _T("c.txt")));
}

TEST_CASE("FindDuplicates: サイズで枝刈りするのでハッシュ計算は最小限")
{
	// サイズが違うファイルはハッシュを取らない (ToOppSameHash と同じ枝刈り)
	TempDir tmp;
	mkfile_data(tmp.file(_T("a.txt")), "aaa");
	mkfile_data(tmp.file(_T("b.txt")), "bbb");    // 同じサイズ → 計算する
	mkfile_data(tmp.file(_T("c.txt")), "cccccc"); // サイズが違う → 計算しない

	const auto r = find_files::FindDuplicates(tmp.path, find_files::DuplicateBy::Content,
	                                           false, false);
	CHECK(r.groups == 0);
	CHECK(r.hashed == 2);  // c.txt は計算していない
}

TEST_CASE("FindDuplicates: 名前とサイズでの判定")
{
	TempDir tmp;
	mkdir_(tmp.file(_T("s1")));
	mkdir_(tmp.file(_T("s2")));
	mkfile_data(tmp.file(_T("s1\\same.txt")), "xxx");
	mkfile_data(tmp.file(_T("s2\\same.txt")), "yyy");  // 中身は違うが名前とサイズが同じ

	const auto by_name = find_files::FindDuplicates(tmp.path, find_files::DuplicateBy::NameSize,
	                                                 false, false);
	CHECK(by_name.groups == 1);
	CHECK(by_name.items.size() == 2);

	// 内容で見れば重複ではない
	const auto by_content = find_files::FindDuplicates(tmp.path, find_files::DuplicateBy::Content,
	                                                    false, false);
	CHECK(by_content.groups == 0);
}

TEST_CASE("FindDuplicates: 空ファイルは対象外")
{
	// 互いに「同じ内容」になってしまい、大量に並ぶだけで役に立たない
	TempDir tmp;
	mkfile(tmp.file(_T("e1.txt")));
	mkfile(tmp.file(_T("e2.txt")));

	const auto r = find_files::FindDuplicates(tmp.path, find_files::DuplicateBy::Content,
	                                           false, false);
	CHECK(r.groups == 0);
	CHECK(r.items.empty());
}

TEST_CASE("FindDuplicates: 重複が無ければ空")
{
	TempDir tmp;
	mkfile_data(tmp.file(_T("a.txt")), "one");
	mkfile_data(tmp.file(_T("b.txt")), "two2");

	const auto r = find_files::FindDuplicates(tmp.path, find_files::DuplicateBy::Content,
	                                           false, false);
	CHECK(r.items.empty());
}
