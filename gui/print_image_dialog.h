/**
 * @file gui/print_image_dialog.h
 * @brief 画像印刷ダイアログ (wx 依存)
 *
 * @details VCL 実測:
 * - `src/PrnImgDlg.cpp` (`TPrintImgDlg`) / `src/PrnImgDlg.h` / `src/PrnImgDlg.dfm`
 * - 呼び出しは `src/MainFrm.cpp:35689-35697`、コマンドは
 *   `src/usr_cmdlist.cpp:394` の `Print`
 *
 * 設定の正規化・印刷範囲の解決は `gui/print_image.h` が担当する。
 *
 * 未移植 (未実装扱い):
 * - プリンタ選択/設定、画像の描画、実際の印刷実行
 * - 設定の ini 永続化、フォント選択、EXIF 文字の描画
 * - 前/次/先頭/末尾ボタンによる対象画像の移動
 */
#ifndef NYANFI_GUI_PRINT_IMAGE_DIALOG_H
#define NYANFI_GUI_PRINT_IMAGE_DIALOG_H

#include <vector>

#include <wx/wx.h>

#include "gui/print_image.h"

namespace print_image_dialog {

/// 現在画像と印刷範囲のコンテキスト。
struct Context {
	UnicodeString image_path;
	int current_page = 1;
	int page_count = 1;
	std::vector<int> selected_pages;
};

/**
 * @brief 画像印刷設定ダイアログを表示する
 * @param[out] options_out 印刷ボタンで確定した設定
 * @param[out] print_requested_out 印刷ボタンが押されたか
 * @return true で印刷ボタンから閉じた場合
 */
bool Run(wxWindow *parent, const Context &context, print_image::PrintOptions &options_out,
         bool &print_requested_out);

}  // namespace print_image_dialog

#endif  // NYANFI_GUI_PRINT_IMAGE_DIALOG_H
