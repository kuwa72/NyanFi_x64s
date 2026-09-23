/**
 * @file gui/exe_cmd.h
 * @brief 外部コマンド実行ダイアログの判断ロジック (wx 非依存)
 *
 * @details VCL 版の `src/ExeDlg.cpp` (`TExeCmdDlg`) と
 *          `src/MainFrm.cpp:17039` の `ExeCommandLineActionExecute` を実測した。
 *
 *          未移植 (未実装扱い):
 *          - ユーザー設定 FExtExeFile を使う実行可能拡張子の判定
 *          - 権限昇格時に UAC の ForcedElevation フラグを付ける差
 *          - 標準出力の保存文字コード (移植版は UTF-8 固定)
 *          - VCL のユーザー編集メニュー、ダイアログ位置と設定の保存
 */
#ifndef NYANFI_GUI_EXE_CMD_H
#define NYANFI_GUI_EXE_CMD_H

#include <cstddef>
#include <vector>

#include "gui/f_misc_ops.h"
#include "usr_file_ex.h"
#include "usr_str.h"

namespace exe_cmd {

/// usr_cmdlist.cpp:741 の FN/LC
enum class InitialInput { None, CurrentFile, LastCommand };

struct Options {
	UnicodeString command;
	bool log_stdout = false;
	bool copy_stdout = false;
	bool save_stdout = false;
	bool list_stdout = false;
	UnicodeString save_name;
	bool run_as = false;
	bool uac_dialog = false;
	std::vector<UnicodeString> history;
};

struct CommandSeed {
	UnicodeString text;
	int caret = 0;
};

/// VCL の FExtExeFile 設定がない場合の既定拡張子
inline bool IsExecutablePath(const UnicodeString &path)
{
	const UnicodeString ext = get_extension(path);
	return SameText(ext, _T(".exe")) || SameText(ext, _T(".com"))
	    || SameText(ext, _T(".bat")) || SameText(ext, _T(".cmd"));
}

/// FN は LC より優先する (VCL は if/else if の順)
inline InitialInput ParseInitialInput(const UnicodeString &param)
{
	if (f_misc_ops::HasParamToken(param, _T("FN"))) return InitialInput::CurrentFile;
	if (f_misc_ops::HasParamToken(param, _T("LC"))) return InitialInput::LastCommand;
	return InitialInput::None;
}

/// VCL (ExeDlg.cpp:49) と同じ初期入力とカーソル位置を作る
inline CommandSeed MakeSeed(InitialInput mode, const UnicodeString &current_path,
                            bool current_is_executable,
                            const std::vector<UnicodeString> &history)
{
	CommandSeed seed;
	if (mode == InitialInput::CurrentFile && !current_path.IsEmpty()) {
		const UnicodeString quoted = add_quot_if_spc(current_path);
		if (current_is_executable) {
			seed.text = quoted + _T(" ");
			seed.caret = static_cast<int>(seed.text.Length());
		}
		else {
			seed.text = _T(" ") + quoted;
			seed.caret = 0;
		}
	}
	else if (mode == InitialInput::LastCommand && !history.empty()) {
		seed.text = history.front();
		seed.caret = static_cast<int>(seed.text.Length());
	}
	return seed;
}

/// VCL (ExeDlg.cpp:121): 空コマンドは不可。保存時は保存名も必須
inline bool CanSubmit(const Options &options)
{
	if (Trim(options.command).IsEmpty()) return false;
	return !options.save_stdout || !Trim(options.save_name).IsEmpty();
}

/// VCL (ExeDlg.cpp:129): 権限昇格中は標準出力関連を無効にする
inline bool OutputOptionsEnabled(const Options &options)
{
	return !options.run_as && !options.uac_dialog;
}

inline void PromoteHistory(std::vector<UnicodeString> &history, const UnicodeString &entry)
{
	if (entry.IsEmpty()) return;
	for (std::size_t i = 0; i < history.size();) {
		if (SameStr(history[i], entry)) history.erase(history.begin() + static_cast<std::ptrdiff_t>(i));
		else ++i;
	}
	history.insert(history.begin(), entry);
}

inline UnicodeString SaveOutputPath(const UnicodeString &current_dir, const UnicodeString &name)
{
	if (!ExtractFilePath(name).IsEmpty()) return name;
	return IncludeTrailingPathDelimiter(current_dir) + name;
}

}  // namespace exe_cmd

#endif  // NYANFI_GUI_EXE_CMD_H
