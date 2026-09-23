/**
 * @file gui/app_list.cpp
 * @brief gui/app_list.h の実装
 */
#include "gui/app_list.h"

namespace app_list {

namespace {

// 大小無視の部分列照合 (get_fuzzy_ptn の正規表現化の簡略版)
bool is_subsequence(const UnicodeString &text, const UnicodeString &pat)
{
	UnicodeString t = text.LowerCase();
	UnicodeString p = pat.LowerCase();
	int ti = 1;
	for (int pi = 1; pi <= p.Length() && ti <= t.Length(); pi++) {
		bool found = false;
		for (; ti <= t.Length(); ti++) {
			if (t[ti] == p[pi]) {
				found = true;
				ti++;
				break;
			}
		}
		if (!found) return false;
	}
	return true;
}

}  // namespace

std::vector<UnicodeString> SplitExcList(const UnicodeString &exc_text)
{
	std::vector<UnicodeString> out;
	if (exc_text.IsEmpty()) return out;
	TStringDynArray parts = split_strings_semicolon(exc_text, true);
	for (int i = 0; i < parts.Length; i++) {
		if (!parts[i].IsEmpty()) out.push_back(parts[i]);
	}
	return out;
}

bool MatchesExc(const UnicodeString &win_text, const std::vector<UnicodeString> &exc_list)
{
	for (const UnicodeString &e : exc_list) {
		if (!e.IsEmpty() && ContainsText(win_text, e)) return true;
	}
	return false;
}

bool ShouldListWindow(const WindowAttrs &attrs, const UnicodeString &win_text,
                      const std::vector<UnicodeString> &exc_list)
{
	// UpdateAppList の do{...}while(false) 連鎖の break 条件を実測。
	// MainForm 自体は表示対象のため、VCL の
	// `!IsWindowVisible && !ClassNameIs(cnam)` 分岐は
	// visible=false かつメインでない場合に落とす扱いとし、
	// ここでは visible=false を一律で落とす (呼び出し側でメイン例外を渡す責務)
	if (!attrs.visible) return false;
	if (attrs.is_self_dialog) return false;
	if (attrs.cloaked) return false;
	if (attrs.tool_window) return false;
	if (attrs.layered_no_edge) return false;
	if (attrs.has_parent_no_appwindow) return false;
	if (attrs.rect_empty) return false;
	if (attrs.text_empty) return false;
	if (MatchesExc(win_text, exc_list)) return false;
	return true;
}

int ResolveMonNo(const Rect &win, const std::vector<Rect> &monitors)
{
	const long area = static_cast<long>(win.width) * win.height;
	if (area <= 0) return -1;
	for (std::size_t i = 0; i < monitors.size(); i++) {
		const Rect &m = monitors[i];
		const int ix0 = std::max(win.left, m.left);
		const int iy0 = std::max(win.top, m.top);
		const int ix1 = std::min(win.left + win.width, m.left + m.width);
		const int iy1 = std::min(win.top + win.height, m.top + m.height);
		const long iw = static_cast<long>(ix1 - ix0);
		const long ih = static_cast<long>(iy1 - iy0);
		if (iw <= 0 || ih <= 0) continue;
		// VCL: (rc.Width*rc.Height) >= (wd*hi/2) を実測
		if (iw * ih >= area / 2) return static_cast<int>(i);
	}
	return -1;
}

bool IsLaunchAllPattern(const UnicodeString &word)
{
	if (word.IsEmpty()) return false;
	// contained_wd_i("*|?| ", IncSeaWord) を実測
	return ContainsText(word, _T("*")) || ContainsText(word, _T("?")) ||
	       ContainsText(word, _T(" "));
}

bool MatchesLaunchName(const UnicodeString &base_name, const UnicodeString &word,
                       bool fuzzy, bool ignore_case)
{
	if (word.IsEmpty()) return true;
	if (fuzzy) {
		if (ignore_case) return is_subsequence(base_name, word);
		// 大小区別版の部分列照合
		int ti = 1;
		for (int pi = 1; pi <= word.Length() && ti <= base_name.Length(); pi++) {
			bool found = false;
			for (; ti <= base_name.Length(); ti++) {
				if (base_name[ti] == word[pi]) {
					found = true;
					ti++;
					break;
				}
			}
			if (!found) return false;
		}
		return true;
	}
	if (ignore_case) return ContainsText(base_name, word);
	return base_name.Pos(word) > 0;
}

int CompareLaunchNormal(const LaunchEntry &a, const LaunchEntry &b)
{
	// SortComp_Launch を実測 (簡略版): 上へ・ディレクトリ優先・
	// 同拡張子は名前比較。StrCmpLogicalW は移植層に無いため大小無視比較で代替
	if (a.is_up != b.is_up) return a.is_up ? -1 : 1;
	if (a.is_dir != b.is_dir) {
		if (a.is_dir && b.is_dir) {
			const int c = CompareText(a.base_name, b.base_name);
			if (c != 0) return c;
			return CompareText(a.ext, b.ext);
		}
		return a.is_dir ? -1 : 1;
	}
	if (!SameText(a.ext, b.ext)) return CompareText(a.ext, b.ext);
	UnicodeString an = !a.alias.IsEmpty() ? a.alias : a.base_name;
	UnicodeString bn = !b.alias.IsEmpty() ? b.alias : b.base_name;
	const int c = CompareText(an, bn);
	if (c != 0) return c;
	return 0;
}

std::vector<int> CloseTargetsWithSameFile(const std::vector<UnicodeString> &file_names,
                                          int index)
{
	std::vector<int> out;
	if (index < 0 || index >= static_cast<int>(file_names.size())) return out;
	for (int i = 0; i < static_cast<int>(file_names.size()); i++) {
		if (SameText(file_names[static_cast<std::size_t>(i)],
		             file_names[static_cast<std::size_t>(index)]))
			out.push_back(i);
	}
	return out;
}

AppView ResolveAppView(bool only_app, bool only_launcher)
{
	// AppListActionExecute を実測:
	// OnlyAppList (AO) ならランチャー非表示、OnlyLauncher (LO/LI) なら一覧非表示
	AppView v;
	if (only_app) {
		v.show_app = true;
		v.show_launcher = false;
	}
	else if (only_launcher) {
		v.show_app = false;
		v.show_launcher = true;
	}
	else {
		v.show_app = true;
		v.show_launcher = true;
	}
	return v;
}

UnicodeString FormatStatus(const AppEntry &entry)
{
	// UpdateAppSttBar の PID/メモリ/サイズ部を実測・簡略版
	// (サイズ書式 get_size_str_K・開始時刻は呼び出し側表示のため含めない)
	UnicodeString s;
	s.sprintf(_T("PID:%u WS:%lu/%lu Win:%dx%d"), entry.pid, entry.mem_ws,
	          entry.mem_pws, entry.win_wd, entry.win_hi);
	if (entry.minimized) s += _T(" (_)");
	else if (entry.mon_no >= 0)
		s.cat_sprintf(_T(" (%d)"), entry.mon_no + 1);
	else
		s += _T(" (?)");
	if (entry.no_response) s += _T(" (無応答)");
	return s;
}

}  // namespace app_list
