/**
 * @file gui/selection.h
 * @brief 一覧の選択操作 (wx 非依存の純粋ロジック)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の `Sel*ActionExecute` 群。
 *          いずれも `GetCurList()` を回して `file_rec::selected` を書き換える
 *          だけなので、**判断の部分をそのまま純関数に切り出せる** (規約8)。
 *          wx に触る層 (`gui/main_frame.cpp`) は一覧を渡して結果を受け取るだけにする。
 *
 *          各関数のコメントに VCL の該当行を書いてある。挙動はそこから実測して
 *          合わせた。特に以下は間違えやすいので明記しておく:
 *
 *          - `SelAllFile` は「全選択」ではなく**トグル**。選択が0件なら全選択、
 *            1件でもあれば全解除 (MainFrm.cpp:24829 の `GetSelCount(lst)==0`)。
 *            さらに**ディレクトリは常に解除**される
 *          - `SelReverseAll` は反転。こちらは**ディレクトリも対象**
 *          - `SelSameExt` は「同じ拡張子を追加選択」ではなく
 *            **一致するものだけを選択し直す** (`fp->selected = SameText(...)`)
 *
 *          `..` (親ディレクトリ) はどの操作でも選択しない
 *          (VCL の `is_selectable()` が `is_dummy` を弾くのと同じ)。
 */
#ifndef NYANFI_GUI_SELECTION_H
#define NYANFI_GUI_SELECTION_H

#include <vector>

#include "gui/file_item.h"

namespace selection {

/// 選択されている件数
int MarkedCount(const std::vector<FileItem> &items);

/**
 * @brief すべての項目の選択状態を反転する (SelReverseAll)
 * @details MainFrm.cpp:25284。**ディレクトリも対象**。`..` は対象外
 */
void ReverseAll(std::vector<FileItem> &items);

/**
 * @brief ファイルだけの選択状態を反転する (SelReverse)
 * @details ディレクトリは触らない
 */
void ReverseFiles(std::vector<FileItem> &items);

/**
 * @brief すべてのファイルを選択/解除する (SelAllFile)
 * @details MainFrm.cpp:24823。**トグル**であって全選択ではない。
 *          選択が0件なら全ファイルを選択、1件でもあれば全解除。
 *          いずれの場合も**ディレクトリは解除**される
 */
void ToggleAllFiles(std::vector<FileItem> &items);

/**
 * @brief すべての項目を選択/解除する (SelAllItem)
 * @details MainFrm.cpp:24846。トグル。ディレクトリも含めて同じ値にする
 */
void ToggleAllItems(std::vector<FileItem> &items);

/// すべての選択を解除する (ClearAll)。MainFrm.cpp:12078 の `ClrSelect(lst)`
void ClearAll(std::vector<FileItem> &items);

/**
 * @brief カーソル位置と同じ拡張子のファイルを選択する (SelSameExt)
 * @details MainFrm.cpp:25325。**一致するものだけを選択し直す** (追加ではない)。
 *          ディレクトリは対象外。カーソルがディレクトリまたは範囲外なら
 *          何もせず false を返す
 * @return 選択できたら true
 */
bool SelectSameExt(std::vector<FileItem> &items, int cursor);

/**
 * @brief カーソル位置とファイル名主部が同じファイルを選択する (SelSameName)
 * @details MainFrm.cpp:25365。VCL は `SelectMask("base.*;base")` を使うが、
 *          ここでは同じ意味 (拡張子を除いた部分が一致するファイル) を直接書いた。
 *          ディレクトリは対象外
 * @return 選択できたら true
 */
bool SelectSameName(std::vector<FileItem> &items, int cursor);

/**
 * @brief 指定文字列を名前に含むファイルを選択する (MatchSelect)
 * @details 大文字小文字は区別しない。ディレクトリも対象にする
 * @return 選択された件数
 */
int SelectMatching(std::vector<FileItem> &items, const UnicodeString &word);

/// 日付の比較方法 (MainFrm.cpp の DateSelect が受け付ける 3種)
enum class DateCompare { Before, Same, After };

/**
 * @brief 指定した日付条件に合うファイルを選択する (DateSelect)
 * @param items 一覧
 * @param border 境界の日付 (時刻を含む)
 * @param how border に対して「より古い / 同じ日 / より新しい」のどれか
 * @return 選択された件数
 * @details `Same` は**同じ日付**の比較 (時刻は見ない)。VCL の
 *          `format_Date()` 同士の比較と揃えてある
 */
int SelectByDate(std::vector<FileItem> &items, const TDateTime &border, DateCompare how);

/**
 * @brief 次 (または前) の選択項目の位置を返す (NextSelItem / PrevSelItem)
 * @param items 一覧
 * @param cursor 現在位置
 * @param forward true なら次、false なら前
 * @return 見つかった位置。無ければ -1 (**巡回しない**)
 */
int FindNextMarked(const std::vector<FileItem> &items, int cursor, bool forward);

/**
 * @brief 範囲を選択する (CursorUpSel / CursorDownSel などの共通部分)
 * @param items 一覧
 * @param from 選択を始める位置 (この位置を含む)
 * @param to 選択を終える位置 (この位置は**含まない**)
 * @details VCL の `Shift+↑` / `Shift+↓` は「移動する前の位置」を選択してから
 *          カーソルを動かす。範囲の向きはどちらでもよい
 */
void MarkRange(std::vector<FileItem> &items, int from, int to);

}  // namespace selection

#endif  // NYANFI_GUI_SELECTION_H
