/**
 * @file gui/view_settings.cpp
 * @brief 表示切り替えの引数解釈・計算 (根拠と VCL の該当行は gui/view_settings.h を参照)
 */
#include "gui/view_settings.h"

#include <algorithm>

#include "usr_str.h"

namespace view_settings {

//===========================================================================
// トグル系
//===========================================================================

//---------------------------------------------------------------------------
Toggle ParseToggle(const UnicodeString &param)
{
	TStringDynArray lst = split_strings_semicolon(param);
	for (int i=0; i<lst.Length; i++) if (SameText("ON", lst[i])) return Toggle::On;
	for (int i=0; i<lst.Length; i++) if (SameText("OFF", lst[i])) return Toggle::Off;
	return Toggle::Flip;
}

//---------------------------------------------------------------------------
bool ApplyToggle(bool current, Toggle how)
{
	switch (how) {
	case Toggle::On:  return true;
	case Toggle::Off: return false;
	default:          return !current;
	}
}

//===========================================================================
// フォントサイズ
//===========================================================================

//---------------------------------------------------------------------------
int ClampFontSize(int size)
{
	return std::clamp(size, kMinFontSize, kMaxFontSize);
}

//---------------------------------------------------------------------------
int AdjustFontSize(int current, int delta)
{
	return ClampFontSize(current + std::clamp(delta, -12, 12));
}

//---------------------------------------------------------------------------
bool ParseFontSize(const UnicodeString &param, int current, int base, int &out)
{
	UnicodeString buf = param;
	bool toggle_same = remove_top_s(buf, '^');
	if (buf.IsEmpty()) return false;

	int requested = ClampFontSize(buf.ToIntDef(base));

	if (toggle_same && current==requested) {
		out = base;
	}
	else {
		out = requested;
	}
	return true;
}

//===========================================================================
// 透過度
//===========================================================================

//---------------------------------------------------------------------------
AlphaAction ParseAlpha(const UnicodeString &param, int current_value, bool current_enabled, int &out)
{
	if (param.IsEmpty()) return AlphaAction::Disable;

	TStringDynArray lst = split_strings_semicolon(param);
	for (int i=0; i<lst.Length; i++) if (SameText("IN", lst[i])) return AlphaAction::NeedsDialog;

	UnicodeString buf = param;
	bool x_sw = false;
	int  r_flg = 0;

	switch (buf[1]) {
	case '^': x_sw  = true;	break;
	case '+': r_flg = 1;	break;
	case '-': r_flg = -1;	break;
	}
	if (x_sw || r_flg!=0) buf.Delete(1, 1);

	int a = buf.ToIntDef(255);

	if (x_sw) {
		if (current_enabled) a = 255;
	}
	else if (r_flg==1) {
		a = current_value + a;
	}
	else if (r_flg==-1) {
		a = current_value - a;
	}

	out = std::clamp(a, kMinAlpha, kMaxAlpha);
	return AlphaAction::Apply;
}

//===========================================================================
// ウィンドウ位置
//===========================================================================

//---------------------------------------------------------------------------
bool ParseWindowPos(const UnicodeString &param, WindowEdges &out)
{
	if (param.IsEmpty()) return false;

	WindowEdges edges;
	TStringDynArray prm_lst = split_strings_semicolon(param);

	for (int i=0; i<prm_lst.Length; i++) {
		UnicodeString lbuf = prm_lst[i].UpperCase();
		if (!starts_tchs("LTRB", lbuf)) return false;
		WideChar c = split_top_wch(lbuf);

		bool is_rel = starts_tchs("+-", lbuf);
		int rel_sig = 0;
		if (is_rel) {
			rel_sig = (lbuf[1]=='-')? -1 : 1;
			lbuf.Delete(1, 1);
		}
		if (lbuf.IsEmpty()) return false;

		// VCL と同じく ToIntDef(-1) を失敗判定に使う (MainFrm.cpp:27724)。
		// 符号は上で既に取り除いているので通常は衝突しないが、"L--1" のような
		// 二重符号を与えると残りが "-1" になり、正しく -1 と解釈できるにも
		// 関わらず「失敗」として弾かれる。VCL 本体も同じ書き方でこの制限を
		// 持つので、ここでもそのまま踏襲する (規約6: 既存の挙動は直さず記録する)
		int v = lbuf.ToIntDef(-1);
		if (v==-1) return false;

		EdgeSpec spec;
		spec.set = true;
		spec.relative = is_rel;
		spec.value = is_rel? (rel_sig * v) : v;

		switch (c) {
		case 'L': edges.left   = spec; break;
		case 'T': edges.top    = spec; break;
		case 'R': edges.right  = spec; break;
		case 'B': edges.bottom = spec; break;
		}
	}

	out = edges;
	return true;
}

//---------------------------------------------------------------------------
bool ApplyWindowPos(const WindowEdges &edges,
	int cur_left, int cur_top, int cur_right, int cur_bottom,
	int &out_left, int &out_top, int &out_right, int &out_bottom)
{
	int l = cur_left, t = cur_top, r = cur_right, b = cur_bottom;

	if (edges.left.set)   l = edges.left.relative?   l + edges.left.value   : edges.left.value;
	if (edges.top.set)    t = edges.top.relative?    t + edges.top.value    : edges.top.value;
	if (edges.right.set)  r = edges.right.relative?  r + edges.right.value  : edges.right.value;
	if (edges.bottom.set) b = edges.bottom.relative? b + edges.bottom.value : edges.bottom.value;

	if (l>=r || t>=b) return false;

	out_left = l; out_top = t; out_right = r; out_bottom = b;
	return true;
}

//===========================================================================
// サブウィンドウのサイズ
//===========================================================================

//---------------------------------------------------------------------------
bool ParseSubSize(const UnicodeString &param, int &out, bool &out_relative)
{
	int size = param.ToIntDef(0);
	if (size==0) return false;

	out = size;
	out_relative = StartsStr("+", param) || StartsStr("-", param);
	return true;
}

//---------------------------------------------------------------------------
int ResolveSubSize(int requested, bool relative, int current, int container_size, int min_size)
{
	int new_size = relative? (current + requested) : requested;
	if ((container_size - new_size) < min_size) return current;
	return new_size;
}

//===========================================================================
// ステータスバーの書式
//===========================================================================

//---------------------------------------------------------------------------
UnicodeString ReplaceDirDelimiter(const UnicodeString &path, const UnicodeString &delimiter)
{
	return ReplaceStr(path, "\\", delimiter);
}

//---------------------------------------------------------------------------
static UnicodeString SortModeWord(int mode, bool short_form)
{
	return short_form? get_word_i_idx(_T("名|拡|時|サ|属|無|場"), mode)
	                  : get_word_i_idx(_T("名前|拡張子|日時|サイズ|属性|なし|場所"), mode);
}

//---------------------------------------------------------------------------
UnicodeString ExpandStatusFormat(const UnicodeString &fmt_in, const StatusFormatValues &values,
	const StatusFieldLookup &field_lookup)
{
	UnicodeString fmt = fmt_in;
	UnicodeString stt_str;
	int div_p = 0;

	UnicodeString full_path  = values.path + values.base_name;
	UnicodeString path2      = ReplaceDirDelimiter(values.path, values.dir_delimiter);
	UnicodeString full_path2 = ReplaceDirDelimiter(full_path, values.dir_delimiter);

	while (!fmt.IsEmpty()) {
		WideChar c = split_top_wch(fmt);
		if (c!='$') {
			stt_str += c;
			continue;
		}

		//※ "P2"/"F2"/"S2" のような2文字綴りは、単文字版より先に判定する
		//  (remove_top_s は前方一致で削除するので、"P" を先に試すと "P2" の
		//  "2" が残ってしまう)
		if (StartsStr("PR(", fmt)) {
			TStringDynArray s_buf = get_csv_array(split_in_paren(fmt), 3);
			if (values.has_file && s_buf.Length>0) {
				UnicodeString vstr = field_lookup? field_lookup(s_buf[0]) : EmptyStr;
				if (!vstr.IsEmpty()) {
					if (s_buf.Length>1) stt_str += s_buf[1];
					stt_str += vstr;
					if (s_buf.Length>2) stt_str += s_buf[2];
				}
			}
		}
		else if (remove_top_s(fmt, "P2")) stt_str += path2;
		else if (remove_top_s(fmt, 'P'))  stt_str += values.path;
		else if (remove_top_s(fmt, "F2")) stt_str += full_path2;
		else if (remove_top_s(fmt, 'F'))  stt_str += full_path;
		else if (remove_top_s(fmt, 'B'))  stt_str += values.base_name;
		else if (remove_top_s(fmt, "S2")) stt_str += SortModeWord(values.sort_mode, true);
		else if (remove_top_s(fmt, 'S'))  stt_str += SortModeWord(values.sort_mode, false);
		else if (remove_top_s(fmt, "HS")) {
			stt_str.cat_sprintf(_T("%c%c"), (values.show_hidden? 'H' : '_'), (values.show_system? 'S' : '_'));
		}
		else if (remove_top_s(fmt, "DV")) div_p = stt_str.Length() + 1;
		else if (remove_top_s(fmt, 'M'))  { if (values.has_file) stt_str += values.mark_memo; }
		else if (remove_top_s(fmt, 'Z'))  { if (values.has_file) stt_str += values.size_str; }
		else if (remove_top_s(fmt, 'Y'))  { if (values.has_file) stt_str += values.size_str_alt; }
		else if (remove_top_s(fmt, 'T'))  { if (values.has_file) stt_str += values.time_str; }
		// 上記のいずれにも一致しない "$X" は VCL と同じく何も出さず読み捨てる
	}

	if (div_p>0) stt_str.Insert("\t", div_p);

	return stt_str;
}

}  // namespace view_settings
