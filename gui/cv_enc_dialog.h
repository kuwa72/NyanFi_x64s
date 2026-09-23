/**
 * @file gui/cv_enc_dialog.h
 * @brief 文字コード変換ダイアログ (wx 依存)
 *
 * @details VCL 版の `src/CvEncDlg.cpp` (`TCvTxtEncDlg`) の入力画面を移植する。
 *          選択肢は wx 非依存の `gui/cv_enc.h` が担当する。
 *
 *          未移植 (未実装扱い):
 *          - XML/HTML の charset 宣言書き換え
 *          - VCL のユーザー編集メニュー、位置・設定の保存
 */
#ifndef NYANFI_GUI_CV_ENC_DIALOG_H
#define NYANFI_GUI_CV_ENC_DIALOG_H

#include <wx/wx.h>

#include "gui/cv_enc.h"

namespace cv_enc_dialog {

struct Options {
	int code_index = 0;
	int line_break_index = 0;
	bool with_bom = true;
	UnicodeString title_suffix;
};

/// @return OK で閉じたなら true
bool Run(wxWindow *parent, Options &options);

}  // namespace cv_enc_dialog

#endif  // NYANFI_GUI_CV_ENC_DIALOG_H
