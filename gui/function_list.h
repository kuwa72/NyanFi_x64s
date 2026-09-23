/**
 * @file gui/function_list.h
 * @brief 関数・ユーザー定義文字列・マーク行一覧の判断ロジック (wx 非依存)
 *
 * @details VCL 版の `src/FuncDlg.cpp` (`TFuncListDlg`) の実測:
 *          - `src/MainFrm.cpp:33040-33045` が FunctionList/UserDefList/MarkList を振り分け
 *          - `src/MainFrm.cpp:33600-33637` が3モードの入力と表示条件を決める
 *          - `src/FuncDlg.cpp:120-195` が一覧を作り、206-255 がフィルタする
 *          - `src/FuncDlg.cpp:578-594` がコピー/保存用の表示文字列を作る
 *
 *          wx のダイアログは `gui/function_list_dialog.h`、一覧の表示と入力は
 *          そちらで受け持つ。ここでは行データとモード/フィルタ/選択の判断だけを
 *          固定する。
 *
 *          未移植 (未実装扱い):
 *          - Migemo 辞書を使うあいまい検索 (通常の fuzzy/regex 判定は実装)
 *          - VCL の全文強調描画、ソース/ヘッダ切替、複雑な GetFuncPtns の完全再現
 */
#ifndef NYANFI_GUI_FUNCTION_LIST_H
#define NYANFI_GUI_FUNCTION_LIST_H

#include <vector>

#include "usr_str.h"

namespace function_list {

/// VCL の ListMode (0/1/2)。
enum class Mode {
	Function = 0,
	UserDefined = 1,
	MarkLine = 2,
};

/// 1行と元の行番号。
struct Entry {
	UnicodeString text;
	int line_no = -1;
};

/// wx 側へ渡す文書スナップショット。
struct Source {
	UnicodeString file_name;
	std::vector<UnicodeString> lines;
	std::vector<int> marks;
	int current_line = -1;
	UnicodeString function_pattern;
	UnicodeString user_pattern;
	UnicodeString name_pattern;
	bool is_dfm = false;
	bool user_regex = false;
};

/// コマンドの FF/FZ パラメータ。
struct CommandOptions {
	bool to_filter = false;
	bool fuzzy = false;
};

/// ダイアログの初期表示オプション。
struct Options {
	Mode mode = Mode::Function;
	bool to_filter = false;
	bool fuzzy = false;
	bool regex = false;
	bool name_only = false;
	bool link = true;
	UnicodeString user_pattern;
};

/// 範囲外の ListMode を安全に正規化する。
Mode NormalizeMode(int mode);

/// モードの表示名。
UnicodeString ModeTitle(Mode mode);

/// 入力パラメータ (FF/FZ) を解決する。
CommandOptions ParseCommandOptions(const UnicodeString &param);

/// そのモードを VCL が表示するか。
bool IsAvailable(Mode mode, bool is_text, bool has_marks);

/// 3モードの一覧を作る。function_pattern が空なら保守的な簡易判定を使う。
std::vector<Entry> BuildEntries(const Source &source, Mode mode);

/// フィルタ指定 (fuzzy/regex/case_sensitive) で一覧を絞る。
struct FilterOptions {
	bool fuzzy = false;
	bool regex = false;
	bool case_sensitive = false;
};
std::vector<Entry> FilterEntries(const std::vector<Entry> &entries,
                                 const UnicodeString &filter,
                                 const FilterOptions &options = {});

/// 名前だけを表示する (NameOnlyAction) 場合の文字列。
UnicodeString NameOnlyText(const UnicodeString &text, bool name_only,
                           const UnicodeString &name_pattern);

/// 現在の行に最も近い一覧項目を返す (VCL UpdateList の idx 計算)。
int SelectNearest(const std::vector<Entry> &entries, int line_no);

}  // namespace function_list

#endif  // NYANFI_GUI_FUNCTION_LIST_H
