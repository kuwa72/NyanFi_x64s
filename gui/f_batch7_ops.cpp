/**
 * @file gui/f_batch7_ops.cpp
 * @brief Fモード残コマンド batch7 の判断ロジックの実装 (wx 非依存)
 */
#include "gui/f_batch7_ops.h"

#include "compat/exception.h"
#include "usr_file_ex.h"
#include "usr_str.h"

namespace f_batch7_ops {

bool HasParamToken(const UnicodeString &param, const UnicodeString &token)
{
	if (param.IsEmpty() || token.IsEmpty()) return false;
	TStringDynArray toks = split_strings_semicolon(param);
	for (int i = 0; i < toks.Length; i++) {
		if (SameText(toks[i], token)) return true;
	}
	return false;
}

AppListOpts ParseAppListOpts(const UnicodeString &param)
{
	AppListOpts o;
	o.only_app      = HasParamToken(param, _T("AO"));
	o.only_launcher = HasParamToken(param, _T("LO")) || HasParamToken(param, _T("LI"));
	o.to_app        = HasParamToken(param, _T("FA"));
	o.to_launcher   = HasParamToken(param, _T("FL"));
	o.to_incsea     = HasParamToken(param, _T("FI")) || HasParamToken(param, _T("LI"));
	o.fuzzy         = HasParamToken(param, _T("FZ"));
	o.add_start     = HasParamToken(param, _T("AS"));
	return o;
}

DebugCmdSrc ResolveDebugCmdSrc(bool has_param, bool is_flist, bool has_cursor,
                               bool cursor_is_nbt)
{
	if (has_param) return DebugCmdSrc::Param;
	if (is_flist && has_cursor && cursor_is_nbt) return DebugCmdSrc::Cursor;
	return DebugCmdSrc::None;
}

DistributionMode ResolveDistributionMode(const UnicodeString &param)
{
	if (HasParamToken(param, _T("XC"))) return DistributionMode::ImmediateCopy;
	if (HasParamToken(param, _T("XM"))) return DistributionMode::ImmediateMove;
	if (HasParamToken(param, _T("SN"))) return DistributionMode::SetMask;
	return DistributionMode::Dialog;
}

DotNyanMode ResolveDotNyanMode(bool is_flist, const UnicodeString &param)
{
	if (!is_flist) return DotNyanMode::Denied;
	if (HasParamToken(param, _T("RS"))) return DotNyanMode::Reapply;
	return DotNyanMode::Dialog;
}

int ResolveToolBtnIndex(const UnicodeString &param, int count)
{
	const int idx = param.ToIntDef(0);
	if (idx < 1 || idx > count) return -1;
	return idx - 1;
}

bool ValidateExtractChmSrc(bool is_ads, bool is_ftp, bool is_opp_flist,
                           bool has_cursor, bool is_dir, const UnicodeString &ext,
                           bool is_virtual, bool tmp_ok, bool fnam_has_space,
                           bool odir_has_space, UnicodeString &error_out)
{
	if (is_ads || is_ftp || !is_opp_flist) {
		error_out = UnicodeString(_T("操作できません"));
		return false;
	}
	if (!has_cursor || is_dir || !SameText(ext, _T(".chm"))) {
		error_out = UnicodeString(_T("CHMファイルを指定してください"));
		return false;
	}
	if (is_virtual && !tmp_ok) {
		error_out = UnicodeString(_T("一時展開に失敗しました"));
		return false;
	}
	if (fnam_has_space || odir_has_space) {
		error_out = UnicodeString(_T("空白を含む名前には対応していません。"));
		return false;
	}
	return true;
}

bool ValidateExtractGif(bool has_sel, bool all_sel_gif, bool cursor_is_gif,
                        UnicodeString &error_out)
{
	if (has_sel) {
		if (!all_sel_gif) {
			error_out = UnicodeString(_T("対応していない形式です"));
			return false;
		}
		return true;
	}
	if (!cursor_is_gif) {
		error_out = UnicodeString(_T("GIFファイルを指定してください"));
		return false;
	}
	return true;
}

bool ValidateLockWord(const UnicodeString &param, UnicodeString &error_out)
{
	if (!param.IsEmpty() && !is_alnum_str(param)) {
		error_out = UnicodeString(_T("パラメータが不正です"));
		return false;
	}
	return true;
}

UpdateFromArcSrc ResolveUpdateFromArcSrc(const UnicodeString &param)
{
	if (HasParamToken(param, _T("UN"))) return UpdateFromArcSrc::Newest;
	return UpdateFromArcSrc::Dialog;
}

}  // namespace f_batch7_ops
