/**
 * @file tests/core/test_gui_f_batch5_ops.cpp
 * @brief gui/f_batch5_ops.h/.cpp (Fモード残コマンド batch5) の回帰テスト
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の以下 (src 必読済み):
 *          ExeExtMenu (17125行) / ExeExtTool (17150行) /
 *          RegDirPopup (24167行) / PathMaskDlg (23553行) /
 *          BinaryEdit (13799行) / EditHighlight (16892行) /
 *          FixedLen (33660行) / HtmlToText (33643行) /
 *          SetColor (34202行) / ShowRuby (33786行) /
 *          ListDuration (20722行) / ListExpFunc (20837行) /
 *          WatchTail (34217行)。
 *          判断・変換だけを wx 非依存の純関数にしてここで固定する (規約8)。
 */
#include "doctest/doctest.h"

#include "gui/f_batch5_ops.h"

using namespace f_batch5_ops;

//===========================================================================
// HasParamToken
//===========================================================================

TEST_CASE("HasParamToken: ';' 区切り・大小無視の完全一致")
{
	CHECK(HasParamToken(_T("OP"), _T("OP")) == true);
	CHECK(HasParamToken(_T("op"), _T("OP")) == true);
	CHECK(HasParamToken(_T("XX;OP"), _T("OP")) == true);
	CHECK(HasParamToken(_T("OPP"), _T("OP")) == false);
	CHECK(HasParamToken(_T(""), _T("OP")) == false);
}

//===========================================================================
// ResolveAccKeyIndex: get_IndexFromAccKey (UserFunc.cpp:1196)
//===========================================================================

TEST_CASE("ResolveAccKeyIndex: 1文字のアクセスキー解決")
{
	const std::vector<UnicodeString> cols = {
		UnicodeString(_T("&File操作")),
		UnicodeString(_T("&Edit編集")),
		UnicodeString(_T(">サブメニュー")),
	};
	CHECK(ResolveAccKeyIndex(cols, _T("F")) == 0);
	CHECK(ResolveAccKeyIndex(cols, _T("f")) == 0);
	CHECK(ResolveAccKeyIndex(cols, _T("E")) == 1);
	CHECK(ResolveAccKeyIndex(cols, _T("Z")) == -1);
	CHECK(ResolveAccKeyIndex(cols, _T("")) == -1);
	CHECK(ResolveAccKeyIndex(cols, _T("Fi")) == -1);
	CHECK(ResolveAccKeyIndex({}, _T("F")) == -1);
}

TEST_CASE("IsSubMenuHead: '>' で始まればサブメニュー")
{
	CHECK(IsSubMenuHead(_T(">項目")) == true);
	CHECK(IsSubMenuHead(_T("通常")) == false);
	CHECK(IsSubMenuHead(_T("")) == false);
}

//===========================================================================
// RegDirPopup / PathMaskDlg
//===========================================================================

TEST_CASE("ResolveRegDirPopupTarget: OP なら OppDir")
{
	CHECK(ResolveRegDirPopupTarget(_T("")) == _T("RegDir"));
	CHECK(ResolveRegDirPopupTarget(_T("OP")) == _T("OppDir"));
	CHECK(ResolveRegDirPopupTarget(_T("op")) == _T("OppDir"));
	CHECK(ResolveRegDirPopupTarget(_T("XX")) == _T("RegDir"));
}

TEST_CASE("ResolvePathMaskMode: ND ならポップアップ、Find/Work 中は不可")
{
	CHECK(ResolvePathMaskMode(_T(""), false, false) == PathMaskMode::Dialog);
	CHECK(ResolvePathMaskMode(_T("ND"), false, false) == PathMaskMode::Popup);
	CHECK(ResolvePathMaskMode(_T("nd"), false, false) == PathMaskMode::Popup);
	CHECK(ResolvePathMaskMode(_T(""), true, false) == PathMaskMode::Denied);
	CHECK(ResolvePathMaskMode(_T(""), false, true) == PathMaskMode::Denied);
	CHECK(ResolvePathMaskMode(_T("ND"), true, false) == PathMaskMode::Denied);
}

//===========================================================================
// BinaryEdit / EditHighlight
//===========================================================================

TEST_CASE("ResolveBinaryEditTarget: 選択優先、無ければカーソル位置")
{
	CHECK(ResolveBinaryEditTarget(true, _T("a;b"), _T("cur")) == _T("a;b"));
	CHECK(ResolveBinaryEditTarget(false, _T("a;b"), _T("cur")) == _T("cur"));
}

TEST_CASE("ValidateBinaryEditor: 空は不可")
{
	UnicodeString err;
	CHECK(ValidateBinaryEditor(_T(""), err) == false);
	CHECK(err.IsEmpty() == false);
}

TEST_CASE("BuildEditHighlightCommand: FileEdit_\"定義\" 形式")
{
	CHECK(BuildEditHighlightCommand(_T("C:\\h\\a.ini")) == _T("FileEdit_\"C:\\h\\a.ini\""));
}

//===========================================================================
// FixedLen / HtmlToText / SetColor / ShowRuby
//===========================================================================

TEST_CASE("ResolveFixedLen: TVIEW 中は委譲、数値指定は上限化して ON")
{
	const FixedLenPlan d = ResolveFixedLen(true, false, _T(""));
	CHECK(d.delegate_to_viewer == true);

	const FixedLenPlan t = ResolveFixedLen(false, false, _T(""));
	CHECK(t.delegate_to_viewer == false);
	CHECK(t.new_enabled == true);
	CHECK(t.limit == 0);

	const FixedLenPlan off = ResolveFixedLen(false, true, _T("OFF"));
	CHECK(off.new_enabled == false);

	const FixedLenPlan n = ResolveFixedLen(false, false, _T("80"));
	CHECK(n.limit == 80);
	CHECK(n.new_enabled == true);

	// 最低 4 (VCL std::max(lmt, 4) と同じ)
	const FixedLenPlan small = ResolveFixedLen(false, false, _T("2"));
	CHECK(small.limit == 4);
	CHECK(small.new_enabled == true);
}

TEST_CASE("ParseHtmlToText: ON/OFF/MD/^MD/TX の ';' 区切り")
{
	const HtmlToTextPlan d = ParseHtmlToText(_T(""), true);
	CHECK(d.delegate_to_viewer == true);

	const HtmlToTextPlan on = ParseHtmlToText(_T("ON"), false);
	CHECK(on.delegate_to_viewer == false);
	CHECK(on.toggle_param == _T("ON"));
	CHECK(on.markdown == MarkdownMode::Keep);

	const HtmlToTextPlan md = ParseHtmlToText(_T("MD"), false);
	CHECK(md.markdown == MarkdownMode::ToMarkdown);

	const HtmlToTextPlan tmd = ParseHtmlToText(_T("^MD"), false);
	CHECK(tmd.markdown == MarkdownMode::Toggle);

	const HtmlToTextPlan tx = ParseHtmlToText(_T("TX"), false);
	CHECK(tx.markdown == MarkdownMode::ToText);

	const HtmlToTextPlan mix = ParseHtmlToText(_T("OFF;MD"), false);
	CHECK(mix.toggle_param == _T("OFF"));
	CHECK(mix.markdown == MarkdownMode::ToMarkdown);
}

TEST_CASE("ResolveSetColorMode: 空ならダイアログ、有りならファイル指定")
{
	CHECK(ResolveSetColorMode(_T("")) == SetColorMode::Dialog);
	CHECK(ResolveSetColorMode(_T("C:\\c\\a.col")) == SetColorMode::FromFile);
}

TEST_CASE("IsShowRubyViewerDelegate/ToggleEnabled: TVIEW 中は委譲、他は反転")
{
	CHECK(IsShowRubyViewerDelegate(true) == true);
	CHECK(IsShowRubyViewerDelegate(false) == false);
	CHECK(ToggleEnabled(false, _T("")) == true);
	CHECK(ToggleEnabled(true, _T("")) == false);
	CHECK(ToggleEnabled(false, _T("ON")) == true);
	CHECK(ToggleEnabled(true, _T("OFF")) == false);
}

//===========================================================================
// ListDuration
//===========================================================================

TEST_CASE("ValidateListDuration: 書庫/FTP 不可、選択必須")
{
	UnicodeString err;
	CHECK(ValidateListDuration(false, false, 3, err) == true);
	CHECK(ValidateListDuration(true, false, 3, err) == false);
	CHECK(ValidateListDuration(false, true, 3, err) == false);
	CHECK(ValidateListDuration(false, false, 0, err) == false);
	CHECK(err.IsEmpty() == false);
}

TEST_CASE("FormatDurationMs: HH:MM:SS.cc / HH:MM:SS")
{
	CHECK(FormatDurationMs(0, true) == _T("00:00:00.00"));
	CHECK(FormatDurationMs(61000, true) == _T("00:01:01.00"));
	CHECK(FormatDurationMs(3661000, false) == _T("01:01:01"));
}

TEST_CASE("FormatDurationRow/Total: 一覧の行書式")
{
	CHECK(FormatDurationRow(_T("a.mp3"), 61000, true) == _T("a.mp3\t 00:01:01.00"));
	const UnicodeString total = FormatDurationTotal(61000, true, 2, 0);
	CHECK(total.Pos(_T("合計")) > 0);
	CHECK(total.Pos(_T("2")) > 0);
	const UnicodeString with_err = FormatDurationTotal(61000, true, 2, 1);
	CHECK(with_err.Pos(_T("ERR:1")) > 0);
}

//===========================================================================
// ListExpFunc
//===========================================================================

TEST_CASE("ParseExpFuncSort: SI=1/SR=2/SN=3、無指定=0")
{
	CHECK(ParseExpFuncSort(_T("")) == 0);
	CHECK(ParseExpFuncSort(_T("SI")) == 1);
	CHECK(ParseExpFuncSort(_T("SR")) == 2);
	CHECK(ParseExpFuncSort(_T("SN")) == 3);
	// VCL の連鎖三項と同じ優先順位 (SI が先)
	CHECK(ParseExpFuncSort(_T("SI;SR")) == 1);
}

TEST_CASE("ParseExpFuncListMode: .csv=1/.tsv=2")
{
	CHECK(ParseExpFuncListMode(_T("")) == 0);
	CHECK(ParseExpFuncListMode(_T("out.csv")) == 1);
	CHECK(ParseExpFuncListMode(_T("out.tsv")) == 2);
}

//===========================================================================
// WatchTail
//===========================================================================

TEST_CASE("ParseWatchTailSubCmd: AC/ST/CC/既定")
{
	CHECK(ParseWatchTailSubCmd(_T("")) == WatchTailSubCmd::Watch);
	CHECK(ParseWatchTailSubCmd(_T("AC")) == WatchTailSubCmd::AllCancel);
	CHECK(ParseWatchTailSubCmd(_T("ac")) == WatchTailSubCmd::AllCancel);
	CHECK(ParseWatchTailSubCmd(_T("ST")) == WatchTailSubCmd::Status);
	CHECK(ParseWatchTailSubCmd(_T("CC")) == WatchTailSubCmd::CancelOne);
	CHECK(ParseWatchTailSubCmd(_T("keyword")) == WatchTailSubCmd::Watch);
}
