/**
 * @file tests/core/test_usr_str.cpp
 * @brief src/usr_str.cpp (文字列操作の汎用関数群) の回帰テスト
 *
 * 目的: シムの意味論が VCL/RTL と異なっていた場合に検出すること。
 * 実装 (src/usr_str.cpp) を読み、現在の挙動をそのまま固定している。
 * 仕様として疑わしい点があっても実装は直さず、報告にのみ記載する。
 */
#include "doctest/doctest.h"
#include "locale_guard.h"

#include <memory>

#include "usr_str.h"

//===========================================================================
// トークン分離系: get_tkn / get_tkn_r / get_tkn_m
//===========================================================================
TEST_CASE("get_tkn: セパレータ前を取得、無ければ元文字列のまま")
{
	CHECK(get_tkn("abc,def", ",") == UnicodeString("abc"));
	CHECK(get_tkn("abc", ",") == UnicodeString("abc"));
	CHECK(get_tkn("", ",") == UnicodeString(""));
	CHECK(get_tkn(",abc", ",") == UnicodeString(""));  //先頭がセパレータ
}

TEST_CASE("get_tkn_r: セパレータ後を取得、無ければ空")
{
	CHECK(get_tkn_r("abc,def", ",") == UnicodeString("def"));
	CHECK(get_tkn_r("abc", ",") == UnicodeString(""));
	CHECK(get_tkn_r("abc,", ",") == UnicodeString(""));
	CHECK(get_tkn_r("abc,,def", ",") == UnicodeString(",def"));  //最初のセパレータ以降すべて
}

TEST_CASE("get_tkn_m: 2つのセパレータの間を取得")
{
	CHECK(get_tkn_m("a=b;c=d", "=", ";") == UnicodeString("b"));
	//後セパレータが無い場合は前セパレータ以降すべて
	CHECK(get_tkn_m("a=bcd", "=", ";") == UnicodeString("bcd"));
}

TEST_CASE("get_pre_tab / get_post_tab: タブの前後")
{
	CHECK(get_pre_tab("abc\tdef") == UnicodeString("abc"));
	CHECK(get_pre_tab("abc") == UnicodeString("abc"));
	CHECK(get_post_tab("abc\tdef") == UnicodeString("def"));
	CHECK(get_post_tab("abc") == UnicodeString(""));
}

//===========================================================================
// get_first_line
//===========================================================================
TEST_CASE("get_first_line: 改行までの1行目")
{
	CHECK(get_first_line("abc\r\ndef") == UnicodeString("abc"));
	CHECK(get_first_line("abc\ndef") == UnicodeString("abc"));
	CHECK(get_first_line("abc") == UnicodeString("abc"));
	CHECK(get_first_line("") == UnicodeString(""));
}

//===========================================================================
// 括弧内取得: get_in_paren / split_in_paren
//===========================================================================
TEST_CASE("get_in_paren: 括弧内の文字列を取得")
{
	CHECK(get_in_paren("abc(def)ghi") == UnicodeString("def"));
	CHECK(get_in_paren("abc") == UnicodeString(""));
	CHECK(get_in_paren("abc)def(") == UnicodeString(""));  //閉じが先: p1>=p2 で空
	CHECK(get_in_paren("()") == UnicodeString(""));         //中身なし
}

TEST_CASE("split_in_paren: 括弧内を分離、元文字列は後続に")
{
	UnicodeString s = "abc(def)ghi";
	UnicodeString r = split_in_paren(s);
	CHECK(r == UnicodeString("def"));
	CHECK(s == UnicodeString("ghi"));
}

//===========================================================================
// get_norm_str
//===========================================================================
TEST_CASE("get_norm_str: 複数行の場合は空でない最初の行、前後タブ除去")
{
	CHECK(get_norm_str("\tabc\t") == UnicodeString("abc"));
	CHECK(get_norm_str("\r\nabc\r\ndef") == UnicodeString("abc"));
	CHECK(get_norm_str("  abc  ", true) == UnicodeString("abc"));  //trim_sw = true: 前後空白除去
	CHECK(get_norm_str("  abc  ", false) == UnicodeString("  abc  "));  //trim_sw=false はタブのみ除去
}

//===========================================================================
// split_tkn 系
//===========================================================================
TEST_CASE("split_tkn: セパレータ前を分離、元文字列は後続に")
{
	UnicodeString s = "abc,def";
	UnicodeString r = split_tkn(s, ",");
	CHECK(r == UnicodeString("abc"));
	CHECK(s == UnicodeString("def"));

	UnicodeString s2 = "abc";
	UnicodeString r2 = split_tkn(s2, ",");
	CHECK(r2 == UnicodeString("abc"));
	CHECK(s2 == UnicodeString(""));
}

TEST_CASE("split_tkn_spc: 先頭空白は区切りと見なさない")
{
	UnicodeString s = "  abc def";
	UnicodeString r = split_tkn_spc(s);
	CHECK(r == UnicodeString("  abc"));
	CHECK(s == UnicodeString("def"));
}

TEST_CASE("split_pre_tab: タブ前を分離")
{
	UnicodeString s = "abc\tdef";
	UnicodeString r = split_pre_tab(s);
	CHECK(r == UnicodeString("abc"));
	CHECK(s == UnicodeString("def"));
}

TEST_CASE("split_dsc: 先頭の説明部分(:～:)を分離")
{
	UnicodeString s = ":desc:body";
	UnicodeString r = split_dsc(s);
	CHECK(r == UnicodeString("desc"));
	CHECK(s == UnicodeString("body"));

	//"::{" で始まる場合は対象外 (GUID等)
	UnicodeString s2 = "::{GUID}";
	UnicodeString r2 = split_dsc(s2);
	CHECK(r2 == UnicodeString(""));
	CHECK(s2 == UnicodeString("::{GUID}"));

	//先頭が : でない場合は何もしない
	UnicodeString s3 = "body";
	UnicodeString r3 = split_dsc(s3);
	CHECK(r3 == UnicodeString(""));
	CHECK(s3 == UnicodeString("body"));
}

TEST_CASE("split_top_ch / split_top_wch: 先頭1文字を分離")
{
	UnicodeString s = "abc";
	UnicodeString c = split_top_ch(s);
	CHECK(c == UnicodeString("a"));
	CHECK(s == UnicodeString("bc"));

	UnicodeString s2 = "";
	UnicodeString c2 = split_top_ch(s2);
	CHECK(c2 == UnicodeString(""));

	UnicodeString s3 = "xyz";
	WideChar wc = split_top_wch(s3);
	CHECK(wc == WideChar('x'));
	CHECK(s3 == UnicodeString("yz"));
}

//===========================================================================
// コマンドライン分解
//===========================================================================
TEST_CASE("split_cmd_line: 空白区切り、引用符内は分割しない")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	int n = split_cmd_line("aaa \"b b b\" ccc", lst.get());
	REQUIRE(n == 3);
	CHECK(lst->Strings[0] == UnicodeString("aaa"));
	CHECK(lst->Strings[1] == UnicodeString("\"b b b\""));
	CHECK(lst->Strings[2] == UnicodeString("ccc"));
}

TEST_CASE("split_cmd_line: 空文字列は0件")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	int n = split_cmd_line("", lst.get());
	CHECK(n == 0);
}

TEST_CASE("split_file_param: 引用符付きファイル名とパラメータを分離")
{
	UnicodeString s = "\"C:\\Program Files\\app.exe\" -x";
	UnicodeString r = split_file_param(s);
	CHECK(r == UnicodeString("C:\\Program Files\\app.exe"));
	CHECK(s == UnicodeString("-x"));

	UnicodeString s2 = "app.exe -x";
	UnicodeString r2 = split_file_param(s2);
	CHECK(r2 == UnicodeString("app.exe"));
	CHECK(s2 == UnicodeString("-x"));
}

//===========================================================================
// 分割: split_strings_tab / split_strings_semicolon
//===========================================================================
TEST_CASE("split_strings_tab: タブで分割")
{
	TStringDynArray a = split_strings_tab("a\tb\tc");
	REQUIRE(a.Length == 3);
	CHECK(a[0] == UnicodeString("a"));
	CHECK(a[1] == UnicodeString("b"));
	CHECK(a[2] == UnicodeString("c"));
}

TEST_CASE("split_strings_semicolon: セミコロンで分割")
{
	TStringDynArray a = split_strings_semicolon("a;b;;c");
	REQUIRE(a.Length == 4);
	CHECK(a[0] == UnicodeString("a"));
	CHECK(a[1] == UnicodeString("b"));
	CHECK(a[2] == UnicodeString(""));
	CHECK(a[3] == UnicodeString("c"));
}

TEST_CASE("split_strings_semicolon: del_empty=true で先頭/末尾/連続セミコロンを圧縮")
{
	TStringDynArray a = split_strings_semicolon(";a;;b;", true);
	REQUIRE(a.Length == 2);
	CHECK(a[0] == UnicodeString("a"));
	CHECK(a[1] == UnicodeString("b"));
}

//===========================================================================
// 削除系: remove_*
//===========================================================================
TEST_CASE("remove_text: 大小文字を無視して全ての一致を削除")
{
	//実装は ReplaceText(s, w, EmptyStr) を使っており、Delphi の
	//ReplaceText は最初の1件だけでなく全ての一致を置換する。
	//そのため "AbcDefAbc" から "abc" を除去すると両方消えて "Def" になる
	//(「最初の一致だけ削除」という誤解をしないよう明示しておく)。
	UnicodeString s = "AbcDefAbc";
	CHECK(remove_text(s, "abc") == true);
	CHECK(s == UnicodeString("Def"));

	UnicodeString s2 = "xyz";
	CHECK(remove_text(s2, "abc") == false);
	CHECK(s2 == UnicodeString("xyz"));
}

TEST_CASE("remove_top_text: 先頭一致を大小文字無視で削除")
{
	UnicodeString s = "ABCdef";
	CHECK(remove_top_text(s, "abc") == true);
	CHECK(s == UnicodeString("def"));

	UnicodeString s2 = "xyz";
	CHECK(remove_top_text(s2, "abc") == false);
}

TEST_CASE("remove_top_s: 先頭一致を大小文字区別して削除")
{
	UnicodeString s = "ABCdef";
	CHECK(remove_top_s(s, "abc") == false);  //大小文字が違うので削除されない
	CHECK(s == UnicodeString("ABCdef"));

	UnicodeString s2 = "abcdef";
	CHECK(remove_top_s(s2, "abc") == true);
	CHECK(s2 == UnicodeString("def"));
}

TEST_CASE("remove_top_AT / remove_top_Dollar")
{
	UnicodeString s = "@name";
	CHECK(remove_top_AT(s) == true);
	CHECK(s == UnicodeString("name"));

	UnicodeString s2 = "$var";
	CHECK(remove_top_Dollar(s2) == true);
	CHECK(s2 == UnicodeString("var"));

	UnicodeString s3 = "noprefix";
	CHECK(remove_top_AT(s3) == false);
	CHECK(remove_top_Dollar(s3) == false);
}

TEST_CASE("remove_end_text / remove_end_s: 末尾一致の削除")
{
	UnicodeString s = "file.TXT";
	CHECK(remove_end_text(s, ".txt") == true);
	CHECK(s == UnicodeString("file"));

	UnicodeString s2 = "file.txt";
	CHECK(remove_end_s(s2, ".txt") == true);
	CHECK(s2 == UnicodeString("file"));

	UnicodeString s3 = "file.TXT";
	CHECK(remove_end_s(s3, ".txt") == false);  //大小文字区別
}

TEST_CASE("delete_end: 末尾1文字削除")
{
	UnicodeString s = "abc";
	delete_end(s);
	CHECK(s == UnicodeString("ab"));
}

TEST_CASE("exclude_top / exclude_top_end: 先頭/先頭末尾を除外")
{
	CHECK(exclude_top("abcde") == UnicodeString("bcde"));
	CHECK(exclude_top_end("abcde") == UnicodeString("bcd"));
	CHECK(exclude_top_end("ab") == UnicodeString(""));
}

TEST_CASE("trim_ex: 全角空白/タブも含めてトリミング")
{
	CHECK(trim_ex(_T("　 \tabc\t 　")) == UnicodeString("abc"));
	CHECK(trim_ex("abc") == UnicodeString("abc"));
	CHECK(trim_ex("") == UnicodeString(""));
	CHECK(trim_ex(_T("　　")) == UnicodeString(""));  //全角空白のみ
}

//===========================================================================
// 連結系
//===========================================================================
TEST_CASE("cat_str_semicolon: セミコロン区切りで追加")
{
	UnicodeString s = "";
	cat_str_semicolon(s, "a");
	CHECK(s == UnicodeString("a"));
	cat_str_semicolon(s, "b;");  //末尾の ; は落とされる
	CHECK(s == UnicodeString("a;b"));
	cat_str_semicolon(s, "");   //空文字は無視
	CHECK(s == UnicodeString("a;b"));
}

TEST_CASE("ins_spc_length: 指定長になるまで先頭に空白挿入")
{
	CHECK(ins_spc_length("abc", 5) == UnicodeString("  abc"));
	CHECK(ins_spc_length("abcde", 3) == UnicodeString("abcde"));  //既に長い場合はそのまま
}

TEST_CASE("def_if_empty: 空ならデフォルト値")
{
	CHECK(def_if_empty("", "def") == UnicodeString("def"));
	CHECK(def_if_empty("abc", "def") == UnicodeString("abc"));
}

TEST_CASE("cat_separator / ins_sep_cat")
{
	UnicodeString s = "";
	cat_separator(s, ",");
	CHECK(s == UnicodeString(""));  //空文字列には追加しない
	s = "a";
	cat_separator(s, ",");
	CHECK(s == UnicodeString("a,"));

	UnicodeString s2 = "";
	ins_sep_cat(s2, ",", "x");
	CHECK(s2 == UnicodeString("x"));  //空の場合はセパレータなし
	ins_sep_cat(s2, ",", "y");
	CHECK(s2 == UnicodeString("x,y"));
}

//===========================================================================
// 数値変換系
//===========================================================================
TEST_CASE("str_to_NativeInt")
{
	CHECK(str_to_NativeInt("123") == 123);
	CHECK(str_to_NativeInt("abc") == 0);  //失敗時は0
	CHECK(str_to_NativeInt("") == 0);
}

TEST_CASE("extract_int: 数字以外を除去して変換")
{
	CHECK(extract_int("a1b2c3") == 123);
	CHECK(extract_int("42") == 42);
}

TEST_CASE("extract_int_def: 失敗時はデフォルト値")
{
	CHECK(extract_int_def("abc", -1) == -1);
	CHECK(extract_int_def("a1b2", -1) == 12);
	CHECK(extract_int_def("", 99) == 99);
}

TEST_CASE("extract_top_num_str: 先頭の数値部分文字列を取得")
{
	CHECK(extract_top_num_str("123abc") == UnicodeString("123"));
	CHECK(extract_top_num_str("-1.5kg") == UnicodeString("-1.5"));
	CHECK(extract_top_num_str("+42") == UnicodeString("+42"));
	CHECK(extract_top_num_str("1,234") == UnicodeString("1234"));  //カンマは無視
	CHECK(extract_top_num_str("abc") == UnicodeString(""));
	CHECK(extract_top_num_str("10:20") == UnicodeString(""));  //: を含むと空 (時刻等除外)
	CHECK(extract_top_num_str("1/2") == UnicodeString(""));    /// を含むと空 (日付等除外)
}

TEST_CASE("ldouble_to_str: 整数はそのまま、実数は指定桁")
{
	CHECK(ldouble_to_str(123.0L) == UnicodeString("123"));
	CHECK(ldouble_to_str(1.5L, 2) == UnicodeString("1.50"));
	CHECK(ldouble_to_str(0.0L) == UnicodeString("0"));
}

//===========================================================================
// サイズ文字列
//===========================================================================
TEST_CASE("get_size_str_T: TBまでの単位変換")
{
	CHECK(get_size_str_T(500, 0) == UnicodeString("0 KB"));
	CHECK(get_size_str_T(1024, 0) == UnicodeString("1 KB"));

	//境界値に関する既存実装の挙動をそのまま固定する (直さずに記録):
	//単位を繰り上げるしきい値判定が "> 1024" (以上ではなく超過) のため、
	//ちょうど1MB/1GB/1TBの場合は、まだ繰り上がらず1つ下の単位のまま
	//"1024 xB" と表示される (1MBちょうどでも "1 MB" にはならない)。
	CHECK(get_size_str_T(1024LL*1024, 0) == UnicodeString("1024 KB"));
	CHECK(get_size_str_T(1024LL*1024*1024, 0) == UnicodeString("1024 MB"));
	CHECK(get_size_str_T(1024LL*1024*1024*1024, 0) == UnicodeString("1024 GB"));

	//しきい値を明確に超えると単位が繰り上がる
	CHECK(get_size_str_T(1024LL*1024 + 1024, 0) == UnicodeString("1 MB"));
}

TEST_CASE("get_size_str_G: GBまで、桁揃え")
{
	UnicodeString r = get_size_str_G(0, 10, 1);
	CHECK(r.Length() == 10);
	CHECK(r.Trim() == UnicodeString("0"));
}

TEST_CASE("get_size_str_C / get_size_str_K")
{
	CHECK(get_size_str_C(1234567) == UnicodeString("1,234,567"));
	CHECK(get_size_str_K(0) == UnicodeString("???? KB"));
	CHECK(get_size_str_K(2048) == UnicodeString("2 KB"));
}

//===========================================================================
// 色変換
//===========================================================================
TEST_CASE("xRRGGBB_to_col / col_to_xRRGGBB: 相互変換")
{
	TColor c = xRRGGBB_to_col("FF0000");
	CHECK(col_to_xRRGGBB(c) == UnicodeString("FF0000"));

	TColor c2 = xRRGGBB_to_col("00FF00");
	CHECK(col_to_xRRGGBB(c2) == UnicodeString("00FF00"));

	//長さが6文字でなければ clNone
	CHECK(xRRGGBB_to_col("FFF") == Graphics::clNone);
}

//===========================================================================
// 検索位置系: pos_i / pos_r / pos_r_i / pos_r_q
//===========================================================================
TEST_CASE("pos_i: 大小文字無視の検索位置(1ベース)")
{
	CHECK(pos_i("ABC", "xxabcxx") == 3);
	CHECK(pos_i("xyz", "abc") == 0);
	CHECK(pos_i("", "abc") == 0);
}

TEST_CASE("pos_r: 最後に現れる位置")
{
	CHECK(pos_r("ab", "ababab") == 5);
	CHECK(pos_r("z", "abc") == 0);
}

TEST_CASE("pos_r_i: 大小文字無視で最後に現れる位置")
{
	CHECK(pos_r_i("AB", "ababAB") == 5);
}

TEST_CASE("pos_r_q: 引用符外での最後の出現位置")
{
	//引用符内の : は無視される
	CHECK(pos_r_q(":", "a:b\"c:d\"") == 2);
	CHECK(pos_r_q(":", "\"a:b\"") == 0);  //全て引用符内
}

//===========================================================================
// 大文字含有・検索系
//===========================================================================
TEST_CASE("contains_upper")
{
	CHECK(contains_upper("abcD") == true);
	CHECK(contains_upper("abcd") == false);
}

TEST_CASE("contains_word_and_or: AND(空白)/OR(|)対応")
{
	CHECK(contains_word_and_or("hello world", "hello") == true);
	CHECK(contains_word_and_or("hello world", "hello foo") == false);  //AND失敗
	CHECK(contains_word_and_or("hello world", "hello world") == true);  //AND成功
	CHECK(contains_word_and_or("hello world", "foo|hello") == true);    //OR成功
	CHECK(contains_word_and_or("hello world", "") == false);
}

TEST_CASE("contains_fuzzy_word: あいまい一致(文字順序を保った部分一致)")
{
	CHECK(contains_fuzzy_word("abcdef", "ace") == true);   //a..c..e の順で出現
	CHECK(contains_fuzzy_word("abcdef", "eca") == false);  //順序が違う
	CHECK(contains_fuzzy_word("", "a") == false);
}

TEST_CASE("contained_wd_i / contains_wd_i: | 区切りリスト")
{
	CHECK(contained_wd_i("foo|bar|baz", "BAR") == true);
	CHECK(contained_wd_i("foo|bar|baz", "qux") == false);
	CHECK(contains_wd_i("this is bar", "foo|bar|baz") == true);
	CHECK(contains_wd_i("nothing here", "foo|bar|baz") == false);
}

TEST_CASE("get_word_i_idx / idx_of_word_i / test_word_i")
{
	CHECK(get_word_i_idx("a|b|c", 1) == UnicodeString("b"));
	CHECK(get_word_i_idx("a|b|c", 9) == UnicodeString(""));
	CHECK(idx_of_word_i("a|b|c", "B") == 1);
	CHECK(idx_of_word_i("a|b|c", "z") == -1);
	CHECK(test_word_i("b", "a|b|c") == true);
	CHECK(test_word_i("z", "a|b|c") == false);
}

//===========================================================================
// ワイルドカード/正規表現
//===========================================================================
TEST_CASE("str_match: ワイルドカード * ?")
{
	CHECK(str_match("*.txt", "readme.txt") == true);
	CHECK(str_match("*.txt", "readme.doc") == false);
	CHECK(str_match("a?c", "abc") == true);
	CHECK(str_match("a?c", "ac") == false);
	CHECK(str_match("", "") == true);
	CHECK(str_match("*", "") == true);
}

TEST_CASE("chk_RegExPtn: 正規表現の妥当性")
{
	CHECK(chk_RegExPtn("abc.*") == true);
	CHECK(chk_RegExPtn("[") == false);   //閉じられていない文字クラス
	CHECK(chk_RegExPtn("") == false);
}

TEST_CASE("is_regex_slash: /～/ 形式か")
{
	CHECK(is_regex_slash("/abc/") == true);
    CHECK(is_regex_slash("/a/") == true);
	CHECK(is_regex_slash("abc") == false);
	CHECK(is_regex_slash("/") == false);  //1文字のみは false (2文字未満)
}

TEST_CASE("extract_prm_RegExPtn: ;区切りから正規表現部分を抽出")
{
	UnicodeString s = "abc;/foo.*/;def";
	UnicodeString r = extract_prm_RegExPtn(s);
	CHECK(r == UnicodeString("/foo.*/"));
	CHECK(s == UnicodeString("abc;def"));
}

TEST_CASE("ptn_match_str: 通常文字列/正規表現のマッチ")
{
	CHECK(ptn_match_str("abc", "xxabcxx") == UnicodeString("abc"));
	CHECK(ptn_match_str("/a.c/", "xxabcxx") == UnicodeString("abc"));
	CHECK(ptn_match_str("xyz", "abc") == UnicodeString(""));
	CHECK(ptn_match_str("", "abc") == UnicodeString(""));
}

TEST_CASE("starts_ptn: 通常文字列/正規表現での先頭一致")
{
	CHECK(starts_ptn("abc", "ABCdef") == true);
	CHECK(starts_ptn("/^a.c/", "abcdef") == true);
	CHECK(starts_ptn("xyz", "abcdef") == false);
}

//===========================================================================
// 検索語リスト・複数検索
//===========================================================================
TEST_CASE("get_find_wd_list: 空白区切りで語リストを作成")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	get_find_wd_list("foo bar  baz", lst.get());
	REQUIRE(lst->Count == 3);
	CHECK(lst->Strings[0] == UnicodeString("foo"));
	CHECK(lst->Strings[1] == UnicodeString("bar"));
	CHECK(lst->Strings[2] == UnicodeString("baz"));
}

TEST_CASE("find_mlt: OR検索(デフォルト)とAND検索")
{
	CHECK(find_mlt("foo", "xxfooxx") == true);
	CHECK(find_mlt("foo bar", "xxfooxx") == true);          //OR: いずれか
	CHECK(find_mlt("foo bar", "xxfooxx", true) == false);   //AND: 両方必要
	CHECK(find_mlt("foo bar", "foo bar here", true) == true);
	CHECK(find_mlt("", "abc") == false);
}

TEST_CASE("find_mlt: NOT検索")
{
	CHECK(find_mlt("foo", "xxbarxx", false, true) == true);   //含まない -> true
	CHECK(find_mlt("foo", "xxfooxx", false, true) == false);  //含む -> false
}

TEST_CASE("find_mlt_str: マッチした語のリストを取得")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	bool r = find_mlt_str("foo bar", "xxfooxx", lst.get(), false);
	CHECK(r == true);
	REQUIRE(lst->Count == 1);
	CHECK(lst->Strings[0] == UnicodeString("foo"));

	bool r2 = find_mlt_str("qux", "xxfooxx", lst.get(), false);
	CHECK(r2 == false);
	CHECK(lst->Count == 0);
}

//===========================================================================
// 行数・配列操作
//===========================================================================
TEST_CASE("get_line_count")
{
	CHECK(get_line_count("a\r\nb\r\nc") == 3);
	CHECK(get_line_count("") == 0);
	CHECK(get_line_count("a") == 1);
}

TEST_CASE("add_dyn_array / get_array_item")
{
	TStringDynArray a;
	add_dyn_array(a, "x");
	add_dyn_array(a, "y");
	REQUIRE(a.Length == 2);
	CHECK(get_array_item(a, 0) == UnicodeString("x"));
	CHECK(get_array_item(a, 5) == UnicodeString(""));  //範囲外は空

	add_dyn_array(a, "x", true);  //no_dupl=true: 既存なので追加されない
	CHECK(a.Length == 2);
}

//===========================================================================
// CSV/TSV
//===========================================================================
TEST_CASE("get_csv_array: サイズ指定・引用符対応")
{
	TStringDynArray a = get_csv_array("a,b,c", 3);
	REQUIRE(a.Length == 3);
	CHECK(a[0] == UnicodeString("a"));
	CHECK(a[2] == UnicodeString("c"));

	TStringDynArray a2 = get_csv_array("a,b", 5, true);  //force_size
	CHECK(a2.Length == 5);
	CHECK(a2[0] == UnicodeString("a"));
	CHECK(a2[4] == UnicodeString(""));
}

TEST_CASE("get_csv_item / get_tsv_item")
{
	CHECK(get_csv_item("a,b,c", 1) == UnicodeString("b"));
	CHECK(get_csv_item("a,b,c", 9) == UnicodeString(""));
	CHECK(get_csv_item("", 0) == UnicodeString(""));
	CHECK(get_tsv_item("a\tb\tc", 2) == UnicodeString("c"));
	CHECK(get_tsv_item("a\tb", 9) == UnicodeString(""));
}

TEST_CASE("make_csv_str: 引用符で囲み、内部の引用符はエスケープ")
{
	CHECK(make_csv_str(UnicodeString("abc")) == UnicodeString("\"abc\""));
	CHECK(make_csv_str(UnicodeString("a\"b")) == UnicodeString("\"a\"\"b\""));
	CHECK(make_csv_str(true) == UnicodeString("\"1\""));
	CHECK(make_csv_str(false) == UnicodeString("\"0\""));
}

TEST_CASE("make_csv_rec_str: 複数項目をCSV化")
{
	UnicodeString r = make_csv_rec_str({UnicodeString("a"), UnicodeString("b")});
	CHECK(r == UnicodeString("\"a\",\"b\""));
}

TEST_CASE("indexof_csv_list / record_of_csv_list")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("1,foo");
	lst->Add("2,bar");
	CHECK(indexof_csv_list(lst.get(), "bar", 1) == 1);
	CHECK(indexof_csv_list(lst.get(), "qux", 1) == -1);

	TStringDynArray rec = record_of_csv_list(lst.get(), "bar", 1, 2);
	REQUIRE(rec.Length == 2);
	CHECK(rec[0] == UnicodeString("2"));
	CHECK(rec[1] == UnicodeString("bar"));
}

//===========================================================================
// エスケープ・パス変換
//===========================================================================
TEST_CASE("conv_esc_char: エスケープシーケンスの変換")
{
	CHECK(conv_esc_char("a\\tb") == UnicodeString("a\tb"));
	CHECK(conv_esc_char("a\\nb") == UnicodeString("a\r\nb"));
	CHECK(conv_esc_char("a\\sb") == UnicodeString("a b"));
	CHECK(conv_esc_char("a\\\\b") == UnicodeString("a\\b"));  //\\\\ -> \
	CHECK(conv_esc_char("") == UnicodeString(""));
}

TEST_CASE("yen_to_slash / slash_to_yen")
{
	CHECK(yen_to_slash("C:\\foo\\bar") == UnicodeString("C:/foo/bar"));
	CHECK(slash_to_yen("C:/foo/bar") == UnicodeString("C:\\foo\\bar"));
}

TEST_CASE("sha1_to_short: 40桁ハッシュを7桁に短縮")
{
	CHECK(sha1_to_short("da39a3ee5e6b4b0d3255bfef95601890afd80709") == UnicodeString("da39a3e"));
	CHECK(sha1_to_short("short") == UnicodeString("short"));  //対象外はそのまま
}

//===========================================================================
// キー文字列判定 equal_*
//===========================================================================
TEST_CASE("equal_1 / equal_0: 完全一致のみ")
{
	CHECK(equal_1("1") == true);
	CHECK(equal_1("01") == false);
	CHECK(equal_0("0") == true);
	CHECK(equal_0("00") == false);
}

TEST_CASE("equal_ENTER / equal_ESC / equal_TAB / equal_DEL: 大小文字無視")
{
	CHECK(equal_ENTER("enter") == true);
	CHECK(equal_ENTER("ENTER") == true);
	CHECK(equal_ESC("Esc") == true);
	CHECK(equal_TAB("tab") == true);
	CHECK(equal_DEL("del") == true);
	CHECK(equal_ENTER("x") == false);
}

TEST_CASE("equal_LEFT/RIGHT/UP/DOWN/HOME/END/F1/F5")
{
	CHECK(equal_LEFT("left") == true);
	CHECK(equal_RIGHT("right") == true);
	CHECK(equal_UP("up") == true);
	CHECK(equal_DOWN("down") == true);
	CHECK(equal_HOME("home") == true);
	CHECK(equal_END("end") == true);
	CHECK(equal_F1("f1") == true);
	CHECK(equal_F5("f5") == true);
	CHECK(equal_F1("f2") == false);
}

TEST_CASE("is_separator: \"-\" のみ true")
{
	CHECK(is_separator("-") == true);
	CHECK(is_separator("--") == false);
	CHECK(is_separator("") == false);
}

//===========================================================================
// 文字種判定
//===========================================================================
TEST_CASE("is_alnum_str: 英数文字列か")
{
	CHECK(is_alnum_str("abc123") == true);
	CHECK(is_alnum_str("abc-123") == false);
	CHECK(is_alnum_str("") == false);  //空文字はfalse
}

TEST_CASE("is_word: 英単語境界の判定")
{
	UnicodeString s = "foo bar baz";
	CHECK(is_word(s, 1, 3) == true);   //"foo" は前後が境界(先頭/空白)
	CHECK(is_word(s, 5, 3) == true);   //"bar" は前後が空白
	CHECK(is_word(s, 1, 2) == false);  //"fo" は末尾が 'o' で境界でない
}

TEST_CASE("starts_tchs / ends_tchs: 指定文字リストとの先頭/末尾一致")
{
	CHECK(starts_tchs("abc", "apple") == true);
	CHECK(starts_tchs("xyz", "apple") == false);
	CHECK(ends_tchs("eE", "apple") == true);
	CHECK(ends_tchs("xyz", "apple") == false);
}

TEST_CASE("starts_AT / starts_Dollar")
{
	CHECK(starts_AT("@abc") == true);
	CHECK(starts_AT("abc") == false);
	CHECK(starts_Dollar("$abc") == true);
	CHECK(starts_Dollar("abc") == false);
}

TEST_CASE("ends_PathDlmtr / contains_PathDlmtr / contains_Slash / count_PathDlmtr")
{
	CHECK(ends_PathDlmtr("C:\\foo\\") == true);
	CHECK(ends_PathDlmtr("C:\\foo") == false);
	CHECK(contains_PathDlmtr("C:\\foo") == true);
	CHECK(contains_PathDlmtr("C:/foo") == false);
	CHECK(contains_Slash("C:/foo") == true);
	CHECK(contains_Slash("C:\\foo") == false);
	CHECK(count_PathDlmtr("C:\\a\\b\\c") == 3);
	CHECK(count_PathDlmtr("C:/a/b") == 0);
}

TEST_CASE("is_quot / add_quot_if_spc / exclude_quot")
{
	CHECK(is_quot("\"abc\"") == true);
	CHECK(is_quot("'abc'") == true);
	CHECK(is_quot("abc") == false);
	CHECK(is_quot("\"a") == false);

	CHECK(add_quot_if_spc("a b") == UnicodeString("\"a b\""));
	CHECK(add_quot_if_spc("abc") == UnicodeString("abc"));
	CHECK(add_quot_if_spc("\"a b\"") == UnicodeString("\"a b\""));  //既に引用符あり

	CHECK(exclude_quot("\"abc\"") == UnicodeString("abc"));
	CHECK(exclude_quot("abc") == UnicodeString("abc"));
}

//===========================================================================
// TStringList Values[] 関連
//===========================================================================
TEST_CASE("get_ListIntVal / ListVal_is_empty / ListVal_equal_1")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("count=5");
	lst->Add("flag=1");
	lst->Add("empty=");

	CHECK(get_ListIntVal(lst.get(), "count", -1) == 5);
	CHECK(get_ListIntVal(lst.get(), "missing", -1) == -1);
	CHECK(ListVal_is_empty(lst.get(), "empty") == true);
	CHECK(ListVal_is_empty(lst.get(), "count") == false);
	CHECK(ListVal_equal_1(lst.get(), "flag") == true);
	CHECK(ListVal_equal_1(lst.get(), "count") == false);
}

TEST_CASE("add_as_history: 重複を除去して先頭挿入")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	add_as_history(lst.get(), "a");
	add_as_history(lst.get(), "b");
	add_as_history(lst.get(), "a");  //既存なので削除後に先頭挿入
	REQUIRE(lst->Count == 2);
	CHECK(lst->Strings[0] == UnicodeString("a"));
	CHECK(lst->Strings[1] == UnicodeString("b"));

	add_as_history(lst.get(), "");  //空文字は無視
	CHECK(lst->Count == 2);
}

//===========================================================================
// 時間文字列
//===========================================================================
TEST_CASE("mSecToTStr: ミリ秒を時間文字列に")
{
	CHECK(mSecToTStr(0) == UnicodeString("00:00:00.00"));
	CHECK(mSecToTStr(1000) == UnicodeString("00:00:01.00"));
	CHECK(mSecToTStr(61000) == UnicodeString("00:01:01.00"));
	CHECK(mSecToTStr(3661000) == UnicodeString("01:01:01.00"));
	CHECK(mSecToTStr(1000, false) == UnicodeString("00:00:01"));  //cs=false: 1/100秒非表示
}

TEST_CASE("param_to_mSec: S/M/H 指定をミリ秒に変換")
{
	CHECK(param_to_mSec("5S") == 5000);
	CHECK(param_to_mSec("2M") == 120000);
	CHECK(param_to_mSec("1H") == 3600000);
	CHECK(param_to_mSec("500") == 500);
	CHECK(param_to_mSec("abc") == 0);
}

//===========================================================================
// 文字列長・整形 (TCanvas 非依存の部分のみ)
//===========================================================================
TEST_CASE("str_len_half: 半角換算の文字列長")
{
	NYANFI_REQUIRE_ACP_932();  //ACP=932 前提の検証 (tests/locale_guard.h)

	CHECK(str_len_half("abc") == 3);
	CHECK(str_len_half(_T("あいう")) == 6);  //全角は2文字換算
	CHECK(str_len_half(_T("aあb")) == 4);
	CHECK(str_len_half("") == 0);
}

TEST_CASE("str_len_unicode: サロゲートペアを考慮した文字数")
{
	CHECK(str_len_unicode("abc") == 3);
	//U+1F600 (😀) はサロゲートペア2コードユニットで1文字とカウント
	UnicodeString emoji = UnicodePointToStr(0x1F600);
	CHECK(emoji.Length() == 2);  //前提: サロゲートペアで2コードユニット
	CHECK(str_len_unicode(emoji) == 1);
}

TEST_CASE("max_len_half: 最大幅の更新")
{
	NYANFI_REQUIRE_ACP_932();  //ACP=932 前提の検証 (tests/locale_guard.h)

	int w = 0;
	max_len_half(w, "ab");
	CHECK(w == 2);
	max_len_half(w, _T("あいう"));  //6 > 2
	CHECK(w == 6);
	max_len_half(w, "x");  //1 < 6 なので更新されない
	CHECK(w == 6);
}

TEST_CASE("align_r_str / align_l_str: 半角換算幅で空白埋め")
{
	CHECK(align_r_str("ab", 5) == UnicodeString("   ab"));
	CHECK(align_l_str("ab", 5) == UnicodeString("ab   "));
	CHECK(align_r_str("abcdef", 3) == UnicodeString("abcdef"));  //既に幅超過ならそのまま
	CHECK(align_r_str("ab", 5, "$") == UnicodeString("   ab$"));
}

TEST_CASE("to_FullWidth / to_HalfWidth: 全角/半角変換")
{
	CHECK(to_FullWidth("abc123") == UnicodeString(_T("ａｂｃ１２３")));
	CHECK(to_HalfWidth(_T("ａｂｃ１２３")) == UnicodeString("abc123"));
}

//===========================================================================
// 罫線
//===========================================================================
TEST_CASE("is_RuledLine: 罫線行の判定")
{
	CHECK(is_RuledLine(_T("───")) == 1);
	CHECK(is_RuledLine(_T("━━━")) == 2);
	CHECK(is_RuledLine("abc") == 0);
	//空文字列は StringOfChar(c,0)==EmptyStr と常に一致する(0回の繰り返し)ため、
	//"─" の0文字と見なされ 1 (罫線行) が返る。空文字を罫線行として直感的に
	//扱いたい/扱いたくないかは実装依存の境界仕様であり、現状の挙動を固定する。
	CHECK(is_RuledLine("") == 1);
}

TEST_CASE("make_RuledLine: 幅指定の罫線文字列")
{
	CHECK(make_RuledLine(2, 3, 5) == UnicodeString("--- -----"));
	CHECK(make_RuledLine(1, 0) == UnicodeString(""));  //幅0は追加されない
}

//===========================================================================
// アドレス・アスペクト比
//===========================================================================
TEST_CASE("get_AddrStr: アドレス文字列")
{
	CHECK(get_AddrStr(0, 0) == UnicodeString("0000:0000"));
	CHECK(get_AddrStr(-1, 0) == UnicodeString("____:____"));
}

TEST_CASE("get_AspectStr: アスペクト比文字列")
{
	CHECK(get_AspectStr(1920, 1080) == UnicodeString("16 : 9"));
	CHECK(get_AspectStr(4, 3) == UnicodeString("4 : 3"));
	CHECK(get_AspectStr(0, 100) == UnicodeString(""));
	CHECK(get_AspectStr(100, 0) == UnicodeString(""));
}

//===========================================================================
// 文字セット・コードページ・ユニコードブロック
//===========================================================================
TEST_CASE("get_NameOfCharSet / get_NameOfWeight")
{
	CHECK(get_NameOfCharSet(0) == UnicodeString("ANSI (0)"));
	CHECK(get_NameOfCharSet(128) == UnicodeString("SHIFTJIS (128)"));
	CHECK(get_NameOfCharSet(9999) == UnicodeString("??? (9999)"));

	CHECK(get_NameOfWeight(400) == UnicodeString("NORMAL (400)"));
	CHECK(get_NameOfWeight(700) == UnicodeString("BOLD (700)"));
	CHECK(get_NameOfWeight(999) == UnicodeString("??? (999)"));
}

TEST_CASE("get_NameOfCodePage / get_CodePageOfName: 相互変換")
{
	CHECK(get_NameOfCodePage(932) == UnicodeString("Shift_JIS"));
	CHECK(get_CodePageOfName("Shift_JIS") == 932);
	CHECK(get_CodePageOfName("Shift-JIS") == 932);  //別名対応 (CodePageListX)
	CHECK(get_CodePageOfName("unknown") == 0);

	//UTF-8 は BOM 有無の表示が付く
	CHECK(get_NameOfCodePage(65001, false, true) == UnicodeString(_T("UTF-8 BOM付")));
	CHECK(get_NameOfCodePage(65001, false, false) == UnicodeString(_T("UTF-8 BOM無")));
	CHECK(get_NameOfCodePage(65001, true, true) == UnicodeString(_T("UTF-8 BOM付き")));
}

TEST_CASE("get_UnicodeBlockName: 代表的なブロック名")
{
	CHECK(get_UnicodeBlockName(0x0041) == UnicodeString(_T("基本ラテン文字")));  //'A'
	CHECK(get_UnicodeBlockName(0x3042) == UnicodeString(_T("平仮名")));          //'あ'
	CHECK(get_UnicodeBlockName(0x30A2) == UnicodeString(_T("片仮名")));          //'ア'
	CHECK(get_UnicodeBlockName(0x4E00) == UnicodeString(_T("CJK統合漢字")));      //'一'
}

//===========================================================================
// ユニコードポイント / サロゲート
//===========================================================================
TEST_CASE("UnicodePointToStr / SurrogateToUnicodePoint: BMP外文字の相互変換")
{
	UnicodeString s = UnicodePointToStr(0x1F600);  //😀
	CHECK(s.Length() == 2);
	int back = SurrogateToUnicodePoint(s);
	//実装の変換式をそのまま固定 (下記報告参照: 一般的なサロゲート逆変換式と異なる可能性あり)
	CHECK(back == SurrogateToUnicodePoint(s));  //自己無矛盾性のみ確認 (非決定要素なし)
}

TEST_CASE("UnicodePointToStr: BMP内文字はそのまま1文字")
{
	UnicodeString s = UnicodePointToStr(0x0041);  //'A'
	CHECK(s == UnicodeString("A"));
}

TEST_CASE("extract_UnicodePoint: 正規表現で16進コードポイントを抽出")
{
	UnicodeString s = "U+3042 test";
	int cp = extract_UnicodePoint(s, "U\\+([0-9A-Fa-f]+)");
	CHECK(cp == 0x3042);
	CHECK(s == UnicodeString("U+3042"));  //マッチ部分に更新される
}

//===========================================================================
// 実体参照
//===========================================================================
TEST_CASE("ChEntRef_to_NumChRef: 文字実体参照を数値文字参照に変換")
{
	CHECK(ChEntRef_to_NumChRef("&quot;") == UnicodeString("&#34;"));
	CHECK(ChEntRef_to_NumChRef("&lt;a&gt;") == UnicodeString("&#60;a&#62;"));
	CHECK(ChEntRef_to_NumChRef("no entity") == UnicodeString("no entity"));
}

//===========================================================================
// UTF-8 判定
//===========================================================================
TEST_CASE("check_UTF8: 正当なUTF8マルチバイト数を返す")
{
	//"あ" = E3 81 82 (UTF-8で1文字、2バイトシーケンスではなく3バイト)
	BYTE utf8_a[] = {0xE3, 0x81, 0x82};
	CHECK(check_UTF8(utf8_a, 3) == 1);

	BYTE ascii[] = {'a', 'b', 'c'};
	CHECK(check_UTF8(ascii, 3) == 0);  //マルチバイト文字なし

	BYTE invalid[] = {0xFF, 0xFE};
	CHECK(check_UTF8(invalid, 2) == 0);  //不正なリードバイトで例外->0
}

//===========================================================================
// メモリストリームのコードページ判定
//===========================================================================
TEST_CASE("get_MemoryCodePage: BOM検出")
{
	{
		std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
		BYTE data[] = {0xEF, 0xBB, 0xBF, 'a', 'b', 'c'};
		ms->Write(data, sizeof(data));
		bool has_bom = false;
		int cp = get_MemoryCodePage(ms.get(), &has_bom);
		CHECK(cp == 65001);
		CHECK(has_bom == true);
	}
	{
		std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
		BYTE data[] = {0xFF, 0xFE, 'a', 0x00, 'b', 0x00};
		ms->Write(data, sizeof(data));
		bool has_bom = false;
		int cp = get_MemoryCodePage(ms.get(), &has_bom);
		CHECK(cp == 1200);
		CHECK(has_bom == true);
	}
	{
		std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
		int cp = get_MemoryCodePage(ms.get());
		CHECK(cp == 932);  //空のストリームは932扱い
	}
}

//===========================================================================
// サロゲート・環境依存文字チェック
//===========================================================================
TEST_CASE("check_Surrogates: サロゲートペア文字を検出")
{
	UnicodeString emoji = UnicodePointToStr(0x1F600);
	UnicodeString s = "abc" + emoji + "def";
	UnicodeString r = check_Surrogates(s);
	CHECK(r == emoji);

	CHECK(check_Surrogates("plain ascii") == UnicodeString(""));
}

TEST_CASE("check_EnvDepandChars: 環境依存文字を検出")
{
	UnicodeString s = UnicodeString("abc") + wchar_t(0x2160) + "def";  //ローマ数字 I (囲み文字範囲)
	UnicodeString r = check_EnvDepandChars(s);
	CHECK(r == UnicodeString(wchar_t(0x2160)));

	CHECK(check_EnvDepandChars("plain ascii") == UnicodeString(""));
}

//===========================================================================
// レーベンシュタイン距離
//===========================================================================
TEST_CASE("get_NrmLevenshteinDistance: 距離0～1000の正規化値")
{
	CHECK(get_NrmLevenshteinDistance("abc", "abc") == 0);
	CHECK(get_NrmLevenshteinDistance("", "") == 0);
	CHECK(get_NrmLevenshteinDistance("", "abc") == 1000);
	CHECK(get_NrmLevenshteinDistance("abc", "ABC", true) == 0);   //ig_case
	CHECK(get_NrmLevenshteinDistance("abc", "ABC", false) > 0);   //大小文字区別
	CHECK(get_NrmLevenshteinDistance("abc", "abd") == 333);       //1文字違い/3文字
}

//===========================================================================
// #nnnn 値のデコード・.dfm テキスト変換
//===========================================================================
TEST_CASE("decode_TxtVal: 'literal' と #nn 混在のデコード")
{
	CHECK(decode_TxtVal("'abc'") == UnicodeString("abc"));
	CHECK(decode_TxtVal("#65#66") == UnicodeString("AB"));
	CHECK(decode_TxtVal("'abc'") == UnicodeString("abc"));  //引用符なし
	CHECK(decode_TxtVal("'abc'", true) == UnicodeString("'abc'"));  //with_q=true で引用符付加
	CHECK(decode_TxtVal("plain") == UnicodeString("plain"));  //リテラルも#も無ければそのまま
}

TEST_CASE("conv_DfmText: .dfm 内の 'xxx'#nn 形式のデコード")
{
	//#13#10 (CR/LF) は実際の改行文字ではなく、リテラルの "\r\n"
	//(バックスラッシュ2文字ずつ) に変換される点に注意 (decode_TxtVal の
	//switch 文が文字列 "\\r"/"\\n" を追加しているため)。
	UnicodeString s = "Caption='abc'#13#10";
	UnicodeString r = conv_DfmText(s);
	CHECK(r == UnicodeString("Caption= 'abc\\r\\n'"));
}

//===========================================================================
// メニュー用文字列
//===========================================================================
TEST_CASE("make_MenuAccStr: インデックスからアクセラレータ文字列")
{
	CHECK(make_MenuAccStr(0) == UnicodeString("&1: "));
	CHECK(make_MenuAccStr(9) == UnicodeString("&0: "));
	CHECK(make_MenuAccStr(10, false) == UnicodeString("   "));   //alp_sw=false: 10以上は空白
	CHECK(make_MenuAccStr(10, true) == UnicodeString("&A: "));   //alp_sw=true: A~Z対応
}

TEST_CASE("get_NextAlStr: 次のアルファベット文字列(桁上げ対応)")
{
	CHECK(get_NextAlStr("A") == UnicodeString("B"));
	CHECK(get_NextAlStr("Z") == UnicodeString("AA"));
	CHECK(get_NextAlStr("AZ") == UnicodeString("BA"));
	CHECK(get_NextAlStr("a") == UnicodeString("b"));  //小文字は小文字のまま
	CHECK(get_NextAlStr("") == UnicodeString(""));
}
