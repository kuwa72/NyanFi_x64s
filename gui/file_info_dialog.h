/**
 * @file gui/file_info_dialog.h
 * @brief ファイル情報・CSV/TSV 項目集計ダイアログ (TFileInfoDlg 相当、wx 依存)
 *
 * @details VCL は `src/FileInfDlg.cpp` の `TFileInfoDlg`。ファイル情報の実取得は
 *          移植済み gui/file_info.h に委譲し、表示・コピー・全選択・場所移動・
 *          ハッシュ計算を wx で移植した。CSV/TSV 集計も同じ wx ダイアログで表示する。
 *
 *          VCL 呼び出し位置 (grep 実測):
 *          - ファイル情報: src/MainFrm.cpp:25919-25965
 *          - CSV/TSV 集計: src/MainFrm.cpp:33828-33834
 *          - 他画面からの表示: src/FileExtDlg.cpp:950-953, src/DirDlg.cpp:1004,
 *            src/GenInfDlg.cpp:1371, src/EditHistDlg.cpp:1293
 *
 *          未移植 (未実装扱い):
 *          - Windows 標準プロパティシート、画像プレビュー、URL/alt クリック
 *          - 作成日時/アクセス日時 (移植済み core の FileItem に持たない)
 *          - 項目の強調設定と前後ファイル移動
 *          - AppDlg/GitView から渡されるアプリ情報/コミット情報の一覧表示
 *          - 集計度数分布の owner-draw グラフ (数値・度数・累積度は表示する)
 */
#ifndef NYANFI_GUI_FILE_INFO_DIALOG_H
#define NYANFI_GUI_FILE_INFO_DIALOG_H

#include <wx/wx.h>

#include "gui/file_info.h"
#include "gui/file_item.h"

namespace file_info_dialog {

/// ダイアログ終了時に呼び出し側へ返す動作
enum class Outcome { Closed, OpenLocation };

struct Result {
	Outcome outcome = Outcome::Closed;
	UnicodeString path;  //!< OpenLocation の対象
};

/// ファイル/ディレクトリの情報を表示する
Result Run(wxWindow *parent, const UnicodeString &full_path, const FileItem &item);

/// CSV/TSV 数値列の集計を表示する
Result Run(wxWindow *parent, const file_info::ColumnStats &stats);

}  // namespace file_info_dialog

#endif  // NYANFI_GUI_FILE_INFO_DIALOG_H
