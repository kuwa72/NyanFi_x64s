/**
 * @file tests/core/test_gui_cv_img.cpp
 * @brief gui/cv_img.h の純関数テスト
 */
#include "doctest/doctest.h"

#include "gui/cv_img.h"

TEST_CASE("cv_img: 形式と拡張子の対応")
{
	CHECK(cv_img::Extension(cv_img::Format::Bmp) == UnicodeString(_T(".bmp")));
	CHECK(cv_img::Extension(cv_img::Format::Jpg) == UnicodeString(_T(".jpg")));
	CHECK(cv_img::Extension(cv_img::Format::Hdp) == UnicodeString(_T(".hdp")));
	CHECK(cv_img::FormatFromIndex(4) == cv_img::Format::Tif);
	CHECK(cv_img::FormatFromIndex(-1) == cv_img::Format::Bmp);
	CHECK(cv_img::FormatIndex(cv_img::Format::Gif) == 3);
}

TEST_CASE("cv_img: 品質と添字を正規化する")
{
	cv_img::Options o;
	o.quality = 240;
	o.ycrcb = -4;
	o.compression = 99;
	o.scale_param1 = 0;
	o.scale_param2 = -1;
	const cv_img::Options n = cv_img::Normalize(o);
	CHECK(n.quality == 100);
	CHECK(n.ycrcb == 0);
	CHECK(n.compression == 7);
	CHECK(n.scale_param1 == 1);
	CHECK(n.scale_param2 == 1);
}

TEST_CASE("cv_img: 形式ごとの品質/YCrCb/TIFF 圧縮の表示")
{
	cv_img::Options jpg;
	jpg.format = cv_img::Format::Jpg;
	const cv_img::FormatVisibility jv = cv_img::ResolveVisibility(jpg);
	CHECK(jv.quality);
	CHECK(jv.ycrcb);
	CHECK_FALSE(jv.compression);

	cv_img::Options tif;
	tif.format = cv_img::Format::Tif;
	const cv_img::FormatVisibility tv = cv_img::ResolveVisibility(tif);
	CHECK_FALSE(tv.quality);
	CHECK_FALSE(tv.ycrcb);
	CHECK(tv.compression);
}

TEST_CASE("cv_img: スケール欄のラベルと有効状態")
{
	cv_img::Options o;
	o.scale_mode = cv_img::ScaleMode::None;
	cv_img::ScaleState none = cv_img::ResolveScaleState(o.scale_mode);
	CHECK_FALSE(none.option);
	CHECK_FALSE(none.param1);
	CHECK(none.label1.IsEmpty());

	o.scale_mode = cv_img::ScaleMode::FitPad;
	cv_img::ScaleState fit = cv_img::ResolveScaleState(o.scale_mode);
	CHECK(fit.option);
	CHECK(fit.param1);
	CHECK(fit.param2);
	CHECK(fit.label1 == UnicodeString(_T("横サイズ")));
	CHECK(fit.label2 == UnicodeString(_T("縦サイズ")));
}

TEST_CASE("cv_img: 実 core が反映しない条件は明示できる")
{
	cv_img::Options plain;
	CHECK_FALSE(cv_img::HasUnsupportedRuntimeOptions(plain));

	cv_img::Options scaled = plain;
	scaled.scale_mode = cv_img::ScaleMode::Percent;
	CHECK(cv_img::HasUnsupportedRuntimeOptions(scaled));

	cv_img::Options clip = plain;
	clip.from_clipboard = true;
	CHECK(cv_img::HasUnsupportedRuntimeOptions(clip));

	cv_img::Options hdp = plain;
	hdp.format = cv_img::Format::Hdp;
	CHECK(cv_img::HasUnsupportedRuntimeOptions(hdp));
}

TEST_CASE("cv_img: 名前変更の前後を正規化できる")
{
	cv_img::Options o;
	o.name_mode = cv_img::NameMode::Prefix;
	o.name_text = _T("new_");
	const cv_img::Options n = cv_img::Normalize(o);
	CHECK(n.name_mode == cv_img::NameMode::Prefix);
	CHECK(n.name_text == UnicodeString(_T("new_")));
}
