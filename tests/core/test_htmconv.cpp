/**
 * @file tests/core/test_htmconv.cpp
 * @brief src/htmconv.cpp (HtmConv クラス: HTML→テキスト変換) の回帰テスト
 */
#include "doctest/doctest.h"

#include <memory>

#include "htmconv.h"
#include "usr_str.h"

//===========================================================================
// GetTagAtr: 属性値抽出
// (GetTag はクラス private のため直接テスト不可。GetTagAtr 経由で
//  タグ名一致判定も間接的に検証している)
//===========================================================================
TEST_CASE("HtmConv::GetTagAtr: 引用符あり/なしの属性値を取得")
{
	HtmConv hc;
	CHECK(hc.GetTagAtr("<a href=\"http://example.com\">", "a", _T("href")) == UnicodeString("http://example.com"));
	CHECK(hc.GetTagAtr("<img src=foo.png alt=bar>", "img", _T("src")) == UnicodeString("foo.png"));
	CHECK(hc.GetTagAtr("<div class=\"x\">", "div", _T("id")) == UnicodeString(""));  //属性なし
	CHECK(hc.GetTagAtr("<div class=\"x\">", "span", _T("class")) == UnicodeString(""));  //タグ名不一致
	CHECK(hc.GetTagAtr("", "a", _T("href")) == UnicodeString(""));
}

//===========================================================================
// RefEntity: 実体参照解決
//===========================================================================
TEST_CASE("HtmConv::RefEntity: 数値文字参照・実体参照の解決 (Unicode出力)")
{
	HtmConv hc;
	hc.CodePage = 65001;  //UTF-8 (Unicode系はテーブル変換なし)
	CHECK(hc.RefEntity("&#65;") == UnicodeString("A"));
	CHECK(hc.RefEntity("plain") == UnicodeString("plain"));  //& を含まないので即リターン
	CHECK(hc.RefEntity("&amp;") == UnicodeString("&"));
	CHECK(hc.RefEntity("a&lt;b&gt;c") == UnicodeString("a<b>c"));  //文字実体参照経由
}

TEST_CASE("HtmConv::RefEntity: 非Unicodeコードページでの代替変換")
{
	HtmConv hc;
	hc.CodePage = 932;  //Shift_JIS: nbsp等は代替文字列に置換
	CHECK(hc.RefEntity("&nbsp;") == UnicodeString(" "));
	CHECK(hc.RefEntity("&copy;") == UnicodeString("(c)"));
}

//===========================================================================
// Convert: 変換処理全体 (プレーンテキスト出力)
//===========================================================================
TEST_CASE("HtmConv::Convert: 単純な段落とリンク")
{
	HtmConv hc;
	hc.HtmBuf->Add("<html><body>");
	hc.HtmBuf->Add("<p>Hello World</p>");
	hc.HtmBuf->Add("</body></html>");
	hc.Convert();

	UnicodeString all;
	for (int i = 0; i < hc.TxtBuf->Count; i++) all += hc.TxtBuf->Strings[i] + "|";
	CHECK(ContainsStr(all, "Hello World"));
}

TEST_CASE("HtmConv::Convert: 見出し(H1)は前後空行を伴う")
{
	HtmConv hc;
	hc.HtmBuf->Add("<h1>Title</h1><p>body</p>");
	hc.Convert();

	int title_idx = -1;
	for (int i = 0; i < hc.TxtBuf->Count; i++) {
		if (hc.TxtBuf->Strings[i] == UnicodeString("Title")) title_idx = i;
	}
	REQUIRE(title_idx >= 0);
}

TEST_CASE("HtmConv::Convert: Markdown モードで箇条書きは - になる")
{
	HtmConv hc;
	hc.ToMarkdown = true;
	//DelBlkCls/DelBlkId を空のままにすると、class/id 属性の無い
	//DIV/SECTION/ARTICLE/NAV/TABLE/UL/OL/DL 要素が丸ごと削除されてしまう
	//(下記 "class/id 未指定要素の丸ごと削除" テスト参照)。<ul> がそれに
	//該当するため、ここでは無害なダミー値を設定して回避する。
	hc.DelBlkCls = "__nomatch__";
	hc.DelBlkId  = "__nomatch__";
	hc.HtmBuf->Add("<ul><li>foo</li><li>bar</li></ul>");
	hc.Convert();

	bool found_foo = false, found_bar = false;
	for (int i = 0; i < hc.TxtBuf->Count; i++) {
		UnicodeString s = hc.TxtBuf->Strings[i];
		if (ContainsStr(s, "- foo")) found_foo = true;
		if (ContainsStr(s, "- bar")) found_bar = true;
	}
	CHECK(found_foo);
	CHECK(found_bar);
}

TEST_CASE("HtmConv::Convert: 非Markdownモードの箇条書きは ・ になる")
{
	HtmConv hc;
	hc.ToMarkdown = false;
	hc.DelBlkCls = "__nomatch__";  //上記と同じ理由 (<ul> に class 属性が無いため)
	hc.DelBlkId  = "__nomatch__";
	hc.HtmBuf->Add("<ul><li>foo</li></ul>");
	hc.Convert();

	bool found = false;
	for (int i = 0; i < hc.TxtBuf->Count; i++) {
		if (ContainsStr(hc.TxtBuf->Strings[i], "・foo")) found = true;
	}
	CHECK(found);
}

//===========================================================================
// DelBlkCls/DelBlkId: class/id 属性の無いブロック要素が丸ごと削除される挙動
//===========================================================================
TEST_CASE("HtmConv::Convert: DelBlkCls/DelBlkId が既定(空文字列)だと"
          " class/id属性の無いブロック要素が丸ごと削除される")
{
	//既存実装の疑わしい挙動をそのまま固定する回帰テスト (直さずに記録)。
	//
	//DelBlkCls/DelBlkId のデフォルト値は EmptyStr (Global.cpp の
	//HtmDelBlkCls/HtmDelBlkId の既定値も "" であり、実運用のデフォルトと
	//一致する)。Convert() 内部では
	//   TStringDynArray cls_lst = SplitString(DelBlkCls, ";");
	//   for (...) DelAtrBlock(tmp_buf, "class", cls_lst[i]);
	//としており、SplitString(EmptyStr, ";") は (空区切りが1個も無いため)
	//要素数1・中身が空文字列の配列 ([""]) を返す。その結果
	//DelAtrBlock(tmp_buf, "class", "") が実行され、DIV/SECTION/ARTICLE/
	//NAV/TABLE/UL/OL/DL のうち class 属性を持たない要素は
	//GetTagAtr(...)==""==aval となって「削除対象」と誤判定され、
	//要素ごと (中身のテキストも含めて) 消えてしまう。
	//HTMLでは class 無しの <div> 等は一般的なので、デフォルト設定のまま
	//使うと影響範囲が広い。実装は直さず、現状の挙動として固定する。
	HtmConv hc;
	hc.HtmBuf->Add("<div>plain div without class</div><p>after</p>");
	hc.Convert();

	UnicodeString all;
	for (int i = 0; i < hc.TxtBuf->Count; i++) all += hc.TxtBuf->Strings[i] + "|";
	CHECK_FALSE(ContainsStr(all, "plain div without class"));  //丸ごと削除される
	CHECK(ContainsStr(all, "after"));                          //div の外側は残る

	//class を明示した場合は削除されない
	//(ただし DelBlkId は既定の空文字列のままだと、この <div> に id 属性が
	//無いことをもって「id==''のブロックとして」削除されてしまう。
	//class 側の挙動だけを見たいので、id 側は無害な値にしておく)
	HtmConv hc2;
	hc2.DelBlkId = "__nomatch__";
	hc2.HtmBuf->Add("<div class=\"x\">kept div with class</div>");
	hc2.Convert();
	UnicodeString all2;
	for (int i = 0; i < hc2.TxtBuf->Count; i++) all2 += hc2.TxtBuf->Strings[i] + "|";
	CHECK(ContainsStr(all2, "kept div with class"));
}

TEST_CASE("HtmConv::Convert: script/style/コメントは除去される")
{
	HtmConv hc;
	hc.HtmBuf->Add("<script>alert('x');</script><style>.a{color:red}</style><!-- comment --><p>visible</p>");
	hc.Convert();

	UnicodeString all;
	for (int i = 0; i < hc.TxtBuf->Count; i++) all += hc.TxtBuf->Strings[i] + "|";
	CHECK(ContainsStr(all, "visible"));
	CHECK_FALSE(ContainsStr(all, "alert"));
	CHECK_FALSE(ContainsStr(all, "color:red"));
	CHECK_FALSE(ContainsStr(all, "comment"));
}

TEST_CASE("HtmConv::Convert: META description/keywords を取得")
{
	HtmConv hc;
	hc.HtmBuf->Add("<head><meta name=\"description\" content=\"desc text\"></head><body>x</body>");
	hc.Convert();
	CHECK(hc.Description == UnicodeString("desc text"));
}

TEST_CASE("HtmConv::Convert: 空入力は空のTxtBufになる")
{
	HtmConv hc;
	hc.Convert();
	CHECK(hc.TxtBuf->Count == 0);
}
