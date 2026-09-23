/**
 * @file tests/core/test_gui_distribution.cpp
 * @brief gui/distribution.h の判断ロジックの回帰テスト
 */
#include "doctest/doctest.h"

#include "gui/distribution.h"

using namespace distribution;

TEST_CASE("distribution: 登録項目の CSV 表現を往復できる")
{
	Rule r;
	r.title = _T("画像");
	r.enabled = true;
	r.mask = _T("*.jpg");
	r.destination = _T("D:\\backup\\%A");

	CHECK(ParseRule(MakeRuleRecord(r)).title == _T("画像"));
	CHECK(ParseRule(MakeRuleRecord(r)).enabled == true);
	CHECK(ParseRule(MakeRuleRecord(r)).mask == _T("*.jpg"));
	CHECK(ParseRule(MakeRuleRecord(r)).destination == _T("D:\\backup\\%A"));
}

TEST_CASE("distribution: マスクと正規表現の判定")
{
	UnicodeString error;
	CHECK(IsRegexMask(_T("/^a.*/")) == true);
	CHECK(IsRegexMask(_T("*.jpg")) == false);
	CHECK(IsValidMask(_T("*.jpg"), error) == true);
	CHECK(IsValidMask(_T("/[/"), error) == false);
	CHECK(MatchMask(_T("*.jpg"), _T("photo.JPG")) == true);
	CHECK(MatchMask(_T("/^a.*/"), _T("abc")) == true);
	CHECK(MatchMask(_T("/^a.*/"), _T("bbc")) == false);
}

TEST_CASE("distribution: 最初に一致した登録をプレビューする")
{
	std::vector<Input> items = {
		{_T("C:\\src\\a.jpg"), false},
		{_T("C:\\src\\b.txt"), false},
		{_T("C:\\src\\sub\\"), true},
	};
	Rule jpg;
	jpg.title = _T("jpg");
	jpg.enabled = true;
	jpg.mask = _T("*.jpg");
	jpg.destination = _T("backup");
	Rule all;
	all.title = _T("all");
	all.enabled = true;
	all.mask = _T("*");
	all.destination = _T("other");
	Options opt;
	opt.opposite_path = _T("D:\\dst");

	const Preview p = BuildPreview(items, {jpg, all}, opt);
	CHECK(p.items.size() == 3);
	CHECK(p.items[0].destination == _T("D:\\dst\\backup"));
	CHECK(p.items[1].destination == _T("D:\\dst\\other"));
	CHECK(p.items[2].is_dir == true);
	CHECK(p.matched == 3);
	CHECK(p.files == 2);
	CHECK(p.directories == 1);
}

TEST_CASE("distribution: 登録追加の検証と同一タイトル設定")
{
	UnicodeString error;
	CHECK(CanAddRule(true, _T("題"), _T("*.txt"), _T("out"), 0, error) == true);
	CHECK(CanAddRule(true, _T(""), _T("*.txt"), _T("out"), 0, error) == false);
	CHECK(error == _T("タイトルを入力してください"));
	CHECK(CanAddRule(false, _T("題"), _T("*.txt"), _T("out"), 0, error) == false);

	const std::vector<Rule> rules = {
		{_T("A"), true, _T("*.a"), _T("x")},
		{_T("A"), false, _T("*.b"), _T("y")},
		{_T("B"), true, _T("*.c"), _T("z")},
	};
	const std::vector<bool> grouped = GroupChecked(rules, 0, true);
	CHECK(grouped[0] == true);
	CHECK(grouped[1] == true);
	CHECK(grouped[2] == false);
}

TEST_CASE("distribution: ファイル名の書式とコピー競合モードを解決する")
{
	CHECK(FormatDestination(_T("x-\\A.\\E"), _T("photo.jpg"), _T("D:\\dst")) == _T("D:\\dst\\x-photo.jpg"));
	CHECK(FormatDestination(_T("\\A"), _T("photo.jpg"), _T("D:\\dst")) == _T("D:\\dst\\photo"));
	CHECK(ConflictPolicyFor(CopyMode::Overwrite) == file_ops::ConflictPolicy::Overwrite);
	CHECK(ConflictPolicyFor(CopyMode::Newest) == file_ops::ConflictPolicy::NewestWins);
	CHECK(ConflictPolicyFor(CopyMode::Skip) == file_ops::ConflictPolicy::SkipExisting);
	CHECK(ConflictPolicyFor(CopyMode::AutoRename) == file_ops::ConflictPolicy::AutoRename);
}
