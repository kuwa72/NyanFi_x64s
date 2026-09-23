/**
 * @file gui/key.h
 * @brief キー割り当て一覧の行生成・絞り込み・並べ替え (wx 非依存)
 *
 * @details VCL の `TKeyListDlg` は `src/KeyDlg.cpp:20-550`、一覧のタブは
 *          `ScrModeIdStr = "FSVIL"` に合わせて File/Search/Text/Image/List の
 *          5面、列は「キー/コマンド/説明」である。wx 版は既存の
 *          `gui/key_map.*` (F 面の割り当て) と `set_CmdList` の実在コマンド表を
 *          入力にし、表示/絞り込み/並べ替えだけをここに置く。
 *
 *          実測して決めた呼び出し箇所:
 *          - ダイアログの起動と選択後の実行: `src/MainFrm.cpp:20464-20485`
 *          - 一覧生成/タブ切替/検索/ソート: `src/KeyDlg.cpp:97-201`
 *          - F1 の既定割り当て表示: `src/KeyDlg.cpp:197-239`
 *
 *          未移植 (未実装扱い):
 *          - S/V/I/L 面のユーザー ini からの KeyFuncList 読み込み
 *          - 2 ストローク/SELECT+ キー、Migemo辞書、コマンドヘルプの実処理
 *          - VCL のグリッド幅/位置保存 (wx 専用 ini に状態だけを保存する)
 */
#ifndef NYANFI_GUI_KEY_H
#define NYANFI_GUI_KEY_H

#include <cstddef>
#include <vector>

#include "usr_str.h"

class UsrIniFile;

namespace key {

enum class Tab { File = 0, Search, Text, Image, List };
enum class SortMode { Key = 0, Command, Description };

struct Entry {
	UnicodeString key;
	UnicodeString command;
	UnicodeString description;
	Tab tab = Tab::File;  //!< VCL の ScrModeIdStr に対応する面
};

/// タブのラベル (VCL KeyTabControl Tabs.Strings と同じ)
UnicodeString TabLabel(Tab tab);

/// usr_cmdlist の行名 `F:Command=説明` からモード文字と項目を分ける。
bool ParseCommandRecord(const UnicodeString &record, UnicodeString &mode_out,
                        UnicodeString &command_out, UnicodeString &description_out);

/// KeyFuncList の `key=command` を Entry にする。説明は呼び出し側が渡す。
bool ParseAssignment(const UnicodeString &line, const UnicodeString &description,
                     Entry &entry_out);

/// VCL の `+`/`~` 表記を表示用に整形する。
UnicodeString FormatKeyForDisplay(const UnicodeString &key);

/// キー・コマンド・説明のいずれかに検索語が含まれるか。
bool MatchesFilter(const Entry &entry, const UnicodeString &filter);

/// 安定並べ替え。sort_mode の順で、同一キーは元の順序を保つ。
void SortEntries(std::vector<Entry> &entries, SortMode mode);

/// 割り当てもコマンド表もない行を、未登録コマンド表示の指定に従って統合する。
/// commands の command が基準。commands にない孤立した assignment は残す。
std::vector<Entry> BuildRows(const std::vector<Entry> &assignments,
                             const std::vector<Entry> &commands,
                             bool show_all);

/// フィルタを適用して並べ替えた表示行を返す。
std::vector<Entry> FilterAndSort(const std::vector<Entry> &entries,
                                 const UnicodeString &filter, SortMode mode);

/// VCL の一覧コピー/保存形式 (キー\tコマンド\t説明)。
UnicodeString FormatList(const std::vector<Entry> &entries);

/// wx 専用 ini の表示設定
class StateStore {
public:
	Tab tab = Tab::File;
	SortMode sort_mode = SortMode::Key;
	bool show_all = false;
	bool migemo = false;
	bool confirm_execute = false;
	UnicodeString filter;

	void LoadFromIni(UsrIniFile &ini);
	void SaveToIni(UsrIniFile &ini) const;
};

}  // namespace key

#endif  // NYANFI_GUI_KEY_H
