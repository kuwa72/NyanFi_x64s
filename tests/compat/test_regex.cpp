/**
 * @file tests/compat/test_regex.cpp
 * @brief System.RegularExpressions (TRegEx) 互換シムの単体テスト
 */
#include "doctest/doctest.h"

#include "compat/regex.h"

//===========================================================================
// TRegEx::IsMatch
//===========================================================================
TEST_CASE("TRegEx::IsMatch: 基本的な一致判定")
{
	CHECK(TRegEx::IsMatch("abc123", "\\d+"));
	CHECK_FALSE(TRegEx::IsMatch("abcxyz", "\\d+"));
}

TEST_CASE("TRegEx::IsMatch: roIgnoreCase")
{
	TRegExOptions opt;
	CHECK_FALSE(TRegEx::IsMatch("ABC", "abc", opt));
	opt << roIgnoreCase;
	CHECK(TRegEx::IsMatch("ABC", "abc", opt));
}

TEST_CASE("TRegEx::IsMatch: roMultiLine で ^ $ が行頭行末にマッチする")
{
	UnicodeString s = "foo\nbar";
	TRegExOptions opt;
	CHECK_FALSE(TRegEx::IsMatch(s, "^bar$", opt));
	opt << roMultiLine;
	CHECK(TRegEx::IsMatch(s, "^bar$", opt));
}

//===========================================================================
// TRegEx::Match / TMatch / TGroupCollection
//===========================================================================
TEST_CASE("TRegEx::Match: Success/Index/Length/Value (1始まり)")
{
	TMatch mt = TRegEx::Match("hello world", "wor");
	CHECK(mt.Success);
	CHECK(mt.Index == 7);	//1始まり
	CHECK(mt.Length == 3);
	CHECK(UnicodeString(mt.Value) == UnicodeString("wor"));
}

TEST_CASE("TRegEx::Match: 一致しない場合は Success が false")
{
	TMatch mt = TRegEx::Match("hello", "\\d+");
	CHECK_FALSE(mt.Success);
}

TEST_CASE("TRegEx::Match: Groups.Count / Groups.Item[k]")
{
	//src/Global.cpp の実使用パターンに近い例 (バージョン番号の分解)
	TMatch mt = TRegEx::Match("10.0.22000.", "(\\d+)\\.(\\d+)\\.(\\d{5})\\.");
	REQUIRE(mt.Success);
	CHECK(mt.Groups.Count == 4);	//グループ0(全体)+3キャプチャ
	CHECK(mt.Groups.Item[1].Success);
	CHECK(UnicodeString(mt.Groups.Item[1].Value) == UnicodeString("10"));
	CHECK(UnicodeString(mt.Groups.Item[2].Value) == UnicodeString("0"));
	CHECK(UnicodeString(mt.Groups.Item[3].Value) == UnicodeString("22000"));
}

TEST_CASE("TRegEx::Match: 非キャプチャグループ (?:...) はグループ数に含まれない")
{
	//src/file_filter.cpp 等で使われる形
	TMatch mt = TRegEx::Match("./foo/bar.txt", "^(?:\\./)?(.+/)?([^:]+)$");
	REQUIRE(mt.Success);
	CHECK(mt.Groups.Count == 3);
}

//===========================================================================
// TRegEx::Matches / TMatchCollection
//===========================================================================
TEST_CASE("TRegEx::Matches: 複数一致の列挙")
{
	TMatchCollection mts = TRegEx::Matches("a1 b22 c333", "\\d+");
	REQUIRE(mts.Count == 3);
	CHECK(UnicodeString(mts.Item[0].Value) == UnicodeString("1"));
	CHECK(UnicodeString(mts.Item[1].Value) == UnicodeString("22"));
	CHECK(UnicodeString(mts.Item[2].Value) == UnicodeString("333"));
}

TEST_CASE("TRegEx::Matches: 一致しなければ Count は 0")
{
	TMatchCollection mts = TRegEx::Matches("abc", "\\d+");
	CHECK(mts.Count == 0);
}

//===========================================================================
// TRegEx::Replace
//===========================================================================
TEST_CASE("TRegEx::Replace: 後方参照無しの単純置換 (全置換)")
{
	UnicodeString out = TRegEx::Replace("a   b    c", "\\s{2,}", " ");
	CHECK(UnicodeString(out) == UnicodeString("a b c"));
}

TEST_CASE("TRegEx::Replace: \\1 形式の後方参照 (src/ で最も多いパターン)")
{
	//src/CalcDlg.cpp: TRegEx::Replace(lbuf, "\\b([a-z]\\w*)\\(", "\\1 (", opt);
	UnicodeString out = TRegEx::Replace("foo(1)", "\\b([a-z]\\w*)\\(", "\\1 (");
	CHECK(UnicodeString(out) == UnicodeString("foo (1)"));
}

TEST_CASE("TRegEx::Replace: $1 形式の後方参照 (htmconv.cpp/MainFrm.cpp で使用)")
{
	UnicodeString out = TRegEx::Replace("https://example.com/path", "(^https?://[\\w\\.\\-]+)(/.*)*", "$1");
	CHECK(UnicodeString(out) == UnicodeString("https://example.com"));
}

//===========================================================================
// TRegEx::Escape
//===========================================================================
TEST_CASE("TRegEx::Escape: 特殊文字をエスケープしリテラル一致になる")
{
	UnicodeString escaped = TRegEx::Escape("a.b*c");
	CHECK(TRegEx::IsMatch("a.b*c", escaped));
	CHECK_FALSE(TRegEx::IsMatch("axbyc", escaped));
}

//===========================================================================
// TRegEx コンストラクタ (パターン検証用途。usr_str.cpp の chk_RegExPtn 相当)
//===========================================================================
TEST_CASE("TRegEx: 不正なパターンは例外を送出する")
{
	CHECK_THROWS_AS(TRegEx("(unterminated", TRegExOptions() << roCompiled), ERegularExpressionError);
}

TEST_CASE("TRegEx: 妥当なパターンは例外を送出しない")
{
	CHECK_NOTHROW(TRegEx("^valid$", TRegExOptions() << roCompiled));
}

//===========================================================================
// TRegEx::Split (src/ では未使用。API完全性の確認)
//===========================================================================
TEST_CASE("TRegEx::Split: 区切りパターンで分割する")
{
	TStringDynArray parts = TRegEx::Split("a,b,,c", ",");
	REQUIRE(parts.Length == 4);
	CHECK(UnicodeString(parts[0]) == UnicodeString("a"));
	CHECK(UnicodeString(parts[1]) == UnicodeString("b"));
	CHECK(UnicodeString(parts[2]) == UnicodeString(""));
	CHECK(UnicodeString(parts[3]) == UnicodeString("c"));
}
