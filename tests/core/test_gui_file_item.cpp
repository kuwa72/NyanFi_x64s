/**
 * @file tests/core/test_gui_file_item.cpp
 * @brief gui/file_item.cpp (一覧の並べ替え比較・パスマスク照合) の回帰テスト
 *
 * gui/file_item.h/.cpp は wx に依存しない (nyanfi_gui_core、CMakeLists.txt
 * 参照) ため、GUI (wxWidgets) 無しでもここでテストできる。
 */
#include "doctest/doctest.h"

#include <algorithm>
#include <vector>

#include "gui/file_item.h"

namespace {

FileItem make_file(const UnicodeString &name, Int64 size = 0, int attr = 0)
{
	FileItem itm;
	itm.name = name;
	itm.size = size;
	itm.attr = attr;
	itm.is_dir = false;
	return itm;
}

FileItem make_dir(const UnicodeString &name)
{
	FileItem itm;
	itm.name = name;
	itm.is_dir = true;
	itm.size = -1;
	return itm;
}

FileItem make_parent()
{
	FileItem itm;
	itm.name = "..";
	itm.is_dir = true;
	itm.is_parent = true;
	itm.size = -1;
	return itm;
}

void sort_items(std::vector<FileItem> &items, SortKey key, bool descending, bool dirs_first)
{
	std::sort(items.begin(), items.end(), [&](const FileItem &a, const FileItem &b) {
		return CompareFileItems(a, b, key, descending, dirs_first) < 0;
	});
}

}  // namespace

//===========================================================================
// CompareFileItems: 並べ替え
//===========================================================================
TEST_CASE("CompareFileItems: \"..\" は常に先頭")
{
	std::vector<FileItem> items{make_file("b.txt"), make_parent(), make_dir("a_dir")};
	sort_items(items, SortKey::Name, false, true);

	CHECK(items[0].is_parent == true);
}

TEST_CASE("CompareFileItems: dirs_first=true だとディレクトリが先に集まる")
{
	std::vector<FileItem> items{make_file("a.txt"), make_dir("z_dir"), make_file("b.txt"), make_dir("a_dir")};
	sort_items(items, SortKey::Name, false, true);

	CHECK(items[0].is_dir == true);
	CHECK(items[1].is_dir == true);
	CHECK(items[2].is_dir == false);
	CHECK(items[3].is_dir == false);
	// ディレクトリ同士・ファイル同士は名前の自然順
	CHECK(items[0].name == UnicodeString("a_dir"));
	CHECK(items[1].name == UnicodeString("z_dir"));
	CHECK(items[2].name == UnicodeString("a.txt"));
	CHECK(items[3].name == UnicodeString("b.txt"));
}

TEST_CASE("CompareFileItems: dirs_first=false だとディレクトリもファイルも同列に並ぶ")
{
	std::vector<FileItem> items{make_file("b.txt"), make_dir("a_dir"), make_file("c.txt")};
	sort_items(items, SortKey::Name, false, false);

	CHECK(items[0].name == UnicodeString("a_dir"));
	CHECK(items[1].name == UnicodeString("b.txt"));
	CHECK(items[2].name == UnicodeString("c.txt"));
}

TEST_CASE("CompareFileItems: Name キーは自然順 (StrCmpLogicalW) で数値を数として比較する")
{
	std::vector<FileItem> items{make_file("file10.txt"), make_file("file2.txt"), make_file("file1.txt")};
	sort_items(items, SortKey::Name, false, false);

	CHECK(items[0].name == UnicodeString("file1.txt"));
	CHECK(items[1].name == UnicodeString("file2.txt"));
	CHECK(items[2].name == UnicodeString("file10.txt"));
}

TEST_CASE("CompareFileItems: 降順は昇順の逆になる")
{
	std::vector<FileItem> items{make_file("a.txt"), make_file("b.txt"), make_file("c.txt")};
	sort_items(items, SortKey::Name, true, false);

	CHECK(items[0].name == UnicodeString("c.txt"));
	CHECK(items[1].name == UnicodeString("b.txt"));
	CHECK(items[2].name == UnicodeString("a.txt"));
}

TEST_CASE("CompareFileItems: Ext キーは拡張子で比較し、同じ拡張子なら名前で決める")
{
	std::vector<FileItem> items{make_file("b.txt"), make_file("a.doc"), make_file("a.txt")};
	sort_items(items, SortKey::Ext, false, false);

	CHECK(items[0].name == UnicodeString("a.doc"));  // .doc < .txt
	CHECK(items[1].name == UnicodeString("a.txt"));  // .txt 同士は名前順
	CHECK(items[2].name == UnicodeString("b.txt"));
}

TEST_CASE("CompareFileItems: Size キーはサイズの数値で比較する (文字列比較にはしない)")
{
	std::vector<FileItem> items{make_file("a.txt", 1024), make_file("b.txt", 9), make_file("c.txt", 128)};
	sort_items(items, SortKey::Size, false, false);

	// 文字列として比較すると "1024" < "128" < "9" になってしまうが、数値比較なら 9 < 128 < 1024
	CHECK(items[0].name == UnicodeString("b.txt"));
	CHECK(items[1].name == UnicodeString("c.txt"));
	CHECK(items[2].name == UnicodeString("a.txt"));
}

TEST_CASE("CompareFileItems: Attr キーは属性値で比較する")
{
	std::vector<FileItem> items{make_file("a.txt", 0, 32), make_file("b.txt", 0, 1), make_file("c.txt", 0, 2)};
	sort_items(items, SortKey::Attr, false, false);

	CHECK(items[0].name == UnicodeString("b.txt"));
	CHECK(items[1].name == UnicodeString("c.txt"));
	CHECK(items[2].name == UnicodeString("a.txt"));
}

//===========================================================================
// MatchPathMask: パスマスク照合 (MainFrm.cpp::ApplyPathMask 相当)
//===========================================================================
TEST_CASE("MatchPathMask: 空文字列/\"*\" は常に一致する")
{
	CHECK(MatchPathMask("", "readme.txt", false) == true);
	CHECK(MatchPathMask("*", "readme.txt", false) == true);
}

TEST_CASE("MatchPathMask: ワイルドカードで絞り込む")
{
	CHECK(MatchPathMask("*.txt", "readme.txt", false) == true);
	CHECK(MatchPathMask("*.txt", "readme.doc", false) == false);
}

TEST_CASE("MatchPathMask: セミコロン区切りで複数マスクを OR する")
{
	CHECK(MatchPathMask("*.txt;*.doc", "readme.doc", false) == true);
	CHECK(MatchPathMask("*.txt;*.doc", "readme.ini", false) == false);
}

TEST_CASE("MatchPathMask: 先頭 ! は除外指定")
{
	CHECK(MatchPathMask("*.txt;!secret*.txt", "readme.txt", false) == true);
	CHECK(MatchPathMask("*.txt;!secret*.txt", "secret_note.txt", false) == false);
}

TEST_CASE("MatchPathMask: 末尾 \\ はディレクトリ専用マスク (ファイルには適用されない)")
{
	// "work*\" はディレクトリ名にだけ効く。ファイル側は除外指定が無いので既定の * が補われる
	CHECK(MatchPathMask("work*\\", "work_dir", true) == true);
	CHECK(MatchPathMask("work*\\", "other_dir", true) == false);
	CHECK(MatchPathMask("work*\\", "anything.txt", false) == true);
}
