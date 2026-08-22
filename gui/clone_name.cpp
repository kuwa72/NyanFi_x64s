/**
 * @file gui/clone_name.cpp
 * @brief クローン名の組み立ての実装 (設計は gui/clone_name.h)
 */
#include "gui/clone_name.h"

#include "usr_file_ex.h"
#include "usr_str.h"

namespace clone_name {

const wchar_t *const kDefaultFormat = L"\\N_\\SN(1)";

//---------------------------------------------------------------------------
/**
 * @details `format_CloneName` (src/Global.cpp:3820) の内側のループを
 * 1回分だけ切り出したもの。書式の解釈は1行ずつ突き合わせてある
 */
UnicodeString Expand(const UnicodeString &fmt_in, const UnicodeString &base, int seq,
                     const TDateTime &stamp, const TDateTime &now)
{
	UnicodeString fmt = fmt_in.IsEmpty()? UnicodeString(kDefaultFormat) : fmt_in;

	// `\-` は「1回目はここから後ろを捨てる」印。2回目以降は印だけ消す
	UnicodeString tmp = (seq == 0)? get_tkn(fmt, _T("\\-")) : ReplaceStr(fmt, _T("\\-"), EmptyStr);

	UnicodeString out;
	while (!tmp.IsEmpty()) {
		WideChar c = split_top_wch(tmp);
		if (c != L'\\') {
			out.cat_sprintf(_T("%c"), c);
			continue;
		}

		// 連番: \SN(n) は n の桁数でゼロ詰めし、n + seq を出す
		if (StartsStr(_T("SN("), tmp)) {
			UnicodeString nstr = split_in_paren(tmp);
			if (nstr.IsEmpty()) nstr = _T("1");
			out.cat_sprintf(_T("%0*u"), nstr.Length(), nstr.ToIntDef(0) + seq);
		}
		// 日時 (\DT) / 元のタイムスタンプ (\TS)
		else if (StartsStr(_T("DT("), tmp) || StartsStr(_T("TS("), tmp)) {
			const bool use_stamp = StartsStr(_T("TS("), tmp);
			const UnicodeString dt_fmt = split_in_paren(tmp);
			out += FormatDateTime(dt_fmt, use_stamp? stamp : now);
		}
		else {
			c = split_top_wch(tmp);
			if (c == L'N') out += base;  // 名前の主部
			// それ以外の \x は VCL も何も出さない (読み飛ばす)
		}
	}
	return out;
}

//---------------------------------------------------------------------------
UnicodeString MakeUnique(const UnicodeString &fmt, const UnicodeString &src_path,
                         const UnicodeString &dst_dir, bool is_dir,
                         const std::function<bool(const UnicodeString &)> &taken,
                         int limit)
{
	const UnicodeString src = ExcludeTrailingPathDelimiter(src_path);
	// ディレクトリは名前全体が主部で、拡張子は付けない (format_CloneName と同じ)
	const UnicodeString base = is_dir? ExtractFileName(src) : get_base_name(src);
	const UnicodeString ext  = is_dir? EmptyStr : get_extension(src);
	const UnicodeString dir  = dst_dir.IsEmpty()? EmptyStr : IncludeTrailingPathDelimiter(dst_dir);

	const TDateTime stamp = get_file_age(src);
	const TDateTime now = Now();

	for (int seq = 0; seq < limit; ++seq) {
		const UnicodeString path = dir + Expand(fmt, base, seq, stamp, now) + ext;
		if (!taken(path)) return path;
	}
	return EmptyStr;  // 上限まで空きが無かった (暴走させない)
}

}  // namespace clone_name
