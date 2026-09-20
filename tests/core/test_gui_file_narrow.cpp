/**
 * @file tests/core/test_gui_file_narrow.cpp
 * @brief gui/file_narrow.h/.cpp (Filter の語照合と SimilarSort の類似度順位付け) の回帰テスト
 *
 * @details wx に依存しない部分だけをここでテストする (`nyanfi_gui_core`、
 * ルート CMakeLists.txt 参照)。照合の実体は移植済みの
 * `contains_word_and_or` / `contains_fuzzy_word` (src/usr_str.h)、
 * 距離は `get_NrmLevenshteinDistance` (同) をそのまま使う。
 * VCL 版の該当は `src/MainFrm.cpp` の `FilterActionExecute` (17751行) と
 * `SimilarSortActionExecute` (26176行)。
 */
#include "doctest/doctest.h"

#include <vector>

#include "gui/file_narrow.h"

using namespace file_narrow;

//===========================================================================
// Filter: MatchFilter (MainFrm.cpp:17751)
//===========================================================================

TEST_CASE("MatchFilter: 空のキーワードはすべて通す")
{
	// VCL は空で絞り込み解除。純関数は「通す」を返す
	CHECK(MatchFilter(_T("foo.txt"), EmptyStr, {}));
	CHECK(MatchFilter(_T("bar"), EmptyStr, {.case_sensitive = true, .fuzzy = true}));
}

TEST_CASE("MatchFilter: 部分一致 (AND/OR)")
{
	// MainFrm.cpp:17790 の contains_word_and_or。半角スペース区切りが AND
	CHECK(MatchFilter(_T("foo_bar.txt"), _T("foo bar"), {}));
	CHECK_FALSE(MatchFilter(_T("foo.txt"), _T("foo bar"), {}));

	// '|' 区切りが OR
	CHECK(MatchFilter(_T("foo.txt"), _T("foo|baz"), {}));
	CHECK_FALSE(MatchFilter(_T("qux.txt"), _T("foo|baz"), {}));
}

TEST_CASE("MatchFilter: 大小文字の区別")
{
	// 既定は区別しない (contains_word_and_or の case_sw=false)
	CHECK(MatchFilter(_T("Foo.TXT"), _T("foo"), {}));
	CHECK_FALSE(MatchFilter(_T("Foo.TXT"), _T("foo"), {.case_sensitive = true}));
	CHECK(MatchFilter(_T("Foo.TXT"), _T("Foo"), {.case_sensitive = true}));
}

TEST_CASE("MatchFilter: あいまい一致")
{
	// MainFrm.cpp:17790 の contains_fuzzy_word (部分列で一致)
	CHECK(MatchFilter(_T("abcdef.txt"), _T("ace"), {.fuzzy = true}));
	CHECK_FALSE(MatchFilter(_T("abcdef.txt"), _T("aec"), {.fuzzy = true}));
	CHECK_FALSE(MatchFilter(_T("abc.txt"), _T(""), {.fuzzy = true}) == false);
}

//===========================================================================
// SimilarSort: RankBySimilarity (MainFrm.cpp:26176)
//===========================================================================

TEST_CASE("RankBySimilarity: 参照項目が先頭、親(..)は末尾")
{
	// VCL はカーソル項目の distance=-1 (先頭)、is_up/is_dummy は 1000 (末尾)
	const std::vector<UnicodeString> names = {_T(".."), _T("apple.txt"), _T("apply.txt"), _T("zebra.txt")};
	const std::vector<bool> parents = {true, false, false, false};

	const auto order = RankBySimilarity(2, names, parents, {});
	REQUIRE(order.size() == 4);
	CHECK(order[0] == 2);              // 参照 (apply.txt) が先頭
	CHECK(order.back() == 0);          // 親 (..) は末尾
	// apple.txt は zebra.txt より apply.txt に近い
	CHECK(order[1] == 1);
	CHECK(order[2] == 3);
}

TEST_CASE("RankBySimilarity: 同名は距離0で参照の直後")
{
	const std::vector<UnicodeString> names = {_T("a.txt"), _T("b.txt"), _T("a.txt")};
	const std::vector<bool> parents = {false, false, false};

	const auto order = RankBySimilarity(0, names, parents, {});
	REQUIRE(order.size() == 3);
	CHECK(order[0] == 0);
	CHECK(order[1] == 2);  // 同名 (距離0)
	CHECK(order[2] == 1);
}

TEST_CASE("RankBySimilarity: 空・範囲外はそのまま返す")
{
	CHECK(RankBySimilarity(0, {}, {}, {}).empty());

	const std::vector<UnicodeString> names = {_T("a.txt"), _T("b.txt")};
	const std::vector<bool> parents = {false, false};
	// 範囲外の参照は恒等順 (何もしない)
	const auto order = RankBySimilarity(9, names, parents, {});
	REQUIRE(order.size() == 2);
	CHECK(order[0] == 0);
	CHECK(order[1] == 1);
}

TEST_CASE("RankBySimilarity: 大文字小文字を無視できる")
{
	// VCL の "IA" パラメータに相当 (既定は区別する)
	const std::vector<UnicodeString> names = {_T("APPLE.TXT"), _T("apple.txt"), _T("zebra.txt")};
	const std::vector<bool> parents = {false, false, false};

	const auto order = RankBySimilarity(0, names, parents, {.ignore_case = true});
	REQUIRE(order.size() == 3);
	CHECK(order[0] == 0);
	CHECK(order[1] == 1);  // 大文字小文字を無視すれば距離0
}
