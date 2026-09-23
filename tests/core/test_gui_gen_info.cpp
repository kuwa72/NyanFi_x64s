/**
 * @file tests/core/test_gui_gen_info.cpp
 * @brief gui/gen_info.h/.cpp (汎用一覧ダイアログの判断) のテスト
 *
 * VCL の実測根拠は gui/gen_info.h 冒頭の対応表を参照。
 */
#include "doctest/doctest.h"

#include <vector>

#include "gui/gen_info.h"

using namespace gen_info;

namespace {

std::vector<UnicodeString> texts(const std::vector<Entry> &entries)
{
	std::vector<UnicodeString> out;
	out.reserve(entries.size());
	for (const Entry &entry : entries) out.push_back(entry.text);
	return out;
}

}  // namespace

//===========================================================================
// 自動判定 (FormShow の isGit / isVarList 判定)
//===========================================================================

TEST_CASE("DetectKind: 先頭の git コマンドで Git 一覧と判定する")
{
	CHECK(DetectKind({_T("$ git status")}, Kind::Generic) == Kind::Git);
	CHECK(DetectKind({_T("  $ git status")}, Kind::Generic) == Kind::Generic);
}

TEST_CASE("DetectKind: 空行を含まない name=value は変数一覧と判定する")
{
	CHECK(DetectKind({_T("OS=Windows"), _T("Arch=x64")}, Kind::Generic) == Kind::Variable);
	CHECK(DetectKind({_T("OS=Windows"), EmptyStr}, Kind::Generic) == Kind::Variable);
	CHECK(DetectKind({_T("bad name=value")}, Kind::Generic) == Kind::Generic);
	CHECK(DetectKind({_T("no equals")}, Kind::Generic) == Kind::Generic);
}

TEST_CASE("DetectKind: VCL のフラグが指定済みなら自動判定より優先する")
{
	CHECK(DetectKind({_T("$ git status")}, Kind::CommandHistory) == Kind::CommandHistory);
	CHECK(DetectKind({}, Kind::Log) == Kind::Log);
}

//===========================================================================
// フィルタ (UpdateList / filter_List 相当)
//===========================================================================

TEST_CASE("BuildEntries: 元の行番号を保ったまま通常一覧を作る")
{
	FilterOptions opt;
	const auto entries = BuildEntries({_T("one"), _T("two")}, Kind::Generic, opt);
	REQUIRE(entries.size() == 2);
	CHECK(entries[0].text == _T("one"));
	CHECK(entries[0].source_index == 0);
	CHECK(entries[1].text == _T("two"));
	CHECK(entries[1].source_index == 1);
}

TEST_CASE("BuildEntries: 通常検索は検索語全体が大文字小文字を区別せず一致")
{
	FilterOptions opt;
	opt.keyword = _T("alpha beta");
	const auto entries = BuildEntries(
		{_T("ALPHA"), _T("prefix ALPHA BETA suffix"), _T("alpha and beta"), _T("gamma")},
		Kind::Generic, opt);
	REQUIRE(entries.size() == 1);
	CHECK(entries[0].text == _T("prefix ALPHA BETA suffix"));
	CHECK(entries[0].source_index == 1);
}

TEST_CASE("BuildEntries: AND/OR は | 区切りの OR と空白区切りの AND")
{
	FilterOptions opt;
	opt.keyword = _T("alpha beta|gamma");
	opt.any_term = true;
	const auto entries = BuildEntries(
		{_T("ALPHA and BETA"), _T("nothing"), _T("gamma only")}, Kind::Generic, opt);
	CHECK(texts(entries) == std::vector<UnicodeString>{_T("ALPHA and BETA"), _T("gamma only")});
}

TEST_CASE("BuildEntries: 大文字小文字を区別する指定も使える")
{
	FilterOptions opt;
	opt.keyword = _T("Alpha");
	opt.case_sensitive = true;
	const auto entries = BuildEntries({_T("alpha"), _T("Alpha")}, Kind::Generic, opt);
	REQUIRE(entries.size() == 1);
	CHECK(entries[0].text == _T("Alpha"));
}

TEST_CASE("BuildEntries: 検索語に大文字があれば VCL 同様に大小文字を区別する")
{
	FilterOptions opt;
	opt.keyword = _T("Alpha");
	const auto entries = BuildEntries({_T("alpha"), _T("Alpha")}, Kind::Generic, opt);
	REQUIRE(entries.size() == 1);
	CHECK(entries[0].text == _T("Alpha"));
}

TEST_CASE("BuildEntries: ログのエラー部分抽出は VCL の E 行と四空白の継続行だけ")
{
	FilterOptions opt;
	opt.errors_only = true;
	opt.keyword = _T("last");  // VCL どおり ErrOnly なら通常検索は無視する
	const auto entries = BuildEntries({
		_T(" > COPY ok.txt"),
		_T(" >E COPY bad.txt"),
		_T("    detailed failure"),
		_T("    second detail"),
		_T(" > COPY next.txt"),
		_T(" >E COPY last.txt"),
	}, Kind::Log, opt);
	CHECK(texts(entries) == std::vector<UnicodeString>{
		_T(" >E COPY bad.txt"), _T("    detailed failure"), _T("    second detail"),
		_T(" >E COPY last.txt")});
}

TEST_CASE("BuildEntries: ファイル一覧/ツリーはタブ前だけで検索する")
{
	FilterOptions opt;
	opt.keyword = _T("needle");
	const auto entries = BuildEntries({
		_T("label\tneedle"), _T("needle\tother")
	}, Kind::FileList, opt);
	REQUIRE(entries.size() == 1);
	CHECK(entries[0].source_index == 1);
}

//===========================================================================
// 並べ替え・重複除去 (SortGenList / DelDuplActionExecute 相当)
//===========================================================================

TEST_CASE("SortEntries: 昇順は先頭の数値列を自然順で優先する")
{
	std::vector<Entry> entries{{_T("10 ten"), 0}, {_T("2 two"), 1}, {_T("1 one"), 2}};
	SortEntries(entries, SortMode::Ascending);
	CHECK(texts(entries) == std::vector<UnicodeString>{_T("1 one"), _T("2 two"), _T("10 ten")});
}

TEST_CASE("SortEntries: 降順と元順序は source_index で安定する")
{
	std::vector<Entry> entries{{_T("b"), 1}, {_T("a"), 0}, {_T("b"), 2}};
	SortEntries(entries, SortMode::Descending);
	CHECK(texts(entries) == std::vector<UnicodeString>{_T("b"), _T("b"), _T("a")});
	SortEntries(entries, SortMode::Original);
	CHECK(entries[0].source_index == 0);
	CHECK(entries[1].source_index == 1);
	CHECK(entries[2].source_index == 2);
}

TEST_CASE("RemoveDuplicates: VCL と同じく全文字一致だけ除去する")
{
	const auto result = RemoveDuplicates({{_T("same"), 0}, {_T("Same"), 1}, {_T("same"), 2}});
	REQUIRE(result.size() == 2);
	CHECK(result[0].source_index == 0);
	CHECK(result[1].source_index == 1);
}

//===========================================================================
// 表示・コピー・保存用文字列
//===========================================================================

TEST_CASE("DisplayText: ファイル一覧/ツリーはタブ前の文字列を表示する")
{
	CHECK(DisplayText(_T("label\tC:\\dir\\file.txt"), Kind::FileList) == _T("label"));
	CHECK(DisplayText(_T("branch\tC:\\dir\\file.txt"), Kind::Tree) == _T("branch"));
	CHECK(DisplayText(_T("plain"), Kind::Generic) == _T("plain"));
}

TEST_CASE("TargetText: タブ付き一覧は実ファイル名を取り出す")
{
	CHECK(TargetText(_T("label\tC:\\dir\\file.txt"), Kind::FileList) == _T("C:\\dir\\file.txt"));
	CHECK(TargetText(_T("plain"), Kind::Generic) == _T("plain"));
}

TEST_CASE("ValueText: 最初の '=' 以降を返す")
{
	CHECK(ValueText(_T("URL=https://example.test/a=b")) == _T("https://example.test/a=b"));
	CHECK(ValueText(_T("no value")) == EmptyStr);
}

TEST_CASE("JoinEntries: 選択行を CRLF 区切りで連結する")
{
	const std::vector<Entry> entries{{_T("one"), 0}, {EmptyStr, 1}, {_T("three"), 2}};
	CHECK(JoinEntries(entries, {0, 1, 2}) == _T("one\r\n\r\nthree"));
	CHECK(JoinEntries({}, {}) == EmptyStr);
}

//===========================================================================
// 検索・ステータス・コマンド履歴
//===========================================================================

TEST_CASE("FindNext: 現在位置自身は含めず指定方向へ検索する")
{
	const std::vector<Entry> entries{{_T("one"), 0}, {_T("target"), 1}, {_T("two"), 2}};
	CHECK(FindNext(entries, 0, _T("target"), true, false) == 1);
	CHECK(FindNext(entries, 2, _T("target"), false, false) == 1);
	CHECK(FindNext(entries, 1, _T("target"), true, false) == -1);
}

TEST_CASE("StatusText: 絞り込み・選択数・カーソル位置を VCL 相当で表示する")
{
	CHECK(StatusText(2, 5, 1, 0, true, false) == _T("項目: 2/5  -  1    選択: 1"));
	CHECK(StatusText(1, 1, 0, 0, false, true) == _T("ERR: 1  -  1"));
}

TEST_CASE("CommandText: VCL の時刻・モード付き履歴から実行文字列を取り出す")
{
	CHECK(CommandText(_T("12:34:56.789 F L_OpenStandard\tD:\\work")) == _T("L_OpenStandard"));
	CHECK(CommandText(_T("12:34:56.789 - F SortDlg")) == EmptyStr);
	CHECK(CommandText(_T("ListClipboard")) == _T("ListClipboard"));
}
