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

TEST_CASE("ResolveDiffDirSource: AL/DL/空の分岐")
{
	// VCL DiffDirActionExecute (MainFrm.cpp:16480付近) と同じ分岐。
	// AL=全件プリセット、DL=保存値プリセット、それ以外はダイアログ
	CHECK(compare::ResolveDiffDirSource(_T("AL")) == compare::DiffDirSource::AllPreset);
	CHECK(compare::ResolveDiffDirSource(_T("DL")) == compare::DiffDirSource::DefaultPreset);
	CHECK(compare::ResolveDiffDirSource(_T("")) == compare::DiffDirSource::Dialog);
	CHECK(compare::ResolveDiffDirSource(_T("CS")) == compare::DiffDirSource::Dialog);
	CHECK(compare::ResolveDiffDirSource(_T("AL;CS")) == compare::DiffDirSource::AllPreset);
}

TEST_CASE("NormalizeDiffIncMask: 空は *.* (VCL FormClose と同じ)")
{
	CHECK(compare::NormalizeDiffIncMask(_T("")) == UnicodeString(_T("*.*")));
	CHECK(compare::NormalizeDiffIncMask(_T("  ")) == UnicodeString(_T("*.*")));
	CHECK(compare::NormalizeDiffIncMask(_T("*.cpp")) == UnicodeString(_T("*.cpp")));
}

TEST_CASE("IsDiffExcDirEnabled: サブディレクトリ対象のときだけ有効")
{
	// VCL StartActionUpdate (DiffDlg.cpp:89): 除外欄は SubDir のときだけ有効
	CHECK(compare::IsDiffExcDirEnabled(true));
	CHECK_FALSE(compare::IsDiffExcDirEnabled(false));
}

TEST_CASE("AllDiffPreset: 全件・サブディレクトリ込み (VCL AL 分岐)")
{
	const compare::DiffDirOptions o = compare::AllDiffPreset();
	CHECK(o.inc_mask == UnicodeString(_T("*.*")));
	CHECK(o.exc_mask.IsEmpty());
	CHECK(o.exc_dir.IsEmpty());
	CHECK(o.sub_dir);
}

TEST_CASE("DefaultDiffPreset: 保存値をそのまま使い対象マスクだけ正規化")
{
	const compare::DiffDirOptions o =
		compare::DefaultDiffPreset(_T(""), _T("*.bak"), _T("bin"), true);
	CHECK(o.inc_mask == UnicodeString(_T("*.*")));
	CHECK(o.exc_mask == UnicodeString(_T("*.bak")));
	CHECK(o.exc_dir == UnicodeString(_T("bin")));
	CHECK(o.sub_dir);
}

TEST_CASE("FilterDiffItems: 対象に合い除外に合わないものだけ残す")
{
	const std::vector<FileItem> items = {f(_T("a.cpp")), f(_T("b.txt")), f(_T("c.bak")),
	                                     parent(), d(_T("dir"))};
	// 親 (..) とディレクトリは落とす。除外マスク *.bak も落とす
	const auto r = compare::FilterDiffItems(items, _T("*.cpp;*.txt;*.bak"), _T("*.bak"));
	REQUIRE(r.size() == 2);
	CHECK(r[0].name == UnicodeString(_T("a.cpp")));
	CHECK(r[1].name == UnicodeString(_T("b.txt")));
}
