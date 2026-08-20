/**
 * @file tests/compat/test_ustring.cpp
 * @brief UnicodeString / AnsiStringT / DynamicArray 互換シムの単体テスト
 */
#include "doctest/doctest.h"

#include "compat/exception.h"
#include "compat/ustring.h"

//===========================================================================
// UnicodeString: 基本
//===========================================================================
TEST_CASE("UnicodeString: 構築とLength/IsEmpty")
{
	UnicodeString empty;
	CHECK(empty.IsEmpty());
	CHECK(empty.Length() == 0);

	UnicodeString s(L"あいうえお");
	CHECK_FALSE(s.IsEmpty());
	CHECK(s.Length() == 5);

	UnicodeString ch(L'A');
	CHECK(ch.Length() == 1);
	CHECK(ch[1] == L'A');

	UnicodeString from_int(42);
	CHECK(from_int == UnicodeString(L"42"));

	UnicodeString from_neg(-7);
	CHECK(from_neg == UnicodeString(L"-7"));
}

TEST_CASE("UnicodeString: operator[] は1始まり")
{
	UnicodeString s(L"ABC");
	CHECK(s[1] == L'A');
	CHECK(s[2] == L'B');
	CHECK(s[3] == L'C');

	s[2] = L'Z';
	CHECK(s == UnicodeString(L"AZC"));
}

TEST_CASE("UnicodeString: CP_ACP 往復 (narrow <-> wide)")
{
	// narrow リテラルは -fexec-charset=CP932 でビルドしているため、
	// UnicodeString(const char*) は CP_ACP(=CP932) 変換で正しく戻るはず。
	UnicodeString s(_T("あいうえお"));
	CHECK(s == UnicodeString(L"あいうえお"));

	AnsiString a = s;             // UnicodeString -> AnsiString (CP_ACP)
	UnicodeString back = a;       // AnsiString -> UnicodeString (CP_ACP)
	CHECK(back == s);
}

TEST_CASE("UnicodeString: 空文字列のCP_ACP構築")
{
	UnicodeString s("");
	CHECK(s.IsEmpty());

	UnicodeString s2(static_cast<const char *>(nullptr));
	CHECK(s2.IsEmpty());
}

TEST_CASE("UnicodeString(const char*, int): NUL終端に依存しないCP_ACP変換")
{
	// src/usr_file_inf.cpp の get_id_str4/get_chunk_hdr と同じ使い方:
	// BYTE buf[4] から UnicodeString((char*)&buf, 4) で固定長識別子を作る。
	// 4バイト全てが有効データであり、NUL 終端は無い。
	char id[4] = {'R', 'I', 'F', 'F'};
	UnicodeString ret_str(id, 4);
	CHECK(ret_str.Length() == 4);
	CHECK(ret_str == UnicodeString(L"RIFF"));

	// バッファ中に NUL (0x00) を含む場合でも len バイト分をそのまま変換し、
	// 途中で打ち切らないこと (strlen 相当の挙動をしない)。
	char with_nul[4] = {'A', 'B', '\0', 'D'};
	UnicodeString ret_nul(with_nul, 4);
	CHECK(ret_nul.Length() == 4);
	CHECK(ret_nul[1] == L'A');
	CHECK(ret_nul[2] == L'B');
	CHECK(ret_nul[3] == L'\0');
	CHECK(ret_nul[4] == L'D');

	// 8バイトバッファの先頭4バイトだけを使うケース (get_chunk_hdr 相当)
	unsigned char buf8[8] = {'d', 'a', 't', 'a', 0x10, 0x00, 0x00, 0x00};
	UnicodeString chunk_id(reinterpret_cast<char *>(buf8), 4);
	CHECK(chunk_id.Length() == 4);
	CHECK(chunk_id == UnicodeString(L"data"));

	// len<=0 やヌルポインタは空文字列 (例外にしない)
	CHECK(UnicodeString(id, 0).IsEmpty());
	CHECK(UnicodeString(static_cast<const char *>(nullptr), 4).IsEmpty());

	// 既存の UnicodeString(const wchar_t*, int) と型で区別され曖昧にならない
	UnicodeString from_wide(L"XY", 2);
	CHECK(from_wide == UnicodeString(L"XY"));
}

TEST_CASE("UnicodeString(const char*, int): CP932マルチバイト境界を含む変換")
{
	// AnsiString(UnicodeString) で得た CP_ACP(=CP932) バイト列を、そのバイト
	// 長で UnicodeString(const char*, int) に戻し、元の文字列と一致することを
	// 確認する (日本語はCP932で2バイト/文字になるため境界を跨ぐ)。
	UnicodeString original(L"あいうえお漢字ABC");
	AnsiString bytes = original;
	CHECK(bytes.Length() > original.Length());  // CP932では2バイト文字混在で長くなる

	UnicodeString restored(bytes.c_str(), bytes.Length());
	CHECK(restored == original);
}

//===========================================================================
// UnicodeString: SubString / Pos (境界値含む)
//===========================================================================
TEST_CASE("UnicodeString: SubString 1始まり境界")
{
	UnicodeString s(L"ABCDE");

	CHECK(s.SubString(1, 1) == UnicodeString(L"A"));
	CHECK(s.SubString(1, 5) == UnicodeString(L"ABCDE"));
	CHECK(s.SubString(5, 1) == UnicodeString(L"E"));      // index == Length
	CHECK(s.SubString(1) == UnicodeString(L"ABCDE"));      // 1引数版: 末尾まで
	CHECK(s.SubString(3) == UnicodeString(L"CDE"));

	// 範囲外はクランプされ、例外にならない
	CHECK(s.SubString(0, 2) == UnicodeString(L"AB"));      // index<1 は 1 とみなす
	CHECK(s.SubString(6, 3) == UnicodeString(L""));        // index>Length は空
	CHECK(s.SubString(4, 100) == UnicodeString(L"DE"));    // count 超過はクランプ
	CHECK(s.SubString(3, 0) == UnicodeString(L""));        // count<=0 は空
	CHECK(s.SubString(3, -1) == UnicodeString(L""));

	UnicodeString empty;
	CHECK(empty.SubString(1, 1) == UnicodeString(L""));
}

TEST_CASE("UnicodeString: Pos 1始まり、見つからなければ0")
{
	UnicodeString s(L"foo/bar/baz");
	CHECK(s.Pos(UnicodeString(L"/")) == 4);
	CHECK(s.Pos(L'/') == 4);
	CHECK(s.Pos(UnicodeString(L"baz")) == 9);
	CHECK(s.Pos(UnicodeString(L"nope")) == 0);
	CHECK(s.Pos(UnicodeString(L"")) == 0);
	CHECK(s.Pos(L'z') == 11);  // 末尾の文字 (Length() と同じ位置)
}

//===========================================================================
// UnicodeString: Insert / Delete
//===========================================================================
TEST_CASE("UnicodeString: Insert")
{
	UnicodeString s(L"ABCD");
	s.Insert(UnicodeString(L"XY"), 1);
	CHECK(s == UnicodeString(L"XYABCD"));

	UnicodeString s2(L"ABCD");
	s2.Insert(UnicodeString(L"XY"), 5);  // 末尾への挿入 (index==Length+1)
	CHECK(s2 == UnicodeString(L"ABCDXY"));

	UnicodeString s3(L"ABCD");
	s3.Insert(UnicodeString(L"XY"), 100);  // 範囲外は末尾にクランプ
	CHECK(s3 == UnicodeString(L"ABCDXY"));

	UnicodeString s4(L"ABCD");
	s4.Insert(UnicodeString(L"XY"), 0);  // index<1 は 1 にクランプ
	CHECK(s4 == UnicodeString(L"XYABCD"));

	UnicodeString empty;
	empty.Insert(UnicodeString(L"Z"), 1);
	CHECK(empty == UnicodeString(L"Z"));
}

TEST_CASE("UnicodeString: Delete")
{
	UnicodeString s(L"ABCDE");
	s.Delete(2, 2);
	CHECK(s == UnicodeString(L"ADE"));

	UnicodeString s2(L"ABCDE");
	s2.Delete(4, 100);  // count がはみ出す場合は末尾までクランプ
	CHECK(s2 == UnicodeString(L"ABC"));

	UnicodeString s3(L"ABCDE");
	s3.Delete(100, 1);  // index が範囲外なら何もしない
	CHECK(s3 == UnicodeString(L"ABCDE"));

	UnicodeString s4(L"ABCDE");
	s4.Delete(5, 1);  // 末尾1文字
	CHECK(s4 == UnicodeString(L"ABCD"));

	UnicodeString s5(L"ABCDE");
	s5.Delete(1, 0);  // count<=0 は何もしない
	CHECK(s5 == UnicodeString(L"ABCDE"));
}

//===========================================================================
// UnicodeString: Trim / UpperCase / LowerCase (ASCIIのみ)
//===========================================================================
TEST_CASE("UnicodeString: Trim系")
{
	UnicodeString s(L"  \t ABC \r\n");
	CHECK(s.Trim() == UnicodeString(L"ABC"));
	CHECK(s.TrimLeft() == UnicodeString(L"ABC \r\n"));
	CHECK(s.TrimRight() == UnicodeString(L"  \t ABC"));

	UnicodeString allspace(L"   ");
	CHECK(allspace.Trim().IsEmpty());
}

TEST_CASE("UnicodeString: UpperCase/LowerCase はASCIIのみ変換する")
{
	UnicodeString s(L"aAbBあいうzZ");
	CHECK(s.UpperCase() == UnicodeString(L"AABBあいうZZ"));
	CHECK(s.LowerCase() == UnicodeString(L"aabbあいうzz"));

	// 日本語部分はASCIIでないため大文字/小文字変換されない
	UnicodeString jp(L"あ");
	CHECK(jp.UpperCase() == jp);
	CHECK(jp.LowerCase() == jp);
}

//===========================================================================
// UnicodeString: サロゲートペア
//===========================================================================
TEST_CASE("UnicodeString: サロゲートペア判定")
{
	// U+1F600 (😀) は UTF-16 で D83D DE00 のサロゲートペア
	UnicodeString s;
	s.wstr().push_back(static_cast<wchar_t>(0xD83D));
	s.wstr().push_back(static_cast<wchar_t>(0xDE00));
	s.wstr() += L"X";

	CHECK(s.Length() == 3);
	CHECK(s.IsLeadSurrogate(1));
	CHECK_FALSE(s.IsTrailSurrogate(1));
	CHECK(s.IsTrailSurrogate(2));
	CHECK_FALSE(s.IsLeadSurrogate(2));
	CHECK_FALSE(s.IsLeadSurrogate(3));
	CHECK_FALSE(s.IsTrailSurrogate(3));

	// 範囲外は false (例外にしない)
	CHECK_FALSE(s.IsLeadSurrogate(0));
	CHECK_FALSE(s.IsLeadSurrogate(100));
}

TEST_CASE("UnicodeString: LastDelimiter / IsDelimiter")
{
	UnicodeString s(L"C:\\path\\to\\file.txt");
	CHECK(s.LastDelimiter(UnicodeString(L"\\")) == 11);  // 末尾から探して1始まり
	CHECK(s.LastDelimiter(UnicodeString(L"/")) == 0);    // 無ければ0

	CHECK(s.IsDelimiter(UnicodeString(L"\\"), 3));
	CHECK_FALSE(s.IsDelimiter(UnicodeString(L"\\"), 1));
	CHECK_FALSE(s.IsDelimiter(UnicodeString(L"\\"), 0));    // 範囲外
	CHECK_FALSE(s.IsDelimiter(UnicodeString(L"\\"), 100));  // 範囲外

	CHECK(s.IsPathDelimiter(3));
	CHECK_FALSE(s.IsPathDelimiter(1));
}

//===========================================================================
// UnicodeString: sprintf / cat_sprintf
//===========================================================================
TEST_CASE("UnicodeString: sprintf の書式")
{
	UnicodeString s;
	s.sprintf(L"%s:%s", UnicodeString(L"key").c_str(), UnicodeString(L"val").c_str());
	CHECK(s == UnicodeString(L"key:val"));

	s.sprintf(L"%d", 42);
	CHECK(s == UnicodeString(L"42"));

	s.sprintf(L"%u", 7u);
	CHECK(s == UnicodeString(L"7"));

	s.sprintf(L"%c", L'A');
	CHECK(s == UnicodeString(L"A"));

	// Windows の wide printf は %c を素のまま渡すと下位バイトのみのマルチ
	// バイト文字として解釈してしまう(実機で確認した既知の落とし穴)。
	// 日本語 (BMP, サロゲート対象外) の文字が欠けずに出力できることを確認する。
	s.sprintf(L"%c", static_cast<wchar_t>(L'あ'));
	CHECK(s == UnicodeString(L"あ"));

	s.sprintf(L"%x", 255);
	CHECK(s == UnicodeString(L"ff"));

	s.sprintf(L"%.2f", 3.14159);
	CHECK(s == UnicodeString(L"3.14"));

	s.sprintf(L"%lld", static_cast<long long>(-123456789012LL));
	CHECK(s == UnicodeString(L"-123456789012"));

	// 幅・精度を可変引数で指定する書式 (usr_str.cpp で多用)
	s.sprintf(L"%.*f", 3, 1.0 / 3.0);
	CHECK(s == UnicodeString(L"0.333"));
}

TEST_CASE("UnicodeString: cat_sprintf は追記する")
{
	UnicodeString s(L"prefix:");
	s.cat_sprintf(L"%d", 1);
	s.cat_sprintf(L"%d", 2);
	CHECK(s == UnicodeString(L"prefix:12"));

	UnicodeString letters;
	for (int i = 0; i < 3; i++) letters.cat_sprintf(L"%c\n", static_cast<wchar_t>(L'A' + i));
	CHECK(letters == UnicodeString(L"A\nB\nC\n"));
}

TEST_CASE("UnicodeString: sprintf は長い出力でもバッファを拡張できる")
{
	UnicodeString s;
	UnicodeString longw;
	for (int i = 0; i < 500; i++) longw += UnicodeString(L"0123456789");  // 5000文字

	s.sprintf(L"%s", longw.c_str());
	CHECK(s.Length() == 5000);
	CHECK(s == longw);
}

//===========================================================================
// UnicodeString: ToInt / ToIntDef / ToDouble
//===========================================================================
TEST_CASE("UnicodeString: ToInt 正常系")
{
	CHECK(UnicodeString(L"123").ToInt() == 123);
	CHECK(UnicodeString(L"-123").ToInt() == -123);
	CHECK(UnicodeString(L"+123").ToInt() == 123);
	CHECK(UnicodeString(L"  123  ").ToInt() == 123);  // 前後の空白許容
	CHECK(UnicodeString(L"$FF").ToInt() == 255);       // $ 接頭の16進
	CHECK(UnicodeString(L"0xFF").ToInt() == 255);       // 0x 接頭の16進
	CHECK(UnicodeString(L"0XFF").ToInt() == 255);       // 大文字 0X も可
	CHECK(UnicodeString(L"-0x10").ToInt() == -16);
}

TEST_CASE("UnicodeString: ToInt 失敗系はEConvertError")
{
	CHECK_THROWS_AS(UnicodeString(L"abc").ToInt(), EConvertError);
	CHECK_THROWS_AS(UnicodeString(L"").ToInt(), EConvertError);
	CHECK_THROWS_AS(UnicodeString(L"12a").ToInt(), EConvertError);
	CHECK_THROWS_AS(UnicodeString(L"--1").ToInt(), EConvertError);
	CHECK_THROWS_AS(UnicodeString(L"$").ToInt(), EConvertError);       // 接頭辞だけ
	CHECK_THROWS_AS(UnicodeString(L"99999999999999999999").ToInt(), EConvertError);  // 範囲超過
}

TEST_CASE("UnicodeString: ToIntDef は失敗時に既定値")
{
	CHECK(UnicodeString(L"123").ToIntDef(-1) == 123);
	CHECK(UnicodeString(L"abc").ToIntDef(-1) == -1);
	CHECK(UnicodeString(L"").ToIntDef(99) == 99);
	CHECK(UnicodeString(L"0x10").ToIntDef(0) == 16);

	// usr_str.cpp の xRRGGBB_to_col と同じ使い方 ("0x" + SubString)
	UnicodeString rrggbb(L"1A2B3C");
	CHECK((UnicodeString(L"0x") + rrggbb.SubString(1, 2)).ToIntDef(0) == 0x1A);
	CHECK((UnicodeString(L"0x") + rrggbb.SubString(3, 2)).ToIntDef(0) == 0x2B);
	CHECK((UnicodeString(L"0x") + rrggbb.SubString(5, 2)).ToIntDef(0) == 0x3C);
}

TEST_CASE("UnicodeString: ToDouble")
{
	CHECK(UnicodeString(L"3.14").ToDouble() == doctest::Approx(3.14));
	CHECK(UnicodeString(L"-2.5").ToDouble() == doctest::Approx(-2.5));
	CHECK(UnicodeString(L"  1.5  ").ToDouble() == doctest::Approx(1.5));
	CHECK_THROWS_AS(UnicodeString(L"abc").ToDouble(), EConvertError);
	CHECK_THROWS_AS(UnicodeString(L"").ToDouble(), EConvertError);
}

//===========================================================================
// UnicodeString: 比較
//===========================================================================
TEST_CASE("UnicodeString: 比較演算子は序数比較")
{
	CHECK(UnicodeString(L"A") < UnicodeString(L"B"));
	CHECK(UnicodeString(L"abc") == UnicodeString(L"abc"));
	CHECK(UnicodeString(L"abc") != UnicodeString(L"abd"));
	CHECK(UnicodeString(L"B") > UnicodeString(L"A"));
	CHECK(UnicodeString(L"A") <= UnicodeString(L"A"));
	CHECK(UnicodeString(L"A") >= UnicodeString(L"A"));
}

TEST_CASE("UnicodeString: CompareIC はASCIIの大小文字無視")
{
	CHECK(UnicodeString(L"ABC").CompareIC(UnicodeString(L"abc")) == 0);
	CHECK(UnicodeString(L"ABC").Compare(UnicodeString(L"abc")) != 0);
	CHECK(UnicodeString(L"あ").CompareIC(UnicodeString(L"あ")) == 0);
}

//===========================================================================
// DynamicArray<T> / TStringDynArray / TBytes
//===========================================================================
TEST_CASE("DynamicArray: Length プロパティを括弧なしで読み書き")
{
	TStringDynArray lst;
	CHECK(lst.Length == 0);

	lst.Length = 3;
	CHECK(lst.Length == 3);
	lst[0] = UnicodeString(L"a");
	lst[1] = UnicodeString(L"b");
	lst[2] = UnicodeString(L"c");
	CHECK(lst[0] == UnicodeString(L"a"));
	CHECK(lst[2] == UnicodeString(L"c"));
}

TEST_CASE("DynamicArray: 添字は0始まり、伸長しても既存要素を保持する")
{
	TStringDynArray lst;
	int len = lst.Length;
	lst.Length = len + 1;
	lst[len] = UnicodeString(L"first");

	len = lst.Length;
	lst.Length = len + 1;
	lst[len] = UnicodeString(L"second");

	CHECK(lst.Length == 2);
	CHECK(lst[0] == UnicodeString(L"first"));
	CHECK(lst[1] == UnicodeString(L"second"));
}

TEST_CASE("DynamicArray<Byte>: TBytes の基本動作")
{
	TBytes b;
	b.Length = 4;
	for (int i = 0; i < b.Length; i++) b[i] = static_cast<Byte>(i * 2);
	CHECK(b.Length == 4);
	CHECK(b[0] == 0);
	CHECK(b[3] == 6);
}

TEST_CASE("DynamicArray: コピーは独立した実体になる")
{
	TStringDynArray a;
	a.Length = 1;
	a[0] = UnicodeString(L"orig");

	TStringDynArray b = a;
	b[0] = UnicodeString(L"changed");

	CHECK(a[0] == UnicodeString(L"orig"));
	CHECK(b[0] == UnicodeString(L"changed"));
}

//===========================================================================
// AnsiStringT<CodePage>
//===========================================================================
TEST_CASE("AnsiString: UnicodeStringとの相互変換 (CP_ACP)")
{
	UnicodeString u(L"テスト123");
	AnsiString a = u;
	CHECK_FALSE(a.IsEmpty());

	UnicodeString back = a.ToUnicode();
	CHECK(back == u);
}

TEST_CASE("UTF8String: UnicodeStringとの相互変換 (UTF-8)")
{
	UnicodeString u(L"テスト");
	UTF8String u8 = u;
	CHECK(u8.Length() == 9);  // "テスト" は UTF-8 で 3 バイト x 3 文字 = 9 バイト

	UnicodeString back = u8.ToUnicode();
	CHECK(back == u);
}

TEST_CASE("AnsiStringT: SubString/Pos は1始まり")
{
	AnsiString a("ABCDE");
	CHECK(a.Length() == 5);
	CHECK(a.SubString(1, 2).str() == "AB");
	CHECK(a.SubString(4, 100).str() == "DE");  // 範囲外はクランプ
	CHECK(a.Pos(AnsiString("CD")) == 3);
	CHECK(a.Pos(AnsiString("zz")) == 0);
}

TEST_CASE("AnsiStringT: 連結・比較")
{
	AnsiString a("foo");
	AnsiString b("bar");
	CHECK((a + b).str() == "foobar");

	AnsiString c("foo");
	CHECK(a == c);
	CHECK(a != b);

	a += b;
	CHECK(a.str() == "foobar");
}
