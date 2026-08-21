/**
 * @file gui/file_info_panel.h
 * @brief ファイル情報ダイアログ (PropertyDlg 相当)
 *
 * @details 情報の組み立ては wx 非依存の gui/file_info.h (BuildFileInfoLines /
 * AppendHashLines) が行う。ここは結果をテキストで表示するだけの薄い
 * wxDialog。読み取り専用の wxTextCtrl (複数行) に表示し、ハッシュ計算は
 * 大きいファイルだと時間がかかるためボタンで明示的に行う。
 */
#ifndef NYANFI_GUI_FILE_INFO_PANEL_H
#define NYANFI_GUI_FILE_INFO_PANEL_H

#include <wx/wx.h>

#include "gui/file_item.h"

/**
 * @brief ファイル情報ダイアログを表示する (モーダル)
 * @param parent 親ウィンドウ
 * @param full_path 対象のフルパス
 * @param item 一覧から得た基本情報 (BuildFileInfoLines に渡す)
 * @details 解析関数が例外を投げた場合はここで捕捉し、エラーとして1行表示する
 * (アーカイブや壊れたファイルなどで例外が起きても、ダイアログごと落ちない
 * ようにするため)
 */
void ShowFileInfoDialog(wxWindow *parent, const UnicodeString &full_path, const FileItem &item);

#endif  // NYANFI_GUI_FILE_INFO_PANEL_H
