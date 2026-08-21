/**
 * @file tests/core/test_usr_color.cpp
 * @brief src/usr_color.cpp (色の計算) の回帰テスト
 *
 * GUI (TStyleManager::ActiveStyle 経由で実際の Windows システムカラーに依存)
 * な関数 (get_WinColor / get_TextColor / get_PanelColor / get_LabelColor /
 * get_OptSysColor / set_EditColor / InvColIfEmpty / SetHighlight) は実行環境の
 * システム設定に応じて結果が変わりうるため対象外とした (詳細は報告を参照)。
 */
#include "doctest/doctest.h"

#include "usr_color.h"

//===========================================================================
// GetLuminance: 輝度計算
//===========================================================================
TEST_CASE("GetLuminance: 白/黒/中間色の輝度")
{
	CHECK(GetLuminance(clWhite) == doctest::Approx(1.0));
	CHECK(GetLuminance(clBlack) == doctest::Approx(0.0));
	//赤(R=255,G=0,B=0): 255*0.3/255 = 0.3
	CHECK(GetLuminance(clRed) == doctest::Approx(0.3));
}

//===========================================================================
// RatioCol: 倍率適用
//===========================================================================
TEST_CASE("RatioCol: 倍率で色を減衰")
{
	TColor c = RatioCol(clWhite, 0.5f);
	CHECK(GetRValue(ColorToRGB(c)) == 127);  //255*0.5 = 127 (int切り捨て)
	CHECK(GetGValue(ColorToRGB(c)) == 127);
	CHECK(GetBValue(ColorToRGB(c)) == 127);

	//範囲外は 0.0～1.0 にクランプされる
	TColor c2 = RatioCol(clWhite, 2.0f);
	CHECK(GetRValue(ColorToRGB(c2)) == 255);
	TColor c3 = RatioCol(clWhite, -1.0f);
	CHECK(GetRValue(ColorToRGB(c3)) == 0);
}

//===========================================================================
// GrayCol: グレースケール化
//===========================================================================
TEST_CASE("GrayCol: RGB成分が均等になる")
{
	TColor c = GrayCol(clRed);
	int cref = ColorToRGB(c);
	CHECK(GetRValue(cref) == GetGValue(cref));
	CHECK(GetGValue(cref) == GetBValue(cref));

	//clNone は clBlack として扱われる
	TColor c2 = GrayCol(Graphics::clNone);
	CHECK(ColorToRGB(c2) == ColorToRGB(clBlack));
}

//===========================================================================
// ComplementaryCol: 補色
//===========================================================================
TEST_CASE("ComplementaryCol: 赤の補色はシアン系")
{
	TColor c = ComplementaryCol(clRed);
	int cref = ColorToRGB(c);
	CHECK(GetRValue(cref) == 0);
	CHECK(GetGValue(cref) == 255);
	CHECK(GetBValue(cref) == 255);
}

//===========================================================================
// RgbToHsl / HslToCol: 相互変換
//===========================================================================
TEST_CASE("RgbToHsl: 赤のHSL値")
{
	int h, s, l;
	RgbToHsl(clRed, &h, &s, &l);
	CHECK(h == 0);
	CHECK(s == 100);
	CHECK(l == 50);
}

TEST_CASE("RgbToHsl: 無彩色(白)はH=0,S=0")
{
	int h, s, l;
	RgbToHsl(clWhite, &h, &s, &l);
	CHECK(h == 0);
	CHECK(s == 0);
	CHECK(l == 100);
}

TEST_CASE("HslToCol: 赤色相からRGBに戻す")
{
	TColor c = HslToCol(0, 100, 50);
	int cref = ColorToRGB(c);
	CHECK(GetRValue(cref) == 255);
	CHECK(GetGValue(cref) == 0);
	CHECK(GetBValue(cref) == 0);
}

//===========================================================================
// RgbToHsv
//===========================================================================
TEST_CASE("RgbToHsv: 赤のHSV値")
{
	int h, s, v;
	RgbToHsv(clRed, &h, &s, &v);
	CHECK(h == 0);
	CHECK(s == 100);
	CHECK(v == 100);
}

TEST_CASE("RgbToHsv(r,g,b,...): バイト直接指定オーバーロード")
{
	int h, s, v;
	RgbToHsv((BYTE)0, (BYTE)255, (BYTE)0, &h, &s, &v);  //緑
	CHECK(h == 120);
	CHECK(s == 100);
	CHECK(v == 100);
}

//===========================================================================
// SelectWorB: 背景輝度に基づく白黒選択
//===========================================================================
TEST_CASE("SelectWorB: 明るい背景には黒、暗い背景には白")
{
	CHECK(SelectWorB(clWhite) == clBlack);
	CHECK(SelectWorB(clBlack) == clWhite);
}

//===========================================================================
// AdjustColor: 明暗の加減
//===========================================================================
TEST_CASE("AdjustColor: 明るい色は暗く、暗い色は明るくなる")
{
	TColor c = AdjustColor(clWhite, 100);  //明→暗方向
	CHECK(ColorToRGB(c) != ColorToRGB(clWhite));
	CHECK(GetRValue(ColorToRGB(c)) < 255);

	TColor c2 = AdjustColor(clBlack, 100);  //暗→明方向
	CHECK(GetRValue(ColorToRGB(c2)) > 0);

	//adj=0 は変化なし
	TColor c3 = AdjustColor(clWhite, 0);
	CHECK(ColorToRGB(c3) == ColorToRGB(clWhite));
}

//===========================================================================
// Mix2Colors: 2色混合
//===========================================================================
TEST_CASE("Mix2Colors: 白と黒の混合はグレー(中間値)")
{
	TColor c = Mix2Colors(clWhite, clBlack);
	int cref = ColorToRGB(c);
	CHECK(GetRValue(cref) == 127);
	CHECK(GetGValue(cref) == 127);
	CHECK(GetBValue(cref) == 127);
}

//===========================================================================
// str_to_Color: 文字列からの色設定
//===========================================================================
TEST_CASE("str_to_Color: 数値文字列を色として設定")
{
	TColor col = clBlack;
	str_to_Color(col, "16711680");  //0x00FF0000 -> clBlue (BGR順のためRGBでは青)
	CHECK(col == 16711680);

	//clNone相当(-1)や不正文字列では変更されない
	TColor col2 = clRed;
	str_to_Color(col2, "abc");  //ToIntDef -> clNone なので変更なし
	CHECK(col2 == clRed);
}
