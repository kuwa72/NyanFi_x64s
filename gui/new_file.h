/**
 * @file gui/new_file.h
 * @brief 新規ファイル作成ダイアログの判断ロジック (wx 非依存)
 *
 * @details VCL 版の `src/NewDlg.cpp` (`TNewFileDlg`) と
 *          `src/MainFrm.cpp:22276` の `NewFileActionExecute` を実測した。
 *
 *          未移植 (未実装扱い):
 *          - NewFileExeCmd のユーザー設定に基づく複数コマンドの内部記法
 *          - 既存ファイルの上書き確認 (移植版は上書きせず失敗)
 *          - VCL のユーザー編集メニュー、ダイアログ位置と設定の保存
 */
#ifndef NYANFI_GUI_NEW_FILE_H
#define NYANFI_GUI_NEW_FILE_H

#include <cstddef>
#include <vector>

#include "usr_str.h"

namespace new_file {

/// TplComboBoxClick (NewDlg.cpp:107): テンプレートから元ファイル名を使う
inline UnicodeString NameFromTemplate(const UnicodeString &template_path)
{
	return ExtractFileName(template_path);
}

/// VCL: 最終ピリオドの直前までを選択する。拡張子が無ければ全体
inline int StemSelectionLength(const UnicodeString &name)
{
	const int dot = pos_r(_T("."), name);
	return dot > 0? dot - 1 : static_cast<int>(name.Length());
}

/// テンプレートと作成名が必要
inline bool CanSubmit(const UnicodeString &template_path, const UnicodeString &name)
{
	return !Trim(template_path).IsEmpty() && !Trim(name).IsEmpty();
}

inline UnicodeString OutputPath(const UnicodeString &dst_dir, const UnicodeString &name)
{
	return IncludeTrailingPathDelimiter(dst_dir) + name;
}

/// 既存の同値を削除し、新しい順を保つ (add_ComboBox_history, UserFunc.cpp:143)
inline void PromoteHistory(std::vector<UnicodeString> &history, const UnicodeString &entry)
{
	if (entry.IsEmpty()) return;
	for (std::size_t i = 0; i < history.size();) {
		if (SameStr(history[i], entry)) history.erase(history.begin() + static_cast<std::ptrdiff_t>(i));
		else ++i;
	}
	history.insert(history.begin(), entry);
}

/// NewFileExeCmd キーが既にないときだけ VCL は OpenByWin を初期値にする
inline UnicodeString DefaultPostCommand(bool setting_exists)
{
	return setting_exists? EmptyStr : _T("OpenByWin");
}

inline UnicodeString TemplateDirectory(const UnicodeString &template_path)
{
	return ExtractFilePath(template_path);
}

}  // namespace new_file

#endif  // NYANFI_GUI_NEW_FILE_H
