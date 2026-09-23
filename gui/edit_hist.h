/**
 * @file gui/edit_hist.h
 * @brief 編集履歴ダイアログの判断ロジック (wx 非依存)
 *
 * @details VCL 版の `src/EditHistDlg.cpp` (`TEditHistoryDlg`) の実測を
 *          次の入口で確認した。
 *          - `src/usr_cmdlist.cpp:187,315,336,348` に `RecentList`,
 *            `EditHistory`, `ViewHistory`, `CmdHistory` の実在コマンドがある。
 *            `EditHistory` のパラメータ説明は `src/usr_cmdlist.cpp:738-740`
 *            (`FF` = フィルタフォーカス、`AC` = 履歴を全消去)。
 *          - `src/MainFrm.cpp:16908-16942` の `EditHistoryActionExecute` が
 *            `TextEditHistory` を `TEditHistoryDlg` に渡し、選択後は
 *            `FileEdit` (編集) または `JumpToList` (移動) を実行する。
 *          - `src/MainFrm.cpp:27571-27574` の `ViewHistoryActionExecute` は
 *            `EditHistoryActionExecute` へ委譲する。
 *          - `src/MainFrm.cpp:24125-24148` の `RecentListActionExecute` と
 *            `src/MainFrm.cpp:14438-14444` の `CmdHistoryActionExecute` は
 *            別の入口なので、このモジュールでは扱わない。
 *
 *          一覧の組み立て・モード絞り込み・ファイル名検索・削除・設定の
 *          読み書きだけをここに置く。状態の保存は既存の
 *          `history::HistoryList` (`gui/history.h`) を再利用する。MRU の
 *          大文字小文字無視の重複除去・先頭移動と、`TextEditHistory` の
 *          `50,true` (max_items/del_quot) 形式は `gui/history.cpp` の責務で
 *          あり、ここでは新しい履歴配列や別の永続化を作らない。
 *
 *          移植範囲:
 *          - 編集履歴の一覧表示、ファイル名検索、OptMode 0/1/2 の絞り込み
 *          - 選択項目の削除、全削除、表示しないパスの整理
 *          - Migemo のチェック状態を含む表示設定の ini 往復
 *
 *          未移植 (未実装扱い):
 *          - Migemo 辞書によるローマ字検索。`migemo=true` は VCL と同じ
 *            大文字小文字無視の正規表現照合を行うが、辞書変換は行わない。
 *          - VCL の TStringGrid の OwnerDraw、列幅のドラッグ並べ替え、
 *            独自スクロールバー、ファイル色/マーク背景の描画
 *          - `ViewHistory` の行番号・マーク CSV、RecentList の Windows
 *            シェル `*.lnk` 列挙、栞マーク/リポジトリ/タグジャンプ画面
 *          - ファイル情報・プロパティ・実際の Git 情報更新の画面
 *          - VCL の Pascal ダイアログ位置/画面寸法と scaled フィルタ幅の換算。設定値
 *            (モード、Migemo、ステータス、フィルタ幅、除外パス) は wx 版
 *            ini に保存する
 */
#ifndef NYANFI_GUI_EDIT_HIST_H
#define NYANFI_GUI_EDIT_HIST_H

#include <cstddef>
#include <vector>

#include "gui/history.h"

class UsrIniFile;

namespace edit_hist {

/// VCL の OptMode0/1/2 に対応する表示範囲。
enum class Mode {
	All = 0,              //!< すべての編集履歴
	CurrentPath = 1,      //!< 現在ディレクトリ以下
	CurrentDirectory = 2, //!< 現在ディレクトリと同じ場所
};

/// 一覧の絞り込み条件。VCL はファイル名だけを正規表現照合する。
struct Context {
	UnicodeString current_path;
	Mode mode = Mode::All;
	UnicodeString filter;
	bool migemo = false;
};

/// wx 側へ渡す1行。時刻/FileItem などの描画情報は dialog 側で補完する。
struct Entry {
	UnicodeString path;
	UnicodeString name;
	UnicodeString location;
};

/// ダイアログの初期設定。状態は既存の HistoryList を inout で受け渡す。
struct Preferences {
	Mode mode = Mode::All;
	bool migemo = false;
	bool status_bar = true;
	int filter_width = 200;
	UnicodeString excluded_paths;
};

/// VCL の TestActionParam と同じ `;` 区切り判定。
struct Request {
	bool focus_filter = false;
	bool clear_all = false;
};

//---------------------------------------------------------------------------
// 表示モード
//---------------------------------------------------------------------------

/// VCL の OptMode 番号へ変換する。
int ModeIndex(Mode mode);

/// OptMode 番号を範囲外の値も All に補正して変換する。
Mode ModeFromIndex(int index);

/// ダイアログの表題。
UnicodeString ModeTitle(Mode mode);

/// パスが表示モードの条件に一致するか。
/// CurrentPath は VCL の StartsText(CurPathName, fnam) を、
/// CurrentDirectory は SameText(CurPathName, ExtractFilePath(fnam)) を表す。
bool MatchesMode(const UnicodeString &path, const UnicodeString &current_path, Mode mode);

//---------------------------------------------------------------------------
// 検索・一覧
//---------------------------------------------------------------------------

/// ファイル名だけを検索する。migemo=false は大文字小文字無視の部分一致、
/// migemo=true は大文字小文字無視の正規表現照合。
bool MatchesFilter(const UnicodeString &path, const UnicodeString &filter, bool migemo);

/// 既存 HistoryList の順序を変えず、表示条件を満たす行を返す。
/// HistoryList 自身が MRU と上限を保持するため、ここではコピーして並べ替えない。
std::vector<Entry> BuildEntries(const history::HistoryList &history, const Context &context);

/// 一覧の件数/選択数表示。
UnicodeString StatusText(int shown, int total, int selected);

//---------------------------------------------------------------------------
// 削除・設定
//---------------------------------------------------------------------------

/// HistoryList の既存 Remove (SameText) を使って1項目を削除する。
/// VCL の履歴が CSV の場合だけ先頭項目も比較する。削除件数を返す。
int RemoveEntry(history::HistoryList &history, const UnicodeString &path);

/// `;` 区切りの部分一致パスをディレクトリに適用し、履歴から除く。
/// 環境変数の展開は VCL の cv_env_str と同じ補助関数を呼ぶ。
int ApplyExcludedPaths(history::HistoryList &history, const UnicodeString &patterns);

/// VCL の EditHistory パラメータ (FF/AC) を解析する。
Request ParseRequest(const UnicodeString &param);

/// ダイアログの操作ボタンの有効条件。
bool CanDelete(int selected, int count);
bool CanClearAll(int count);

/// VCL と同じ General/Option のキーを使う設定の読み書き。
/// max_items/del_quot の本体は gui/history.* が担当する。
void LoadPreferences(UsrIniFile &ini, Preferences &preferences);
void SavePreferences(UsrIniFile &ini, const Preferences &preferences);

}  // namespace edit_hist

#endif  // NYANFI_GUI_EDIT_HIST_H
