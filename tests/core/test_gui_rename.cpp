/**
 * @file tests/core/test_gui_rename.cpp
 * @brief gui/rename.cpp (一括リネームのプレビュー計算・衝突判定・実行) の回帰テスト
 *
 * @details wx に依存しない部分だけをここでテストする (`nyanfi_gui_core`、
 * ルート CMakeLists.txt 参照)。実際にファイルシステムに触れる `ExecutePlan`
 * のテストは tests/temp_dir.h の TempDir が作る一時ディレクトリの中だけで行う
 * (CLAUDE.md「破壊的な機能を足すとき」の方針)。
 */
#include "doctest/doctest.h"

#include <cstring>
#include <memory>

#include "gui/rename.h"
#include "usr_file_ex.h"
#include "usr_str.h"

#include "temp_dir.h"

using nyanfi_test::TempDir;
using rename_core::CaseMode;
using rename_core::CaseOptions;
using rename_core::RegexOptions;
using rename_core::RenamePlan;
using rename_core::RenameTarget;
using rename_core::RowStatus;
using rename_core::SerialOptions;

namespace {

RenameTarget make_file(const UnicodeString &name)
{
	RenameTarget t;
	t.name = name;
	t.is_dir = false;
	return t;
}

RenameTarget make_dir(const UnicodeString &name)
{
	RenameTarget t;
	t.name = name;
	t.is_dir = true;
	return t;
}

/// name のファイルを作るだけの空ファイル
void touch(const UnicodeString &path)
{
	std::unique_ptr<TFileStream> fs(new TFileStream(path, fmCreate));
}

}  // namespace

//===========================================================================
// BuildRegexPlan
//===========================================================================
TEST_CASE("BuildRegexPlan: リテラル置換 (正規表現OFF)")
{
	std::vector<RenameTarget> targets = {make_file(_T("foo.txt")), make_file(_T("bar.txt"))};
	RegexOptions opt;
	opt.pattern = _T("foo");
	opt.replacement = _T("baz");
	opt.use_regex = false;

	RenamePlan plan = rename_core::BuildRegexPlan(_T("C:\\nowhere"), targets, opt);
	REQUIRE(plan.rows.size() == 2);
	CHECK(plan.rows[0].new_name == UnicodeString(_T("baz.txt")));
	CHECK(plan.rows[0].status == RowStatus::Ok);
	CHECK(plan.rows[1].new_name == UnicodeString(_T("bar.txt")));
	CHECK(plan.rows[1].status == RowStatus::Unchanged);
}

TEST_CASE("BuildRegexPlan: 正規表現とグループ参照")
{
	std::vector<RenameTarget> targets = {make_file(_T("IMG_0001.jpg"))};
	RegexOptions opt;
	opt.pattern = _T("IMG_(\\d+)");
	opt.replacement = _T("PHOTO_\\1");
	opt.use_regex = true;

	RenamePlan plan = rename_core::BuildRegexPlan(_T("C:\\nowhere"), targets, opt);
	REQUIRE(plan.rows.size() == 1);
	CHECK(plan.rows[0].new_name == UnicodeString(_T("PHOTO_0001.jpg")));
}

TEST_CASE("BuildRegexPlan: only_base=true なら拡張子を対象にしない")
{
	std::vector<RenameTarget> targets = {make_file(_T("data.txt"))};
	RegexOptions opt;
	opt.pattern = _T("txt");
	opt.replacement = _T("csv");
	opt.use_regex = false;
	opt.only_base = true;

	RenamePlan plan = rename_core::BuildRegexPlan(_T("C:\\nowhere"), targets, opt);
	// ベース名 "data" には "txt" が含まれないので変化しない (拡張子は対象外)
	CHECK(plan.rows[0].new_name == UnicodeString(_T("data.txt")));
	CHECK(plan.rows[0].status == RowStatus::Unchanged);
}

TEST_CASE("BuildRegexPlan: 大小文字を無視する")
{
	std::vector<RenameTarget> targets = {make_file(_T("Report.TXT"))};
	RegexOptions opt;
	opt.pattern = _T("report");
	opt.replacement = _T("summary");
	opt.use_regex = false;
	opt.case_sensitive = false;

	RenamePlan plan = rename_core::BuildRegexPlan(_T("C:\\nowhere"), targets, opt);
	CHECK(plan.rows[0].new_name == UnicodeString(_T("summary.TXT")));
}

TEST_CASE("BuildRegexPlan: 不正な正規表現は pattern_error になる")
{
	std::vector<RenameTarget> targets = {make_file(_T("a.txt"))};
	RegexOptions opt;
	opt.pattern = _T("[abc");  // 閉じ括弧が無い不正なパターン
	opt.use_regex = true;

	RenamePlan plan = rename_core::BuildRegexPlan(_T("C:\\nowhere"), targets, opt);
	CHECK(plan.pattern_error);
	CHECK(plan.rows.empty());
}

TEST_CASE("BuildRegexPlan: ディレクトリは拡張子を持たない")
{
	std::vector<RenameTarget> targets = {make_dir(_T("old.dir"))};
	RegexOptions opt;
	opt.pattern = _T("old");
	opt.replacement = _T("new");
	opt.use_regex = false;

	RenamePlan plan = rename_core::BuildRegexPlan(_T("C:\\nowhere"), targets, opt);
	// ディレクトリ名全体が対象になるので ".dir" 部分の "dir" は変化しないが
	// "old" は名前全体から探して置換される
	CHECK(plan.rows[0].new_name == UnicodeString(_T("new.dir")));
}

//===========================================================================
// BuildSerialPlan
//===========================================================================
TEST_CASE("BuildSerialPlan: 開始番号・増分・桁数")
{
	std::vector<RenameTarget> targets = {make_file(_T("a.jpg")), make_file(_T("b.jpg")), make_file(_T("c.jpg"))};
	SerialOptions opt;
	opt.prefix = _T("IMG_");
	opt.start = 10;
	opt.step = 5;
	opt.width = 3;

	RenamePlan plan = rename_core::BuildSerialPlan(_T("C:\\nowhere"), targets, opt);
	REQUIRE(plan.rows.size() == 3);
	CHECK(plan.rows[0].new_name == UnicodeString(_T("IMG_010.jpg")));
	CHECK(plan.rows[1].new_name == UnicodeString(_T("IMG_015.jpg")));
	CHECK(plan.rows[2].new_name == UnicodeString(_T("IMG_020.jpg")));
}

TEST_CASE("BuildSerialPlan: 前後の文字列と拡張子維持")
{
	std::vector<RenameTarget> targets = {make_file(_T("x.png"))};
	SerialOptions opt;
	opt.prefix = _T("pic-");
	opt.suffix = _T("-done");
	opt.start = 1;
	opt.width = 2;

	RenamePlan plan = rename_core::BuildSerialPlan(_T("C:\\nowhere"), targets, opt);
	CHECK(plan.rows[0].new_name == UnicodeString(_T("pic-01-done.png")));
}

TEST_CASE("BuildSerialPlan: 拡張子の変更")
{
	std::vector<RenameTarget> targets = {make_file(_T("x.jpeg"))};
	SerialOptions opt;
	opt.start = 1;
	opt.width = 1;
	opt.change_ext = true;
	opt.new_ext = _T("jpg");  // 先頭の "." が無くても補われる

	RenamePlan plan = rename_core::BuildSerialPlan(_T("C:\\nowhere"), targets, opt);
	CHECK(plan.rows[0].new_name == UnicodeString(_T("1.jpg")));
}

TEST_CASE("BuildSerialPlan: 桁数0なら連番を付けない")
{
	std::vector<RenameTarget> targets = {make_file(_T("x.txt"))};
	SerialOptions opt;
	opt.prefix = _T("renamed");
	opt.width = 0;

	RenamePlan plan = rename_core::BuildSerialPlan(_T("C:\\nowhere"), targets, opt);
	CHECK(plan.rows[0].new_name == UnicodeString(_T("renamed.txt")));
}

TEST_CASE("BuildSerialPlan: ディレクトリは拡張子の概念を持たない")
{
	std::vector<RenameTarget> targets = {make_dir(_T("olddir"))};
	SerialOptions opt;
	opt.prefix = _T("d");
	opt.width = 2;
	opt.start = 1;

	RenamePlan plan = rename_core::BuildSerialPlan(_T("C:\\nowhere"), targets, opt);
	CHECK(plan.rows[0].new_name == UnicodeString(_T("d01")));
}

//===========================================================================
// BuildCasePlan
//===========================================================================
TEST_CASE("BuildCasePlan: 大文字化 (全体)")
{
	std::vector<RenameTarget> targets = {make_file(_T("Report.Txt"))};
	CaseOptions opt;
	opt.mode = CaseMode::Upper;

	RenamePlan plan = rename_core::BuildCasePlan(_T("C:\\nowhere"), targets, opt);
	CHECK(plan.rows[0].new_name == UnicodeString(_T("REPORT.TXT")));
}

TEST_CASE("BuildCasePlan: 小文字化 (ベース名のみ、拡張子は維持)")
{
	std::vector<RenameTarget> targets = {make_file(_T("REPORT.TXT"))};
	CaseOptions opt;
	opt.mode = CaseMode::Lower;
	opt.only_base = true;

	RenamePlan plan = rename_core::BuildCasePlan(_T("C:\\nowhere"), targets, opt);
	CHECK(plan.rows[0].new_name == UnicodeString(_T("report.TXT")));
}

//===========================================================================
// ResolveConflicts (Build*Plan 経由)
//===========================================================================
TEST_CASE("ResolveConflicts: 対象同士で同じ新しい名前を狙うと衝突")
{
	std::vector<RenameTarget> targets = {make_file(_T("a.txt")), make_file(_T("b.txt"))};
	RegexOptions opt;
	opt.pattern = _T("^.*$");
	opt.replacement = _T("same.txt");
	opt.use_regex = true;

	RenamePlan plan = rename_core::BuildRegexPlan(_T("C:\\nowhere"), targets, opt);
	CHECK(plan.rows[0].status == RowStatus::Conflict);
	CHECK(plan.rows[1].status == RowStatus::Conflict);
}

TEST_CASE("ResolveConflicts: 連鎖 (a→b, b→c) は衝突にしない")
{
	// a.txt を b.txt に、b.txt を c.txt に変える (b は明け渡される)。
	// Build*Plan では連鎖を再現しにくいため、ResolveConflicts を直接使う
	std::vector<RenameTarget> targets = {make_file(_T("a.txt")), make_file(_T("b.txt"))};
	RenamePlan plan;
	rename_core::PreviewRow r1;
	r1.old_name = _T("a.txt");
	r1.new_name = _T("b.txt");
	r1.is_dir = false;
	rename_core::PreviewRow r2;
	r2.old_name = _T("b.txt");
	r2.new_name = _T("c.txt");
	r2.is_dir = false;
	plan.rows = {r1, r2};

	rename_core::ResolveConflicts(plan, _T("C:\\nowhere"), targets);
	CHECK(plan.rows[0].status == RowStatus::Ok);
	CHECK(plan.rows[1].status == RowStatus::Ok);
}

TEST_CASE("ResolveConflicts: 入れ替え (a<->b) も衝突にしない")
{
	std::vector<RenameTarget> targets = {make_file(_T("a.txt")), make_file(_T("b.txt"))};

	RenamePlan plan;
	rename_core::PreviewRow r1;
	r1.old_name = _T("a.txt");
	r1.new_name = _T("b.txt");
	rename_core::PreviewRow r2;
	r2.old_name = _T("b.txt");
	r2.new_name = _T("a.txt");
	plan.rows = {r1, r2};

	rename_core::ResolveConflicts(plan, _T("C:\\nowhere"), targets);
	CHECK(plan.rows[0].status == RowStatus::Ok);
	CHECK(plan.rows[1].status == RowStatus::Ok);
}

TEST_CASE("ResolveConflicts: 大小文字だけの変更は自分自身との衝突にしない")
{
	TempDir dir;
	touch(dir.file(_T("a.txt")));

	std::vector<RenameTarget> targets = {make_file(_T("a.txt"))};
	RenamePlan plan;
	rename_core::PreviewRow row;
	row.old_name = _T("a.txt");
	row.new_name = _T("A.TXT");
	plan.rows = {row};

	rename_core::ResolveConflicts(plan, dir.path, targets);
	CHECK(plan.rows[0].status == RowStatus::Ok);
}

TEST_CASE("ResolveConflicts: 既存の別ファイルと衝突する")
{
	TempDir dir;
	touch(dir.file(_T("a.txt")));
	touch(dir.file(_T("existing.txt")));

	std::vector<RenameTarget> targets = {make_file(_T("a.txt"))};
	RenamePlan plan;
	rename_core::PreviewRow row;
	row.old_name = _T("a.txt");
	row.new_name = _T("existing.txt");
	plan.rows = {row};

	rename_core::ResolveConflicts(plan, dir.path, targets);
	CHECK(plan.rows[0].status == RowStatus::Conflict);
}

TEST_CASE("ResolveConflicts: 空文字列や不正な文字を含む名前は Invalid")
{
	std::vector<RenameTarget> targets = {make_file(_T("a.txt")), make_file(_T("b.txt"))};
	RenamePlan plan;
	rename_core::PreviewRow r1;
	r1.old_name = _T("a.txt");
	r1.new_name = EmptyStr;
	rename_core::PreviewRow r2;
	r2.old_name = _T("b.txt");
	r2.new_name = _T("bad/name.txt");
	plan.rows = {r1, r2};

	rename_core::ResolveConflicts(plan, _T("C:\\nowhere"), targets);
	CHECK(plan.rows[0].status == RowStatus::Invalid);
	CHECK(plan.rows[1].status == RowStatus::Invalid);
}

//===========================================================================
// ExecutePlan (一時ディレクトリで実際にリネームする)
//===========================================================================
TEST_CASE("ExecutePlan: 1件のリネームが成功する")
{
	TempDir dir;
	touch(dir.file(_T("a.txt")));

	RenamePlan plan;
	rename_core::PreviewRow row;
	row.old_name = _T("a.txt");
	row.new_name = _T("b.txt");
	row.status = RowStatus::Ok;
	plan.rows = {row};

	rename_core::RenameExecResult result = rename_core::ExecutePlan(dir.path, plan);
	CHECK(result.success_count == 1);
	CHECK(result.skipped_count == 0);
	CHECK(result.failures.empty());
	CHECK_FALSE(file_exists(dir.file(_T("a.txt"))));
	CHECK(file_exists(dir.file(_T("b.txt"))));
}

TEST_CASE("ExecutePlan: Unchanged/Invalid/Conflict はスキップし、何も変更しない")
{
	TempDir dir;
	touch(dir.file(_T("a.txt")));
	touch(dir.file(_T("existing.txt")));

	RenamePlan plan;
	rename_core::PreviewRow unchanged;
	unchanged.old_name = _T("a.txt");
	unchanged.new_name = _T("a.txt");
	unchanged.status = RowStatus::Unchanged;

	rename_core::PreviewRow invalid;
	invalid.old_name = _T("a.txt");
	invalid.new_name = EmptyStr;
	invalid.status = RowStatus::Invalid;

	rename_core::PreviewRow conflict;
	conflict.old_name = _T("a.txt");
	conflict.new_name = _T("existing.txt");
	conflict.status = RowStatus::Conflict;

	plan.rows = {unchanged, invalid, conflict};

	rename_core::RenameExecResult result = rename_core::ExecutePlan(dir.path, plan);
	CHECK(result.success_count == 0);
	CHECK(result.skipped_count == 3);
	CHECK(file_exists(dir.file(_T("a.txt"))));   // 何も変更されていない
	CHECK(file_exists(dir.file(_T("existing.txt"))));
}

TEST_CASE("ExecutePlan: 連鎖 (a→b, b→c) を一時名経由で正しく解決する")
{
	TempDir dir;
	touch(dir.file(_T("a.txt")));
	touch(dir.file(_T("b.txt")));

	RenamePlan plan;
	rename_core::PreviewRow r1;
	r1.old_name = _T("a.txt");
	r1.new_name = _T("b.txt");
	r1.status = RowStatus::Ok;
	rename_core::PreviewRow r2;
	r2.old_name = _T("b.txt");
	r2.new_name = _T("c.txt");
	r2.status = RowStatus::Ok;
	plan.rows = {r1, r2};

	rename_core::RenameExecResult result = rename_core::ExecutePlan(dir.path, plan);
	CHECK(result.success_count == 2);
	CHECK(result.failures.empty());
	CHECK_FALSE(file_exists(dir.file(_T("a.txt"))));
	CHECK(file_exists(dir.file(_T("b.txt"))));   // a.txt の中身が b.txt になった
	CHECK(file_exists(dir.file(_T("c.txt"))));   // 元の b.txt の中身が c.txt になった
}

TEST_CASE("ExecutePlan: 入れ替え (a<->b) を一時名経由で正しく解決する")
{
	TempDir dir;
	touch(dir.file(_T("a.txt")));
	touch(dir.file(_T("b.txt")));

	RenamePlan plan;
	rename_core::PreviewRow r1;
	r1.old_name = _T("a.txt");
	r1.new_name = _T("b.txt");
	r1.status = RowStatus::Ok;
	rename_core::PreviewRow r2;
	r2.old_name = _T("b.txt");
	r2.new_name = _T("a.txt");
	r2.status = RowStatus::Ok;
	plan.rows = {r1, r2};

	rename_core::RenameExecResult result = rename_core::ExecutePlan(dir.path, plan);
	CHECK(result.success_count == 2);
	CHECK(file_exists(dir.file(_T("a.txt"))));
	CHECK(file_exists(dir.file(_T("b.txt"))));
}

TEST_CASE("ExecutePlan: 大小文字だけの変更が実際にリネームされる")
{
	TempDir dir;
	touch(dir.file(_T("a.txt")));

	RenamePlan plan;
	rename_core::PreviewRow row;
	row.old_name = _T("a.txt");
	row.new_name = _T("A.TXT");
	row.status = RowStatus::Ok;
	plan.rows = {row};

	rename_core::RenameExecResult result = rename_core::ExecutePlan(dir.path, plan);
	CHECK(result.success_count == 1);
	CHECK(file_exists(dir.file(_T("A.TXT"))));

	// 実際に大文字化されたかどうかは FindFirstFileW で確認する (file_exists は
	// 大小文字を区別しないので上の CHECK だけでは確認できない)
	WIN32_FIND_DATAW fd;
	HANDLE h = ::FindFirstFileW(dir.file(_T("A.TXT")).c_str(), &fd);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	CHECK(UnicodeString(fd.cFileName) == UnicodeString(_T("A.TXT")));
	::FindClose(h);
}

//===========================================================================
// phase 2 (一時名 -> 最終名) の失敗時に元の名前へ戻すこと
//
// ExecutePlan は 2件以上を一括で変える場合、必ず一時名を経由する
// (連鎖・入れ替えを成立させるため)。その2段目で失敗すると、対策が無ければ
// ファイルは `~nfren_0000.tmp` のような名前のまま残り、ユーザーには
// 「ファイルが消えた」ように見える。ここではその復帰を固定する。
//===========================================================================
TEST_CASE("ExecutePlan: 最終名にできなかったら元の名前へ戻す")
{
	TempDir tmp;
	const UnicodeString dir = tmp.path;

	//対象2件 (2件以上なので一時名を経由する経路に入る)
	touch(tmp.file(_T("one.txt")));
	touch(tmp.file(_T("two.txt")));

	//片方の最終名を「既に存在するディレクトリ」にしておくと、
	//一時名 -> 最終名 の rename が失敗する (MoveFile は既存の宛先を拒否する)
	REQUIRE(create_Dir(tmp.file(_T("blocked"))));

	//衝突判定を通さず、手でプランを組む (ResolveConflicts は当然これを弾くので)
	rename_core::RenamePlan plan;
	{
		rename_core::PreviewRow row;
		row.old_name = _T("one.txt");
		row.new_name = _T("blocked");            //既存のディレクトリと同名 -> 失敗する
		row.status = rename_core::RowStatus::Ok;
		plan.rows.push_back(row);
	}
	{
		rename_core::PreviewRow row;
		row.old_name = _T("two.txt");
		row.new_name = _T("two_renamed.txt");    //こちらは成功する
		row.status = rename_core::RowStatus::Ok;
		plan.rows.push_back(row);
	}

	const rename_core::RenameExecResult res = rename_core::ExecutePlan(dir, plan);

	//成功したのは1件だけ
	CHECK(res.success_count == 1);
	CHECK(res.failures.size() == 1);
	CHECK(file_exists(tmp.file(_T("two_renamed.txt"))));

	//失敗した方は元の名前で残っていること (一時名のままにしない)
	CHECK(file_exists(tmp.file(_T("one.txt"))));

	//一時名が残っていないこと
	TSearchRec sr;
	int temp_left = 0;
	if (FindFirst(tmp.file(_T("~nfren_*")), faAnyFile, sr) == 0) {
		do {
			++temp_left;
		} while (FindNext(sr) == 0);
		FindClose(sr);
	}
	CHECK(temp_left == 0);
}
