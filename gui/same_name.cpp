/**
 * @file gui/same_name.cpp
 * @brief gui/same_name.h の実装
 */
#include "gui/same_name.h"

#include <cmath>

#include "gui/clone_name.h"
#include "usr_file_ex.h"
#include "usr_str.h"

namespace same_name {

namespace {

constexpr double kTimeToleranceDays = 2.0 / (24.0 * 60.0 * 60.0);

}  // namespace

bool IsModeEnabled(Mode mode, bool same_path)
{
	if (!same_path) return true;
	return mode == Mode::AutoRename || mode == Mode::ManualRename;
}

Mode NormalizeMode(Mode mode, bool same_path)
{
	if (same_path && mode < Mode::AutoRename) return Mode::ManualRename;
	return mode;
}

Options NormalizeOptions(const Context &context, const Options &options)
{
	Options out = options;
	out.mode = NormalizeMode(out.mode, context.same_path);
	// VCL は手動改名欄の初期値を保持したまま、全件適用との排那只を切り替える。
	if (out.rename_name.IsEmpty()) out.rename_name = DefaultRenameName(context);
	if (out.mode == Mode::ManualRename && out.copy_all) {
		// VCL AllCheckBoxClick: 全件適用中を手動改名へ選ぶと自動改名へ戻る
		out.mode = Mode::AutoRename;
		out.copy_all = false;
	}
	return out;
}

bool ShouldCopy(Mode mode, Int64 source_size, double source_time,
                Int64 destination_size, double destination_time)
{
	(void)source_size;
	(void)destination_size;
	switch (mode) {
	case Mode::Skip: return false;
	case Mode::KeepNewer: return source_time > destination_time;
	case Mode::Overwrite:
	case Mode::AutoRename:
	case Mode::ManualRename:
	default: return true;
	}
}

UnicodeString MakeAutoRenamePath(const UnicodeString &source, const UnicodeString &dest_dir,
                                 bool source_is_dir,
                                 const std::function<bool(const UnicodeString &)> &taken,
                                 int limit)
{
	// clone_name::Expand は VCL の FMT_AUTO_REN (\\N_\\SN(1)) の文字列処理だけを
	// 持つ。MakeUnique はファイル時刻を参照するため、同名判定の純関数には使わない。
	const UnicodeString src = ExcludeTrailingPathDelimiter(source);
	const UnicodeString base = source_is_dir ? ExtractFileName(src) : get_base_name(src);
	const UnicodeString ext = source_is_dir ? EmptyStr : get_extension(src);
	const UnicodeString dir = IncludeTrailingPathDelimiter(dest_dir);
	for (int seq = 0; seq < limit; ++seq) {
		const UnicodeString path = dir + clone_name::Expand(EmptyStr, base, seq,
		                                                          TDateTime(), TDateTime()) + ext;
		if (!taken(path)) return path;
	}
	return EmptyStr;
}

UnicodeString DefaultRenameName(const Context &context)
{
	if (!context.initial_name.IsEmpty()) return context.initial_name;
	return ExtractFileName(ExcludeTrailingPathDelimiter(context.destination));
}

UnicodeString SizeSummary(Int64 source_size, Int64 destination_size)
{
	UnicodeString text = _T("サイズ: ");
	if (source_size == destination_size) return text + _T("同じ");
	if (source_size < destination_size) return text + _T("転送先の方が大きい");
	return text + _T("転送先の方が小さい");
}

UnicodeString TimeSummary(double source_time, double destination_time)
{
	UnicodeString text = _T("タイム: ");
	if (std::fabs(source_time - destination_time) < kTimeToleranceDays) {
		text += _T("同じ");
		if (source_time != destination_time) text += _T(" (許容誤差内)");
		return text;
	}
	if (source_time > destination_time) return text + _T("転送先の方が古い");
	return text + _T("転送先の方が新しい");
}

}  // namespace same_name
