/**
 * @file gui/edit_hist_dialog.h
 * @brief 編集履歴ダイアログ (wx 依存)
 *
 * @details VCL の `TEditHistoryDlg` (`src/EditHistDlg.h`,
 *          `src/EditHistDlg.cpp`, `src/EditHistDlg.dfm`) のうち、編集履歴
 *          (`isRecent/isMark/isRepo/isTags` がすべて false) の表示・検索・
 *          削除・設定画面を wx で再構成した。判断は wx 非依存の
 *          `gui/edit_hist.h`、状態は既存の `history::HistoryList` が持つ。
 *
 *          VCL 呼び出し元は `src/MainFrm.cpp:16908-16942` の
 *          `EditHistoryActionExecute` (実測)。`ViewHistory` は
 *          `src/MainFrm.cpp:27571-27574` から同じ VCL ダイアログへ委譲する
 *          が、この wx ダイアログの対象は EditHistory だけである。
 *
 *          未移植 (未実装扱い):
 *          - Migemo 辞書、OwnerDraw の色/アイコン、独自スクロールバー
 *          - TStringGrid の列幅保存・ヘッダソート、ファイル情報/プロパティ
 *          - VCL の Pascal ダイアログ位置/寸法、フィルタの IncSearch キー
 *          - `RecentList` の Windows シェル一覧、栞マーク/リポジトリ/
 *            タグジャンプ/閲覧履歴の別モード
 */
#ifndef NYANFI_GUI_EDIT_HIST_DIALOG_H
#define NYANFI_GUI_EDIT_HIST_DIALOG_H

#include <wx/wx.h>

#include "gui/edit_hist.h"

namespace edit_hist_dialog {

/// 選択確定後に呼び出し側が行う動作。
enum class Action {
	None,
	Move,  //!< ファイル一覧の対象位置へ移動 (VCL の mrClose相当)
	Open,  //!< テキストエディタで開く (VCL の FileEdit相当)
};

/// wx 側渡す初期値。履歴本体は Run の参照引数で受け渡す。
struct Input {
	UnicodeString current_path;
	edit_hist::Preferences preferences;
	bool focus_filter = false;
};

/// ダイアログ終了時の状態。Cancel でも preferences は受け取る。
struct Result {
	Action action = Action::None;
	UnicodeString path;
	edit_hist::Preferences preferences;
	bool changed = false;  //!< 履歴本体/除外パスを変更したか
	bool accepted = false;
};

/**
 * @brief 編集履歴ダイアログを表示する。
 * @param history MainFrame の既存 MRU。削除/整理は inout で反映される。
 * @param input 現在パス、表示設定、FF パラメータ。
 * @param out OK/Cancel 後の選択と設定。Cancel でも preferences を更新する。
 * @return OK で選択が確定したとき true。Cancel/選択なしは false。
 */
bool Run(wxWindow *parent, history::HistoryList &history, const Input &input, Result &out);

}  // namespace edit_hist_dialog

#endif  // NYANFI_GUI_EDIT_HIST_DIALOG_H
