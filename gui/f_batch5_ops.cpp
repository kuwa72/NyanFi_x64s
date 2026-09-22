/**
 * @file gui/f_batch5_ops.cpp
 * @brief Fモード残コマンド batch5 の判断ロジックの実装 (wx 非依存)
 */
#include "gui/f_batch5_ops.h"

#include "usr_file_ex.h"
#include "usr_str.h"

namespace f_batch5_ops {

bool HasParamToken(const UnicodeString &param, const UnicodeString &token)
{
	if (param.IsEmpty() || token.IsEmpty()) return false;
	TStringDynArray toks = split_strings_semicolon(param);
	for (int i = 0; i < toks.Length; i++) {
		if (SameText(toks[i], token)) return true;
	}
	return false;
}

int ResolveAccKeyIndex(const std::vector<UnicodeString> &first_cols,
                       const UnicodeString &key)
{
	// VCL get_IndexFromAccKey (UserFunc.cpp:1196): 1文字でなければ -1
	if (key.Length() != 1) return -1;
	UnicodeString pat = UnicodeString(_T("&")) + key;
	for (std::size_t i = 0; i < first_cols.size(); i++) {
		if (ContainsText(first_cols[i], pat)) return static_cast<int>(i);
	}
	return -1;
}

UnicodeString ResolveRegDirPopupTarget(const UnicodeString &param)
{
	return HasParamToken(param, _T("OP")) ? UnicodeString(_T("OppDir"))
	                                      : UnicodeString(_T("RegDir"));
}

PathMaskMode ResolvePathMaskMode(const UnicodeString &param, bool is_find, bool is_work)
{
	if (is_find || is_work) return PathMaskMode::Denied;
	return HasParamToken(param, _T("ND")) ? PathMaskMode::Popup : PathMaskMode::Dialog;
}

UnicodeString ResolveBinaryEditTarget(bool has_selection, const UnicodeString &sel_str,
                                      const UnicodeString &cur_str)
{
	return has_selection ? sel_str : cur_str;
}

bool ValidateBinaryEditor(const UnicodeString &editor_path, UnicodeString &error_out)
{
	if (editor_path.IsEmpty() || !file_exists(editor_path)) {
		error_out = UnicodeString(_T("アプリケーションが見つかりません"));
		return false;
	}
	return true;
}

UnicodeString BuildEditHighlightCommand(const UnicodeString &highlight_path)
{
	return UnicodeString(_T("FileEdit_\"")) + highlight_path + UnicodeString(_T("\""));
}

FixedLenPlan ResolveFixedLen(bool is_tview, bool cur_enabled, const UnicodeString &param)
{
	FixedLenPlan plan;
	if (is_tview) {
		plan.delegate_to_viewer = true;
		return plan;
	}
	const int lmt = extract_int_def(param);
	if (lmt > 0) {
		plan.limit = std::max(lmt, 4);
		plan.new_enabled = true;
		return plan;
	}
	if (HasParamToken(param, _T("ON"))) {
		plan.new_enabled = true;
		return plan;
	}
	if (HasParamToken(param, _T("OFF"))) {
		plan.new_enabled = false;
		return plan;
	}
	plan.new_enabled = !cur_enabled;
	return plan;
}

HtmlToTextPlan ParseHtmlToText(const UnicodeString &param, bool is_tview)
{
	HtmlToTextPlan plan;
	if (is_tview) {
		plan.delegate_to_viewer = true;
		return plan;
	}
	// VCL TTxtViewer::SetHtmlToText (TxtViewer.cpp:3143) と同じ ';' 区切り
	TStringDynArray toks = split_strings_semicolon(param);
	for (int i = 0; i < toks.Length; i++) {
		const UnicodeString s = toks[i];
		if (SameText(s, _T("ON")) || SameText(s, _T("OFF"))) {
			plan.toggle_param = s;
		}
		else if (SameText(s, _T("MD"))) {
			plan.markdown = MarkdownMode::ToMarkdown;
		}
		else if (SameText(s, _T("^MD"))) {
			plan.markdown = MarkdownMode::Toggle;
		}
		else if (SameText(s, _T("TX"))) {
			plan.markdown = MarkdownMode::ToText;
		}
	}
	return plan;
}

SetColorMode ResolveSetColorMode(const UnicodeString &param)
{
	return param.IsEmpty() ? SetColorMode::Dialog : SetColorMode::FromFile;
}

bool ToggleEnabled(bool cur, const UnicodeString &param)
{
	if (HasParamToken(param, _T("ON"))) return true;
	if (HasParamToken(param, _T("OFF"))) return false;
	return !cur;
}

bool ValidateListDuration(bool is_arc, bool is_ftp, int sel_count, UnicodeString &error_out)
{
	if (is_arc || is_ftp) {
		error_out = UnicodeString(_T("操作できません"));
		return false;
	}
	if (sel_count <= 0) {
		error_out = UnicodeString(_T("対象がありません"));
		return false;
	}
	return true;
}

UnicodeString FormatDurationMs(unsigned int ms, bool with_cs)
{
	// VCL mSecToTStr (usr_str.cpp:1659) と同じ書式
	const unsigned int scnt = ms / 1000;
	const int c = static_cast<int>(ms / 10.0 + 0.5) % 100;
	int s = static_cast<int>(scnt % 60);
	int m = static_cast<int>((scnt / 60) % 60);
	const int h = static_cast<int>(scnt / 3600);
	UnicodeString ret;
	if (with_cs) {
		ret.sprintf(_T("%02u:%02u:%02u.%02u"), h, m, s, c);
	}
	else {
		if (c > 50) s++;
		ret.sprintf(_T("%02u:%02u:%02u"), h, m, s);
	}
	return ret;
}

UnicodeString FormatDurationRow(const UnicodeString &name, unsigned int ms, bool with_cs)
{
	// VCL (20722行): msg.sprintf("%s\t %-11s", n_name, mSecToTStr)
	return name + UnicodeString(_T("\t ")) + FormatDurationMs(ms, with_cs);
}

UnicodeString FormatDurationTotal(unsigned int total_ms, bool with_cs, int file_count,
                                  int err_count)
{
	UnicodeString msg;
	msg.sprintf(_T("%-8s%8u  %-11s"), _T("合計"), file_count,
	            FormatDurationMs(total_ms, with_cs).c_str());
	if (err_count > 0) msg.cat_sprintf(_T("  ERR:%u"), err_count);
	return msg;
}

int ParseExpFuncSort(const UnicodeString &param)
{
	// VCL (20837行) の連鎖三項と同じ優先順位: SI > SR > SN
	if (HasParamToken(param, _T("SI"))) return 1;
	if (HasParamToken(param, _T("SR"))) return 2;
	if (HasParamToken(param, _T("SN"))) return 3;
	return 0;
}

int ParseExpFuncListMode(const UnicodeString &param)
{
	if (ContainsText(param, _T(".csv"))) return 1;
	if (ContainsText(param, _T(".tsv"))) return 2;
	return 0;
}

WatchTailSubCmd ParseWatchTailSubCmd(const UnicodeString &param)
{
	if (HasParamToken(param, _T("AC"))) return WatchTailSubCmd::AllCancel;
	if (HasParamToken(param, _T("ST"))) return WatchTailSubCmd::Status;
	if (HasParamToken(param, _T("CC"))) return WatchTailSubCmd::CancelOne;
	return WatchTailSubCmd::Watch;
}

}  // namespace f_batch5_ops
