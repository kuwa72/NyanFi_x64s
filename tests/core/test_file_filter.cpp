/**
 * @file tests/core/test_file_filter.cpp
 * @brief src/file_filter.cpp (ファイルフィルタ解析) の回帰テスト
 */
#include "doctest/doctest.h"

#include <memory>

#include "file_filter.h"
#include "usr_str.h"

//===========================================================================
// GetFilterInfStr: フィルタ情報文字列
//===========================================================================
TEST_CASE("GetFilterInfStr: Head/Tail 指定")
{
	CHECK(GetFilterInfStr("Head(10)", false) == UnicodeString("先頭から10行"));
	CHECK(GetFilterInfStr("Tail(5)", false) == UnicodeString("最後から5行"));
	CHECK(GetFilterInfStr("", false) == UnicodeString(""));
}

TEST_CASE("GetFilterInfStr: HtmlHead/HtmlBody/HtmlRem")
{
	CHECK(GetFilterInfStr("HtmlHead", false) == UnicodeString("head要素行"));
	CHECK(GetFilterInfStr("HtmlBody", false) == UnicodeString("body要素行"));
	CHECK(GetFilterInfStr("HtmlRem", false) == UnicodeString("コメント行"));
	CHECK(GetFilterInfStr("HtmlRem", true) == UnicodeString("コメント"));  //is_grep=true で表現が変わる
}

TEST_CASE("GetFilterInfStr: SubStr は is_grep=true のみ有効")
{
	CHECK(GetFilterInfStr("SubStr(3,5)", true) == UnicodeString("3文字目から5文字"));
	CHECK(GetFilterInfStr("SubStr(3)", true) == UnicodeString("3文字目以降"));
	//is_grep=false だと SubStr は認識されずエラー表記になる
	CHECK(GetFilterInfStr("SubStr(3)", false) == UnicodeString("!ERR:[SubStr(3)]"));
}

TEST_CASE("GetFilterInfStr: 複数条件は | 区切りで連結")
{
	CHECK(GetFilterInfStr("Head(3)|HtmlBody", false) == UnicodeString("先頭から3行 | body要素行"));
}

TEST_CASE("GetFilterInfStr: 不正な指定はエラー表記")
{
	CHECK(GetFilterInfStr("Unknown(1)", false) == UnicodeString("!ERR:[Unknown(1)]"));
}

//===========================================================================
// TestFilter: フィルタ構文チェック
//===========================================================================
TEST_CASE("TestFilter: 正当なフィルタは true")
{
	CHECK(TestFilter("Head(10)", false) == true);
	CHECK(TestFilter("Tail(5)|HtmlHead", false) == true);
	CHECK(TestFilter("", false) == true);  //空は常にOK
}

TEST_CASE("TestFilter: 不正なフィルタは false")
{
	CHECK(TestFilter("Unknown(1)", false) == false);
	CHECK(TestFilter("SubStr(1)", false) == false);  //is_grep=false では無効
	CHECK(TestFilter("SubStr(1)", true) == true);
}

//===========================================================================
// FileFilter クラス: 初期化とHead/Tail範囲
//===========================================================================
TEST_CASE("FileFilter::Initialize: Head 指定で範囲を絞る")
{
	std::unique_ptr<TStringList> buf(new TStringList());
	for (int i = 0; i < 10; i++) buf->Add(UnicodeString().sprintf(_T("line%u"), i));

	FileFilter ff;
	FilterOption opt;
	bool ok = ff.Initialize("test.txt", buf.get(), "Head(3)", opt);
	CHECK(ok == true);
	CHECK(ff.topLine == 0);
	CHECK(ff.endLine == 2);
}

TEST_CASE("FileFilter::Initialize: Tail 指定で範囲を絞る")
{
	std::unique_ptr<TStringList> buf(new TStringList());
	for (int i = 0; i < 10; i++) buf->Add(UnicodeString().sprintf(_T("line%u"), i));

	FileFilter ff;
	FilterOption opt;
	bool ok = ff.Initialize("test.txt", buf.get(), "Tail(3)", opt);
	CHECK(ok == true);
	CHECK(ff.endLine == 9);
	CHECK(ff.topLine == 7);
}

TEST_CASE("FileFilter::Initialize: 不正なフィルタは false を返す")
{
	std::unique_ptr<TStringList> buf(new TStringList());
	buf->Add("line0");

	FileFilter ff;
	FilterOption opt;
	bool ok = ff.Initialize("test.txt", buf.get(), "Unknown(1)", opt);
	CHECK(ok == false);
}

TEST_CASE("FileFilter::Initialize: フィルタなしは全行が対象")
{
	std::unique_ptr<TStringList> buf(new TStringList());
	buf->Add("a");
	buf->Add("b");
	buf->Add("c");

	FileFilter ff;
	FilterOption opt;
	bool ok = ff.Initialize("test.txt", buf.get(), "", opt);
	CHECK(ok == true);
	CHECK(ff.topLine == 0);
	CHECK(ff.endLine == 2);
}

//===========================================================================
// GetDispLine / IsValidLine: フィルタ無しの基本動作
//===========================================================================
TEST_CASE("FileFilter::GetDispLine / IsValidLine: フィルタなし時は全行有効")
{
	std::unique_ptr<TStringList> buf(new TStringList());
	buf->Add("hello");
	buf->Add("world");

	FileFilter ff;
	FilterOption opt;
	ff.Initialize("test.txt", buf.get(), "", opt);

	CHECK(ff.GetDispLine(0) == UnicodeString("hello"));
	CHECK(ff.GetDispLine(1) == UnicodeString("world"));
	CHECK(ff.GetDispLine(9) == UnicodeString(""));  //範囲外
	CHECK(ff.IsValidLine(0) == true);
	CHECK(ff.IsValidLine(1) == true);
}

//===========================================================================
// HtmlRem (コメント行) フィルタ
//===========================================================================
TEST_CASE("FileFilter: HtmlRem 指定でコメント行以外は無効になる")
{
	std::unique_ptr<TStringList> buf(new TStringList());
	buf->Add("<!-- this is a comment -->");
	buf->Add("plain text");

	FileFilter ff;
	FilterOption opt{foIsGrep};
	bool ok = ff.Initialize("test.txt", buf.get(), "HtmlRem", opt);
	CHECK(ok == true);
	CHECK(ff.IsValidLine(0) == true);   //コメント行
	CHECK(ff.IsValidLine(1) == false);  //コメントでない行
}

//===========================================================================
// GetLinePart: SubStr 指定
//===========================================================================
TEST_CASE("FileFilter::GetLinePart: SubStr(idx,len) で部分文字列を抽出")
{
	std::unique_ptr<TStringList> buf(new TStringList());
	buf->Add("0123456789");

	FileFilter ff;
	FilterOption opt{foIsGrep};
	bool ok = ff.Initialize("test.txt", buf.get(), "SubStr(3,4)", opt);
	CHECK(ok == true);

	int r_idx = 0, r_len = 0;
	UnicodeString lbuf;
	UnicodeString sbuf = ff.GetLinePart(0, r_idx, r_len, lbuf);
	CHECK(r_idx == 3);
	CHECK(r_len == 4);
	CHECK(sbuf == UnicodeString("2345"));  //1ベースの3文字目から4文字
	CHECK(lbuf == UnicodeString("0123456789"));
}
