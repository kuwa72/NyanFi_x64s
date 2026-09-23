/**
 * @file tests/core/test_gui_cv_enc.cpp
 * @brief gui/cv_enc.h (TCvTxtEncDlg の判断ロジック) のテスト
 */
#include "doctest/doctest.h"

#include "gui/cv_enc.h"

TEST_CASE("cv_enc: UTF-8N を除く保存文字コード一覧")
{
	const std::vector<UnicodeString> names = cv_enc::EncodingNames();
	REQUIRE(names.size() == 6);
	CHECK(names[0] == _T("Shift_JIS"));
	CHECK(names[1] == _T("UTF-8"));
	CHECK(names[2] == _T("UTF-16"));
	CHECK(names[3] == _T("UTF-16(BE)"));
	CHECK(names[4] == _T("EUC-JP"));
	CHECK(names[5] == _T("ISO-2022-JP"));
}

TEST_CASE("cv_enc: 選択肢の補正とコードページ")
{
	CHECK(cv_enc::NormalizeSelection(-1, 6) == 0);
	CHECK(cv_enc::NormalizeSelection(0, 6) == 0);
	CHECK(cv_enc::NormalizeSelection(5, 6) == 5);
	CHECK(cv_enc::NormalizeSelection(6, 6) == 0);
	CHECK(cv_enc::CodePageFor(0) == 932);
	CHECK(cv_enc::CodePageFor(1) == 65001);
	CHECK(cv_enc::CodePageFor(2) == 1200);
	CHECK(cv_enc::CodePageFor(3) == 1201);
	CHECK(cv_enc::CodePageFor(4) == 20932);
	CHECK(cv_enc::CodePageFor(5) == 50220);
}

TEST_CASE("cv_enc: BOM の可否と宣言名")
{
	CHECK(cv_enc::BomAvailable(_T("UTF-8")) == true);
	CHECK(cv_enc::BomAvailable(_T("UTF-16(BE)")) == true);
	CHECK(cv_enc::BomAvailable(_T("EUC-JP")) == false);
	CHECK(cv_enc::DeclarationCharset(_T("UTF-16(BE)")) == _T("UTF-16"));
	CHECK(cv_enc::DeclarationCharset(_T("UTF-8")) == _T("UTF-8"));
}

TEST_CASE("cv_enc: 改行と開始ボタン条件")
{
	CHECK(cv_enc::LineBreakFor(0) == _T("\r\n"));
	CHECK(cv_enc::LineBreakFor(1) == _T("\n"));
	CHECK(cv_enc::LineBreakFor(2) == _T("\r"));
	CHECK(cv_enc::LineBreakFor(-1) == _T("\r\n"));
	CHECK(cv_enc::CanSubmit(-1) == false);
	CHECK(cv_enc::CanSubmit(0) == true);
}

TEST_CASE("cv_enc: ダイアログ表題の補足")
{
	CHECK(cv_enc::TitleSuffix(3, _T("C:\\dir\\a.txt")) == _T(" - 選択 3"));
	CHECK(cv_enc::TitleSuffix(0, _T("C:\\dir\\a.txt")) == _T(" - a.txt"));
	CHECK(cv_enc::TitleSuffix(0, EmptyStr).IsEmpty());
}
