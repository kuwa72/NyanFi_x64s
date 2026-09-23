/**
 * @file gui/hist.h
 * @brief ディレクトリ履歴ダイアログの判定・一覧操作 (wx 非依存)
 *
 * @details VCL 版の `TDirHistoryDlg` は `src/HistDlg.cpp:22-539`、
 *          実呼び出しは `src/MainFrm.cpp:16720-16785` にある。現在の wx 版は
 *          1 ペイン/1 タブの `navigation::DirHistory` を使うため、VCL の複数
 *          タブ histories とは差がある。表示順・検索・削除権限・CSV の保存
 *          形式だけをここに集約し、wxDialog は入力と描画に専念する。
 *
 *          実測して決めた VCL の呼び出し箇所:
 *          - 通常/全体/全体検索/最近使ったディレクトリの入口:
 *            `src/MainFrm.cpp:16720-16770` (`DirHistory` の GA/GS/FM/AC/GC/RD)
 *          - ディレクトリ・スタック: `src/MainFrm.cpp:16780-16785`
 *          - 履歴の更新/絞り込み/削除/コピー/追加:
 *            `src/HistDlg.cpp:85-169`, `226-342`, `367-448`, `461-521`
 *
 *          未移植 (未実装扱い):
 *          - Windows シェル `FOLDERID_Recent` の `.lnk` 列挙・並び順
 *          - Migemo 辞書によるローマ字検索 (通常/AND/OR の判定は移植)
 *          - UNC/登録ディレクトリの表示名変換、ダイアログ位置の永続化
 *          - ワークリストの実ファイル読み込み (選択結果の受け渡しは行う)
 */
#ifndef NYANFI_GUI_HIST_H
#define NYANFI_GUI_HIST_H

#include <cstddef>
#include <vector>

#include "usr_str.h"

class UsrIniFile;

namespace hist {

/// TDirHistoryDlg のモード (VCL の Is* フラグを整理したもの)
enum class Mode {
	Current = 0,  //!< 現在のペインの履歴
	All,          //!< 全体履歴
	Search,       //!< 全体履歴のインクリメンタル検索
	Recent,       //!< 最近使ったディレクトリ
	Stack,        //!< ディレクトリ・スタック
};

/// 一覧の 1 項目。path は 表示/移動に使う実体、record は全体履歴の CSV。
struct Entry {
	UnicodeString path;
	UnicodeString record;  //!< VCL 互換の CSV (path,count 等)
	UnicodeString link;    //!< Recent モードのリンク元 (未移植なら空)
	bool worklist = false; //!< .nwl として開ける項目か
};

/// ダイアログが保持する一覧状態
struct State {
	Mode mode = Mode::Current;
	std::vector<Entry> entries;
	UnicodeString filter;
	bool migemo = false;
	int selected = -1;
};

/// 絞り込み後の表示行 (元の entries の添字を保持する)
struct View {
	std::vector<std::size_t> visible;
};

/// ダイアログの OK/操作結果
struct Result {
	bool accepted = false;
	int selected = -1;          //!< 元の entries の添字
	bool clear_all = false;
	bool clear_filtered = false;
	bool delete_selected = false;
	UnicodeString deleted_path;
	bool copy = false;
	bool property = false;
};

/// usr_cmdlist.cpp:715-723 の DirHistory パラメータをモードへ変換する。
/// AC/GC は MainFrame 側で先に処理するため、ここでは false。
bool ParseMode(const UnicodeString &param, Mode &mode_out);

/// モード名/件数から VCL 相当のタイトルを作る
UnicodeString ModeTitle(Mode mode, int shown, int total);

/// 検索モードかどうか (VCL IsFindDirHist)
bool UsesFilter(Mode mode);

/// 項目の表示名。CSV/実体のどちらかを優先し、空なら空文字。
UnicodeString DisplayPath(const Entry &entry);

/// 項目のフィルタ一致。case_sensitive は VCL の contains_upper 相当。
bool MatchesFilter(const Entry &entry, const UnicodeString &filter, bool case_sensitive);

/// 検索モードだけ filter を適用して表示行を返す。
View BuildView(const State &state, bool case_sensitive);

/// 画面から削除した元添字を entries から一度に除く。
void RemoveVisible(std::vector<Entry> &entries, const std::vector<std::size_t> &visible);

/// コピーするパス列 (VCL CopyAllActionExecute: CSV の先頭項目だけ)
std::vector<UnicodeString> CopyLines(const State &state, const View &view);

/// ディレクトリの重複を除いて追加する。VCL は lowercase で CSV 全体を検索する。
/// 既に末尾区切りがある項目は二重に付けない。
void MergeDirectories(std::vector<Entry> &entries, const std::vector<UnicodeString> &directories);

/// VCL の 1--10 の accelerators を表示文字列へ付ける。
UnicodeString DisplayWithAccelerator(const Entry &entry, int index);

/// リスト(store)を使うダイアログの有効/無効判定
bool CanClearAll(Mode mode, int count);
bool CanClearFiltered(Mode mode, int shown, int total);
bool CanCopy(int count);
bool CanProperty(int selected);

/// wx 専用 ini に全体履歴を保存する。VCL の元 ini は変更しない。
class Store {
public:
	const std::vector<Entry> &Entries() const { return entries_; }
	std::vector<Entry> &MutableEntries() { return entries_; }
	void Clear() { entries_.clear(); }
	void LoadFromIni(UsrIniFile &ini);
	void SaveToIni(UsrIniFile &ini) const;

private:
	std::vector<Entry> entries_;
};

/// 検索モードの Migemo 設定。辞書は wx 側で未移植だが、状態は保持する。
struct Preferences {
	bool migemo = false;
};
void LoadPreferences(UsrIniFile &ini, Preferences &prefs);
void SavePreferences(UsrIniFile &ini, const Preferences &prefs);

}  // namespace hist

#endif  // NYANFI_GUI_HIST_H
