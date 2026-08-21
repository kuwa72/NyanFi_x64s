/**
 * @file tests/compat/test_netencoding.cpp
 * @brief compat/src/netencoding.cpp (System.NetEncoding::TURLEncoding) のテスト
 *
 * @details **本物の Delphi の既定と一致するかは未確認**。2つの実呼び出し箇所の
 *          用途から逆算して決めた挙動を、ここで固定する。経緯は
 *          compat/netencoding.h と docs/port/decisions-needed.md を参照。
 */
#include "doctest/doctest.h"

#include "compat/netencoding.h"

TEST_CASE("TURLEncoding::Encode: 未予約文字はそのまま")
{
	CHECK(TURLEncoding::URL->Encode(_T("abcXYZ019-._~"))
	      == UnicodeString(_T("abcXYZ019-._~")));
}

TEST_CASE("TURLEncoding::Encode: 予約文字も符号化する")
{
	// 検索キーワードを URL のクエリに埋めるので、& や = を残すと
	// キーワードの中身でクエリが壊れる (src/UserFunc.cpp:1635)
	CHECK(TURLEncoding::URL->Encode(_T("a&b=c")) == UnicodeString(_T("a%26b%3Dc")));
	CHECK(TURLEncoding::URL->Encode(_T("a/b?c")) == UnicodeString(_T("a%2Fb%3Fc")));
	CHECK(TURLEncoding::URL->Encode(_T("a b")) == UnicodeString(_T("a%20b")));
}

TEST_CASE("TURLEncoding::Encode: 非 ASCII は UTF-8 の percent-encode")
{
	// 「あ」= U+3042 = UTF-8 で E3 81 82
	CHECK(TURLEncoding::URL->Encode(_T("あ")) == UnicodeString(_T("%E3%81%82")));
}

TEST_CASE("TURLEncoding::Decode: %XX を復号する")
{
	CHECK(TURLEncoding::URL->Decode(_T("%E3%81%82")) == UnicodeString(_T("あ")));
	CHECK(TURLEncoding::URL->Decode(_T("a%20b")) == UnicodeString(_T("a b")));
}

TEST_CASE("TURLEncoding::Decode: + は空白にしない")
{
	// 取り出す対象がファイル名なので (src/usr_shell.cpp:420)、
	// `a+b.txt` の + は + のままであってほしい
	CHECK(TURLEncoding::URL->Decode(_T("a+b.txt")) == UnicodeString(_T("a+b.txt")));
}

TEST_CASE("TURLEncoding::Decode: 不正な %XX はそのまま通す")
{
	CHECK(TURLEncoding::URL->Decode(_T("100%")) == UnicodeString(_T("100%")));
	CHECK(TURLEncoding::URL->Decode(_T("%ZZ")) == UnicodeString(_T("%ZZ")));
}

TEST_CASE("TURLEncoding: Encode と Decode が往復する")
{
	const UnicodeString src = _T("日本語 と ASCII & 記号/?=");
	CHECK(TURLEncoding::URL->Decode(TURLEncoding::URL->Encode(src)) == src);
}

TEST_CASE("TURLEncoding: 空文字列")
{
	CHECK(TURLEncoding::URL->Encode(UnicodeString()).IsEmpty());
	CHECK(TURLEncoding::URL->Decode(UnicodeString()).IsEmpty());
}
