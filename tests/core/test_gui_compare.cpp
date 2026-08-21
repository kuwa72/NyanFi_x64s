/**
 * @file tests/core/test_gui_compare.cpp
 * @brief gui/compare.cpp (左右の比較) のテスト
 */
#include "doctest/doctest.h"

#include "gui/compare.h"

namespace {

FileItem f(const UnicodeString &name, Int64 size = 100, double day = 46000.0)
{
	FileItem it;
	it.name = name;
	it.size = size;
	it.stamp = day;
	return it;
}

FileItem d(const UnicodeString &name)
{
	FileItem it;
	it.name = name;
	it.is_dir = true;
	it.size = -1;
	return it;
}

FileItem parent()
{
	FileItem it;
	it.name = _T("..");
	it.is_dir = true;
	it.is_parent = true;
	return it;
}

}  // namespace

TEST_CASE("IsSameItem: 名前だけの比較")
{
	CHECK(compare::IsSameItem(f(_T("a.txt"), 1), f(_T("A.TXT"), 999),
	                          compare::MatchBy::Name));
	CHECK_FALSE(compare::IsSameItem(f(_T("a.txt")), f(_T("b.txt")),
	                                compare::MatchBy::Name));
}

TEST_CASE("IsSameItem: 名前とサイズ")
{
	CHECK(compare::IsSameItem(f(_T("a.txt"), 100), f(_T("a.txt"), 100),
	                          compare::MatchBy::NameSize));
	CHECK_FALSE(compare::IsSameItem(f(_T("a.txt"), 100), f(_T("a.txt"), 200),
	                                compare::MatchBy::NameSize));
}

TEST_CASE("IsSameItem: 更新日時は2秒までの差を無視する")
{
	// FAT は2秒単位なので、ファイルシステムをまたぐと同じファイルでも
	// 厳密比較では一致しない
	const double sec = 1.0 / (24.0 * 60.0 * 60.0);
	CHECK(compare::IsSameItem(f(_T("a"), 1, 46000.0), f(_T("a"), 1, 46000.0 + sec),
	                          compare::MatchBy::NameTime));
	CHECK_FALSE(compare::IsSameItem(f(_T("a"), 1, 46000.0), f(_T("a"), 1, 46000.0 + sec * 10),
	                                compare::MatchBy::NameTime));
}

TEST_CASE("IndicesOnlyHere: こちらだけにあるファイル")
{
	const std::vector<FileItem> left = {parent(), d(_T("dir")), f(_T("both.txt")),
	                                    f(_T("only_left.txt"))};
	const std::vector<FileItem> right = {f(_T("both.txt")), f(_T("only_right.txt"))};

	const std::vector<int> idx = compare::IndicesOnlyHere(left, right, compare::MatchBy::Name);
	REQUIRE(idx.size() == 1);
	CHECK(left[idx[0]].name == UnicodeString(_T("only_left.txt")));
}

TEST_CASE("IndicesOnlyHere: ディレクトリと .. は対象外")
{
	const std::vector<FileItem> left = {parent(), d(_T("onlydir"))};
	const std::vector<FileItem> right = {};
	CHECK(compare::IndicesOnlyHere(left, right, compare::MatchBy::Name).empty());
}

TEST_CASE("IndicesOnlyHere: サイズが違えば「こちらだけ」に数える")
{
	const std::vector<FileItem> left = {f(_T("a.txt"), 100)};
	const std::vector<FileItem> right = {f(_T("a.txt"), 200)};

	CHECK(compare::IndicesOnlyHere(left, right, compare::MatchBy::Name).empty());
	CHECK(compare::IndicesOnlyHere(left, right, compare::MatchBy::NameSize).size() == 1);
}

TEST_CASE("DiffDirectories: 違うものだけを返す")
{
	const std::vector<FileItem> left = {f(_T("same.txt"), 10), f(_T("diff.txt"), 10),
	                                    f(_T("left_only.txt"))};
	const std::vector<FileItem> right = {f(_T("same.txt"), 10), f(_T("diff.txt"), 99),
	                                     f(_T("right_only.txt"))};

	const auto rows = compare::DiffDirectories(left, right, compare::MatchBy::NameSize);

	// same.txt は含まれない
	REQUIRE(rows.size() == 3);
	for (const auto &r : rows) CHECK(r.name != UnicodeString(_T("same.txt")));
}

TEST_CASE("DiffDirectories: 片側だけ / 両方あるが違う を区別する")
{
	const std::vector<FileItem> left = {f(_T("a.txt"), 10), f(_T("l.txt"))};
	const std::vector<FileItem> right = {f(_T("a.txt"), 99), f(_T("r.txt"))};

	const auto rows = compare::DiffDirectories(left, right, compare::MatchBy::NameSize);
	REQUIRE(rows.size() == 3);

	// 名前順に並ぶ: a.txt, l.txt, r.txt
	CHECK(rows[0].name == UnicodeString(_T("a.txt")));
	CHECK(rows[0].in_left);
	CHECK(rows[0].in_right);
	CHECK(rows[0].differs);

	CHECK(rows[1].name == UnicodeString(_T("l.txt")));
	CHECK(rows[1].in_left);
	CHECK_FALSE(rows[1].in_right);

	CHECK(rows[2].name == UnicodeString(_T("r.txt")));
	CHECK_FALSE(rows[2].in_left);
	CHECK(rows[2].in_right);
}

TEST_CASE("DiffDirectories: 大文字小文字を区別せず突き合わせる")
{
	const std::vector<FileItem> left = {f(_T("Data.TXT"), 10)};
	const std::vector<FileItem> right = {f(_T("data.txt"), 10)};
	CHECK(compare::DiffDirectories(left, right, compare::MatchBy::NameSize).empty());
}

TEST_CASE("DiffDirectories: 空同士なら空")
{
	CHECK(compare::DiffDirectories({}, {}, compare::MatchBy::Name).empty());
}
