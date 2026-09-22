/**
 * @file tests/core/test_gui_f_batch6_ops.cpp
 * @brief gui/f_batch6_ops.h/.cpp (Fモード残コマンド batch6) の回帰テスト
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の以下 (src 必読済み):
 *          BgImgMode (13784行) / Library (20595行) /
 *          JsonViewer (20266行) / XmlViewer (27874行) /
 *          LoadFindSet (21402行) / SaveAsFindSet (24523行) /
 *          LockTextPreview (21697行) / ShowIndent (33735行) /
 *          FindTagName (19142行) / Grep2 (19407行) /
 *          ExPopupMenu (17223行) / WebMap (34302行) /
 *          PlayList (23604行)。
 *          判断・変換だけを wx 非依存の純関数にしてここで固定する (規約8)。
 */
#include "doctest/doctest.h"

#include "gui/f_batch6_ops.h"

using namespace f_batch6_ops;

//===========================================================================
// HasParamToken
//===========================================================================

TEST_CASE("HasParamToken: ';' 区切り・大小無視の完全一致")
{
	CHECK(HasParamToken(_T("OFF"), _T("OFF")) == true);
	CHECK(HasParamToken(_T("off"), _T("OFF")) == true);
	CHECK(HasParamToken(_T("XX;OFF"), _T("OFF")) == true);
	CHECK(HasParamToken(_T("OFFX"), _T("OFF")) == false);
	CHECK(HasParamToken(_T(""), _T("OFF")) == false);
}

//===========================================================================
// ResolveBgImgMode: BgImgModeActionExecute (13784行)
//===========================================================================

TEST_CASE("ResolveBgImgMode: OFF/1〜3 と ^ 反転・不正は abort")
{
	CHECK(ResolveBgImgMode(1, _T("OFF")).mode == 0);
	CHECK(ResolveBgImgMode(1, _T("OFF")).abort == false);
	CHECK(ResolveBgImgMode(0, _T("1")).mode == 1);
	CHECK(ResolveBgImgMode(0, _T("2")).mode == 2);
	CHECK(ResolveBgImgMode(0, _T("3")).mode == 3);
	// ^付き同値は反転で 0
	CHECK(ResolveBgImgMode(1, _T("^1")).mode == 0);
	CHECK(ResolveBgImgMode(2, _T("^2")).mode == 0);
	// ^付き異値はその値
	CHECK(ResolveBgImgMode(1, _T("^2")).mode == 2);
	// ^無し同値はその値のまま
	CHECK(ResolveBgImgMode(1, _T("1")).mode == 1);
	// 不正は abort
	CHECK(ResolveBgImgMode(0, _T("")).abort == true);
	CHECK(ResolveBgImgMode(0, _T("4")).abort == true);
	CHECK(ResolveBgImgMode(0, _T("ON")).abort == true);
}

//===========================================================================
// ResolveLibraryMode: LibraryActionExecute (20595行)
//===========================================================================

TEST_CASE("ResolveLibraryMode: SD/名前付き/既定")
{
	CHECK(ResolveLibraryMode(_T("SD")) == LibraryMode::ShareDlg);
	CHECK(ResolveLibraryMode(_T("sd")) == LibraryMode::ShareDlg);
	CHECK(ResolveLibraryMode(_T("MyLib")) == LibraryMode::OpenNamed);
	CHECK(ResolveLibraryMode(_T("")) == LibraryMode::OpenDefault);
}

//===========================================================================
// IsClipboardViewer / CanViewFile: JsonViewer (20266行) / XmlViewer (27874行)
//===========================================================================

TEST_CASE("IsClipboardViewer: CB 指定の有無")
{
	CHECK(IsClipboardViewer(_T("CB")) == true);
	CHECK(IsClipboardViewer(_T("cb")) == true);
	CHECK(IsClipboardViewer(_T("")) == false);
}

TEST_CASE("CanViewFile: dummy/dir/無効/無しは不可")
{
	CHECK(CanViewFile(true, false, false, false) == true);
	CHECK(CanViewFile(false, false, false, false) == false);
	CHECK(CanViewFile(true, true, false, false) == false);
	CHECK(CanViewFile(true, false, true, false) == false);
	CHECK(CanViewFile(true, false, false, true) == false);
}

//===========================================================================
// ResolveLoadFindSetSrc / CanSaveFindSet
//===========================================================================

TEST_CASE("ResolveLoadFindSetSrc: */名前付き/ダイアログ")
{
	CHECK(ResolveLoadFindSetSrc(_T("*")) == LoadFindSetSrc::Star);
	CHECK(ResolveLoadFindSetSrc(_T("myfind.ini")) == LoadFindSetSrc::Named);
	CHECK(ResolveLoadFindSetSrc(_T("")) == LoadFindSetSrc::Dialog);
}

TEST_CASE("CanSaveFindSet: find_DUPL 中は不可")
{
	CHECK(CanSaveFindSet(false) == true);
	CHECK(CanSaveFindSet(true) == false);
}

//===========================================================================
// ResolveToggle / ResolveShowIndent: SetToggleAction (12651行)
//===========================================================================

TEST_CASE("ResolveToggle: ON/OFF/反転")
{
	CHECK(ResolveToggle(false, _T("ON")) == true);
	CHECK(ResolveToggle(true, _T("OFF")) == false);
	CHECK(ResolveToggle(false, _T("")) == true);
	CHECK(ResolveToggle(true, _T("")) == false);
}

TEST_CASE("ResolveShowIndent: TVIEW 中は委譲")
{
	const ShowIndentPlan d = ResolveShowIndent(true, false, _T(""));
	CHECK(d.delegate_to_viewer == true);
	const ShowIndentPlan t = ResolveShowIndent(false, false, _T(""));
	CHECK(t.delegate_to_viewer == false);
	CHECK(t.new_value == true);
	const ShowIndentPlan f = ResolveShowIndent(false, true, _T("OFF"));
	CHECK(f.new_value == false);
}

//===========================================================================
// ResolveFindTagPlan: FindTagNameActionExecute (19142行)
//===========================================================================

TEST_CASE("ResolveFindTagPlan: EJ/CO と TVIEW")
{
	CHECK(ResolveFindTagPlan(_T("EJ"), false).tag_cmd == _T("EDIT"));
	CHECK(ResolveFindTagPlan(_T(""), false).tag_cmd == _T("VIEW"));
	CHECK(ResolveFindTagPlan(_T("ej"), false).tag_cmd == _T("EDIT"));
	CHECK(ResolveFindTagPlan(_T("CO"), true).use_current_file == true);
	CHECK(ResolveFindTagPlan(_T("CO"), false).use_current_file == false);
	CHECK(ResolveFindTagPlan(_T(""), true).use_current_file == false);
}

//===========================================================================
// ValidateGrep2: Grep2ActionExecute (19407行)
//===========================================================================

TEST_CASE("ValidateGrep2: 書庫/FTP/検索Dir/grep未設定は不可")
{
	UnicodeString err;
	CHECK(ValidateGrep2(false, false, false, false, true, err) == true);
	CHECK(ValidateGrep2(true, false, false, false, true, err) == false);
	CHECK(ValidateGrep2(false, true, false, false, true, err) == false);
	CHECK(ValidateGrep2(false, false, true, true, true, err) == false);
	// 検索中でも Dir でなければ可
	CHECK(ValidateGrep2(false, false, true, false, true, err) == true);
	CHECK(ValidateGrep2(false, false, false, false, false, err) == false);
	CHECK(err.IsEmpty() == false);
}

//===========================================================================
// ResolveExPopupTarget: ExPopupMenuActionExecute (17223行)
//===========================================================================

TEST_CASE("ResolveExPopupTarget: MN/TL/両方")
{
	const ExPopupTarget mn = ResolveExPopupTarget(_T("MN"));
	CHECK(mn.menu == true);
	CHECK(mn.tool == false);
	const ExPopupTarget tl = ResolveExPopupTarget(_T("TL"));
	CHECK(tl.menu == false);
	CHECK(tl.tool == true);
	const ExPopupTarget both = ResolveExPopupTarget(_T(""));
	CHECK(both.menu == true);
	CHECK(both.tool == true);
}

//===========================================================================
// ParseWebMapParams / ParseLatLng: WebMapActionExecute (34302行)
//===========================================================================

TEST_CASE("ParseWebMapParams: M/Z と既定値")
{
	const WebMapPlan dflt = ParseWebMapParams(_T(""));
	CHECK(dflt.ok == true);
	CHECK(dflt.map_idx == 0);
	CHECK(dflt.zoom == 16);

	const WebMapPlan mz = ParseWebMapParams(_T("M2;Z10"));
	CHECK(mz.ok == true);
	CHECK(mz.map_idx == 2);
	CHECK(mz.zoom == 10);

	// IN は取り除かれる
	const WebMapPlan in = ParseWebMapParams(_T("IN;Z12"));
	CHECK(in.ok == true);
	CHECK(in.zoom == 12);

	// 先頭 MZ 以外・空・0 は abort
	CHECK(ParseWebMapParams(_T("X1")).ok == false);
	CHECK(ParseWebMapParams(_T("M0")).ok == false);
	CHECK(ParseWebMapParams(_T("Z")).ok == false);
	CHECK(ParseWebMapParams(_T("m1")).ok == true);  // 大文字化される
}

TEST_CASE("ParseLatLng: 区切りの優先順 (, TAB ; 空白)")
{
	double lat = 0.0, lng = 0.0;
	CHECK(ParseLatLng(_T("35.68,139.69"), lat, lng) == true);
	CHECK(lat == doctest::Approx(35.68));
	CHECK(lng == doctest::Approx(139.69));
	CHECK(ParseLatLng(_T("35.68 139.69"), lat, lng) == true);
	CHECK(lat == doctest::Approx(35.68));
	CHECK(ParseLatLng(_T("35.68;139.69"), lat, lng) == true);
	CHECK(lng == doctest::Approx(139.69));
	CHECK(ParseLatLng(_T("35.68"), lat, lng) == false);
	CHECK(ParseLatLng(_T(""), lat, lng) == false);
	CHECK(ParseLatLng(_T("abc,def"), lat, lng) == false);
}

//===========================================================================
// ResolvePlayListSub: PlayListActionExecute (23604行)
//===========================================================================

TEST_CASE("ResolvePlayListSub: NX/PR/PS/RS/PP/CA/FI と設定")
{
	CHECK(ResolvePlayListSub(_T("NX")) == PlayListSub::Next);
	CHECK(ResolvePlayListSub(_T("PR")) == PlayListSub::Prev);
	CHECK(ResolvePlayListSub(_T("PS")) == PlayListSub::Pause);
	CHECK(ResolvePlayListSub(_T("RS")) == PlayListSub::Resume);
	CHECK(ResolvePlayListSub(_T("PP")) == PlayListSub::PlayPause);
	CHECK(ResolvePlayListSub(_T("CA")) == PlayListSub::ClearAll);
	CHECK(ResolvePlayListSub(_T("FI")) == PlayListSub::FileInfo);
	CHECK(ResolvePlayListSub(_T("")) == PlayListSub::Setup);
	CHECK(ResolvePlayListSub(_T("nx")) == PlayListSub::Next);
}
