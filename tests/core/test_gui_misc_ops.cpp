/**
 * @file tests/core/test_gui_misc_ops.cpp
 * @brief gui/misc_ops.cpp のテスト
 */
#include "doctest/doctest.h"

#include "gui/misc_ops.h"

TEST_CASE("HasInvalidNameChar: Windows で使えない文字")
{
	CHECK_FALSE(misc_ops::HasInvalidNameChar(_T("normal.txt")));
	CHECK_FALSE(misc_ops::HasInvalidNameChar(_T("日本語 と空白.txt")));

	CHECK(misc_ops::HasInvalidNameChar(_T("a/b")));
	CHECK(misc_ops::HasInvalidNameChar(_T("a\\b")));
	CHECK(misc_ops::HasInvalidNameChar(_T("a:b")));
	CHECK(misc_ops::HasInvalidNameChar(_T("a*b")));
	CHECK(misc_ops::HasInvalidNameChar(_T("a?b")));
	CHECK(misc_ops::HasInvalidNameChar(_T("a\"b")));
	CHECK(misc_ops::HasInvalidNameChar(_T("a<b")));
	CHECK(misc_ops::HasInvalidNameChar(_T("a>b")));
	CHECK(misc_ops::HasInvalidNameChar(_T("a|b")));
}

TEST_CASE("NameFromClipboard: そのまま使える名前")
{
	UnicodeString error;
	CHECK(misc_ops::NameFromClipboard(_T("report.txt"), error) == UnicodeString(_T("report.txt")));
	CHECK(error.IsEmpty());
}

TEST_CASE("NameFromClipboard: パスなら末尾の要素だけを使う")
{
	// VCL も ExtractFileName に通す
	UnicodeString error;
	CHECK(misc_ops::NameFromClipboard(_T("C:\\dir\\report.txt"), error)
	      == UnicodeString(_T("report.txt")));
}

TEST_CASE("NameFromClipboard: 引用符を外す")
{
	UnicodeString error;
	CHECK(misc_ops::NameFromClipboard(_T("\"quoted name.txt\""), error)
	      == UnicodeString(_T("quoted name.txt")));
}

TEST_CASE("NameFromClipboard: 前後の空白と改行を落とす")
{
	// クリップボードには改行が付いていることが多い
	UnicodeString error;
	CHECK(misc_ops::NameFromClipboard(_T("  name.txt  \r\n"), error)
	      == UnicodeString(_T("name.txt")));
	CHECK(misc_ops::NameFromClipboard(_T("first.txt\r\nsecond.txt"), error)
	      == UnicodeString(_T("first.txt")));
}

TEST_CASE("NameFromClipboard: 空なら理由つきで失敗する")
{
	UnicodeString error;
	CHECK(misc_ops::NameFromClipboard(EmptyStr, error).IsEmpty());
	CHECK_FALSE(error.IsEmpty());

	error = EmptyStr;
	CHECK(misc_ops::NameFromClipboard(_T("   \r\n"), error).IsEmpty());
	CHECK_FALSE(error.IsEmpty());
}

TEST_CASE("NameFromClipboard: 使えない文字が残っていたら弾く")
{
	// 弾かないと rename が失敗するだけで理由が分からない。
	// ExtractFileName を通した後に残る '?' などが対象
	UnicodeString error;
	CHECK(misc_ops::NameFromClipboard(_T("bad?name.txt"), error).IsEmpty());
	CHECK(ContainsText(error, _T("使えない文字")));
}

TEST_CASE("EnumLocalShares: 失敗しても落ちない")
{
	// 環境によっては権限が無くて取れない。そのとき理由が返ること
	std::vector<misc_ops::ShareEntry> out;
	UnicodeString error;
	const bool ok = misc_ops::EnumLocalShares(out, error);
	if (!ok) CHECK_FALSE(error.IsEmpty());
	// 取れた場合、管理共有 ($ 終わり) が含まれないこと
	for (const auto &e : out) CHECK_FALSE(EndsStr(_T("$"), e.name));
}
