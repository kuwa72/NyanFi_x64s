/**
 * @file gui/compare.h
 * @brief 左右のペインの比較とハッシュ (wx 非依存)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の `GetHash` / `CompareHash` /
 *          `SelOnlyCur` / `ToOppSameHash` / `DiffDir`。
 *          ハッシュの計算そのものは移植済みの `get_HashStr()`
 *          (src/usr_file_inf.h) が持つので、ここは**どう比べるか**だけを置く。
 */
#ifndef NYANFI_GUI_COMPARE_H
#define NYANFI_GUI_COMPARE_H

#include <vector>

#include "gui/file_item.h"

namespace compare {

/// 比べ方
enum class MatchBy {
	Name,       //!< 名前だけ
	NameSize,   //!< 名前とサイズ
	NameTime,   //!< 名前と更新日時
	Content,    //!< 内容 (ハッシュ)
};

/**
 * @brief 反対側に「同じもの」が無い項目の添字を返す (SelOnlyCur)
 * @param items こちら側の一覧
 * @param others 反対側の一覧
 * @param how 比べ方 (Content はここでは扱わない。呼び出し側がハッシュを渡す)
 * @return こちら側だけにある項目の添字
 * @details ディレクトリと `..` は対象外 (VCL の SelOnlyCur も
 *          ファイルだけを選ぶ)
 */
std::vector<int> IndicesOnlyHere(const std::vector<FileItem> &items,
                                 const std::vector<FileItem> &others, MatchBy how);

/// 2件が「同じもの」か (Content 以外)
bool IsSameItem(const FileItem &a, const FileItem &b, MatchBy how);

/// ディレクトリ比較の1行
struct DiffRow {
	UnicodeString name;
	bool in_left = false;
	bool in_right = false;
	bool differs = false;   //!< 両方にあるが内容が違う (how の基準で)
};

/**
 * @brief 左右のディレクトリを突き合わせる (DiffDir)
 * @details 名前順に並べた突き合わせ結果を返す。ディレクトリと `..` は除く。
 *          **両方にあって同じものは含めない** (違いだけを見たいため)
 */
std::vector<DiffRow> DiffDirectories(const std::vector<FileItem> &left,
                                     const std::vector<FileItem> &right, MatchBy how);

}  // namespace compare

#endif  // NYANFI_GUI_COMPARE_H
