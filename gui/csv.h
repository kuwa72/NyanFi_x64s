/**
 * @file gui/csv.h
 * @brief CSV/TSV 項目グラフ・レコード表示・エクスポートの判断ロジック (wx 非依存)
 *
 * @details VCL 版の該当 (いずれも src を実測):
 *   - CsvGraph: src/MainFrm.cpp:33847-33853 (CsvGraphActionExecute)。
 *     GraphForm に TxtViewer の TxtBufList/TopIsHeader/CsvCol を渡して
 *     ShowModal する。GraphForm 自体 (src/GraphFrm.cpp) は UI 依存のため
 *     ここでは「グラフ用データの抽出」だけを純関数として持つ
 *   - CsvRecord: src/MainFrm.cpp:33865-33876 (CsvRecordActionExecute)。
 *     CsvRecForm の表示トグル (SetToggleAction)。isBinary なら強制非表示
 *   - ExportCsv: src/MainFrm.cpp:34000-34005 (ExportCsvActionExecute)。
 *     ExpCsvDlg を ShowModal。TSV 判定 (1行目に \t を含むか) と
 *     引用符既定 (QuotCheckBox=true) は src/ExpCsv.cpp:38-53 を実測
 *
 * tests/core/test_gui_csv.cpp から直接テストできる
 * (nyanfi_gui_core、ルート CMakeLists.txt に追加)。
 */
#ifndef NYANFI_GUI_CSV_H
#define NYANFI_GUI_CSV_H

#include <vector>

#include "usr_str.h"

namespace csv {

//---------------------------------------------------------------------------
// グラフ用データ (CsvGraph)
//---------------------------------------------------------------------------

/**
 * @brief グラフ用に抽出した数値列
 * @details VCL の GraphForm が受け取るデータ (DataList/TopIsHeader/CsvCol)
 *          のうち、純粋ロジックで扱える部分
 */
struct GraphData {
	int column = -1;                        //!< 抽出した列 (-1 なら範囲外)
	std::vector<UnicodeString> labels;      //!< ラベル列 (1列目、ヘッダ行を除く)
	std::vector<double> values;             //!< 数値列の値
};

/**
 * @brief 2次元配列からグラフ用数値列を抽出する
 * @details VCL の GraphForm が行う「数値列の抽出」を純関数化したもの。
 *          数値に変換できない行はスキップする (VCL の GraphFrm.cpp の
 *          StrToFloat 失敗時と同じ)
 * @param rows 2次元配列 (各行が1レコード)
 * @param column 抽出する列 (0-based)
 * @param top_is_header 先頭行をヘッダとして扱うか (VCL の TopIsHeader)
 * @return 抽出結果。column が範囲外なら column=-1 で空
 */
GraphData ExtractGraphData(const std::vector<std::vector<UnicodeString>> &rows,
                           int column, bool top_is_header);

//---------------------------------------------------------------------------
// レコード表示のトグル (CsvRecord)
//---------------------------------------------------------------------------

/**
 * @brief レコード表示の表示状態をトグルする
 * @details VCL の CsvRecordActionExecute (src/MainFrm.cpp:33865-33876) と
 *          SetToggleAction (src/MainFrm.cpp:12651) を実測。
 *          param が "ON" なら true、"OFF" なら false、それ以外 (空を含む)
 *          は反転する
 * @param visible 現在の表示状態
 * @param param アクションパラメータ
 */
bool ToggleCsvRecord(bool visible, const UnicodeString &param);

//---------------------------------------------------------------------------
// エクスポート設定 (ExportCsv)
//---------------------------------------------------------------------------

/**
 * @brief CSV/TSV エクスポート設定
 * @details VCL の ExpCsvDlg (src/ExpCsv.cpp:38-53) の FormShow を実測
 */
struct ExportSettings {
	bool is_tsv = false;  //!< TSV (タブ区切り) か
	bool quote = true;    //!< 引用符を付けるか (QuotCheckBox 既定 true)
};

/**
 * @brief エクスポート設定を解釈する
 * @param param アクションパラメータ ("NOQUOT" で引用符なし)
 * @param is_tsv 1行目に \t を含むか (VCL の TSV 判定と同じ)
 */
ExportSettings ParseExportSettings(const UnicodeString &param, bool is_tsv);

}  // namespace csv

#endif  // NYANFI_GUI_CSV_H
