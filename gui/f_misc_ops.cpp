/**
 * @file gui/f_misc_ops.cpp
 * @brief Fモード残コマンド batch4 の判断ロジックの実装 (wx 非依存)
 */
#include "gui/f_misc_ops.h"

#include "usr_file_ex.h"
#include "usr_str.h"

namespace f_misc_ops {

bool HasParamToken(const UnicodeString &param, const UnicodeString &token)
{
	if (param.IsEmpty() || token.IsEmpty()) return false;
	TStringDynArray toks = split_strings_semicolon(param);
	for (int i = 0; i < toks.Length; i++) {
		if (SameText(toks[i], token)) return true;
	}
	return false;
}

bool ToggleFlagValue(bool cur, const UnicodeString &param)
{
	if (HasParamToken(param, _T("ON"))) return true;
	if (HasParamToken(param, _T("OFF"))) return false;
	return !cur;
}

bool AnyPaused(const std::vector<bool> &pauses)
{
	for (bool p : pauses) {
		if (p) return true;
	}
	return false;
}

UnicodeString PauseAllCaption(int paused_count)
{
	return (paused_count > 0) ? UnicodeString(_T("すべて再開"))
	                          : UnicodeString(_T("すべて一旦停止"));
}

UnicodeString SuspendCaption(bool suspended)
{
	return suspended ? UnicodeString(_T("解除")) : UnicodeString(_T("保留"));
}

bool ValidateBackupPaths(const UnicodeString &cur_path, const UnicodeString &opp_path,
                         UnicodeString &error_out)
{
	if (cur_path.IsEmpty() || opp_path.IsEmpty()) {
		error_out = UnicodeString(_T("操作できません"));
		return false;
	}
	if (SameText(cur_path, opp_path)) {
		error_out = UnicodeString(_T("コピー先が同じです"));
		return false;
	}
	return true;
}

std::vector<int> CompressTargetIndices(const std::vector<bool> &selected,
                                       const std::vector<bool> &is_dir)
{
	std::vector<int> out;
	const std::size_t n = std::min(selected.size(), is_dir.size());
	for (std::size_t i = 0; i < n; i++) {
		if (selected[i] && is_dir[i]) out.push_back(static_cast<int>(i));
	}
	return out;
}

bool MatchFileNames(const UnicodeString &a, const UnicodeString &b, bool case_sensitive)
{
	return case_sensitive ? SameStr(a, b) : SameText(a, b);
}

bool CanFindHardLink(bool is_arc, bool is_ads, bool is_ftp, bool is_unc, bool is_dir,
                     int link_count, UnicodeString &error_out)
{
	if (is_arc || is_ads || is_ftp || is_unc) {
		error_out = UnicodeString(_T("操作できません"));
		return false;
	}
	if (is_dir) {
		error_out = UnicodeString(_T("ディレクトリは対象外です"));
		return false;
	}
	if (link_count < 2) {
		error_out = UnicodeString(_T("ハードリンクではありません。"));
		return false;
	}
	return true;
}

UnicodeString PickOppHardLink(const UnicodeString &cur_file,
                              const std::vector<UnicodeString> &candidates,
                              const std::vector<UnicodeString> &opp_files)
{
	UnicodeString fallback;
	for (const UnicodeString &hnam : candidates) {
		if (SameText(cur_file, hnam)) continue;
		if (fallback.IsEmpty()) fallback = hnam;
		for (const UnicodeString &onam : opp_files) {
			if (SameText(hnam, onam)) return hnam;
		}
	}
	return fallback;
}

FolderIconCmd ParseFolderIconParam(const UnicodeString &param)
{
	if (HasParamToken(param, _T("RD"))) return FolderIconCmd::ClearDefault;
	if (HasParamToken(param, _T("SD"))) return FolderIconCmd::SelectDefault;
	if (HasParamToken(param, _T("RS"))) return FolderIconCmd::ResetToDefault;
	if (HasParamToken(param, _T("ND"))) return FolderIconCmd::Menu;
	if (param.IsEmpty()) return FolderIconCmd::ChooseFile;
	return FolderIconCmd::SetFile;
}

bool ValidateFolderIconSearch(const UnicodeString &path, bool icons_empty,
                              UnicodeString &error_out)
{
	(void)path;
	if (icons_empty) {
		error_out = UnicodeString(_T("アイコンが選択されていません"));
		return false;
	}
	return true;
}

UnicodeString ResolveJumpTarget(const UnicodeString &action_param,
                                const UnicodeString &cur_file, const UnicodeString &cur_dir,
                                UnicodeString &error_out)
{
	UnicodeString fnam = action_param.IsEmpty() ? cur_file : action_param;
	if (fnam.IsEmpty()) {
		error_out = UnicodeString(_T("パラメータがありません"));
		return EmptyStr;
	}
	return to_absolute_name(fnam, cur_dir);
}

UnicodeString ResolveDriveName(const UnicodeString &action_param, const UnicodeString &cur_path)
{
	if (!action_param.IsEmpty()) {
		UnicodeString dnam;
		dnam.sprintf(_T("%c:"), action_param[1]);
		return dnam;
	}
	if (StartsStr("\\\\", cur_path)) return EmptyStr;
	UnicodeString drv = ExtractFileDrive(cur_path);
	if (drv.IsEmpty()) return EmptyStr;
	return drv;
}

UnicodeString FormatTaskSummary(int busy_count, int paused_count)
{
	if (busy_count <= 0) return UnicodeString(_T("実行中のタスクはありません"));
	UnicodeString s;
	s.sprintf(_T("実行中: %d 件 (停止中: %d 件)"), busy_count, paused_count);
	return s;
}

}  // namespace f_misc_ops
