/**
 * @file gui/find_files.h
 * @brief ファイル名の検索 (wx 非依存)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の `FindFileDlg` / `FindDirDlg` /
 *          `FindFileDirDlg`。結果は「結果リスト」という**別のディレクトリの
 *          項目が混ざった一覧**として表示される。
 *
 *          歩く量に上限を設け、**超えたら結果に残す** (grep や
 *          ディレクトリ集計と同じ方針。報告書 §16.2)。
 */
#ifndef NYANFI_GUI_FIND_FILES_H
#define NYANFI_GUI_FIND_FILES_H

#include <vector>

#include "gui/file_item.h"

namespace find_files {

/// 走査するファイル数の上限
inline constexpr int kMaxScanFiles = 200000;
/// 結果の件数の上限
inline constexpr int kMaxResults = 5000;

/// 何を探すか
enum class Target { Files, Directories, Both };

/// 検索の条件
struct Query {
	UnicodeString mask;        //!< セミコロン区切りのマスク ("*.txt;*.dat")。空なら全部
	Target target = Target::Files;
	bool recursive = true;
	bool show_hidden = false;
	bool show_system = false;
};

/// 検索の結果
struct Result {
	std::vector<FileItem> items;  //!< 見つかった項目 (full_path 付き)
	int scanned = 0;              //!< 走査した項目数
	bool truncated_scan = false;  //!< 走査の上限に達した
	bool truncated_hits = false;  //!< 結果の上限に達した
};

/**
 * @brief 名前がマスクに一致するか
 * @details `gui/file_item.h` の `MatchPathMask` はディレクトリ専用/除外指定まで
 *          扱うが、ここで要るのは**単純なワイルドカード照合**だけなので分けてある。
 *          `;` 区切りで複数指定でき、いずれかに一致すれば真。空なら常に真
 */
bool MatchesMask(const UnicodeString &name, const UnicodeString &mask);

/**
 * @brief ディレクトリを再帰的に歩いて名前で探す
 * @param root 起点
 * @param query 条件
 */
Result Search(const UnicodeString &root, const Query &query);

/// 重複の判定基準
enum class DuplicateBy {
	NameSize,   //!< 名前とサイズ (速い。中身は見ない)
	Content,    //!< 内容 (サイズで絞ってからハッシュを取る)
};

/// 重複検索の結果
struct DuplicateResult {
	std::vector<FileItem> items;  //!< 重複しているファイル (グループごとに固まって並ぶ)
	int groups = 0;               //!< 重複グループの数
	int hashed = 0;               //!< ハッシュを計算したファイル数 (Content のときだけ)
	bool truncated_scan = false;
};

/**
 * @brief 重複しているファイルを探す (FindDuplDlg)
 * @param root 起点
 * @param how 判定基準
 * @param show_hidden 隠しファイルも見るか
 * @param show_system システムファイルも見るか
 * @details `Content` でも**まずサイズで束ねてから**ハッシュを取る。
 *          サイズが違えば内容も違うので、全ファイルのハッシュを計算する必要は無い
 *          (`ToOppSameHash` と同じ枝刈り)。
 *
 *          **サイズ 0 のファイルは対象外**。空ファイルは互いに「同じ内容」に
 *          なってしまい、重複として大量に並ぶだけで役に立たないため
 *          (こちらの判断)
 */
DuplicateResult FindDuplicates(const UnicodeString &root, DuplicateBy how,
                               bool show_hidden, bool show_system);

}  // namespace find_files

#endif  // NYANFI_GUI_FIND_FILES_H
