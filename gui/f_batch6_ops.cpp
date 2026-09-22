/**
 * @file gui/f_batch6_ops.cpp
 * @brief Fモード残コマンド batch6 の判断ロジックの実装 (wx 非依存)
 */
#include "gui/f_batch6_ops.h"

#include "compat/exception.h"
#include "usr_str.h"

namespace f_batch6_ops {

bool HasParamToken(const UnicodeString &param, const UnicodeString &token)
{
	if (param.IsEmpty() || token.IsEmpty()) return false;
	TStringDynArray toks = split_strings_semicolon(param);
	for (int i = 0; i < toks.Length; i++) {
		if (SameText(toks[i], token)) return true;
	}
	return false;
}

BgImgModePlan ResolveBgImgMode(int cur_mode, const UnicodeString &param)
{
	// VCL remove_top_s(ActionParam, '^'): 先頭の ^ を取り除いて反転指定にする
	UnicodeString p = param;
	const bool x_sw = remove_top_s(p, _T("^"));
	BgImgModePlan plan;
	if (SameText(p, _T("OFF"))) {
		plan.mode = 0;
	}
	else if (SameText(p, _T("1")) || SameText(p, _T("2")) || SameText(p, _T("3"))) {
		const int v = p.ToIntDef(0);
		plan.mode = (x_sw && cur_mode == v) ? 0 : v;
	}
	else {
		plan.abort = true;
	}
	return plan;
}

LibraryMode ResolveLibraryMode(const UnicodeString &param)
{
	if (HasParamToken(param, _T("SD"))) return LibraryMode::ShareDlg;
	if (!param.IsEmpty()) return LibraryMode::OpenNamed;
	return LibraryMode::OpenDefault;
}

LoadFindSetSrc ResolveLoadFindSetSrc(const UnicodeString &param)
{
	if (SameStr(param, _T("*"))) return LoadFindSetSrc::Star;
	if (!param.IsEmpty()) return LoadFindSetSrc::Named;
	return LoadFindSetSrc::Dialog;
}

ShowIndentPlan ResolveShowIndent(bool is_tview, bool cur, const UnicodeString &param)
{
	ShowIndentPlan plan;
	if (is_tview) {
		plan.delegate_to_viewer = true;
		plan.new_value = cur;
		return plan;
	}
	plan.new_value = ResolveToggle(cur, param);
	return plan;
}

FindTagPlan ResolveFindTagPlan(const UnicodeString &param, bool is_tview)
{
	FindTagPlan plan;
	plan.tag_cmd = HasParamToken(param, _T("EJ")) ? UnicodeString(_T("EDIT"))
	                                             : UnicodeString(_T("VIEW"));
	plan.use_current_file = is_tview && HasParamToken(param, _T("CO"));
	return plan;
}

bool ValidateGrep2(bool is_arc, bool is_ftp, bool is_find, bool find_dir,
                   bool grep_exists, UnicodeString &error_out)
{
	if (is_arc || is_ftp) {
		error_out = UnicodeString(_T("書庫・FTP では実行できません"));
		return false;
	}
	if (is_find && find_dir) {
		error_out = UnicodeString(_T("検索中のディレクトリでは実行できません"));
		return false;
	}
	if (!grep_exists) {
		error_out = UnicodeString(_T("grep.exe が設定されていません"));
		return false;
	}
	return true;
}

ExPopupTarget ResolveExPopupTarget(const UnicodeString &param)
{
	ExPopupTarget target;
	if (HasParamToken(param, _T("MN"))) {
		target.menu = true;
		target.tool = false;
	}
	else if (HasParamToken(param, _T("TL"))) {
		target.menu = false;
		target.tool = true;
	}
	return target;
}

WebMapPlan ParseWebMapParams(const UnicodeString &param)
{
	WebMapPlan plan;
	plan.ok = true;
	if (param.IsEmpty()) return plan;

	TStringDynArray toks = split_strings_semicolon(param);
	for (int i = 0; i < toks.Length; i++) {
		UnicodeString lbuf = toks[i];
		// TestDelActionParam("IN") 相当: IN 指定は取り除く
		if (SameText(lbuf, _T("IN"))) continue;
		lbuf = lbuf.UpperCase();
		if (lbuf.IsEmpty() || !(lbuf[1] == L'M' || lbuf[1] == L'Z')) {
			plan.ok = false;
			plan.error = UnicodeString(_T("パラメータが不正です"));
			return plan;
		}
		const WideChar c = lbuf[1];
		lbuf.Delete(1, 1);
		if (lbuf.IsEmpty()) {
			plan.ok = false;
			plan.error = UnicodeString(_T("パラメータが不正です"));
			return plan;
		}
		const int v = lbuf.ToIntDef(0);
		if (v == 0) {
			plan.ok = false;
			plan.error = UnicodeString(_T("パラメータが不正です"));
			return plan;
		}
		if (c == L'M')
			plan.map_idx = v;
		else
			plan.zoom = v;
	}
	return plan;
}

bool ParseLatLng(const UnicodeString &text, double &lat, double &lng)
{
	// VCL と同じ優先順で区切りを探す (Pos は1始まり。>1 は「先頭以外にある」)
	UnicodeString sc;
	if (text.Pos(_T(",")) > 1)
		sc = _T(",");
	else if (text.Pos(_T("\t")) > 1)
		sc = _T("\t");
	else if (text.Pos(_T(";")) > 1)
		sc = _T(";");
	else if (text.Pos(_T(" ")) > 1)
		sc = _T(" ");
	else
		return false;

	UnicodeString buf = text;
	UnicodeString lat_str = Trim(split_tkn(buf, sc));
	buf = Trim(buf);
	if (lat_str.IsEmpty() || buf.IsEmpty()) return false;
	try {
		lat = lat_str.ToDouble();
		lng = buf.ToDouble();
	}
	catch (EConvertError &) {
		return false;
	}
	return true;
}

UnicodeString BuildMapUrl(double lat, double lng, int zoom)
{
	UnicodeString url;
	url.sprintf(_T("https://www.google.com/maps/@%.6f,%.6f,%dz"), lat, lng, zoom);
	return url;
}

PlayListSub ResolvePlayListSub(const UnicodeString &param)
{
	if (HasParamToken(param, _T("NX"))) return PlayListSub::Next;
	if (HasParamToken(param, _T("PR"))) return PlayListSub::Prev;
	if (HasParamToken(param, _T("PS"))) return PlayListSub::Pause;
	if (HasParamToken(param, _T("RS"))) return PlayListSub::Resume;
	if (HasParamToken(param, _T("PP"))) return PlayListSub::PlayPause;
	if (HasParamToken(param, _T("CA"))) return PlayListSub::ClearAll;
	if (HasParamToken(param, _T("FI"))) return PlayListSub::FileInfo;
	return PlayListSub::Setup;
}

}  // namespace f_batch6_ops
