/**
 * @file gui/cmd_list.h
 * @brief コマンドファイル一覧の判断ロジック (wx 非依存)
 *
 * @details VCL 版の `src/CmdListDlg.cpp` (`TCmdFileListDlg`) の実測:
 *          - `src/MainFrm.cpp:14426-14432` が `CmdFileList` から開く
 *          - `src/OptDlg.cpp:2615-2633,3693-3697,4155-4160` と
 *            `src/BtnDlg.cpp:209-211` が選択用の補助画面として開く
 *          - `src/CmdListDlg.cpp:192-254` が .nbt の列挙・フィルタ
 *
 *          wx の一覧 UI は `gui/cmd_list_dialog.h`。ここには、入力された
 *          コマンドファイルパスの正規化、フィルタ、選択位置、実行要求の
 *          判定だけを置く。
 *
 *          未移植 (未実装扱い):
 *          - VCL の TStringGrid のOwnerDraw/列幅/ソートヘッダ
 *          - ファイル情報の詳細取得と参照元の内部設定追随
 *          - 改名・削除後の全設定 (KeyFuncList/ExtMenuList 等) の一括更新
 */
#ifndef NYANFI_GUI_CMD_LIST_H
#define NYANFI_GUI_CMD_LIST_H

#include <vector>

#include "usr_str.h"

namespace cmd_list {

/// 一覧の1行。VCL の file_rec から UI が必要にする部分だけを持つ。
struct Entry {
	UnicodeString path;
	UnicodeString name;
	UnicodeString description;
	Int64 size = 0;
	int run_count = 0;
};

/// ダイアログの入力モード。
struct Options {
	bool to_filter = false;
	bool select_only = false;
	bool confirm_execute = false;  //!< CmdFileListCnfExe (VCL のフィルタ確定実行)
	bool preview = false;           //!< CmdFileListPreview (VCL のプレビュー表示)
	UnicodeString initial_file;
};

/// ダイアログが返す操作。
enum class Action {
	None,
	Execute,
	Edit,
	Preview,
};

/// 結果。
struct Result {
	Action action = Action::None;
	UnicodeString path;
	bool preview = false;
	bool confirm_execute = false;
	bool fuzzy = false;
};

/// フィルタ設定。
struct FilterOptions {
	bool fuzzy = false;
	bool regex = false;
	bool case_sensitive = false;
};

/// .nbt かどうか。
bool IsCommandFile(const UnicodeString &path);

/// 先頭の @ と引用符を外す。VCL get_cmdfile/exclude_quot の path 部分。
UnicodeString NormalizeCommandPath(const UnicodeString &path);

/// VCL の `ExeCommands_"@file"` を作る。
UnicodeString MakeExecutionCommand(const UnicodeString &path);

/// VCL の `FileEdit_"file"` を作る。
UnicodeString MakeEditCommand(const UnicodeString &path);

/// 名前+説明文をフィルタする。
std::vector<Entry> FilterEntries(const std::vector<Entry> &entries,
                                 const UnicodeString &filter,
                                 const FilterOptions &options = {});

/// パスの自然順比較 (VCL NaturalOrder=true の UI 部分)。
int CompareNatural(const Entry &a, const Entry &b);

/// 初期ファイルの表示位置。見つからなければ -1。
int FindSelected(const std::vector<Entry> &entries, const UnicodeString &path);

/// VCL の「確定即実行」条件。select_only のときは実行しない。
bool ShouldAutoExecute(bool select_only, bool confirm_execute, bool filter_focused,
                       bool filter_empty, int visible_count);

}  // namespace cmd_list

#endif  // NYANFI_GUI_CMD_LIST_H
