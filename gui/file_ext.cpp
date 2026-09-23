/**
 * @file gui/file_ext.cpp
 * @brief gui/file_ext.h の実装
 */
#include "gui/file_ext.h"

#include <algorithm>

#include "gui/dir_info.h"
#include "usr_file_ex.h"

namespace file_ext {

namespace {

bool has_param_token(const UnicodeString &param, const UnicodeString &token)
{
	return test_word_i(param, token);
}

UnicodeString report_extension(const UnicodeString &ext)
{
	if (SameText(ext, _T("(none)"))) return _T(".");
	return StartsStr(_T("."), ext)? ext : _T(".") + ext;
}

}  // namespace

//---------------------------------------------------------------------------
Summary Collect(const UnicodeString &path, bool recursive, bool show_hidden, bool show_system,
                bool &truncated_out)
{
	Summary out;
	out.root = IncludeTrailingPathDelimiter(path);
	const std::vector<dir_info::ExtStat> stats =
		dir_info::CalcExtStats(path, recursive, show_hidden, show_system, truncated_out);
	out.truncated = truncated_out;
	out.extensions.reserve(stats.size());
	for (const dir_info::ExtStat &stat : stats) {
		Entry entry;
		entry.extension = stat.ext;
		entry.count = stat.count;
		entry.bytes = stat.bytes;
		entry.average = stat.count > 0? static_cast<long double>(stat.bytes) / stat.count : 0.0L;
		entry.files = stat.files;
		out.extensions.push_back(std::move(entry));
		out.total_count += stat.count;
		out.total_bytes += stat.bytes;
	}
	return out;
}

//---------------------------------------------------------------------------
void SortExtensions(Summary &summary, ExtensionSort key, bool descending)
{
	std::stable_sort(summary.extensions.begin(), summary.extensions.end(),
	                 [&](const Entry &a, const Entry &b) {
		                 if (key == ExtensionSort::Extension) {
			                 const int cmp = CompareText(a.extension, b.extension);
			                 return descending? cmp > 0 : cmp < 0;
		                 }

		                 long double av = 0.0L;
		                 long double bv = 0.0L;
		                 if (key == ExtensionSort::Count) {
			                 av = a.count;
			                 bv = b.count;
		                 }
		                 else if (key == ExtensionSort::Bytes) {
			                 av = a.bytes;
			                 bv = b.bytes;
		                 }
		                 else {
			                 av = a.average;
			                 bv = b.average;
		                 }
		                 if (av != bv) return descending? av > bv : av < bv;
		                 // 数値が同じときは VCL と同じく拡張子名を安定の基準にする。
		                 return CompareText(a.extension, b.extension) < 0;
	                 });
}

//---------------------------------------------------------------------------
void SortFiles(std::vector<UnicodeString> &files, FileSort key, bool descending)
{
	std::stable_sort(files.begin(), files.end(), [&](const UnicodeString &a, const UnicodeString &b) {
		const UnicodeString ak = key == FileSort::Name? ExtractFileName(a) : ExtractFileDir(a);
		const UnicodeString bk = key == FileSort::Name? ExtractFileName(b) : ExtractFileDir(b);
		const int cmp = CompareText(ak, bk);
		if (cmp != 0) return descending? cmp > 0 : cmp < 0;

		const UnicodeString as = key == FileSort::Name? ExtractFileDir(a) : ExtractFileName(a);
		const UnicodeString bs = key == FileSort::Name? ExtractFileDir(b) : ExtractFileName(b);
		const int tie = CompareText(as, bs);
		return descending? tie > 0 : tie < 0;
	});
}

//---------------------------------------------------------------------------
UnicodeString BuildMask(const UnicodeString &extension)
{
	if (extension.IsEmpty() || SameText(extension, _T("(none)"))) return _T("*");
	UnicodeString ext = extension;
	if (!StartsStr(_T("."), ext)) ext.Insert(_T("."), 1);
	UnicodeString out(_T("*"));
	out += ext;
	return out;
}

//---------------------------------------------------------------------------
UnicodeString ResolveTargetPath(const UnicodeString &param, const UnicodeString &current_path,
                                const UnicodeString &cursor_path, bool cursor_is_dir,
                                bool cursor_is_parent, UnicodeString &error_out)
{
	error_out = EmptyStr;
	if (!has_param_token(param, _T("CP"))) return ExcludeTrailingPathDelimiter(current_path);
	if (cursor_path.IsEmpty() || !cursor_is_dir) {
		error_out = _T("カーソル位置にディレクトリがありません");
		return EmptyStr;
	}
	if (cursor_is_parent) return ExcludeTrailingPathDelimiter(current_path);
	return ExcludeTrailingPathDelimiter(cursor_path);
}

//---------------------------------------------------------------------------
UnicodeString FormatReport(const Summary &summary, OutputFormat format, const UnicodeString &root)
{
	UnicodeString out;
	if (format == OutputFormat::Csv) {
		out = _T("\"拡張子\",\"ファイル数\",\"合計サイズ\",\"平均サイズ\"\r\n");
		for (const Entry &entry : summary.extensions) {
			out.cat_sprintf(_T("\"%s\",%d,%lld,%lld\r\n"), report_extension(entry.extension).c_str(),
			                entry.count, static_cast<long long>(entry.bytes),
			                static_cast<long long>(entry.average));
		}
		return out;
	}
	if (format == OutputFormat::Tsv) {
		out = _T("拡張子\tファイル数\t合計サイズ\t平均サイズ\r\n");
		for (const Entry &entry : summary.extensions) {
			out.cat_sprintf(_T("%s\t%d\t%lld\t%lld\r\n"), report_extension(entry.extension).c_str(),
			                entry.count, static_cast<long long>(entry.bytes),
			                static_cast<long long>(entry.average));
		}
		return out;
	}

	out = IncludeTrailingPathDelimiter(root) + _T("\r\n");
	out += _T("拡張子              ファイル数           サイズ     比率      平均\r\n");
	out += _T("------------------------------------------------------------\r\n");
	for (const Entry &entry : summary.extensions) {
		const long double ratio = summary.total_bytes > 0
			? 100.0L * entry.bytes / summary.total_bytes
			: 0.0L;
		out.cat_sprintf(_T("%-12s %14s %14s %6.1f%% %14s\r\n"),
		                report_extension(entry.extension).c_str(),
		                get_size_str_B(entry.count, 14).Trim().c_str(),
		                get_size_str_G(entry.bytes, 14, 2).Trim().c_str(), ratio,
		                get_size_str_G(static_cast<Int64>(entry.average), 14, 2).Trim().c_str());
	}
	out += _T("------------------------------------------------------------\r\n");
	const long double average = summary.total_count > 0
		? static_cast<long double>(summary.total_bytes) / summary.total_count
		: 0.0L;
	out.cat_sprintf(_T("%-12d %14s %14s %17s\r\n"),
	                static_cast<int>(summary.extensions.size()),
	                get_size_str_B(summary.total_count, 14).Trim().c_str(),
	                get_size_str_G(summary.total_bytes, 14, 2).Trim().c_str(),
	                get_size_str_G(static_cast<Int64>(average), 14, 2).Trim().c_str());
	if (summary.truncated) out += _T("※ 走査上限に達したため途中までです\r\n");
	return out;
}

}  // namespace file_ext
