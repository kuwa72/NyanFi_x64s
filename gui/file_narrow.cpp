/**
 * @file gui/file_narrow.cpp
 * @brief gui/file_narrow.h の実装 (wx 非依存)
 */
#include "gui/file_narrow.h"

#include <algorithm>
#include <numeric>

#include "usr_str.h"

namespace file_narrow {

//---------------------------------------------------------------------------
bool MatchFilter(const UnicodeString &name, const UnicodeString &keyword, const FilterOptions &opt)
{
	if (Trim(keyword).IsEmpty()) return true;
	if (opt.fuzzy) return contains_fuzzy_word(name, keyword, opt.case_sensitive);
	return contains_word_and_or(name, keyword, opt.case_sensitive);
}

//---------------------------------------------------------------------------
std::vector<std::size_t> RankBySimilarity(std::size_t ref_index,
                                          const std::vector<UnicodeString> &names,
                                          const std::vector<bool> &is_parent,
                                          const SimilarOptions &opt)
{
	std::vector<std::size_t> order(names.size());
	std::iota(order.begin(), order.end(), 0);
	if (names.empty() || ref_index >= names.size()) return order;

	const UnicodeString &ref = names[ref_index];

	// VCL と同じ値域にする (カーソル=-1、親=1000、距離=0～1000)。
	// 実際に並べるのは順位だけなので、値は比較用の中間値
	std::vector<int> dist(names.size(), 0);
	for (std::size_t i = 0; i < names.size(); ++i) {
		if (i == ref_index) {
			dist[i] = -1;
		}
		else if (i < is_parent.size() && is_parent[i]) {
			dist[i] = 1000;
		}
		else {
			dist[i] = get_NrmLevenshteinDistance(ref, names[i], opt.ignore_case,
			                                     opt.ignore_number, opt.ignore_width);
		}
	}

	// 同点は元の順序のまま (VCL の CustomSort も同点の順序は保つ想定。
	// std::stable_sort で明示する)
	std::stable_sort(order.begin(), order.end(),
	                 [&dist](std::size_t a, std::size_t b) { return dist[a] < dist[b]; });
	return order;
}

}  // namespace file_narrow
