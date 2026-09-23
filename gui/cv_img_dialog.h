/**
 * @file gui/cv_img_dialog.h
 * @brief 画像変換設定ダイアログ (wx 依存)
 *
 * VCL の `TCvImageDlg` は `src/CvImgDlg.cpp:22-212`、実呼び出しは
 * `src/MainFrm.cpp:29289-29373`。形式/スケールの判断は gui/cv_img.h、実画像
 * 変換は既存 convert_ops へ渡す。
 *
 * 未移植 (未実装扱い): クリップボード画像の実取得、grayscale/resize/補間/
 * 余白/タイムスタンプの実行、名前変更・自動連番、プレビュー、ini 位置保存。
 */
#ifndef NYANFI_GUI_CV_IMG_DIALOG_H
#define NYANFI_GUI_CV_IMG_DIALOG_H

#include <wx/wx.h>

#include "gui/cv_img.h"

namespace cv_img_dialog {

/// 画像変換条件を入力する。OK なら options を更新して true。
bool Run(wxWindow *parent, cv_img::Options &options, bool from_clipboard,
         const UnicodeString &title_info = EmptyStr);

}  // namespace cv_img_dialog

#endif  // NYANFI_GUI_CV_IMG_DIALOG_H
