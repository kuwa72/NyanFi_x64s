/**
 * @file gui/csv.cpp
 * @brief gui/csv.h の実装 (wx 非依存)
 */
#include "gui/csv.h"

#include <cwchar>

namespace csv {

namespace {

/**
 * @brief UnicodeString を double に変換する (VCL の StrToFloat 相当)
 * @details 変換失敗は false を返す (空文字列・非数値・末尾に余計な文字がある場合)
 */
bool TryParseDouble(const UnicodeString &s, double &out)
{
	if (s.IsEmpty()) return false;
	const wchar_t *begin = s.c_str();
	wchar_t *end = nullptr;
	const double v = std::wcstod(begin, &end);
	if (end == begin) return false;  // 変換できなかった
	// 末尾の空白は許す (VCL の StrToFloat と同じ)
	while (*end == L' ' || *end == L'\t') ++end;
	if (*end != L'\0') return false;  // 余計な文字がある
	out = v;
	return true;
}

}  // namespace

//---------------------------------------------------------------------------
// グラフ用データ (CsvGraph)
// VCL: src/MainFrm.cpp:33847-33853 (CsvGraphActionExecute)、
//      src/GraphFrm.cpp (GraphForm へのデータ設定)
//---------------------------------------------------------------------------
GraphData ExtractGraphData(const std::vector<std::vector<UnicodeString>> &rows,
                           int column, bool top_is_header)
{
	GraphData g;
	if (rows.empty()) return g;

	// 列が範囲外なら空で返す
	if (column < 0 || column >= static_cast<int>(rows[0].size())) return g;
	g.column = column;

	const std::size_t start = top_is_header ? 1 : 0;
	for (std::size_t i = start; i < rows.size(); ++i) {
		const std::vector<UnicodeString> &row = rows[i];
		if (column >= static_cast<int>(row.size())) continue;

		// 数値に変換できない行はスキップ (VCL の StrToFloat 失敗時と同じ)
		const UnicodeString &val = row[column];
		if (val.IsEmpty()) continue;

		// ラベルは1列目 (VCL の GraphForm は1列目をラベルとして使う)
		const UnicodeString label = row.empty() ? EmptyStr : row[0];

		// 数値変換 (VCL の StrToFloat 相当。変換失敗はスキップ)
		// UnicodeString に ToDouble が無いため wcstod で変換する
		double num = 0;
		if (!TryParseDouble(val, num)) continue;

		g.labels.push_back(label);
		g.values.push_back(num);
	}
	return g;
}

//---------------------------------------------------------------------------
// レコード表示のトグル (CsvRecord)
// VCL: src/MainFrm.cpp:33865-33876 (CsvRecordActionExecute)、
//      src/MainFrm.cpp:12651 (SetToggleAction)
//---------------------------------------------------------------------------
bool ToggleCsvRecord(bool visible, const UnicodeString &param)
{
	// VCL: SetToggleAction (src/MainFrm.cpp:12651)
	// param が "ON" なら true、"OFF" なら false、それ以外は反転
	if (SameText(param, _T("ON"))) return true;
	if (SameText(param, _T("OFF"))) return false;
	return !visible;
}

//---------------------------------------------------------------------------
// エクスポート設定 (ExportCsv)
// VCL: src/ExpCsv.cpp:38-53 (FormShow の TSV 判定・QuotCheckBox)
//---------------------------------------------------------------------------
ExportSettings ParseExportSettings(const UnicodeString &param, bool is_tsv)
{
	ExportSettings s;
	s.is_tsv = is_tsv;
	// VCL: QuotCheckBox 既定 true (src/ExpCsv.cpp:44)
	s.quote = true;
	// "NOQUOT" で引用符なし
	if (SameText(param, _T("NOQUOT"))) s.quote = false;
	return s;
}

}  // namespace csv
