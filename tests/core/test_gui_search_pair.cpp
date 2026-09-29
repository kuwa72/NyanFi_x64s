/**
 * @file tests/core/test_gui_search_pair.cpp
 * @brief gui/search_pair.h の純関数テスト
 *
 * @details SearchPair / FindLinkUp/Down / FindSelUp/Down の判断ロジック。
 *          VCL 版は src/TxtViewer.cpp:4488-4667 (SearchPair/SearchPairCore)、
 *          4393-4409 (SearchSel)、5265-5272 (FindLinkDown/Up)。
 */
#include "doctest/doctest.h"

#include <vector>

#include "gui/search_pair.h"

using search_pair::BracketPair;
using search_pair::PairPattern;

//===========================================================================
// BracketPair: 括弧ペアの判定
//===========================================================================

TEST_CASE("search_pair: 標準の括弧ペアが定義されている")
{
	const auto &pairs = search_pair::DefaultPairs();
	CHECK(!pairs.empty());
	// 少なくとも () [] {} が含まれている
	bool has_round = false, has_square = false, has_curly = false;
	for (const auto &p : pairs) {
		if (p.open == _T('(') && p.close == _T(')')) has_round = true;
		if (p.open == _T('[') && p.close == _T(']')) has_square = true;
		if (p.open == _T('{') && p.close == _T('}')) has_curly = true;
	}
	CHECK(has_round);
	CHECK(has_square);
	CHECK(has_curly);
}

TEST_CASE("search_pair: 括弧文字の判定")
{
	CHECK(search_pair::IsOpen(_T('(')));
	CHECK(search_pair::IsOpen(_T('[')));
	CHECK(search_pair::IsOpen(_T('{')));
	CHECK(search_pair::IsClose(_T(')')));
	CHECK(search_pair::IsClose(_T(']')));
	CHECK(search_pair::IsClose(_T('}')));
	CHECK_FALSE(search_pair::IsOpen(_T('a')));
	CHECK_FALSE(search_pair::IsClose(_T('z')));
}

TEST_CASE("search_pair: 開き括弧に対応する閉じ括弧が取れる")
{
	CHECK(search_pair::Match(_T('(')) == _T(')'));
	CHECK(search_pair::Match(_T('[')) == _T(']'));
	CHECK(search_pair::Match(_T('{')) == _T('}'));
}

//===========================================================================
// PairPattern: 拡張子別パターン
//===========================================================================

TEST_CASE("search_pair: 拡張子からペアパターンの種類を判定する")
{
	CHECK(search_pair::GetPairPattern(_T(".pas")) == PairPattern::Pascal);
	CHECK(search_pair::GetPairPattern(_T(".cpp")) == PairPattern::Cpp);
	CHECK(search_pair::GetPairPattern(_T(".h")) == PairPattern::Cpp);
	CHECK(search_pair::GetPairPattern(_T(".vbs")) == PairPattern::Vbs);
	CHECK(search_pair::GetPairPattern(_T(".pl")) == PairPattern::Perl);
	CHECK(search_pair::GetPairPattern(_T(".txt")) == PairPattern::None);
}

TEST_CASE("search_pair: ペアパターンを開始/終了正規表現に分解できる")
{
	UnicodeString begin, end;
	CHECK(search_pair::GetRegexPair(PairPattern::Pascal, begin, end));
	CHECK_FALSE(begin.IsEmpty());
	CHECK_FALSE(end.IsEmpty());

	UnicodeString cbegin, cend;
	CHECK(search_pair::GetRegexPair(PairPattern::Cpp, cbegin, cend));
	CHECK_FALSE(cbegin.IsEmpty());
	CHECK_FALSE(cend.IsEmpty());

	UnicodeString nbegin, nend;
	CHECK_FALSE(search_pair::GetRegexPair(PairPattern::None, nbegin, nend));
	CHECK(nbegin.IsEmpty());
	CHECK(nend.IsEmpty());
}

//===========================================================================
// SearchPairCore: 正規表現ベースの対応探索
//===========================================================================

TEST_CASE("search_pair: 正規表現パターンで対応行を下方向に探す")
{
	const std::vector<UnicodeString> lines = {
		_T("begin"), _T("  x = 1;"), _T("end"), _T("begin"), _T("end"),
	};
	CHECK(search_pair::SearchPairCore(lines, 0, _T("begin"), _T("end")) == 2);
	CHECK(search_pair::SearchPairCore(lines, 3, _T("begin"), _T("end")) == 4);
}

TEST_CASE("search_pair: 正規表現パターンで対応行を上方向に探す")
{
	const std::vector<UnicodeString> lines = {
		_T("begin"), _T("  x = 1;"), _T("end"), _T("begin"), _T("end"),
	};
	CHECK(search_pair::SearchPairCore(lines, 4, _T("begin"), _T("end")) == 3);
	CHECK(search_pair::SearchPairCore(lines, 2, _T("begin"), _T("end")) == 0);
}

TEST_CASE("search_pair: 対応が見つからないときは -1")
{
	const std::vector<UnicodeString> lines = {_T("a"), _T("b"), _T("c")};
	CHECK(search_pair::SearchPairCore(lines, 0, _T("begin"), _T("end")) == -1);
	CHECK(search_pair::SearchPairCore(lines, 2, _T("begin"), _T("end")) == -1);
}

//===========================================================================
// SearchSelection: 選択文字列の検索
//===========================================================================

TEST_CASE("search_pair: 選択文字列を下方向に検索する")
{
	const std::vector<UnicodeString> lines = {
		_T("foo"), _T("bar"), _T("foo"), _T("baz"),
	};
	CHECK(search_pair::SearchSelection(lines, _T("foo"), 0, true) == 2);
	CHECK(search_pair::SearchSelection(lines, _T("bar"), 0, true) == 1);
}

TEST_CASE("search_pair: 選択文字列を上方向に検索する")
{
	const std::vector<UnicodeString> lines = {
		_T("foo"), _T("bar"), _T("foo"), _T("baz"),
	};
	CHECK(search_pair::SearchSelection(lines, _T("foo"), 3, true) == 2);
	CHECK(search_pair::SearchSelection(lines, _T("baz"), 3, true) == 3);
}

TEST_CASE("search_pair: 選択文字列が見つからないときは -1")
{
	const std::vector<UnicodeString> lines = {_T("a"), _T("b")};
	CHECK(search_pair::SearchSelection(lines, _T("z"), 0, true) == -1);
	CHECK(search_pair::SearchSelection(lines, _T("z"), 1, false) == -1);
}

//===========================================================================
// SearchLink: リンクパターン
// (実装は LINK_MATCH_PTN 正規表現 + find_txt::FindNextLine 経由)
//===========================================================================

TEST_CASE("search_pair: リンクを下方向に検索する")
{
	const std::vector<UnicodeString> lines = {
		_T("no link"), _T("http://example.com"), _T("text"),
	};
	CHECK(search_pair::SearchLink(lines, 0, true) == 1);
}

TEST_CASE("search_pair: リンクを上方向に検索する")
{
	const std::vector<UnicodeString> lines = {
		_T("no link"), _T("http://example.com"), _T("text"),
	};
	CHECK(search_pair::SearchLink(lines, 2, false) == 1);
}

TEST_CASE("search_pair: リンクが見つからないときは -1")
{
	const std::vector<UnicodeString> lines = {_T("a"), _T("b")};
	CHECK(search_pair::SearchLink(lines, 0, true) == -1);
	CHECK(search_pair::SearchLink(lines, 1, false) == -1);
}
