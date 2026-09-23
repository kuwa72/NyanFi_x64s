/**
 * @file gui/distribution.h
 * @brief 振り分けダイアログの判断ロジック (wx 非依存)
 *
 * @details VCL 版の `src/DistrDlg.cpp` (`TDistributionDlg`) の登録項目・
 *          マスク照合・プレビュー生成・同名処理の解決だけをここに置く。
 *          wx の入力/表示は `gui/distribution_dialog.h` が担当する。
 *
 *          実測した VCL 呼び出し:
 *          - `src/MainFrm.cpp:16369-16419` が DistributionDlg を開き、
 *            `src/DistrDlg.cpp:278-407` で item/rule をプレビューする。
 *          - `src/DistrDlg.cpp:650-661` が正規表現/マスクの照合を行う。
 *
 *          未移植 (未実装扱い):
 *          - `\\DT`/`\\TS`/`\\XT`/`\\Z` を使うファイル名書式の日時・連番処理
 *          - VCL の進捗バー、リストファイル右クリック編集、ドラッグ並べ替え
 *          - 実タスクの起動 (入力結果の解決までは `BuildPreview` で固定する)
 */
#ifndef NYANFI_GUI_DISTRIBUTION_H
#define NYANFI_GUI_DISTRIBUTION_H

#include <functional>
#include <vector>

#include "gui/file_ops.h"
#include "usr_file_ex.h"
#include "usr_str.h"

namespace distribution {

/// VCL の SameNameComboBox の並び (0 始まり)。
enum class CopyMode {
	Overwrite = 0,   //!< 強制上書き
	Newest = 1,      //!< 最新なら上書き
	Skip = 2,        //!< スキップ
	AutoRename = 3,  //!< 自動的に名前を変更
};

/// 登録1件。VCL の DistrDefList の CSV 4項目に同じ順序で並べる。
struct Rule {
	UnicodeString title;
	bool enabled = false;
	UnicodeString mask;
	UnicodeString destination;
};

/// プレビュー対象。VCL の ItemList の1行。
struct Input {
	UnicodeString path;
	bool is_dir = false;
};

/// プレビュー結果の1行。
struct PreviewItem {
	UnicodeString source;
	UnicodeString destination;
	bool is_dir = false;
	bool skipped = false;
};

/// ダイアログ全体の設定。
struct Options {
	UnicodeString opposite_path;
	bool create_directories = true;
	CopyMode copy_mode = CopyMode::Skip;
	bool move = false;  //!< true=移動 / false=コピー (VCL IsMove)
};

/// プレビュー集計。
struct Preview {
	std::vector<PreviewItem> items;
	int matched = 0;
	int skipped = 0;
	int directories = 0;
	int files = 0;
};

/// 登録CSVを構造体へ変換する。
Rule ParseRule(const UnicodeString &record);

/// 構造体を VCL 互換のCSV文字列へ変換する。
UnicodeString MakeRuleRecord(const Rule &rule);

/// /.../ 形式かどうか。
bool IsRegexMask(const UnicodeString &mask);

/// マスクが空でなく、正規表現なら構文も正しいか。
bool IsValidMask(const UnicodeString &mask, UnicodeString &error_out);

/// ワイルドカード/正規表現マスクをファイル名へ照合する。
bool MatchMask(const UnicodeString &mask, const UnicodeString &name);

/// VCL の \A/\E/\C 等の基本ファイル名書式を展開し、宛先 directories を返す。
/// \DT/\TS/\XT/\Z は未移植 (未実装扱い) なので入力文字列のまま残す。
UnicodeString FormatDestination(const UnicodeString &format, const UnicodeString &source,
                                const UnicodeString &opposite_path);

/// 同名処理の ComboBox 番号を既存 file_ops の方式へ変換する。
file_ops::ConflictPolicy ConflictPolicyFor(CopyMode mode);

/// VCL の UpdatePreview 相当。最初の有効登録だけを使う。
/// directory_exists は「作成しない場合に振分先が存在するか」の判定用。
/// 未指定なら同一パスのみ Skip 扱いにする (ファイルI/O を持たない)。
using DirectoryExists = std::function<bool(const UnicodeString &)>;
Preview BuildPreview(const std::vector<Input> &items, const std::vector<Rule> &rules,
                     const Options &options, const DirectoryExists &directory_exists = {});

/// 登録追加ボタンの入力検証 (VCL AddRegActionUpdate の判定)。
bool CanAddRule(bool registration_enabled, const UnicodeString &title,
                const UnicodeString &mask, const UnicodeString &destination,
                int rule_count, UnicodeString &error_out);

/// 同じ登録 (**mask と destination**) が既にあるか。
bool HasDuplicateRule(const std::vector<Rule> &rules, const UnicodeString &mask,
                      const UnicodeString &destination);

/// 同一タイトルのチェック状態をまとめて設定する (GroupCheckAction)。
std::vector<bool> GroupChecked(const std::vector<Rule> &rules, int selected,
                               bool checked);

}  // namespace distribution

#endif  // NYANFI_GUI_DISTRIBUTION_H
