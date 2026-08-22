/**
 * @file gui/bookmarks.h
 * @brief 栞マーク (ファイルに付ける目印) とタグ
 *
 * @details 保存先は移植済みの `UsrIniFile` (src/UIniFile.cpp) と
 *          `TagManager` (src/usr_tag.cpp) をそのまま使う。**自前で書き直さない。**
 *          ここに置くのは「どこへ飛ぶか」「どれを選ぶか」という判断だけで、
 *          そこは wx にも ini にも依存しない純関数にしてある (規約8)。
 *
 *          VCL の該当は `src/MainFrm.cpp` の `MarkAction` (21772行) /
 *          `NextMark` (22380行) / `PrevMark` (23828行) / `SelMark` (25098行) /
 *          `ClearMark` (14213行) / `MarkMask` (21878行)。
 *
 *          **保存先の ini は VCL 版と別**。GUI 版は `<exe名>_wx.ini` を使う
 *          (理由は gui/settings.h の解説を参照)。したがって**VCL 版で付けた栞は
 *          見えない**。共有したければ ini を分けている理由から見直すこと
 */
#ifndef NYANFI_GUI_BOOKMARKS_H
#define NYANFI_GUI_BOOKMARKS_H

#include <vector>

#include "UIniFile.h"
#include "gui/file_item.h"

namespace bookmarks {

/// 栞1件 (一覧ダイアログ用)
struct Mark {
	UnicodeString path;   //!< フルパス
	UnicodeString memo;   //!< メモ (無ければ空)
	UnicodeString stamp;  //!< 付けた日時。ini に入っている文字列のまま
};

//---------------------------------------------------------------------------
// どこへ飛ぶか (純関数)
//---------------------------------------------------------------------------
/**
 * @brief カーソルより後ろで最初にマークされている位置
 * @param marked 表示順の「マークされているか」
 * @param cursor 現在位置
 * @return 移動先の添字。マークが1つも無ければ -1
 * @details VCL の `NextMarkActionExecute` と同じで、後ろに無ければ
 *          **先頭側へ折り返す** (カーソル位置そのものを含む)。
 *          折り返し先は「カーソル位置以前で最初に見つかったマーク」
 */
int FindNext(const std::vector<bool> &marked, int cursor);

/// FindNext の逆向き (VCL の PrevMarkActionExecute)。前に無ければ末尾側へ折り返す
int FindPrev(const std::vector<bool> &marked, int cursor);

//---------------------------------------------------------------------------
// ini とのやりとり (UsrIniFile の薄い包み)
//---------------------------------------------------------------------------
/// 付ける/外すを切り替える。戻り値は切り替えた後の状態 (true = 付いている)
bool Toggle(UsrIniFile &ini, const UnicodeString &path, const UnicodeString &memo = EmptyStr);

/// 付いているか
bool IsMarked(UsrIniFile &ini, const UnicodeString &path);

/// メモを取り出す (無ければ空)
UnicodeString MemoOf(UsrIniFile &ini, const UnicodeString &path);

/// 指定した複数のパスの栞を外す。外した件数を返す
int ClearOf(UsrIniFile &ini, const std::vector<UnicodeString> &paths);

/**
 * @brief ini に入っている栞をすべて取り出す
 * @param ini 対象
 * @return 栞の一覧 (パスの昇順)
 * @details `UsrIniFile::MarkIdxList` は「ディレクトリ → その中のマーク行」の
 *          2段構造 (`UIniFile.cpp::UpdateMarkIdxList`)。ここで1本に均す
 */
std::vector<Mark> All(UsrIniFile &ini);

/// 実体が見つからない栞を外す。外した件数を返す
int TrimMissing(UsrIniFile &ini);

//---------------------------------------------------------------------------
// 一覧への当てはめ
//---------------------------------------------------------------------------
/// 表示中の項目それぞれについて、栞が付いているかを並び順で返す
std::vector<bool> MarkedFlags(UsrIniFile &ini, const std::vector<UnicodeString> &paths);

}  // namespace bookmarks

#endif  // NYANFI_GUI_BOOKMARKS_H
