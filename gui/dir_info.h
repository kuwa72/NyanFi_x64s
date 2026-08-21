/**
 * @file gui/dir_info.h
 * @brief ディレクトリの集計 (wx 非依存)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の `CalcDirSize` / `CalcDirSizeAll` /
 *          `FileExtList` / `ListTree`。いずれも**ディレクトリを再帰的に歩いて
 *          数える**ところが本体なので、そこを純関数にして固定する (規約8)。
 *
 *          歩く量に上限を設けてある。上限を超えたら**打ち切ったことを結果に
 *          残す** (黙って途中までの数字を出さない)。
 */
#ifndef NYANFI_GUI_DIR_INFO_H
#define NYANFI_GUI_DIR_INFO_H

#include <vector>

namespace dir_info {

/// 走査するファイル数の上限。超えたら打ち切って `truncated` を立てる
inline constexpr int kMaxScanFiles = 200000;

/// ディレクトリの集計結果
struct DirSize {
	Int64 bytes = 0;      //!< 合計サイズ
	int files = 0;        //!< ファイル数
	int dirs = 0;         //!< ディレクトリ数 (自身は含まない)
	bool truncated = false;  //!< 上限に達して打ち切った
};

/**
 * @brief ディレクトリの容量を再帰的に数える
 * @param dir 対象
 * @param show_hidden 隠しファイルも数えるか
 * @param show_system システムファイルも数えるか
 * @details 表示の設定に合わせるのは `SetDirTime` と同じ考え方
 *          (画面に出ていないものを数えると数字が合わない)
 */
DirSize CalcDirSize(const UnicodeString &dir, bool show_hidden, bool show_system);

/// 拡張子ごとの集計
struct ExtStat {
	UnicodeString ext;   //!< 拡張子 (小文字。無い場合は "(なし)")
	int count = 0;
	Int64 bytes = 0;
};

/**
 * @brief 拡張子ごとの件数と容量を数える (FileExtList)
 * @param dir 対象のディレクトリ
 * @param recursive サブディレクトリも見るか
 * @param truncated_out 上限に達して打ち切ったか
 * @return 件数の多い順に並べた集計
 */
std::vector<ExtStat> CalcExtStats(const UnicodeString &dir, bool recursive,
                                  bool show_hidden, bool show_system, bool &truncated_out);

/// ツリー表示の1行
struct TreeLine {
	int depth = 0;
	UnicodeString name;
};

/**
 * @brief ディレクトリ構造をツリーにする (ListTree)
 * @param dir 起点
 * @param max_depth 潜る深さの上限
 * @param truncated_out 上限に達して打ち切ったか
 * @details **ディレクトリだけ**を並べる (VCL の ListTree も同じ)
 */
std::vector<TreeLine> BuildTree(const UnicodeString &dir, int max_depth,
                                bool show_hidden, bool show_system, bool &truncated_out);

}  // namespace dir_info

#endif  // NYANFI_GUI_DIR_INFO_H
