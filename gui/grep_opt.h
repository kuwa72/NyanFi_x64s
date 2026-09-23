/**
 * @file gui/grep_opt.h
 * @brief GREP 拡張設定の判断・サンプル生成 (wx 非依存)
 *
 * @details VCL の実測元は `src/GrepOptDlg.cpp:23-215` と
 *          `src/MainFrm.cpp:30891-30900` (GrepOptionActionExecute) および
 *          `src/Global.cpp:1942-1956` (G:OutMode〜G:RepCrStr) です。
 *          関連コマンド表も実測した。`src/usr_cmdlist.cpp:316-317` には
 *          FV:Grep のみ。GrepOptionAction 自体のコマンド名は表にありません。
 *          wx 実装は入出力と状態表示だけを担い、GREP の実走査・外部アプリ
 *          起動・置換/ログ出力は既存処理側へ委ねます。
 *
 *          未移植 (未実装扱い):
 *          - grep.exe / 起動アプリの実実行と出力ファイル生成
 *          - 置換の実処理、置換ログの書込み/閲覧
 *          - VCL の OwnerDraw タブ、ドラッグ＆ドロップ、ini 位置保存
 */
#ifndef NYANFI_GUI_GREP_OPT_H
#define NYANFI_GUI_GREP_OPT_H

#include "usr_str.h"

namespace grep_opt {

/// 出力先 (src/GrepOptDlg.dfm:165-196 の 3 ラジオボタン)
enum class OutputMode {
	None = 0,       //!< 出力しない
	File = 1,       //!< ファイル
	Clipboard = 2,  //!< クリップボード
};

/// GREP の検索/置換ページ (src/GrepOptDlg.cpp:64-79 の FormShow)
enum class EditMode {
	Search = 0,
	Replace = 1,
};

/// TGrepExOptDlg が Global.cpp の option tag として編集する値
struct Options {
	OutputMode output_mode = OutputMode::None;
	EditMode edit_mode = EditMode::Search;

	UnicodeString output_file;
	bool append_output = false;

	bool app_enabled = false;
	UnicodeString app_name;
	UnicodeString app_param;
	UnicodeString app_dir;

	UnicodeString file_format = _T("$F $L:");
	UnicodeString insert_before;
	UnicodeString insert_after;
	bool trim_left = true;
	bool replace_tab = true;
	bool replace_cr = true;
	UnicodeString replacement = _T(" ／ ");

	bool backup_replace = false;
	UnicodeString backup_extension;
	UnicodeString backup_dir;

	bool save_log = false;
	UnicodeString log_file;
	bool append_log = false;
	bool open_log = false;
};

/// 入力.Enabled の判断 (wx 側でコントロールを greying out するための純関数)
struct EnabledState {
	bool output_file = false;
	bool app = false;
	bool app_name = false;
	bool app_dir = false;
	bool backup = false;
	bool log = false;
	bool insert_words = false;
};

/// 値の範囲とモードを正規化する (VCL の unchecked な設定を安全な範囲に丸める)
Options Normalize(const Options &in);

/// 現在の設定から各入力欄を有効にするか
EnabledState ResolveEnabled(const Options &opt);

/// 出力先ラジオの index ↔ OutputMode
int OutputModeIndex(OutputMode mode);
OutputMode OutputModeFromIndex(int index);

/// 検索/置換ページの index ↔ EditMode
int EditModeIndex(EditMode mode);
EditMode EditModeFromIndex(int index);

/**
 * @brief VCL TGrepExOptDlg::SampleChange と同じサンプル文字列を作る
 * @details ファイル書式の $F/$L、挿入語、タブ/行頭空白/改行の変換を
 *          すべてここで行う。wx ダイアログは結果を表示するだけ。
 */
UnicodeString BuildSample(const Options &opt);

}  // namespace grep_opt

#endif  // NYANFI_GUI_GREP_OPT_H
