/**
 * @file gui/inp_ex.h
 * @brief 拡張入力ダイアログのモード・表示/検証ロジック (wx 非依存)
 *
 * @details VCL の `TInputExDlg` は `src/InpExDlg.cpp:17-410`、対応するモード定数は
 *          `src/InpExDlg.h:18-32`。入力欄の種類、タイトル、履歴キー、保存時の
 *          16 進/10 進正規化、作成ディレクトリの文字数表示をここで定義する。
 *          wxDialog 側は wx の入力コントロールとだけ対話する。
 *
 *          実測して決めた VCL の呼び出し箇所:
 *          - 汎用の inputbox/input_query: `src/InpExDlg.cpp:360-410`
 *          - ディレクトリ作成: `src/MainFrm.cpp:15587-15592`
 *          - テストファイル: `src/MainFrm.cpp:15919-15937`
 *          - クローン: `src/MainFrm.cpp:14358-14361`
 *          - タグ入力: `src/MainFrm.cpp:13433-13436`, `19106-19108`,
 *            `25836-25839`, `26740-26742`
 *          - ビューアの行/アドレス入力: `src/TxtViewer.cpp:4742-4771`
 *
 *          未移植 (未実装扱い):
 *          - VCL の specialized edit popup、IME、ヘルプブラウザ
 *          - 入力履歴の実ファイル保存 (キーは提供し、wx 側で ini に保存する)
 *          - FunctionKey/ClipPaste モードの専用起動側 Action (到達不能分は
 *            MainFrame のコメントにも明記する)
 */
#ifndef NYANFI_GUI_INP_EX_H
#define NYANFI_GUI_INP_EX_H

#include <vector>

#include "usr_str.h"

namespace inp_ex {

/// VCL の INPEX_* 定数と同じ意味
enum class Mode {
	Simple = 0,
	CreateDir = 1,
	NewTextFile = 2,
	Clone = 3,
	FunctionKey = 4,
	CreateTestFile = 5,
	JumpLine = 6,
	JumpAddress = 7,
	SetTopAddress = 8,
	FindTag = 10,
	AddTag = 11,
	SetTag = 12,
	TagSelect = 13,
	ClipPaste = 14,
};

/// ダイアログが読み書きする入力状態
struct Values {
	Mode mode = Mode::Simple;
	UnicodeString value;
	UnicodeString title;
	UnicodeString prompt;
	UnicodeString path_name;
	int custom_width = 0;
	bool numeric_only = false;
	UnicodeString hint;

	int code_page = 0;
	bool use_clipboard = false;
	bool edit_new_text = false;
	bool change_dir = false;
	bool convert_chars = false;
	bool select_default = false;
	UnicodeString test_size = _T("1M");
	int test_count = 1;
	bool hexadecimal = true;
};

/// 入力欄が combo か edit か
bool UsesCombo(Mode mode);

/// 入力欄のラベル (名前/タグ/ファイル名/行番号/アドレス)
UnicodeString Prompt(Mode mode);

/// 入力履歴の ini セクション名。履歴を使わないモードは空文字。
UnicodeString HistorySection(Mode mode);

/// 入力欄のヒント (VCL FormShow:86-88 と同じ条件)
UnicodeString Hint(Mode mode);

/// タイトル (ディレクトリ作成は path_name を付ける)
UnicodeString Title(Mode mode, const UnicodeString &path_name = EmptyStr);

/// モードの値を OK 時に正規化する。16 進アドレスは VCL と同様に "0x" を付ける。
void NormalizeOnClose(Values &values);

/// ディレクトリ作成欄の文字数判定。VCL InpExDlg.cpp:345-353 と同じ境界。
struct LengthStatus {
	int path_length = 0;
	int name_length = 0;
	bool path_ok = true;
	bool name_ok = true;
};
LengthStatus MeasureCreateDir(const UnicodeString &path_name, const UnicodeString &name);

/// 入力モードごとの最低限の検証。失敗時は error_out を設定。
bool Validate(const Values &values, UnicodeString &error_out);

/// VCL UserMdl.cpp:25-32 と同じ保存用文字コード順。
std::vector<UnicodeString> CodePageNames();

}  // namespace inp_ex

#endif  // NYANFI_GUI_INP_EX_H
