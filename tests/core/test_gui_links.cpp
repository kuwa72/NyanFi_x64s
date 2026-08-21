/**
 * @file tests/core/test_gui_links.cpp
 * @brief gui/links.cpp のテスト
 */
#include "doctest/doctest.h"

#include "gui/links.h"
#include "usr_file_ex.h"
#include "temp_dir.h"

using nyanfi_test::TempDir;

namespace {

void write_text(const UnicodeString &fnam, const char *content)
{
	HANDLE h = ::CreateFileW(fnam.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
	                         FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	DWORD written = 0;
	::WriteFile(h, content, static_cast<DWORD>(::strlen(content)), &written, NULL);
	::CloseHandle(h);
}

}  // namespace

TEST_CASE("CanCreateHardLink: 同一ボリュームかつ NTFS のときだけ")
{
	// MainFrm.cpp:15707-15710
	CHECK(links::CanCreateHardLink(_T("C:\\"), _T("C:\\"), _T("NTFS")));

	// ボリュームが違えば不可 (ハードリンクはまたげない)
	CHECK_FALSE(links::CanCreateHardLink(_T("C:\\"), _T("D:\\"), _T("NTFS")));
	// NTFS でなければ不可
	CHECK_FALSE(links::CanCreateHardLink(_T("C:\\"), _T("C:\\"), _T("FAT32")));
	CHECK_FALSE(links::CanCreateHardLink(_T("C:\\"), _T("C:\\"), _T("exFAT")));

	// 大文字小文字は区別しない
	CHECK(links::CanCreateHardLink(_T("c:\\"), _T("C:\\"), _T("ntfs")));
}

TEST_CASE("ShortcutNameFor: .lnk を付ける")
{
	CHECK(links::ShortcutNameFor(_T("a.txt")) == UnicodeString(_T("a.txt.lnk")));
	CHECK(links::ShortcutNameFor(_T("dir")) == UnicodeString(_T("dir.lnk")));
}

TEST_CASE("CreateLinks: ショートカットを作る")
{
	TempDir src, dst;
	write_text(src.file(_T("target.txt")), "x");

	const file_ops::FileOpResult r =
		links::CreateLinks({src.file(_T("target.txt"))}, dst.path, links::LinkKind::Shortcut);

	CHECK(r.success_count == 1);
	CHECK(r.failures.empty());
	CHECK(file_exists(dst.file(_T("target.txt.lnk"))));
}

TEST_CASE("CreateLinks: 宛先に同名があれば上書きせずスキップする")
{
	TempDir src, dst;
	write_text(src.file(_T("t.txt")), "x");
	write_text(dst.file(_T("t.txt.lnk")), "already");

	const file_ops::FileOpResult r =
		links::CreateLinks({src.file(_T("t.txt"))}, dst.path, links::LinkKind::Shortcut);

	CHECK(r.success_count == 0);
	CHECK(r.skipped_existing == 1);
}

TEST_CASE("CreateLinks: ディレクトリにはハードリンクを張れない")
{
	TempDir src, dst;
	::CreateDirectoryW(src.file(_T("sub")).c_str(), NULL);

	const file_ops::FileOpResult r =
		links::CreateLinks({src.file(_T("sub"))}, dst.path, links::LinkKind::Hard);

	CHECK(r.success_count == 0);
	REQUIRE(r.failures.size() == 1);
	CHECK(ContainsText(r.failures[0], _T("ハードリンク")));
}

TEST_CASE("SetDirTimeRecursive: 配下の最新に合わせる")
{
	TempDir tmp;
	::CreateDirectoryW(tmp.file(_T("sub")).c_str(), NULL);
	write_text(tmp.file(_T("sub\\old.txt")), "x");
	write_text(tmp.file(_T("new.txt")), "x");

	const TDateTime got = links::SetDirTimeRecursive(tmp.path, false, false);
	CHECK(static_cast<double>(got) > 0.0);

	// 実際にディレクトリのタイムスタンプが動いていること
	TSearchRec sr;
	REQUIRE(FindFirst(ExcludeTrailingPathDelimiter(tmp.path), faAnyFile, sr) == 0);
	FindClose(sr);
}

TEST_CASE("SetDirTimeRecursive: 空のディレクトリは触らない")
{
	// 0 で上書きすると 1899年になってしまう
	TempDir tmp;
	::CreateDirectoryW(tmp.file(_T("empty")).c_str(), NULL);

	const TDateTime got = links::SetDirTimeRecursive(tmp.file(_T("empty")), false, false);
	CHECK(static_cast<double>(got) == 0.0);
}

TEST_CASE("SetDirTimeRecursive: 隠しファイルしか無ければ数えない")
{
	// 表示していないファイルは数えない (task_thread.cpp:1694)
	TempDir tmp;
	::CreateDirectoryW(tmp.file(_T("h")).c_str(), NULL);
	write_text(tmp.file(_T("h\\secret.txt")), "x");
	::SetFileAttributesW(tmp.file(_T("h\\secret.txt")).c_str(), FILE_ATTRIBUTE_HIDDEN);

	CHECK(static_cast<double>(links::SetDirTimeRecursive(tmp.file(_T("h")), false, false)) == 0.0);
	// 隠しも見る設定なら数える
	CHECK(static_cast<double>(links::SetDirTimeRecursive(tmp.file(_T("h")), true, false)) > 0.0);
}
