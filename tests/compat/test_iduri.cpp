/**
 * @file tests/compat/test_iduri.cpp
 * @brief compat/src/iduri.cpp (Indy の TIdURI::URLEncode 互換) のテスト
 *
 * @details 本物 (Indy) との差は IdURI.hpp の冒頭に書いてある。ここでは
 *          実呼び出し箇所 (`src/Global.cpp:13402` の HTML 変換用 URL 整形と
 *          `src/TxtViewer.cpp:5281` の OpenURL) が期待する挙動を固定する。
 */
#include "doctest/doctest.h"

#include "IdURI.hpp"

TEST_CASE("URLEncode: ASCII の URL は変わらない")
{
	CHECK(TIdURI::URLEncode(_T("https://example.com/a/b.html"))
	      == UnicodeString(_T("https://example.com/a/b.html")));
}

TEST_CASE("URLEncode: 区切り記号は符号化しない")
{
	// これを潰すと URL の構造が壊れる
	CHECK(TIdURI::URLEncode(_T("https://ex.com/p?a=1&b=2#top"))
	      == UnicodeString(_T("https://ex.com/p?a=1&b=2#top")));
}

TEST_CASE("URLEncode: 空白は %20 になる")
{
	CHECK(TIdURI::URLEncode(_T("https://ex.com/a b"))
	      == UnicodeString(_T("https://ex.com/a%20b")));
}

TEST_CASE("URLEncode: 非 ASCII は UTF-8 の percent-encode になる")
{
	// 「あ」= U+3042 = UTF-8 で E3 81 82
	CHECK(TIdURI::URLEncode(_T("https://ex.com/あ"))
	      == UnicodeString(_T("https://ex.com/%E3%81%82")));
}

TEST_CASE("URLEncode: サロゲートペアを1つのコードポイントとして扱う")
{
	// U+1F600 (😀) = UTF-8 で F0 9F 98 80。
	// サロゲートを個別に符号化すると不正な UTF-8 になる
	const wchar_t emoji[] = {0xD83D, 0xDE00, 0};
	CHECK(TIdURI::URLEncode(UnicodeString(_T("https://ex.com/")) + UnicodeString(emoji))
	      == UnicodeString(_T("https://ex.com/%F0%9F%98%80")));
}

TEST_CASE("URLEncode: 既に符号化済みの %xx を二重符号化しない")
{
	// 呼び出し側 (Global.cpp) の `if (url.Pos("%")==0)` と同じ意図。
	// '%' を残すことで、符号化済み URL を渡されても壊さない
	CHECK(TIdURI::URLEncode(_T("https://ex.com/%E3%81%82"))
	      == UnicodeString(_T("https://ex.com/%E3%81%82")));
}

TEST_CASE("URLEncode: 空文字列")
{
	CHECK(TIdURI::URLEncode(UnicodeString()).IsEmpty());
}
