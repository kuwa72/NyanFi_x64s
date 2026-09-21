/**
 * @file tests/core/test_gui_panel_state.cpp
 * @brief gui/panel_state.h/.cpp (パネル表示切替・ログスクロール) の回帰テスト
 *
 * @details wx に依存しない部分だけをここでテストする (`nyanfi_gui_core`)。
 * VCL 版の該当は `src/MainFrm.cpp` の ShowIcon (26006行、AC/FD 付き)、
 * ScrollUpLog/ScrollDownLog (24774/24784行)、SetSubSize (25729行)。
 * ShowPreview/ShowProperty/ShowFKeyBar/ShowToolBar/MenuBar の ON/OFF 反転は
 * 既存の view_settings::ParseToggle/ApplyToggle・text_display::ToggleValue を
 * そのまま使うためここでは扱わない。
 */
#include "doctest/doctest.h"

#include "gui/panel_state.h"

using namespace panel_state;

//===========================================================================
// HasToken: VCL TestActionParam (MainFrm.cpp:12515) と同じ (';' 区切り・大小無視)
//===========================================================================

TEST_CASE("HasToken: 空・単一・複数トークン")
{
	CHECK(HasToken(EmptyStr, _T("AC")) == false);
	CHECK(HasToken(_T("AC"), _T("AC")) == true);
	CHECK(HasToken(_T("ac"), _T("AC")) == true);
	CHECK(HasToken(_T("FD;AC"), _T("AC")) == true);
	CHECK(HasToken(_T("FD;AC"), _T("FD")) == true);
	CHECK(HasToken(_T("FD;AC"), _T("XX")) == false);
	// 部分一致は不可 (完全一致のみ)
	CHECK(HasToken(_T("ACC"), _T("AC")) == false);
}

//===========================================================================
// NextIconModeFD: VCL ShowIconActionExecute の FD 分岐 (MainFrm.cpp:26014-26016)
//===========================================================================

TEST_CASE("NextIconModeFD: 0->1->2->1 の循環")
{
	// IconMode = (IconMode==0)? 1 : (IconMode==1)? 2 : 1
	CHECK(NextIconModeFD(0) == 1);
	CHECK(NextIconModeFD(1) == 2);
	CHECK(NextIconModeFD(2) == 1);
}

//===========================================================================
// ToggleIconMode: 通常トグル (MainFrm.cpp:26017-26021)
//===========================================================================

TEST_CASE("ToggleIconMode: ON/OFF/反転で 0/1 が決まる")
{
	// bool sw = (IconMode>0); SetToggleAction(sw); IconMode = sw? 1 : 0
	// モード2 (詳細) も「表示中」として扱い、反転では 0 へ・ON では 1 へ丸める
	CHECK(ToggleIconMode(0, view_settings::Toggle::Flip) == 1);
	CHECK(ToggleIconMode(1, view_settings::Toggle::Flip) == 0);
	CHECK(ToggleIconMode(2, view_settings::Toggle::Flip) == 0);
	CHECK(ToggleIconMode(2, view_settings::Toggle::On) == 1);
	CHECK(ToggleIconMode(1, view_settings::Toggle::Off) == 0);
	CHECK(ToggleIconMode(0, view_settings::Toggle::Off) == 0);
}

//===========================================================================
// ScrollLogIndex: ログスクロール後の注目行 (ListBoxScrollUp/Down 相当)
//===========================================================================

TEST_CASE("ScrollLogIndex: 空なら -1、範囲内に丸める")
{
	CHECK(ScrollLogIndex(-1, 0, 2, true) == -1);
	CHECK(ScrollLogIndex(0, 5, 2, true) == 2);
	CHECK(ScrollLogIndex(0, 5, 2, false) == 0);
	CHECK(ScrollLogIndex(4, 5, 2, true) == 4);
	CHECK(ScrollLogIndex(1, 5, 9, false) == 0);
	CHECK(ScrollLogIndex(-1, 5, 2, true) == 1);
}
