/**
 * @file tests/core/test_gui_file_ops.cpp
 * @brief gui/file_ops.cpp (コピー・移動・名前変更・ディレクトリ作成・削除) の回帰テスト
 *
 * @details wx に依存しない部分だけをここでテストする (`nyanfi_gui_core`、
 * ルート CMakeLists.txt 参照)。ファイルシステムに触れるテストは
 * tests/temp_dir.h の TempDir が作る一時ディレクトリの中だけで行う。
 *
 * ゴミ箱送り (SendToTrash) はここでは「元の場所から消えたこと」までを確認する
 * (doctest からゴミ箱の中身そのものは検証できない)。実際にゴミ箱に入ったことは
 * 手動で powershell.exe から確認した (報告参照)。
 */
#include "doctest/doctest.h"

#include <cstring>
#include <memory>

#include "gui/file_ops.h"
#include "usr_file_ex.h"
#include "usr_str.h"

#include "temp_dir.h"

using nyanfi_test::TempDir;

namespace {

/// 中身の確認に使うだけの、narrow (1バイト文字) テキストファイルを書く
void write_text(const UnicodeString &fnam, const char *content)
{
	std::unique_ptr<TFileStream> fs(new TFileStream(fnam, fmCreate));
	fs->WriteBuffer(content, static_cast<int>(strlen(content)));
}

/// write_text() で書いた内容と一致するか確認する
bool read_text_equals(const UnicodeString &fnam, const char *expect)
{
	std::unique_ptr<TFileStream> fs(new TFileStream(fnam, fmOpenRead | fmShareDenyNone));
	const int len = static_cast<int>(strlen(expect));
	std::unique_ptr<char[]> buf(new char[static_cast<std::size_t>(len) + 1]);
	fs->ReadBuffer(buf.get(), len);
	buf[len] = '\0';
	return strncmp(buf.get(), expect, static_cast<std::size_t>(len)) == 0;
}

}  // namespace

//===========================================================================
// MakeDirectory
//===========================================================================
TEST_CASE("MakeDirectory: 新規ディレクトリを作成できる")
{
	TempDir dir;
	UnicodeString error;
	CHECK(file_ops::MakeDirectory(dir.path, _T("newdir"), error));
	CHECK(dir_exists(dir.path + _T("newdir")));
	CHECK(error.IsEmpty());
}

TEST_CASE("MakeDirectory: 同名のディレクトリが既にあれば失敗する (上書きしない)")
{
	TempDir dir;
	UnicodeString error;
	CHECK(file_ops::MakeDirectory(dir.path, _T("dup"), error));

	UnicodeString error2;
	CHECK_FALSE(file_ops::MakeDirectory(dir.path, _T("dup"), error2));
	CHECK_FALSE(error2.IsEmpty());
}

TEST_CASE("MakeDirectory: 同名のファイルが既にあれば失敗する")
{
	TempDir dir;
	write_text(dir.file(_T("dup.txt")), "x");

	UnicodeString error;
	CHECK_FALSE(file_ops::MakeDirectory(dir.path, _T("dup.txt"), error));
	CHECK_FALSE(error.IsEmpty());
}

TEST_CASE("MakeDirectory: 名前が空なら失敗する")
{
	TempDir dir;
	UnicodeString error;
	CHECK_FALSE(file_ops::MakeDirectory(dir.path, EmptyStr, error));
	CHECK_FALSE(error.IsEmpty());
}

//===========================================================================
// RenameItem
//===========================================================================
TEST_CASE("RenameItem: ファイルの名前を変更できる")
{
	TempDir dir;
	write_text(dir.file(_T("a.txt")), "hello");

	UnicodeString error;
	CHECK(file_ops::RenameItem(dir.path, _T("a.txt"), _T("b.txt"), error));
	CHECK_FALSE(file_exists(dir.file(_T("a.txt"))));
	CHECK(file_exists(dir.file(_T("b.txt"))));
}

TEST_CASE("RenameItem: ディレクトリの名前も変更できる")
{
	TempDir dir;
	CHECK(create_Dir(dir.file(_T("olddir"))));

	UnicodeString error;
	CHECK(file_ops::RenameItem(dir.path, _T("olddir"), _T("newdir"), error));
	CHECK_FALSE(dir_exists(dir.file(_T("olddir"))));
	CHECK(dir_exists(dir.file(_T("newdir"))));
}

TEST_CASE("RenameItem: 宛先が既に存在すれば失敗する (上書きしない)")
{
	TempDir dir;
	write_text(dir.file(_T("a.txt")), "A");
	write_text(dir.file(_T("b.txt")), "B");

	UnicodeString error;
	CHECK_FALSE(file_ops::RenameItem(dir.path, _T("a.txt"), _T("b.txt"), error));
	CHECK_FALSE(error.IsEmpty());
	// どちらの内容も変わっていないこと
	CHECK(file_exists(dir.file(_T("a.txt"))));
	CHECK(file_exists(dir.file(_T("b.txt"))));
}

TEST_CASE("RenameItem: 元が存在しなければ失敗する")
{
	TempDir dir;
	UnicodeString error;
	CHECK_FALSE(file_ops::RenameItem(dir.path, _T("nothing.txt"), _T("x.txt"), error));
	CHECK_FALSE(error.IsEmpty());
}

TEST_CASE("RenameItem: 同じ名前なら何もせず成功する")
{
	TempDir dir;
	write_text(dir.file(_T("a.txt")), "A");

	UnicodeString error;
	CHECK(file_ops::RenameItem(dir.path, _T("a.txt"), _T("a.txt"), error));
}

//===========================================================================
// CopyItems
//===========================================================================
TEST_CASE("CopyItems: 単一ファイルをコピーする")
{
	TempDir src, dst;
	write_text(src.file(_T("a.txt")), "hello");

	file_ops::FileOpResult r = file_ops::CopyItems({src.file(_T("a.txt"))}, dst.path);
	CHECK(r.success_count == 1);
	CHECK(r.skipped_existing == 0);
	CHECK(r.failures.empty());
	CHECK(file_exists(dst.file(_T("a.txt"))));
	CHECK(file_exists(src.file(_T("a.txt"))));  // コピー元は残る
}

TEST_CASE("CopyItems: 宛先に同名ファイルがあれば上書きせずスキップする")
{
	TempDir src, dst;
	write_text(src.file(_T("a.txt")), "new");
	write_text(dst.file(_T("a.txt")), "old");

	file_ops::FileOpResult r = file_ops::CopyItems({src.file(_T("a.txt"))}, dst.path);
	CHECK(r.success_count == 0);
	CHECK(r.skipped_existing == 1);

	// 内容が上書きされていないこと (中身は "old" のまま)
	CHECK(read_text_equals(dst.file(_T("a.txt")), "old"));
}

TEST_CASE("CopyItems: ディレクトリを再帰的にコピーする")
{
	TempDir src, dst;
	CHECK(create_Dir(src.file(_T("sub"))));
	write_text(src.file(_T("sub\\inner.txt")), "inner");
	write_text(src.file(_T("top.txt")), "top");

	file_ops::FileOpResult r = file_ops::CopyItems({ExcludeTrailingPathDelimiter(src.path)}, dst.path);
	CHECK(r.failures.empty());

	const UnicodeString copied_root = dst.file(ExtractFileName(ExcludeTrailingPathDelimiter(src.path)));
	CHECK(dir_exists(copied_root));
	CHECK(dir_exists(copied_root + _T("\\sub")));
	CHECK(file_exists(copied_root + _T("\\sub\\inner.txt")));
	CHECK(file_exists(copied_root + _T("\\top.txt")));
}

TEST_CASE("CopyItems: 宛先に同名ディレクトリが既にあればマージし、既存ファイルは上書きしない")
{
	TempDir src, dst;
	CHECK(create_Dir(src.file(_T("d"))));
	write_text(src.file(_T("d\\existing.txt")), "new-content");
	write_text(src.file(_T("d\\fresh.txt")), "fresh");

	CHECK(create_Dir(dst.file(_T("d"))));
	write_text(dst.file(_T("d\\existing.txt")), "old-content");

	file_ops::FileOpResult r = file_ops::CopyItems({src.file(_T("d"))}, dst.path);
	CHECK(r.skipped_existing == 1);   // existing.txt はスキップ
	CHECK(r.success_count == 1);      // fresh.txt だけコピーされる (ディレクトリ自体は既存なので新規作成カウントなし)
	CHECK(file_exists(dst.file(_T("d\\fresh.txt"))));
	CHECK(read_text_equals(dst.file(_T("d\\existing.txt")), "old-content"));
}

//===========================================================================
// MoveItems
//===========================================================================
TEST_CASE("MoveItems: 単一ファイルを移動する")
{
	TempDir src, dst;
	write_text(src.file(_T("a.txt")), "hello");

	file_ops::FileOpResult r = file_ops::MoveItems({src.file(_T("a.txt"))}, dst.path);
	CHECK(r.success_count == 1);
	CHECK(file_exists(dst.file(_T("a.txt"))));
	CHECK_FALSE(file_exists(src.file(_T("a.txt"))));  // 移動元は消える
}

TEST_CASE("MoveItems: 宛先に同名ファイルがあれば上書きせずスキップする (移動元は残る)")
{
	TempDir src, dst;
	write_text(src.file(_T("a.txt")), "new");
	write_text(dst.file(_T("a.txt")), "old");

	file_ops::FileOpResult r = file_ops::MoveItems({src.file(_T("a.txt"))}, dst.path);
	CHECK(r.skipped_existing == 1);
	CHECK(file_exists(src.file(_T("a.txt"))));  // 移動されず元のまま残る
}

TEST_CASE("MoveItems: 同一ボリューム内ならディレクトリも移動できる")
{
	TempDir src, dst;
	CHECK(create_Dir(src.file(_T("d"))));
	write_text(src.file(_T("d\\inner.txt")), "inner");

	file_ops::FileOpResult r = file_ops::MoveItems({ExcludeTrailingPathDelimiter(src.file(_T("d")))}, dst.path);
	CHECK(r.success_count == 1);
	CHECK(dir_exists(dst.file(_T("d"))));
	CHECK(file_exists(dst.file(_T("d\\inner.txt"))));
	CHECK_FALSE(dir_exists(src.file(_T("d"))));
}

//===========================================================================
// Summarize
//===========================================================================
TEST_CASE("Summarize: 件数を日本語の文にする")
{
	file_ops::FileOpResult r;
	r.success_count = 2;
	r.skipped_existing = 1;
	r.failures.push_back(_T("x.txt: テスト用の失敗"));

	const UnicodeString s = file_ops::Summarize(r);
	CHECK(ContainsStr(s, _T("成功 2 件")));
	CHECK(ContainsStr(s, _T("スキップ 1 件")));
	CHECK(ContainsStr(s, _T("失敗 1 件")));
	CHECK(ContainsStr(s, _T("x.txt")));
}

//===========================================================================
// SendToTrash
// @details doctest からはゴミ箱の中身そのものは確認できないため、ここでは
// 「元の場所から確実に消えること」までを確認する。実機での動作
// (実際にゴミ箱に入ること) は報告に記載の手動確認を参照。
//===========================================================================
TEST_CASE("SendToTrash: 削除後、元の場所からファイルが消える")
{
	TempDir dir;
	write_text(dir.file(_T("trash_me.txt")), "bye");

	UnicodeString error;
	const bool ok = file_ops::SendToTrash({dir.file(_T("trash_me.txt"))}, error);
	CHECK(ok);
	CHECK_FALSE(file_exists(dir.file(_T("trash_me.txt"))));
}

TEST_CASE("SendToTrash: 対象が空なら何もせず true を返す")
{
	UnicodeString error;
	CHECK(file_ops::SendToTrash({}, error));
}

//===========================================================================
// 自分自身・自分の配下への操作を弾く (無限再帰でディスクを埋める事故の防止)
//===========================================================================
TEST_CASE("IsSameOrInside: 同一パスと配下を検出する")
{
	using file_ops::IsSameOrInside;

	//同一パス (末尾の \ の有無に関わらず)
	CHECK(IsSameOrInside(_T("C:\\work\\a"), _T("C:\\work\\a")));
	CHECK(IsSameOrInside(_T("C:\\work\\a\\"), _T("C:\\work\\a")));
	CHECK(IsSameOrInside(_T("C:\\work\\a"), _T("C:\\work\\a\\")));

	//大小文字は無視する (Windows のパス)
	CHECK(IsSameOrInside(_T("C:\\Work\\A"), _T("c:\\work\\a")));

	//配下
	CHECK(IsSameOrInside(_T("C:\\work\\a"), _T("C:\\work\\a\\sub")));
	CHECK(IsSameOrInside(_T("C:\\work\\a"), _T("C:\\work\\a\\sub\\deep")));

	//配下ではない
	CHECK_FALSE(IsSameOrInside(_T("C:\\work\\a"), _T("C:\\work\\b")));
	CHECK_FALSE(IsSameOrInside(_T("C:\\work\\a"), _T("C:\\work")));
	//前方一致だが別ディレクトリ ("a" と "ab") — 区切りを見ているので誤検出しない
	CHECK_FALSE(IsSameOrInside(_T("C:\\work\\a"), _T("C:\\work\\ab")));
}

TEST_CASE("CopyItems: 自分の配下へのコピーを拒否する")
{
	TempDir tmp;
	const UnicodeString src = tmp.path + _T("src");
	const UnicodeString inner = src + _T("\\inner");
	REQUIRE(create_Dir(src));
	REQUIRE(create_Dir(inner));

	//src を src\inner の下へコピーしようとする (無限再帰になる操作)
	const file_ops::FileOpResult r =
		file_ops::CopyItems({src}, inner);

	CHECK(r.success_count == 0);
	CHECK(r.failures.size() == 1);
	//コピー先に src のコピーが作られていないこと
	CHECK_FALSE(dir_exists(inner + _T("\\src")));
}

TEST_CASE("CopyItems: 同一ディレクトリへのコピーを拒否する")
{
	TempDir tmp;
	const UnicodeString src = tmp.path + _T("dir");
	REQUIRE(create_Dir(src));

	//コピー元とコピー先が同じ (両ペインが同じディレクトリの場合)
	const file_ops::FileOpResult r = file_ops::CopyItems({src}, tmp.path);

	CHECK(r.success_count == 0);
	CHECK(r.failures.size() == 1);
}
