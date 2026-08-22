/**
 * @file gui/links.h
 * @brief リンクの作成とタイムスタンプの調整 (wx 非依存)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の `CreateShortcut` /
 *          `CreateHardLink` / `CreateSymLink` と、`src/task_thread.cpp` の
 *          `SetDirTime`。いずれも**反対ペインのディレクトリに作る**のが VCL の
 *          既定 (MainFrm.cpp:15873 の `GetCurPathStr(OppListTag)`)。
 */
#ifndef NYANFI_GUI_LINKS_H
#define NYANFI_GUI_LINKS_H

#include <vector>

#include "gui/file_ops.h"

namespace links {

/**
 * @brief ハードリンクを作れる組み合わせか
 * @param src_root 元のドライブのルート ("C:\\\\" など)
 * @param dst_root 宛先のドライブのルート
 * @param dst_fs 宛先のファイルシステム名 ("NTFS" など)
 * @details 実測 (MainFrm.cpp:15707-15710): **同じボリュームで、かつ NTFS**
 *          でなければ作れない。ハードリンクはボリュームをまたげないため。
 *          VCL は両方のボリューム情報を比べてから NTFS かを見る
 */
bool CanCreateHardLink(const UnicodeString &src_root, const UnicodeString &dst_root,
                       const UnicodeString &dst_fs);

/// ショートカットのファイル名 (元の名前 + ".lnk")
UnicodeString ShortcutNameFor(const UnicodeString &name);

/// 作るリンクの種類
enum class LinkKind { Shortcut, Hard, Symbolic, Junction };

/**
 * @brief リンクをまとめて作る
 * @param paths 元のフルパス
 * @param dst_dir 作る先のディレクトリ
 * @param kind リンクの種類
 * @return 成功・失敗の件数と失敗の内訳
 * @details 宛先に同名があれば**上書きせずスキップ**する (規約: 上書きを既定にしない)。
 *          シンボリックリンクの作成には管理者権限か開発者モードが要るので、
 *          失敗したらその旨を理由に含める
 */
file_ops::FileOpResult CreateLinks(const std::vector<UnicodeString> &paths,
                                   const UnicodeString &dst_dir, LinkKind kind);

/**
 * @brief ディレクトリのタイムスタンプを配下の最新に合わせる (SetDirTime)
 * @param dir 対象のディレクトリ
 * @param show_hidden 隠しファイルも見るか
 * @param show_system システムファイルも見るか
 * @return 配下の最新の日時 (何も無ければ 0)
 * @details 実測 (task_thread.cpp:1679): **再帰的に**配下を見て最大値を取り、
 *          自分のタイムスタンプをそれに合わせる。**表示していないファイルは
 *          数えない** (ShowHideAtr / ShowSystemAtr を見る) ので、隠しファイル
 *          しか無いディレクトリは変わらない
 */
TDateTime SetDirTimeRecursive(const UnicodeString &dir, bool show_hidden, bool show_system);

}  // namespace links

#endif  // NYANFI_GUI_LINKS_H
