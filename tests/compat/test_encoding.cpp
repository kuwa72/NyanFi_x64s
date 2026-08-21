/**
 * @file tests/compat/test_encoding.cpp
 * @brief TEncoding 互換シムの単体テスト
 */
#include "doctest/doctest.h"

#include <memory>

#include "compat/encoding.h"

//===========================================================================
// 静的インスタンス
//===========================================================================
TEST_CASE("TEncoding: 静的インスタンスの CodePage")
{
	REQUIRE(TEncoding::UTF8 != nullptr);
	CHECK(TEncoding::UTF8->CodePage == static_cast<unsigned int>(CP_UTF8));
	CHECK(TEncoding::Unicode->CodePage == 1200u);
	CHECK(TEncoding::BigEndianUnicode->CodePage == 1201u);
	CHECK(TEncoding::ASCII->CodePage == 20127u);
	// Default は Windows Unicode ビルドでは ANSI と同じという判断 (report 参照)
	CHECK(TEncoding::Default == TEncoding::ANSI);
}

TEST_CASE("TEncoding: GetEncoding は呼ぶたびに新規インスタンスを返す (delete は呼び出し側の責務)")
{
	std::unique_ptr<TEncoding> a(TEncoding::GetEncoding(932));
	std::unique_ptr<TEncoding> b(TEncoding::GetEncoding(932));
	CHECK(a.get() != b.get());
	CHECK(a->CodePage == 932u);
	CHECK(b->CodePage == 932u);
	// unique_ptr のスコープアウトで delete されても、静的インスタンスとは無関係なので安全
}

//===========================================================================
// GetBytes / GetString の往復
//===========================================================================
TEST_CASE("TEncoding: UTF-8 の GetBytes/GetString 往復")
{
	std::unique_ptr<TEncoding> enc(TEncoding::GetEncoding(CP_UTF8));
	const UnicodeString src(_T("あいうえおABC"));
	TBytes bytes = enc->GetBytes(src);
	CHECK(bytes.Length > src.Length());  // 日本語混じりなので UTF-8 はバイト数が増える

	UnicodeString back = enc->GetString(bytes, 0, bytes.Length);
	CHECK(back == src);
}

TEST_CASE("TEncoding: UTF-16 LE (Unicode) の GetBytes/GetString 往復")
{
	const UnicodeString src(_T("hello, 世界"));
	TBytes bytes = TEncoding::Unicode->GetBytes(src);
	CHECK(bytes.Length == src.Length() * 2);

	UnicodeString back = TEncoding::Unicode->GetString(bytes, 0, bytes.Length);
	CHECK(back == src);
}

TEST_CASE("TEncoding: UTF-16 BE (BigEndianUnicode) の GetBytes/GetString 往復")
{
	const UnicodeString src(_T("hello"));
	TBytes le = TEncoding::Unicode->GetBytes(src);
	TBytes be = TEncoding::BigEndianUnicode->GetBytes(src);
	REQUIRE(le.Length == be.Length);
	// バイトオーダーが逆になっているはず (ASCII なので上位バイトは常に 0)
	for (int i = 0; i < le.Length; i += 2) {
		CHECK(le[i] == be[i + 1]);
		CHECK(le[i + 1] == be[i]);
	}

	UnicodeString back = TEncoding::BigEndianUnicode->GetString(be, 0, be.Length);
	CHECK(back == src);
}

TEST_CASE("TEncoding: Shift_JIS (932) の GetBytes/GetString 往復")
{
	std::unique_ptr<TEncoding> enc(TEncoding::GetEncoding(932));
	const UnicodeString src(_T("日本語テスト"));
	TBytes bytes = enc->GetBytes(src);
	CHECK(bytes.Length == 12);  // 全角 6 文字 x 2 バイト (Shift_JIS)

	UnicodeString back = enc->GetString(bytes, 0, bytes.Length);
	CHECK(back == src);
}

//===========================================================================
// GetPreamble (BOM)
//===========================================================================
TEST_CASE("TEncoding: GetPreamble (BOM)")
{
	CHECK(TEncoding::UTF8->GetPreamble().Length == 3);
	CHECK(TEncoding::Unicode->GetPreamble().Length == 2);
	CHECK(TEncoding::BigEndianUnicode->GetPreamble().Length == 2);
	CHECK(TEncoding::ASCII->GetPreamble().Length == 0);

	TBytes u8pre = TEncoding::UTF8->GetPreamble();
	CHECK(u8pre[0] == 0xEF);
	CHECK(u8pre[1] == 0xBB);
	CHECK(u8pre[2] == 0xBF);
}

//===========================================================================
// GetBufferEncoding (BOM 検出、LoadFromFile の既定コード判定に使う内部処理)
//===========================================================================
TEST_CASE("TEncoding: GetBufferEncoding は BOM を優先して検出する")
{
	TBytes buf;
	buf.Length = 5;
	buf[0] = 0xEF;
	buf[1] = 0xBB;
	buf[2] = 0xBF;
	buf[3] = 'a';
	buf[4] = 'b';

	TEncoding *enc = nullptr;
	int skip = TEncoding::GetBufferEncoding(buf, enc);
	CHECK(skip == 3);
	CHECK(enc == TEncoding::UTF8);
}

TEST_CASE("TEncoding: GetBufferEncoding は BOM が無ければ既定 (ANSI) にフォールバックする")
{
	TBytes buf;
	buf.Length = 3;
	buf[0] = 'a';
	buf[1] = 'b';
	buf[2] = 'c';

	TEncoding *enc = nullptr;
	int skip = TEncoding::GetBufferEncoding(buf, enc);
	CHECK(skip == 0);
	CHECK(enc == TEncoding::Default);
}

TEST_CASE("TEncoding: GetBufferEncoding は明示指定を BOM 未検出時に維持する")
{
	std::unique_ptr<TEncoding> explicit_enc(TEncoding::GetEncoding(932));
	TBytes buf;
	buf.Length = 2;
	buf[0] = 'x';
	buf[1] = 'y';

	TEncoding *enc = explicit_enc.get();
	int skip = TEncoding::GetBufferEncoding(buf, enc);
	CHECK(skip == 0);
	CHECK(enc == explicit_enc.get());
}
