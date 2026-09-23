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

//---------------------------------------------------------------------------
// 同名ファイルの比較 (TFileCompDlg / src/CompDlg.cpp、MainFrm.cpp:14464)
//---------------------------------------------------------------------------

TEST_CASE("CompSizeMet: サイズ条件 (VCL の s_mode と同じ並び)")
{
	using SM = compare::CompSizeMode;
	CHECK(compare::CompSizeMet(SM::Ignore, 1, 2));   // 無条件
	CHECK(compare::CompSizeMet(SM::Unequal, 1, 2));
	CHECK_FALSE(compare::CompSizeMet(SM::Unequal, 1, 1));
	CHECK(compare::CompSizeMet(SM::Equal, 5, 5));
	CHECK_FALSE(compare::CompSizeMet(SM::Equal, 5, 6));
	CHECK(compare::CompSizeMet(SM::Greater, 6, 5));
	CHECK_FALSE(compare::CompSizeMet(SM::Greater, 5, 6));
	CHECK(compare::CompSizeMet(SM::Less, 5, 6));
	CHECK_FALSE(compare::CompSizeMet(SM::Less, 6, 5));
}

TEST_CASE("CompTimeMet: 日時は2秒の許容誤差つき (TimeTolerance=2000 相当)")
{
	using TM = compare::CompTimeMode;
	const double sec = 1.0 / (24.0 * 60.0 * 60.0);
	CHECK(compare::CompTimeMet(TM::Ignore, 1.0, 2.0));
	// 誤差内なら「一致」扱い。ファイルシステムをまたぐと秒未満が丸まる
	CHECK(compare::CompTimeMet(TM::Equal, 46000.0, 46000.0 + sec));
	CHECK_FALSE(compare::CompTimeMet(TM::Equal, 46000.0, 46000.0 + sec * 10));
	CHECK(compare::CompTimeMet(TM::Unequal, 46000.0, 46000.0 + sec * 10));
	CHECK_FALSE(compare::CompTimeMet(TM::Unequal, 46000.0, 46000.0 + sec));
	CHECK(compare::CompTimeMet(TM::Newer, 46000.0, 46000.0 - sec * 10));
	CHECK_FALSE(compare::CompTimeMet(TM::Newer, 46000.0, 46000.0 + sec * 10));
	CHECK(compare::CompTimeMet(TM::Older, 46000.0, 46000.0 + sec * 10));
}

TEST_CASE("ResolveCompEnabled: 条件の有効・無効 (TFileCompDlg::OkActionUpdate 相当)")
{
	using EN = compare::CompEnable;
	compare::CompOptions o;
	// 既定 (すべて無視・ディレクトリ無し): サイズと同一性だけ使える
	compare::CompEnable en = compare::ResolveCompEnabled(o, true, false, false, true);
	CHECK(en.size);
	CHECK(en.id);
	CHECK_FALSE(en.hash);
	CHECK_FALSE(en.alg);
	CHECK_FALSE(en.cmp_arc);

	// サイズが「一致」のときだけハッシュが使える (VCL と同じ)
	o.size_mode = compare::CompSizeMode::Equal;
	o.hash_mode = compare::CompHashMode::Equal;
	en = compare::ResolveCompEnabled(o, true, false, false, true);
	CHECK(en.hash);
	CHECK(en.alg);
	// 排他は「値」で持つ (ApplyExclusiveOpt)。有効判定自体は VCL と同じ式
	CHECK(en.id);

	// ディレクトリ比較時は全ディレクトリのサイズが必要。無ければ全部無効
	o.cmp_dir = true;
	o.id_mode = compare::CompIdMode::Ignore;
	o.hash_mode = compare::CompHashMode::Ignore;
	o.size_mode = compare::CompSizeMode::Ignore;
	en = compare::ResolveCompEnabled(o, false, false, false, true);
	CHECK_FALSE(en.size);
	CHECK_FALSE(en.hash);
	CHECK_FALSE(en.id);
	CHECK(en.cmp_arc);  // ディレクトリ比較のときだけ有効
	en = compare::ResolveCompEnabled(o, true, false, false, true);
	CHECK(en.size);

	// FTP は内容比較ができない
	en = compare::ResolveCompEnabled(o, true, true, false, true);
	CHECK_FALSE(en.hash);
	CHECK_FALSE(en.id);
	// 書庫も同一性比較ができない
	en = compare::ResolveCompEnabled(o, true, false, true, true);
	CHECK_FALSE(en.id);

	// 選択マスクは「ファイル一覧か書庫」のときだけ
	CHECK(compare::ResolveCompEnabled(o, true, false, false, false).sel_mask == false);
	CHECK(compare::ResolveCompEnabled(o, true, false, false, true).sel_mask);
}

TEST_CASE("ApplyExclusiveOpt: ハッシュと同一性は排他 (OptRadioGroupClick 相当)")
{
	compare::CompOptions o;
	o.hash_mode = compare::CompHashMode::Equal;
	o.id_mode = compare::CompIdMode::Equal;

	compare::CompOptions h = compare::ApplyExclusiveOpt(o, /*hash_clicked=*/true);
	CHECK(h.hash_mode == compare::CompHashMode::Equal);
	CHECK(h.id_mode == compare::CompIdMode::Ignore);

	compare::CompOptions i = compare::ApplyExclusiveOpt(o, /*hash_clicked=*/false);
	CHECK(i.hash_mode == compare::CompHashMode::Ignore);
	CHECK(i.id_mode == compare::CompIdMode::Equal);
}

TEST_CASE("CompareSameNames: 同名のファイルだけ選ぶ")
{
	const std::vector<FileItem> left = {f(_T("a.txt"), 10), f(_T("b.txt"), 20), parent(),
	                                    d(_T("dir"))};
	const std::vector<FileItem> right = {f(_T("a.txt"), 10), f(_T("z.txt"), 30)};

	compare::CompProbes probes;
	const compare::CompResult r = compare::CompareSameNames(left, right,
	                                                         compare::CompOptions(), probes);
	// 親 (..) は対象外。ディレクトリは cmp_dir が無ければ対象外
	REQUIRE(r.left_count == 2);
	REQUIRE(r.left_hits.size() == 1);
	CHECK(left[r.left_hits[0]].name == UnicodeString(_T("a.txt")));
	// 反対側は sel_opp が無いので選ばない
	CHECK(r.right_hits.empty());
}

TEST_CASE("CompareSameNames: CS 指定で大小を区別する")
{
	const std::vector<FileItem> lower = {f(_T("a.txt"), 10)};
	const std::vector<FileItem> upper = {f(_T("A.txt"), 10)};

	compare::CompProbes probes;
	// 既定 (CS 無指定) は大小無視で一致
	REQUIRE(compare::CompareSameNames(upper, lower, compare::CompOptions(), probes)
	            .left_hits.size() == 1);

	// CS 指定で A.txt と a.txt は別物
	compare::CompOptions cs;
	cs.case_sensitive = true;
	CHECK(compare::CompareSameNames(upper, lower, cs, probes).left_hits.empty());

	// CS 指定でも綴りが同じなら一致
	REQUIRE(compare::CompareSameNames(upper, upper, cs, probes).left_hits.size() == 1);
}

TEST_CASE("CompareSameNames: サイズ条件でHIT/非HITを絞る")
{
	const std::vector<FileItem> left = {f(_T("a.txt"), 10), f(_T("b.txt"), 10)};
	const std::vector<FileItem> right = {f(_T("a.txt"), 99), f(_T("b.txt"), 10)};

	compare::CompOptions o;
	o.size_mode = compare::CompSizeMode::Equal;
	compare::CompProbes probes;
	const compare::CompResult r = compare::CompareSameNames(left, right, o, probes);
	REQUIRE(r.left_hits.size() == 1);
	CHECK(left[r.left_hits[0]].name == UnicodeString(_T("b.txt")));
}

TEST_CASE("CompareSameNames: ハッシュはサイズが同じ時だけ計算する")
{
	const std::vector<FileItem> left = {f(_T("a.txt"), 10), f(_T("b.txt"), 20)};
	const std::vector<FileItem> right = {f(_T("a.txt"), 99), f(_T("b.txt"), 20)};

	compare::CompOptions o;
	o.size_mode = compare::CompSizeMode::Equal;
	o.hash_mode = compare::CompHashMode::Equal;

	int called = 0;
	compare::CompProbes probes;
	probes.hash_equal = [&called](const FileItem &, const FileItem &) {
		called++;
		return true;
	};
	const compare::CompResult r = compare::CompareSameNames(left, right, o, probes);
	// サイズが同じ b.txt だけハッシュ計算 (1回)
	CHECK(called == 1);
	REQUIRE(r.left_hits.size() == 1);
	CHECK(left[r.left_hits[0]].name == UnicodeString(_T("b.txt")));
}

TEST_CASE("CompareSameNames: 同一性 (is_IdenticalFile) の結果で絞る")
{
	const std::vector<FileItem> left = {f(_T("a.txt")), f(_T("b.txt"))};
	const std::vector<FileItem> right = {f(_T("a.txt")), f(_T("b.txt"))};

	compare::CompOptions o;
	o.id_mode = compare::CompIdMode::Equal;
	compare::CompProbes probes;
	probes.identity_equal = [](const FileItem &a, const FileItem &b) {
		return a.name == UnicodeString(_T("b.txt"));
	};
	const compare::CompResult r = compare::CompareSameNames(left, right, o, probes);
	REQUIRE(r.left_hits.size() == 1);
	CHECK(left[r.left_hits[0]].name == UnicodeString(_T("b.txt")));

	// 一致条件なら全部ヒット
	o.id_mode = compare::CompIdMode::Unequal;
	const compare::CompResult r2 = compare::CompareSameNames(left, right, o, probes);
	CHECK(r2.left_hits.size() == 1);
	CHECK(left[r2.left_hits[0]].name == UnicodeString(_T("a.txt")));
}

TEST_CASE("CompareSameNames: sel_opp で反対側の添字も返す")
{
	const std::vector<FileItem> left = {f(_T("a.txt"))};
	const std::vector<FileItem> right = {f(_T("x.txt")), f(_T("a.txt")), f(_T("y.txt"))};

	compare::CompOptions o;
	o.sel_opp = true;
	compare::CompProbes probes;
	const compare::CompResult r = compare::CompareSameNames(left, right, o, probes);
	REQUIRE(r.left_hits.size() == 1);
	REQUIRE(r.right_hits.size() == 1);
	CHECK(right[r.right_hits[0]].name == UnicodeString(_T("a.txt")));
}

TEST_CASE("CompareSameNames: cmp_dir でディレクトリも比較対象にする")
{
	const std::vector<FileItem> left = {d(_T("sub"))};
	const std::vector<FileItem> right = {d(_T("sub"))};

	compare::CompProbes probes;
	CHECK(compare::CompareSameNames(left, right, compare::CompOptions(), probes).left_hits.empty());

	compare::CompOptions o;
	o.cmp_dir = true;
	const compare::CompResult r = compare::CompareSameNames(left, right, o, probes);
	CHECK(r.left_count == 1);
	REQUIRE(r.left_hits.size() == 1);
}

TEST_CASE("CompareSameNames: cmp_dir が無いときはディレクトリとファイルを突き合わせない")
{
	const std::vector<FileItem> left = {d(_T("a.txt"))};
	const std::vector<FileItem> right = {f(_T("a.txt"))};

	compare::CompProbes probes;
	CHECK(compare::CompareSameNames(left, right, compare::CompOptions(), probes).left_hits.empty());

	compare::CompOptions o;
	o.cmp_dir = true;
	o.cmp_arc = true;  // 種類違いを許すのはディレクトリ比較のときだけ
	const compare::CompResult r = compare::CompareSameNames(left, right, o, probes);
	REQUIRE(r.left_hits.size() == 1);
}

TEST_CASE("ReverseCompareSelection: 対象項目だけ選択を反転する")
{
	// 親(..)・ディレクトリの選択は反転しない (VCL と同じ)
	const std::vector<FileItem> items = {f(_T("a.txt")), f(_T("b.txt")), parent(), d(_T("dir"))};
	const std::vector<bool> sel = {true, false, true, true};
	const auto r = compare::ReverseCompareSelection(items, sel, /*cmp_dir=*/false);
	REQUIRE(r.size() == 4);
	CHECK_FALSE(r[0]);
	CHECK(r[1]);
	CHECK(r[2]);
	CHECK(r[3]);

	const auto rd = compare::ReverseCompareSelection(items, sel, /*cmp_dir=*/true);
	CHECK_FALSE(rd[3]);  // ディレクトリ比較時は dir も反転
}
