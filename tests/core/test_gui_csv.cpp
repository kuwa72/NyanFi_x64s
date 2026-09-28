/**
 * @file tests/core/test_gui_csv.cpp
 * @brief gui/csv.h (CSV/TSV 項目グラフ・レコード表示・エクスポートの判断) の回帰テスト
 *
 * @details gui/csv.h は wx に依存しない (nyanfi_gui_core、ルート CMakeLists.txt 参照) ため、
 * GUI (wxWidgets) 無しでもここでテストできる。
 */
#include "doctest/doctest.h"

#include <vector>

#include "gui/csv.h"

//===========================================================================
// ExtractGraphData: グラフ用数値列の抽出 (CsvGraph)
// VCL: src/MainFrm.cpp:33847-33853 (CsvGraphActionExecute)、
//      src/GraphFrm.cpp (GraphForm へのデータ設定)
//===========================================================================

TEST_CASE("ExtractGraphData: 先頭行をヘッダとして数値列を抽出")
{
	const std::vector<std::vector<UnicodeString>> rows = {
		{_T("名前"), _T("値")},
		{_T("A"), _T("10")},
		{_T("B"), _T("20")},
		{_T("C"), _T("30")},
	};
	const csv::GraphData g = csv::ExtractGraphData(rows, 1, true);
	CHECK(g.column == 1);
	CHECK(g.labels.size() == 3);
	CHECK(g.labels[0] == UnicodeString(_T("A")));
	CHECK(g.values.size() == 3);
	CHECK(g.values[0] == 10.0);
	CHECK(g.values[2] == 30.0);
}

TEST_CASE("ExtractGraphData: ヘッダなしで全行をデータとして扱う")
{
	const std::vector<std::vector<UnicodeString>> rows = {
		{_T("A"), _T("10")},
		{_T("B"), _T("20")},
	};
	const csv::GraphData g = csv::ExtractGraphData(rows, 1, false);
	CHECK(g.labels.size() == 2);
	CHECK(g.values.size() == 2);
}

TEST_CASE("ExtractGraphData: 数値でない行はスキップされる")
{
	const std::vector<std::vector<UnicodeString>> rows = {
		{_T("名前"), _T("値")},
		{_T("A"), _T("10")},
		{_T("B"), _T("abc")},
		{_T("C"), _T("30")},
	};
	const csv::GraphData g = csv::ExtractGraphData(rows, 1, true);
	CHECK(g.labels.size() == 2);
	CHECK(g.values.size() == 2);
}

TEST_CASE("ExtractGraphData: 列が範囲外なら column=-1 で空")
{
	const std::vector<std::vector<UnicodeString>> rows = {
		{_T("A"), _T("10")},
	};
	const csv::GraphData g = csv::ExtractGraphData(rows, 5, true);
	CHECK(g.column == -1);
	CHECK(g.labels.empty());
	CHECK(g.values.empty());
}

//===========================================================================
// ToggleCsvRecord: レコード表示のトグル (CsvRecord)
// VCL: src/MainFrm.cpp:33865-33876 (CsvRecordActionExecute)、
//      src/MainFrm.cpp:12651 (SetToggleAction)
//===========================================================================

TEST_CASE("ToggleCsvRecord: 空パラメータは反転、ON/OFF は明示")
{
	CHECK(csv::ToggleCsvRecord(false, EmptyStr) == true);
	CHECK(csv::ToggleCsvRecord(true, EmptyStr) == false);
	CHECK(csv::ToggleCsvRecord(false, _T("ON")) == true);
	CHECK(csv::ToggleCsvRecord(true, _T("ON")) == true);
	CHECK(csv::ToggleCsvRecord(true, _T("OFF")) == false);
	CHECK(csv::ToggleCsvRecord(false, _T("OFF")) == false);
}

//===========================================================================
// ParseExportSettings: エクスポート設定 (ExportCsv)
// VCL: src/ExpCsv.cpp:38-53 (FormShow の TSV 判定・QuotCheckBox)
//===========================================================================

TEST_CASE("ParseExportSettings: タブ区切りなら TSV、引用符既定 true")
{
	const csv::ExportSettings s1 = csv::ParseExportSettings(EmptyStr, false);
	CHECK(s1.is_tsv == false);
	CHECK(s1.quote == true);

	const csv::ExportSettings s2 = csv::ParseExportSettings(EmptyStr, true);
	CHECK(s2.is_tsv == true);
	CHECK(s2.quote == true);
}

TEST_CASE("ParseExportSettings: NOQUOT で引用符なし")
{
	const csv::ExportSettings s = csv::ParseExportSettings(_T("NOQUOT"), false);
	CHECK(s.quote == false);
}
