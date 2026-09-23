/**
 * @file gui/tag_dialog.h
 * @brief タグ管理ダイアログ (TTagManDlg 相当、wx 依存)
 *
 * @details VCL は `src/TagDlg.cpp` の `TTagManDlg` を AddTag / SetTag /
 *          FindTag / TagSelect / FindFolderIcon で共用する。入力・チェック同期・
 *          タグ名/色/使用数/整理・検索コマンド保存を wx で移植した。
 *
 *          未移植 (未実装扱い):
 *          - VCL の NyanFi.ini にある各ダイアログ位置/表示条件の永続化
 *          - タグ色設定の永続化 (入力中の変更反映は行う)
 *          - タグ色背景の反転描画とスポイト (wxCheckListBox に owner draw がない)
 *          - FindFolderIcon の実検索。入力したアイコン条件は呼び出し側へ返す
 */
#ifndef NYANFI_GUI_TAG_DIALOG_H
#define NYANFI_GUI_TAG_DIALOG_H

#include <vector>

#include <wx/wx.h>

#include "gui/tag.h"

class TagManager;

namespace tag_dialog {

/**
 * @brief タグ入力ダイアログを表示する
 * @param manager 移植済み TagManager。改名・色・使用数・整理はここへ反映する
 * @param[in,out] options 初期条件と OK 時に 확정する条件
 * @param folder_icons FindFolderIcon 用。VCL の get_FolderIconList 相当
 * @return OK なら true。Cancel/close は false
 */
bool Run(wxWindow *parent, TagManager &manager, tag::Options &options,
         const std::vector<UnicodeString> &folder_icons = {});

}  // namespace tag_dialog

#endif  // NYANFI_GUI_TAG_DIALOG_H
