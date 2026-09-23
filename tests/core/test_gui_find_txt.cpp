/**
 * @file tests/core/test_gui_find_txt.cpp
 * @brief gui/find_txt.h の純関数テスト
 */
#include "doctest/doctest.h"

#include "gui/find_txt.h"

TEST_CASE("find_txt: テキスト/バイナリで表示条件を決める")
{
	const find_txt::Availability text = find_txt::ResolveAvailability(false);
	CHECK_FALSE(text.binary_panel);
	CHECK_FALSE(text.bytes);
	CHECK(text.word);
	CHECK(text.regex);
	CHECK(text.migemo);
	CHECK(text.highlight);

	const find_txt::Availability binary = find_txt::ResolveAvailability(true);
	CHECK(binary.binary_panel);
	CHECK(binary.bytes);
	CHECK_FALSE(binary.word);
	CHECK_FALSE(binary.regex);
	CHECK_FALSE(binary.migemo);
	CHECK(binary.code_page);
}

TEST_CASE("find_txt: バイト列/ Migemo の排他条件を正規化する")
{
	find_txt::Options o;
	o.keyword = _T("abc");
	o.bytes = true;
	o.regex = true;
	o.migemo = true;
	const find_txt::Options n = find_txt::Normalize(o, true);
	CHECK(n.bytes);
	CHECK_FALSE(n.regex);
	CHECK_FALSE(n.migemo);

	find_txt::Options migemo;
	migemo.keyword = _T("abc");
	migemo.migemo = true;
	migemo.regex = true;
	const find_txt::Options m = find_txt::Normalize(migemo, false);
	CHECK(m.migemo);
	CHECK_FALSE(m.regex);
}

TEST_CASE("find_txt: 大小文字と単語単位を判定する")
{
	find_txt::Options o;
	o.keyword = _T("cat");
	CHECK(find_txt::LineMatches(_T("Cat"), o));
	o.case_sensitive = true;
	CHECK_FALSE(find_txt::LineMatches(_T("Cat"), o));
	o.case_sensitive = false;
	o.whole_word = true;
	CHECK(find_txt::LineMatches(_T("a cat."), o));
	CHECK_FALSE(find_txt::LineMatches(_T("concatenate"), o));
	o.whole_word = false;
	o.keyword = _T("cat dog");
	CHECK(find_txt::LineMatches(_T("a cat dog here"), o));
	CHECK_FALSE(find_txt::LineMatches(_T("a dog cat here"), o));
}

TEST_CASE("find_txt: 正規表現の妥当性")
{
	find_txt::Options o;
	o.keyword = _T("c.t");
	o.regex = true;
	UnicodeString error;
	CHECK(find_txt::Validate(o, error));
	CHECK(error.IsEmpty());
	CHECK(find_txt::LineMatches(_T("cut"), o));

	o.keyword = _T("[");
	CHECK_FALSE(find_txt::Validate(o, error));
	CHECK_FALSE(error.IsEmpty());
}

TEST_CASE("find_txt: 次方向は基準行を含まず末尾で折り返す")
{
	const std::vector<UnicodeString> lines = {_T("zero"), _T("one"), _T("target"), _T("three")};
	find_txt::Options o;
	o.keyword = _T("target");
	CHECK(find_txt::FindNextLine(lines, o, 1, find_txt::Direction::Down) == 2);
	o.keyword = _T("zero");
	CHECK(find_txt::FindNextLine(lines, o, 2, find_txt::Direction::Down) == 0);
	CHECK(find_txt::FindNextLine(lines, o, 2, find_txt::Direction::Up) == 0);
}

TEST_CASE("find_txt: 方向とコードページの index 変換")
{
	CHECK(find_txt::DirectionIndex(find_txt::Direction::Up) == 0);
	CHECK(find_txt::DirectionFromIndex(1) == find_txt::Direction::Down);
	find_txt::Options o;
	o.code_page = -1;
	CHECK(find_txt::Normalize(o, false).code_page == 932);
}
