/**
 * @file tests/compat/test_sysutils.cpp
 * @brief compat/sysutils.h の単体テスト (doctest)
 */
#include "doctest/doctest.h"

#include "compat/sysutils.h"

TEST_CASE("UpperCase/LowerCase は ASCII のみを変換する")
{
	// 日本語や全角文字はそのまま (AnsiUpperCase/AnsiLowerCase と違い ASCII 限定)
	CHECK(UpperCase("abcXYZ123") == "ABCXYZ123");
	CHECK(LowerCase("abcXYZ123") == "abcxyz123");
	CHECK(UpperCase("日本語abc") == "日本語ABC");
}

TEST_CASE("SameText/CompareText は ASCII の大小文字を無視する (実測 179 箇所で最多)")
{
	CHECK(SameText("Hello", "HELLO"));
	CHECK(SameText("Hello", "hello"));
	CHECK_FALSE(SameText("Hello", "Hello!"));
	CHECK(CompareText("abc", "ABC") == 0);
	CHECK(CompareText("abc", "abd") < 0);
}

TEST_CASE("SameStr/CompareStr は序数 (大小文字を区別)")
{
	CHECK(SameStr("abc", "abc"));
	CHECK_FALSE(SameStr("abc", "ABC"));
	CHECK(CompareStr("abc", "abd") < 0);
}

TEST_CASE("ContainsText/StartsText/EndsText は大小文字無視")
{
	CHECK(ContainsText("Hello World", "WORLD"));
	CHECK_FALSE(ContainsText("Hello World", "xyz"));
	CHECK(StartsText("HE", "Hello"));
	CHECK(EndsText("LO", "Hello"));
	CHECK_FALSE(StartsText("lo", "Hello"));
}

TEST_CASE("ContainsStr/StartsStr/EndsStr は大小文字を区別")
{
	CHECK(ContainsStr("Hello World", "World"));
	CHECK_FALSE(ContainsStr("Hello World", "WORLD"));
	CHECK(StartsStr("He", "Hello"));
	CHECK_FALSE(StartsStr("he", "Hello"));
}

TEST_CASE("Trim/TrimLeft/TrimRight")
{
	CHECK(Trim("  abc  ") == "abc");
	CHECK(TrimLeft("  abc  ") == "abc  ");
	CHECK(TrimRight("  abc  ") == "  abc");
	CHECK(Trim("") == "");
	CHECK(Trim("   ") == "");
}

TEST_CASE("SplitString は Delphi 仕様 (区切り文字の集合、空要素も残す)")
{
	TStringDynArray a = SplitString("a,b,,c", ",");
	REQUIRE(a.Length == 4);
	CHECK(a[0] == "a");
	CHECK(a[1] == "b");
	CHECK(a[2] == "");
	CHECK(a[3] == "c");

	// 区切り文字は「集合」: file_filter.cpp 等で SplitString(s, '|') のように
	// 1 文字だけの UnicodeString を渡すケースも多い
	TStringDynArray b = SplitString("x|y|z", "|");
	REQUIRE(b.Length == 3);
	CHECK(b[0] == "x");
	CHECK(b[2] == "z");

	// 複数種の区切り文字を集合として扱う (usr_file_ex.cpp の split_strings_semicolon 相当)
	TStringDynArray c = SplitString("a;b,c", ";,");
	REQUIRE(c.Length == 3);

	// 区切り文字が空なら全体を1要素として返す
	TStringDynArray d = SplitString("abc", "");
	REQUIRE(d.Length == 1);
	CHECK(d[0] == "abc");

	// 先頭/末尾が区切り文字なら空要素が残る
	TStringDynArray e = SplitString(",a,", ",");
	REQUIRE(e.Length == 3);
	CHECK(e[0] == "");
	CHECK(e[2] == "");
}

TEST_CASE("ExtractFileName/ExtractFileExt/ChangeFileExt")
{
	CHECK(ExtractFileName("C:\\foo\\bar.txt") == "bar.txt");
	CHECK(ExtractFileExt("C:\\foo\\bar.txt") == ".txt");
	CHECK(ExtractFileExt("C:\\foo\\bar") == "");
	CHECK(ChangeFileExt("C:\\foo\\bar.txt", ".jpg") == "C:\\foo\\bar.jpg");
	CHECK(ChangeFileExt("C:\\foo\\bar", ".jpg") == "C:\\foo\\bar.jpg");
}

TEST_CASE("ExtractFilePath/ExtractFileDir")
{
	CHECK(ExtractFilePath("C:\\foo\\bar.txt") == "C:\\foo\\");
	CHECK(ExtractFileDir("C:\\foo\\bar.txt") == "C:\\foo");
	// ルート直下は末尾の \\ を残す (実 RTL の ExtractFileDir と同じ特殊ケース)
	CHECK(ExtractFileDir("C:\\foo.txt") == "C:\\");
}

TEST_CASE("ExtractFileDrive は UNC パスを落とさない")
{
	CHECK(ExtractFileDrive("C:\\foo\\bar.txt") == "C:");
	CHECK(ExtractFileDrive("\\\\server\\share\\dir\\file.txt") == "\\\\server\\share");
	CHECK(ExtractFileDrive("relative\\path") == "");
}

TEST_CASE("IncludeTrailingPathDelimiter/ExcludeTrailingPathDelimiter")
{
	CHECK(IncludeTrailingPathDelimiter("C:\\foo") == "C:\\foo\\");
	CHECK(IncludeTrailingPathDelimiter("C:\\foo\\") == "C:\\foo\\");
	CHECK(ExcludeTrailingPathDelimiter("C:\\foo\\") == "C:\\foo");
	CHECK(ExcludeTrailingPathDelimiter("C:\\foo") == "C:\\foo");
}

TEST_CASE("IntToStr/IntToHex/StrToInt/StrToIntDef")
{
	CHECK(IntToStr(12345) == "12345");
	CHECK(IntToStr(-7) == "-7");
	CHECK(IntToHex(255, 1) == "FF");
	CHECK(IntToHex(1, 4) == "0001");
	CHECK(StrToInt("42") == 42);
	CHECK(StrToIntDef("abc", -1) == -1);
	CHECK(StrToIntDef("42", -1) == 42);
}

TEST_CASE("StrToInt は失敗時 EConvertError を送出する")
{
	CHECK_THROWS_AS(StrToInt("abc"), EConvertError);
}

TEST_CASE("FormatFloat: 実測で使われる \",0\" (桁区切り整数)")
{
	CHECK(FormatFloat(",0", 1234567.0) == "1,234,567");
	CHECK(FormatFloat(",0", 0.0) == "0");
	CHECK(FormatFloat(",0", 999.0) == "999");
}

TEST_CASE("StringOfChar / QuotedStr")
{
	CHECK(StringOfChar('_', 5) == "_____");
	CHECK(StringOfChar('x', 0) == "");
	CHECK(QuotedStr("it's") == "'it''s'");
}

TEST_CASE("StringReplace / ReplaceStr / ReplaceText")
{
	CHECK(ReplaceStr("aXbXc", "X", "-") == "a-b-c");
	CHECK(ReplaceText("aXbxc", "X", "-") == "a-b-c");
	CHECK(StringReplace("aXbXc", "X", "-", false, false) == "a-bXc");
}

TEST_CASE("LeftStr/RightStr/MidStr/PosEx")
{
	CHECK(LeftStr("abcdef", 3) == "abc");
	CHECK(RightStr("abcdef", 3) == "def");
	CHECK(MidStr("abcdef", 2, 3) == "bcd");
	CHECK(PosEx("b", "ababab", 2) == 2);
	CHECK(PosEx("b", "ababab", 3) == 4);
}

TEST_CASE("faAnyFile による FindFirst は . と .. を含む (実 RTL の ExcludeAttr=0 相当)")
{
	// 対象コード (usr_file_ex.cpp) は FindFirst の結果を自前で
	// ContainsStr("..", sr.Name) 判定して除外している。つまり RTL 側は
	// "." / ".." を除外せずにそのまま返す必要がある。
	TSearchRec sr;
	int res = FindFirst(GetCurrentDir() + "\\*", faAnyFile, sr);
	if (res == 0) {
		bool sawDot = false, sawDotDot = false;
		do {
			if (sr.Name == ".") sawDot = true;
			if (sr.Name == "..") sawDotDot = true;
		} while (FindNext(sr) == 0);
		FindClose(sr);
		CHECK(sawDot);
		CHECK(sawDotDot);
	}
}
