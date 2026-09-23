/**
 * @file tests/core/test_gui_print_image.cpp
 * @brief gui/print_image.h (画像印刷ダイアログの設定解決) のテスト
 *
 * VCL 実測:
 * - src/usr_cmdlist.cpp:394 に I:Print
 * - src/MainFrm.cpp:35689-35697 が TPrintImgDlg を表示する
 * - src/PrnImgDlg.cpp:30-84/99-108/181-221/347-355 が設定・描画・実行
 */
#include "doctest/doctest.h"

#include "gui/print_image.h"

using namespace print_image;

namespace {

PrintOptions options()
{
	PrintOptions o;
	o.orientation = Orientation::Portrait;
	o.copies = 1;
	o.scale_percent = 100;
	o.fit = ImageFit::FitPage;
	o.print_range = PrintRange::Current;
	o.offset_x_percent = 0;
	o.offset_y_percent = 0;
	o.grayscale = false;
	o.print_text = false;
	o.text_format = _T("$XT(yy/mm/dd)");
	o.text_position = TextPosition::Bottom;
	o.text_alignment = TextAlignment::Center;
	o.text_margin_percent = 5;
	return o;
}

}  // namespace

TEST_CASE("ResolvePrintSettings: 方向・部数・倍率・範囲を解決する")
{
	PrintOptions o = options();
	o.orientation = Orientation::Landscape;
	o.copies = 3;
	o.print_range = PrintRange::All;

	const ResolvedSettings r = ResolvePrintSettings(o, 4, 2, {});
	CHECK(r.valid);
	CHECK(r.orientation == Orientation::Landscape);
	CHECK(r.copies == 3);
	CHECK(r.range == PrintRange::All);
	CHECK(r.first_page == 1);
	CHECK(r.last_page == 4);
	CHECK(r.error.IsEmpty());
}

TEST_CASE("ResolvePrintSettings: 現在ページは範囲へ収める")
{
	const ResolvedSettings r = ResolvePrintSettings(options(), 3, 99, {});
	CHECK(r.valid);
	CHECK(r.range == PrintRange::Current);
	CHECK(r.first_page == 3);
	CHECK(r.last_page == 3);

	const ResolvedSettings zero = ResolvePrintSettings(options(), 3, 0, {});
	CHECK(zero.valid);
	CHECK(zero.first_page == 1);
}

TEST_CASE("ResolvePrintSettings: 選択範囲はソート・重複除去・範囲外除去")
{
	PrintOptions o = options();
	o.print_range = PrintRange::Selection;
	const ResolvedSettings r = ResolvePrintSettings(o, 5, 1, {4, 2, 2, 99, 0});
	CHECK(r.valid);
	CHECK(r.first_page == 2);
	CHECK(r.last_page == 4);
}

TEST_CASE("ResolvePrintSettings: 空の選択範囲は実装ではなく明示的なエラー")
{
	PrintOptions o = options();
	o.print_range = PrintRange::Selection;
	const ResolvedSettings r = ResolvePrintSettings(o, 5, 1, {});
	CHECK_FALSE(r.valid);
	CHECK(r.error == _T("選択範囲に有効なページがありません"));

	const ResolvedSettings no_pages = ResolvePrintSettings(o, 0, 1, {1});
	CHECK_FALSE(no_pages.valid);
	CHECK(no_pages.error == _T("印刷するページがありません"));
}

TEST_CASE("ResolvePrintSettings: VCLのUpDown範囲へ正規化する")
{
	PrintOptions o = options();
	o.copies = 0;
	o.scale_percent = 500;
	o.offset_x_percent = -10;
	o.offset_y_percent = 200;
	o.text_margin_percent = 120;
	o.fit = ImageFit::TopLeft;

	const ResolvedSettings r = ResolvePrintSettings(o, 1, 1, {});
	CHECK(r.valid);
	CHECK(r.copies == 1);
	CHECK(r.scale_percent == 100);
	CHECK(r.offset_x_percent == 0);
	CHECK(r.offset_y_percent == 99);
	CHECK(r.text_margin_percent == 99);
}

TEST_CASE("ResolvePrintSettings: 用紙に合わせる/切取では倍率入力を使わない")
{
	PrintOptions fit = options();
	fit.scale_percent = 25;
	fit.fit = ImageFit::FitPage;
	CHECK(ResolvePrintSettings(fit, 1, 1, {}).effective_scale_percent == 100);

	PrintOptions center = fit;
	center.fit = ImageFit::Center;
	CHECK(ResolvePrintSettings(center, 1, 1, {}).effective_scale_percent == 25);
}

TEST_CASE("FormatPrintSettings: 解決済み設定を人が読める1行にする")
{
	PrintOptions o = options();
	o.orientation = Orientation::Landscape;
	o.copies = 2;
	o.print_range = PrintRange::All;
	o.grayscale = true;
	const ResolvedSettings r = ResolvePrintSettings(o, 3, 1, {});
	const UnicodeString summary = FormatPrintSettings(r);
	CHECK(ContainsStr(summary, _T("横")));
	CHECK(ContainsStr(summary, _T("2部")));
	CHECK(ContainsStr(summary, _T("1-3")));
	CHECK(ContainsStr(summary, _T("グレー")));
}
