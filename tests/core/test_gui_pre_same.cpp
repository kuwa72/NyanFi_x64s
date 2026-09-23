/**
 * @file tests/core/test_gui_pre_same.cpp
 * @brief gui/pre_same.h (TPreSameNemeDlg の判断ロジック) のテスト
 */
#include "doctest/doctest.h"

#include <cstring>

#include "gui/pre_same.h"
#include "temp_dir.h"
#include "usr_file_ex.h"

using nyanfi_test::TempDir;

namespace {

void write_text(const UnicodeString &path, const char *text)
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
	                         FILE_ATTRIBUTE_NORMAL, nullptr);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	DWORD written = 0;
	::WriteFile(h, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
	::CloseHandle(h);
}

}  // namespace

TEST_CASE("pre_same: PR パラメータとモード")
{
	CHECK(pre_same::IsRequested(_T("PR")) == true);
	CHECK(pre_same::IsRequested(_T("KT;PR")) == true);
	CHECK(pre_same::IsRequested(_T("KT")) == false);
	CHECK(pre_same::IsRequested(_T("PRX")) == false);
	CHECK(pre_same::ModeLabel(pre_same::Mode::Automatic) == _T("実行者に委ねる"));
	CHECK(pre_same::ModeLabel(pre_same::Mode::Ask) == _T("作成時に確認"));
	CHECK(pre_same::ModeLabel(pre_same::Mode::Newest) == _T("最新から上書き"));
	CHECK(pre_same::ModeLabel(pre_same::Mode::Skip) == _T("スキップ"));
	CHECK(pre_same::ModeLabel(pre_same::Mode::AutoRename) == _T("自動的に名前を交互.swap"));
}

TEST_CASE("pre_same: VCL の同名処理番号へ対応づける")
{
	CHECK(pre_same::LegacyCopyMode(pre_same::Mode::Automatic) == -1);
	CHECK(pre_same::LegacyCopyMode(pre_same::Mode::Ask) == 0);
	CHECK(pre_same::LegacyCopyMode(pre_same::Mode::Newest) == 1);
	CHECK(pre_same::LegacyCopyMode(pre_same::Mode::Skip) == 2);
	CHECK(pre_same::LegacyCopyMode(pre_same::Mode::AutoRename) == 3);
}

TEST_CASE("pre_same: ファイル操作の競合方針，解决後は確認を省略")
{
	CHECK(pre_same::PolicyFor(pre_same::Mode::Automatic) == file_ops::ConflictPolicy::SkipExisting);
	CHECK(pre_same::PolicyFor(pre_same::Mode::Ask) == file_ops::ConflictPolicy::Overwrite);
	CHECK(pre_same::PolicyFor(pre_same::Mode::Newest) == file_ops::ConflictPolicy::NewestWins);
	CHECK(pre_same::PolicyFor(pre_same::Mode::Skip) == file_ops::ConflictPolicy::SkipExisting);
	CHECK(pre_same::PolicyFor(pre_same::Mode::AutoRename) == file_ops::ConflictPolicy::AutoRename);
	CHECK(pre_same::RequiresConfirmation(pre_same::Mode::Ask) == true);
	CHECK(pre_same::RequiresConfirmation(pre_same::Mode::Automatic) == false);
}

TEST_CASE("pre_same: 範囲外の選択を自動に正規化する")
{
	CHECK(pre_same::NormalizeMode(-1) == pre_same::Mode::Automatic);
	CHECK(pre_same::NormalizeMode(0) == pre_same::Mode::Automatic);
	CHECK(pre_same::NormalizeMode(4) == pre_same::Mode::AutoRename);
	CHECK(pre_same::NormalizeMode(99) == pre_same::Mode::Automatic);
}

TEST_CASE("pre_same: 上書き・自動改名・最新判定をファイル操作へ渡す")
{
	TempDir src, dst;
	write_text(src.file(_T("item.txt")), "new");
	write_text(dst.file(_T("item.txt")), "old");

	file_ops::FileOpResult overwrite = file_ops::CopyItems(
		{src.file(_T("item.txt"))}, dst.path, file_ops::ConflictPolicy::Overwrite);
	CHECK(overwrite.success_count == 1);
	CHECK(overwrite.skipped_existing == 0);

	write_text(src.file(_T("item.txt")), "newer");
	REQUIRE(set_file_age(src.file(_T("item.txt")), Now()));
	REQUIRE(set_file_age(dst.file(_T("item.txt")), Now() - 1));
	file_ops::FileOpResult newest = file_ops::CopyItems(
		{src.file(_T("item.txt"))}, dst.path, file_ops::ConflictPolicy::NewestWins);
	CHECK(newest.success_count == 1);
	CHECK(newest.skipped_existing == 0);

	write_text(dst.file(_T("item.txt")), "collision");
	file_ops::FileOpResult renamed = file_ops::CopyItems(
		{src.file(_T("item.txt"))}, dst.path, file_ops::ConflictPolicy::AutoRename);
	CHECK(renamed.success_count == 1);
	CHECK(renamed.skipped_existing == 0);
	CHECK(file_exists(dst.file(_T("item_1.txt"))));

	file_ops::FileOpResult moved = file_ops::MoveItems(
		{src.file(_T("item.txt"))}, dst.path, file_ops::ConflictPolicy::AutoRename);
	CHECK(moved.success_count == 1);
	CHECK(moved.skipped_existing == 0);
	CHECK(file_exists(dst.file(_T("item_2.txt"))));
	CHECK_FALSE(file_exists(src.file(_T("item.txt"))));
}
