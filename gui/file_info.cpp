/**
 * @file gui/file_info.cpp
 * @brief ファイル情報の組み立ての実装
 *
 * @details 設計・使用関数の一覧は gui/file_info.h の冒頭コメントを参照。
 */
#include "gui/file_info.h"

#include <algorithm>
#include <cmath>

#include "usr_file_ex.h"
#include "usr_file_inf.h"
#include "usr_id3.h"
#include "usr_str.h"

namespace {

/// 拡張子 (ドット付き) を取り出す
UnicodeString ExtOf(const UnicodeString &full_path)
{
	return get_extension(full_path);
}

/// 種別ごとの詳細情報を1つ追加する (該当なしなら何もしない)
void AppendTypeSpecificLines(const UnicodeString &full_path, const UnicodeString &fext, TStringList *lst)
{
	if (test_IcoExt(fext) || test_CurExt(fext)) {
		get_IconInf(full_path, lst);
	}
	else if (test_AniExt(fext)) {
		get_AniInf(full_path, lst);
	}
	else if (SameText(fext, _T(".webp"))) {
		get_WebpInf(full_path, lst);
	}
	else if (test_PspExt(fext)) {
		get_PspInf(full_path, lst);
	}
	else if (test_MetaExt(fext)) {
		get_MetafileInf(full_path, lst);
	}
	// Exif/PNG/GIF は本来 WIC 経由でも表示できるかの判定 (is_ViewableFext /
	// usr_SH->get_PropInf) が絡むが、usr_SH (UserShell) が未移植のため、
	// ここでは拡張子から直接 Exif→PNG→GIF の優先順で1つだけ試す
	// (簡略化。gui/file_info.h の「対象外にした種別」を参照)
	else if (test_ExifExt(fext)) {
		get_ExifInf(full_path, lst);
		if (test_JpgExt(fext)) get_JpgExInf(full_path, lst);
	}
	else if (test_PngExt(fext)) {
		get_PngInf(full_path, lst);
	}
	else if (test_GifExt(fext)) {
		get_GifInf(full_path, lst);
	}
	else if (SameText(fext, _T(".wav"))) {
		get_WavInf(full_path, lst);
	}
	else if (test_Mp3Ext(fext)) {
		ID3_GetInf(full_path, lst);
	}
	else if (test_FlacExt(fext)) {
		get_FlacInf(full_path, lst);
	}
	else if (SameText(fext, _T(".opus"))) {
		get_OpusInf(full_path, lst);
	}
	else if (SameText(fext, _T(".cda"))) {
		get_CdaInf(full_path, lst);
	}
	else if (test_FileExt(fext, _T(".pdf"))) {
		get_PdfVer(full_path, lst);
	}
	else if (test_HtmlExt(fext)) {
		get_HtmlInf(full_path, lst);
	}
	else if (test_FileExt(fext, _T(".cbproj.dproj.cpp.pas.dfm.fmx.h"))) {
		get_BorlandInf(full_path, lst);
	}
	else if (SameText(ExtractFileName(full_path), _T("tags"))) {
		get_TagsInf(full_path, lst);
	}
	else if (test_ExeExt(fext)) {
		// get_AppInf は usr_SH (UserShell、未移植) に依存するため使えない。
		// 実行可能ファイルであることだけを知らせる (gui/file_info.h 参照)
		lst->Add(_T("実行可能ファイルです (詳細情報は未対応)"));
	}
}

}  // namespace

//---------------------------------------------------------------------------
void BuildFileInfoLines(const UnicodeString &full_path, const FileItem &item, TStringList *lst)
{
	lst->Add(_T("名前: ") + item.name);
	lst->Add(_T("パス: ") + full_path);
	lst->Add(_T("種類: ") + UnicodeString(item.is_dir ? _T("ディレクトリ") : _T("ファイル")));
	if (!item.is_dir) {
		UnicodeString size_line;
		size_line.sprintf(_T("サイズ: %s (%s バイト)"),
		                   get_size_str_G(item.size, 0, 2).Trim().c_str(),
		                   get_size_str_B(item.size, 0).Trim().c_str());
		lst->Add(size_line);
	}
	lst->Add(_T("更新日時: ") + FormatDateTime(_T("yyyy/mm/dd hh:nn:ss"), item.stamp));
	lst->Add(_T("属性: ") + get_file_attr_str(item.attr));

	if (item.is_dir) return;  // 種別ごとの詳細情報はファイルのみ対象

	const int ads_cnt = get_ADS_count(full_path);
	if (ads_cnt > 0) {
		lst->Add(EmptyStr);
		UnicodeString ads_line;
		ads_line.sprintf(_T("代替データストリーム: %d 件"), ads_cnt);
		lst->Add(ads_line);
		get_ADS_Inf(full_path, lst);
	}

	const UnicodeString fext = ExtOf(full_path);
	const int before = lst->Count;
	AppendTypeSpecificLines(full_path, fext, lst);
	if (lst->Count > before) lst->Insert(before, EmptyStr);  // 区切りの空行
}

//---------------------------------------------------------------------------
void AppendHashLines(const UnicodeString &full_path, TStringList *lst)
{
	lst->Add(EmptyStr);
	lst->Add(_T("SHA256: ") + get_HashStr(full_path, _T("SHA256")));
	lst->Add(_T("CRC32: ") + get_HashStr(full_path, _T("CRC32")));
}

namespace file_info {

namespace {

TableFormat detect_format(const std::vector<UnicodeString> &rows)
{
	return !rows.empty() && ContainsStr(rows.front(), _T("\t"))
		? TableFormat::Tsv
		: TableFormat::Csv;
}

UnicodeString field_at(const UnicodeString &row, int column, TableFormat format)
{
	if (format == TableFormat::Tsv) {
		const TStringDynArray fields = split_strings_tab(row);
		return column >= 0 && column < fields.Length? fields[column] : EmptyStr;
	}
	const TStringDynArray fields = get_csv_array(row, 99);
	return column >= 0 && column < fields.Length? fields[column] : EmptyStr;
}

int column_count(const std::vector<UnicodeString> &rows, TableFormat format)
{
	int count = 0;
	for (const UnicodeString &row : rows) {
		int current = 0;
		if (format == TableFormat::Tsv) current = split_strings_tab(row).Length;
		else current = get_csv_array(row, 99).Length;
		count = std::max(count, current);
	}
	return count;
}

bool parse_number(const UnicodeString &source, long double &value, int &decimal_places)
{
	const UnicodeString text = extract_top_num_str(source);
	if (text.IsEmpty()) return false;
	try {
		value = text.ToDouble();
	}
	catch (...) {
		return false;
	}
	const int dot = text.Pos(_T("."));
	decimal_places = dot > 0? text.Length() - dot : 0;
	return true;
}

int numeric_count_in_column(const std::vector<UnicodeString> &rows, int column,
                            bool top_is_header, TableFormat format)
{
	int count = 0;
	for (std::size_t i = top_is_header? 1 : 0; i < rows.size(); ++i) {
		long double value = 0.0L;
		int decimals = 0;
		if (parse_number(field_at(rows[i], column, format), value, decimals)) ++count;
	}
	return count;
}

UnicodeString format_value(long double value, int decimals)
{
	return ldouble_to_str(value, decimals);
}

}  // namespace

//---------------------------------------------------------------------------
int ResolveNumericColumn(const std::vector<UnicodeString> &rows, int preferred_column,
                         bool top_is_header)
{
	if (rows.empty() || (top_is_header && rows.size() < 2)) return -1;
	const TableFormat format = detect_format(rows);
	if (preferred_column >= 0) {
		return numeric_count_in_column(rows, preferred_column, top_is_header, format) > 0
			? preferred_column
			: -1;
	}
	for (int column = 0; column < column_count(rows, format); ++column) {
		if (numeric_count_in_column(rows, column, top_is_header, format) > 0) return column;
	}
	return -1;
}

//---------------------------------------------------------------------------
ColumnStats AnalyzeColumn(const std::vector<UnicodeString> &rows, int column, bool top_is_header)
{
	ColumnStats stats;
	if (rows.empty()) {
		stats.error = _T("有効な数値項目がありません");
		return stats;
	}

	stats.format = detect_format(rows);
	stats.column = column;
	const std::size_t first = top_is_header? 1 : 0;
	if (column < 0 || first >= rows.size()) {
		stats.error = _T("有効な数値項目がありません");
		return stats;
	}

	if (top_is_header) {
		const UnicodeString name = field_at(rows.front(), column, stats.format);
		stats.item_name = name.IsEmpty()
			? UnicodeString().sprintf(_T("項目%d"), column + 1)
			: name;
	}
	else {
		stats.item_name.sprintf(_T("項目%d"), column + 1);
	}

	std::vector<long double> values;
	for (std::size_t i = first; i < rows.size(); ++i) {
		long double value = 0.0L;
		int decimals = 0;
		if (!parse_number(field_at(rows[i], column, stats.format), value, decimals)) continue;
		values.push_back(value);
		stats.total += value;
		stats.decimal_places = std::max(stats.decimal_places, decimals);
	}
	if (values.empty()) {
		stats.error = _T("有効な数値項目がありません");
		return stats;
	}

	std::sort(values.begin(), values.end());
	stats.count = static_cast<int>(values.size());
	stats.minimum = values.front();
	stats.maximum = values.back();
	stats.average = stats.total / stats.count;
	stats.median = (stats.count % 2 == 1)
		? values[(stats.count - 1) / 2]
		: (values[stats.count / 2 - 1] + values[stats.count / 2]) / 2.0L;
	for (long double value : values) {
		const long double delta = value - stats.average;
		stats.variance += delta * delta;
	}
	stats.variance /= stats.count;
	stats.standard_deviation = std::sqrt(stats.variance);

	int classes = 1 + static_cast<int>(std::log10(static_cast<long double>(stats.count))
	                                    / std::log10(2.0L) + 0.5L);
	classes = std::max(1, classes);
	const long double width = (stats.maximum - stats.minimum) / classes;
	stats.histogram.resize(classes);
	for (int i = 0; i < classes; ++i) {
		long double lower = stats.minimum + width * i;
		if (stats.decimal_places == 0) lower = std::floor(lower + 0.5L);
		stats.histogram[i].lower = lower;
	}
	for (long double value : values) {
		bool assigned = false;
		for (int i = 0; i < classes; ++i) {
			long double upper = stats.minimum + width * (i + 1);
			if (stats.decimal_places == 0) upper = std::floor(upper + 0.5L);
			if ((i == classes - 1 && value <= stats.maximum) || value < upper) {
				stats.histogram[i].count++;
				assigned = true;
				break;
			}
		}
		if (!assigned) stats.histogram.back().count++;
	}
	int cumulative = 0;
	for (HistogramBin &bin : stats.histogram) {
		cumulative += bin.count;
		bin.cumulative = static_cast<double>(cumulative) / stats.count;
	}

	stats.valid = true;
	return stats;
}

//---------------------------------------------------------------------------
std::vector<UnicodeString> BuildColumnStatLines(const ColumnStats &stats)
{
	std::vector<UnicodeString> lines;
	if (!stats.valid) {
		lines.push_back(stats.error);
		return lines;
	}

	lines.push_back(_T("項目名"));
	lines.push_back(stats.item_name);
	lines.push_back(EmptyStr);
	lines.push_back(_T("有効項目数"));
	lines.push_back(UnicodeString().sprintf(_T("%d"), stats.count));
	lines.push_back(_T("合計値"));
	lines.push_back(format_value(stats.total, stats.decimal_places));
	lines.push_back(_T("最小値"));
	lines.push_back(format_value(stats.minimum, stats.decimal_places));
	lines.push_back(_T("最大値"));
	lines.push_back(format_value(stats.maximum, stats.decimal_places));
	lines.push_back(EmptyStr);
	lines.push_back(_T("平均値"));
	lines.push_back(format_value(stats.average, stats.decimal_places + 1));
	lines.push_back(_T("中央値"));
	lines.push_back(format_value(stats.median, stats.decimal_places));
	lines.push_back(_T("分散"));
	lines.push_back(format_value(stats.variance, stats.decimal_places + 1));
	lines.push_back(_T("標準偏差"));
	lines.push_back(format_value(stats.standard_deviation, stats.decimal_places + 1));
	if (!stats.histogram.empty()) {
		lines.push_back(EmptyStr);
		lines.push_back(_T("度数分布"));
		for (const HistogramBin &bin : stats.histogram) {
			UnicodeString cumulative;
			cumulative.sprintf(_T("%.3f"), bin.cumulative);
			lines.push_back(format_value(bin.lower, stats.decimal_places) + _T("\t")
			                + UnicodeString().sprintf(_T("%d"), bin.count) + _T("\t") + cumulative);
		}
	}
	return lines;
}

}  // namespace file_info
