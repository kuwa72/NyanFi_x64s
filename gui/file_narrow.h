/**
 * @file gui/file_narrow.h
 * @brief 一覧の絞り込み (Filter) と類似度ソート (SimilarSort) の wx 非依存ロジック
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の `FilterActionExecute` (17751行) と
 *          `SimilarSortActionExecute` (26176行)。
 *
 *          - Filter はキーワードによる絞り込み。照合自体は移植済みの
 *            `contains_word_and_or` / `contains_fuzzy_word` (src/usr_str.h)
 *            をそのまま使う (VCL と同じ関数)。空のキーワードは「通す」
 *            (VCL は空で絞り込み解除になるので、ここでは通す側に倒す)。
 *          - SimilarSort はカーソル項目との正規化レーベンシュタイン距離
 *            (`get_NrmLevenshteinDistance`、0～1000) の昇順に並べる。
 *            VCL はカーソル項目の distance を -1 (先頭)、`..`/ダミーを
 *            1000 (末尾) にして `CustomSort` する。こちらも同じ順序になる
 *            よう順位だけを返す (表示順の適用は呼び出し側の役割)。
 */
#ifndef NYANFI_GUI_FILE_NARROW_H
#define NYANFI_GUI_FILE_NARROW_H

#include <cstddef>
#include <vector>

namespace file_narrow {

//---------------------------------------------------------------------------
// Filter (F:Filter)
//---------------------------------------------------------------------------

/// Filter の照合方法 (VCL の ActionParam "CS"/"FZ" に相当)
struct FilterOptions {
	bool case_sensitive = false;  //!< true なら大小文字を区別する ("CS")
	bool fuzzy = false;           //!< true ならあいまい一致にする ("FZ")
};

/**
 * @brief 名前がキーワードに一致するか
 * @param name 対象の名前
 * @param keyword キーワード (空なら true を返す)
 * @param opt 照合方法
 * @details 実測 (MainFrm.cpp:17790): あいまいでなければ
 *          `contains_word_and_or` (半角スペース区切りが AND、`|` が OR)、
 *          あいまいなら `contains_fuzzy_word` (部分列で一致)。
 */
bool MatchFilter(const UnicodeString &name, const UnicodeString &keyword,
                 const FilterOptions &opt = FilterOptions());

//---------------------------------------------------------------------------
// SimilarSort (F:SimilarSort)
//---------------------------------------------------------------------------

/// SimilarSort の距離の測り方 (VCL の ActionParam "IA/IX/IC/IN/IF" のうち
/// Phase 2 骨格で意味のあるものだけ。拡張子だけ・数字だけ等の部分比較は
/// FileItem が名前しか持たないため対象外)
struct SimilarOptions {
	bool ignore_case = false;    //!< 大小文字を無視する ("IA" の一部)
	bool ignore_number = false;  //!< 数字部分を無視する ("IN" の一部)
	bool ignore_width = false;   //!< 全角/半角を無視する (get_NrmLevenshteinDistance の ig_fh)
};

/**
 * @brief 類似度順の並び (添字列) を返す
 * @param ref_index 基準 (カーソル位置) の添字
 * @param names 項目名 (表示順)
 * @param is_parent ".." か (VCL の is_up に相当。末尾に回す)
 * @return names への添字の順列。基準が先頭、親が末尾、その間は距離の昇順
 *         (同点は安定 = 元の順序)。空・範囲外の基準なら恒等順
 */
std::vector<std::size_t> RankBySimilarity(std::size_t ref_index,
                                          const std::vector<UnicodeString> &names,
                                          const std::vector<bool> &is_parent,
                                          const SimilarOptions &opt = SimilarOptions());

}  // namespace file_narrow

#endif  // NYANFI_GUI_FILE_NARROW_H
