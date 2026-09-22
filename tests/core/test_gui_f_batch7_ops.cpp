/**
 * @file tests/core/test_gui_f_batch7_ops.cpp
 * @brief gui/f_batch7_ops.h/.cpp (Fモード残コマンド batch7) の回帰テスト
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の以下 (src 必読済み):
 *          AppList (13499行) / DebugCmdFile (16308行) /
 *          DistributionDlg (16369行) / DotNyanDlg (16792行) /
 *          ExeCommands (17118行) / ExeMenuFile (24017行) /
 *          ExeToolBtn (17175行) / ExtractChmSrc (17254行) /
 *          ExtractGifBmp (17290行) / LockKeyMouse (21668行) /
 *          RegExChecker (24175行) / ToolBarDlg (36789行) /
 *          UpdateFromArc (30205行)。
 *          判断・変換だけを wx 非依存の純関数にしてここで固定する (規約8)。
 */
#include "doctest/doctest.h"

#include "gui/f_batch7_ops.h"

using namespace f_batch7_ops;

//===========================================================================
// HasParamToken
//===========================================================================

TEST_CASE("HasParamToken: ';' 区切り・大小無視の完全一致")
{
	CHECK(HasParamToken(_T("AO"), _T("AO")) == true);
	CHECK(HasParamToken(_T("ao"), _T("AO")) == true);
	CHECK(HasParamToken(_T("XX;AO"), _T("AO")) == true);
	CHECK(HasParamToken(_T("AOX"), _T("AO")) == false);
	CHECK(HasParamToken(_T(""), _T("AO")) == false);
}

//===========================================================================
// ParseAppListOpts: AppListActionExecute (13499行)
//===========================================================================

TEST_CASE("ParseAppListOpts: 各フラグの解決")
{
	AppListOpts o = ParseAppListOpts(_T(""));
	CHECK(o.only_app == false);
	CHECK(o.only_launcher == false);
	CHECK(o.fuzzy == false);

	o = ParseAppListOpts(_T("AO"));
	CHECK(o.only_app == true);

	// LO または LI でランチャーのみ
	CHECK(ParseAppListOpts(_T("LO")).only_launcher == true);
	CHECK(ParseAppListOpts(_T("LI")).only_launcher == true);
	// FI または LI でインクリメンタル検索へ
	CHECK(ParseAppListOpts(_T("FI")).to_incsea == true);
	CHECK(ParseAppListOpts(_T("LI")).to_incsea == true);
	CHECK(ParseAppListOpts(_T("")).to_incsea == false);

	CHECK(ParseAppListOpts(_T("FA")).to_app == true);
	CHECK(ParseAppListOpts(_T("FL")).to_launcher == true);
	CHECK(ParseAppListOpts(_T("FZ")).fuzzy == true);
	CHECK(ParseAppListOpts(_T("AS")).add_start == true);
}

//===========================================================================
// ResolveDebugCmdSrc: DebugCmdFileActionExecute (16308行)
//===========================================================================

TEST_CASE("ResolveDebugCmdSrc: パラメータ優先・無ければカーソル位置")
{
	// パラメータ有りならそれを使う
	CHECK(ResolveDebugCmdSrc(true, false, false, false) == DebugCmdSrc::Param);
	// 空でも FLIST 中に .nbt カーソルがあればカーソル位置
	CHECK(ResolveDebugCmdSrc(false, true, true, true) == DebugCmdSrc::Cursor);
	// .nbt でなければ不可
	CHECK(ResolveDebugCmdSrc(false, true, true, false) == DebugCmdSrc::None);
	// FLIST 外では不可
	CHECK(ResolveDebugCmdSrc(false, false, true, true) == DebugCmdSrc::None);
	// カーソル無しでは不可
	CHECK(ResolveDebugCmdSrc(false, true, false, true) == DebugCmdSrc::None);
}

//===========================================================================
// ResolveDistributionMode: DistributionDlgActionExecute (16369行)
//===========================================================================

TEST_CASE("ResolveDistributionMode: XC/XM/SN の優先順")
{
	CHECK(ResolveDistributionMode(_T("XC")) == DistributionMode::ImmediateCopy);
	CHECK(ResolveDistributionMode(_T("XM")) == DistributionMode::ImmediateMove);
	CHECK(ResolveDistributionMode(_T("SN")) == DistributionMode::SetMask);
	CHECK(ResolveDistributionMode(_T("")) == DistributionMode::Dialog);
	// XC が最優先
	CHECK(ResolveDistributionMode(_T("XM;XC")) == DistributionMode::ImmediateCopy);
}

TEST_CASE("IsDistrIniFile: .ini 指定の有無")
{
	CHECK(IsDistrIniFile(_T("distr.ini")) == true);
	CHECK(IsDistrIniFile(_T("DISTR.INI")) == true);
	CHECK(IsDistrIniFile(_T("distr.txt")) == false);
	CHECK(IsDistrIniFile(_T("")) == false);
}

TEST_CASE("CanUseDistributionDlg: 特殊リストでは不可")
{
	CHECK(CanUseDistributionDlg(false, false, false, false) == true);
	CHECK(CanUseDistributionDlg(true, false, false, false) == false);  // 書庫
	CHECK(CanUseDistributionDlg(false, true, false, false) == false);  // ADS
	CHECK(CanUseDistributionDlg(false, false, true, false) == false);  // Work
	CHECK(CanUseDistributionDlg(false, false, false, true) == false);  // FTP
}

//===========================================================================
// ResolveDotNyanMode: DotNyanDlgActionExecute (16792行)
//===========================================================================

TEST_CASE("ResolveDotNyanMode: RS/ダイアログ/対象外")
{
	CHECK(ResolveDotNyanMode(true, _T("RS")) == DotNyanMode::Reapply);
	CHECK(ResolveDotNyanMode(true, _T("")) == DotNyanMode::Dialog);
	CHECK(ResolveDotNyanMode(false, _T("")) == DotNyanMode::Denied);
	CHECK(ResolveDotNyanMode(false, _T("RS")) == DotNyanMode::Denied);
}

//===========================================================================
// ExeCommands / ExeMenuFile (17118/24017行)
//===========================================================================

TEST_CASE("CanExeCommands: 空パラメータは不可")
{
	CHECK(CanExeCommands(_T("@cmds.txt")) == true);
	CHECK(CanExeCommands(_T("")) == false);
}

TEST_CASE("CanExeMenuFile: 空パラメータは不可")
{
	CHECK(CanExeMenuFile(_T("menu.txt")) == true);
	CHECK(CanExeMenuFile(_T("")) == false);
}

//===========================================================================
// ResolveToolBtnIndex: ExeToolBtnActionExecute (17175行)
//===========================================================================

TEST_CASE("ResolveToolBtnIndex: 1 始まり・範囲外は -1")
{
	CHECK(ResolveToolBtnIndex(_T("1"), 3) == 0);
	CHECK(ResolveToolBtnIndex(_T("3"), 3) == 2);
	CHECK(ResolveToolBtnIndex(_T("0"), 3) == -1);
	CHECK(ResolveToolBtnIndex(_T("4"), 3) == -1);
	CHECK(ResolveToolBtnIndex(_T("x"), 3) == -1);
	CHECK(ResolveToolBtnIndex(_T(""), 3) == -1);
	CHECK(ResolveToolBtnIndex(_T("1"), 0) == -1);
}

//===========================================================================
// ValidateExtractChmSrc: ExtractChmSrcActionExecute (17254行)
//===========================================================================

TEST_CASE("ValidateExtractChmSrc: ガード条件")
{
	UnicodeString err;
	CHECK(ValidateExtractChmSrc(false, false, true, true, false, _T(".chm"),
	                            false, true, false, false, err) == true);
	// ADS/FTP/反対側非リストは不可
	CHECK(ValidateExtractChmSrc(true, false, true, true, false, _T(".chm"),
	                            false, true, false, false, err) == false);
	CHECK(ValidateExtractChmSrc(false, true, true, true, false, _T(".chm"),
	                            false, true, false, false, err) == false);
	CHECK(ValidateExtractChmSrc(false, false, false, true, false, _T(".chm"),
	                            false, true, false, false, err) == false);
	// カーソル無し・ディレクトリ・非chm は不可
	CHECK(ValidateExtractChmSrc(false, false, true, false, false, _T(".chm"),
	                            false, true, false, false, err) == false);
	CHECK(ValidateExtractChmSrc(false, false, true, true, true, _T(".chm"),
	                            false, true, false, false, err) == false);
	CHECK(ValidateExtractChmSrc(false, false, true, true, false, _T(".txt"),
	                            false, true, false, false, err) == false);
	// 仮想で一時展開に失敗したら不可
	CHECK(ValidateExtractChmSrc(false, false, true, true, false, _T(".chm"),
	                            true, false, false, false, err) == false);
	// 空白を含む名前は未対応
	CHECK(ValidateExtractChmSrc(false, false, true, true, false, _T(".chm"),
	                            false, true, true, false, err) == false);
	CHECK(ValidateExtractChmSrc(false, false, true, true, false, _T(".chm"),
	                            false, true, false, true, err) == false);
	// .CHM (大文字) も可
	CHECK(ValidateExtractChmSrc(false, false, true, true, false, _T(".CHM"),
	                            false, true, false, false, err) == true);
}

//===========================================================================
// ValidateExtractGif: ExtractGifBmpActionExecute (17290行)
//===========================================================================

TEST_CASE("ValidateExtractGif: 選択優先・無ければカーソル位置")
{
	UnicodeString err;
	// 選択あり・全て gif なら可
	CHECK(ValidateExtractGif(true, true, false, err) == true);
	// 選択あり・非gif混じりは不可
	CHECK(ValidateExtractGif(true, false, true, err) == false);
	// 選択無し・カーソルが gif なら可
	CHECK(ValidateExtractGif(false, true, true, err) == true);
	// 選択無し・カーソルが非gif は不可
	CHECK(ValidateExtractGif(false, true, false, err) == false);
}

//===========================================================================
// ValidateLockWord: LockKeyMouseActionExecute (21668行)
//===========================================================================

TEST_CASE("ValidateLockWord: 空または英数字のみ")
{
	UnicodeString err;
	CHECK(ValidateLockWord(_T(""), err) == true);
	CHECK(ValidateLockWord(_T("abc123"), err) == true);
	CHECK(ValidateLockWord(_T("ABC"), err) == true);
	CHECK(ValidateLockWord(_T("abc-123"), err) == false);
	CHECK(ValidateLockWord(_T("a b"), err) == false);
	CHECK(ValidateLockWord(_T("a@b"), err) == false);
}

//===========================================================================
// RegExChecker / ToolBarDlg (24175/36789行)
//===========================================================================

TEST_CASE("ShouldShowRegExChecker: 非表示のときだけ開く")
{
	CHECK(ShouldShowRegExChecker(false) == true);
	CHECK(ShouldShowRegExChecker(true) == false);
}

TEST_CASE("IsToolBarDlgEnabled: 一覧系かつプライマリのときだけ有効")
{
	CHECK(IsToolBarDlgEnabled(true, true) == true);
	CHECK(IsToolBarDlgEnabled(true, false) == false);
	CHECK(IsToolBarDlgEnabled(false, true) == false);
}

//===========================================================================
// ResolveUpdateFromArcSrc: UpdateFromArcActionExecute (30205行)
//===========================================================================

TEST_CASE("ResolveUpdateFromArcSrc: UN/選択")
{
	CHECK(ResolveUpdateFromArcSrc(_T("UN")) == UpdateFromArcSrc::Newest);
	CHECK(ResolveUpdateFromArcSrc(_T("un")) == UpdateFromArcSrc::Newest);
	CHECK(ResolveUpdateFromArcSrc(_T("")) == UpdateFromArcSrc::Dialog);
}
