/**
 * @file tests/core/test_gui_text_display.cpp
 * @brief gui/text_display.h/.cpp (テキスト表示設定のパラメータ解釈) の回帰テスト
 *
 * @details wx に依存しない部分だけをここでテストする (`nyanfi_gui_core`)。
 * VCL 版の該当は `src/MainFrm.cpp` の ShowLineNo/ShowRuler/ShowTAB/ShowCR
 * (33701行〜)、SetTab (34185行)、SetWidth、SetMargin、ScrollUpText/ScrollDownText
 * (24808行)、ViewTail (27615行)。
 */
#include "doctest/doctest.h"

#include "gui/text_display.h"

using namespace text_display;

//===========================================================================
// ToggleValue: VCL SetToggleAction (MainFrm.cpp:12651) と同じ判定
//===========================================================================

TEST_CASE("ToggleValue: 空なら反転する")
{
	CHECK(ToggleValue(false, EmptyStr) == true);
	CHECK(ToggleValue(true, EmptyStr) == false);
}

TEST_CASE("ToggleValue: ON/OFF (大小無視) で決まる")
{
	CHECK(ToggleValue(false, _T("ON")) == true);
	CHECK(ToggleValue(true, _T("ON")) == true);
	CHECK(ToggleValue(false, _T("OFF")) == false);
	CHECK(ToggleValue(true, _T("OFF")) == false);
	CHECK(ToggleValue(false, _T("on")) == true);
	CHECK(ToggleValue(true, _T("off")) == false);
}

//===========================================================================
// ParseTabWidth: VCL SetTabActionExecute (MainFrm.cpp:34185)
//===========================================================================

TEST_CASE("ParseTabWidth: 数値はそのまま、範囲外は丸める")
{
	// VCL は ActionParam.ToIntDef(0) を TabLength に入れる。
	// 空は「入力ボックスを出す」なので純関数では扱わず、呼び出し側で弾く
	CHECK(ParseTabWidth(_T("4")) == 4);
	CHECK(ParseTabWidth(_T("0")) == 0);
	CHECK(ParseTabWidth(_T("abc"), 8) == 8);
	CHECK(ParseTabWidth(_T("-3")) == 0);
	CHECK(ParseTabWidth(_T("99")) == kMaxTabWidth);
}

//===========================================================================
// ParseFoldWidth: VCL SetWidthActionExecute (0でウィンドウ幅)
//===========================================================================

TEST_CASE("ParseFoldWidth: 0はウィンドウ幅追従、範囲外は丸める")
{
	CHECK(ParseFoldWidth(_T("0")) == 0);
	CHECK(ParseFoldWidth(_T("80")) == 80);
	CHECK(ParseFoldWidth(_T("abc"), 80) == 80);
	CHECK(ParseFoldWidth(_T("-5")) == 0);
	CHECK(ParseFoldWidth(_T("99999")) == kMaxFoldWidth);
}

//===========================================================================
// ParseMargin: VCL SetMarginActionExecute
//===========================================================================

TEST_CASE("ParseMargin: 左余白は0〜上限に丸める")
{
	CHECK(ParseMargin(_T("10")) == 10);
	CHECK(ParseMargin(_T("0")) == 0);
	CHECK(ParseMargin(_T("abc"), 10) == 10);
	CHECK(ParseMargin(_T("-1")) == 0);
	CHECK(ParseMargin(_T("999")) == kMaxMargin);
}

//===========================================================================
// ParseScrollLines: VCL ScrollUpTextActionExecute (ListWheelScrLn 既定2)
//===========================================================================

TEST_CASE("ParseScrollLines: 空は既定行、数値は1以上に丸める")
{
	CHECK(ParseScrollLines(EmptyStr, kDefaultScrollLines) == kDefaultScrollLines);
	CHECK(ParseScrollLines(_T("5")) == 5);
	CHECK(ParseScrollLines(_T("0")) == 1);
	CHECK(ParseScrollLines(_T("abc"), 2) == 2);
}

//===========================================================================
// ParseTailParam: VCL ViewTailActionExecute (MainFrm.cpp:27615)
//===========================================================================

TEST_CASE("ParseTailParam: 空は100行、Rで逆順")
{
	// VCL は remove_top_text(ActionParam, "R") で逆順を取り、
	// 残りを ToIntDef(100) で行数にする
	const TailParam d = ParseTailParam(EmptyStr);
	CHECK(d.limit_lines == 100);
	CHECK(d.reverse == false);

	const TailParam r = ParseTailParam(_T("R"));
	CHECK(r.reverse == true);
	CHECK(r.limit_lines == 100);

	const TailParam n = ParseTailParam(_T("50"));
	CHECK(n.limit_lines == 50);
	CHECK(n.reverse == false);

	const TailParam rn = ParseTailParam(_T("R30"));
	CHECK(rn.reverse == true);
	CHECK(rn.limit_lines == 30);
}
