/**
 * @file gui/file_ext_dialog.h
 * @brief 拡張子別一覧ダイアログ (TFileExtensionDlg 相当、wx 依存)
 *
 * @details VCL は `src/FileExtDlg.cpp` の `TFileExtensionDlg`。再帰集計は
 *          gui/dir_info.h、並べ替え・CP 入力・出力書式は gui/file_ext.h に
 *          分離し、ここは一覧・ファイル選択・コピー/保存・マスク検索を wx 化。
 *
 *          VCL 呼び出し位置 (grep 実測): src/MainFrm.cpp:17650-17679。
 *
 *          未移植 (未実装扱い):
 *          - クラスタ占有サイズ/ギャップ、ライブラリ/書庫内ファイルの再帰
 *          - 拡張子アイコン、Windows 標準プロパティ、位置/並べ替えの ini 保存
 *          - アクセス拒否ディレクトリの件数集計 (core 側で保持していない)
 */
#ifndef NYANFI_GUI_FILE_EXT_DIALOG_H
#define NYANFI_GUI_FILE_EXT_DIALOG_H

#include <functional>
#include <vector>

#include <wx/wx.h>

namespace file_ext_dialog {

/// ダイアログ終了時に MainFrame へ返す動作
enum class Outcome { Closed, OpenFile, FindMask };

struct Result {
	Outcome outcome = Outcome::Closed;
	UnicodeString path;  //!< OpenFile の対象
	UnicodeString mask;  //!< FindMask のマスク
};

struct Context {
	bool show_hidden = false;
	bool show_system = false;
	/// 一覧出力ボタンで呼び出すログ出力。空ならログ出力は未実装とする。
	std::function<void(const std::vector<UnicodeString> &)> log_output;
};

/// 指定ディレクトリの拡張子別一覧を表示する
Result Run(wxWindow *parent, const UnicodeString &path, const Context &context);

}  // namespace file_ext_dialog

#endif  // NYANFI_GUI_FILE_EXT_DIALOG_H
