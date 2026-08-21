/**
 * @file gui/find_files.cpp
 * @brief ファイル名検索の実装 (設計は gui/find_files.h)
 */
#include "gui/find_files.h"

#include "gui/view_state.h"
#include "usr_file_ex.h"
#include "usr_str.h"

namespace find_files {

namespace {

/// 1つのワイルドカードとの照合 (`*` と `?`)。大文字小文字は区別しない
bool match_one(const wchar_t *name, const wchar_t *pat)
{
	// 素朴なバックトラック。マスクは短いので十分
	const wchar_t *star = nullptr;
	const wchar_t *mark = nullptr;

	while (*name != L'\0') {
		if (*pat == L'?' || ::towupper(*pat) == ::towupper(*name)) {
			pat++;
			name++;
		}
		else if (*pat == L'*') {
			star = pat++;
			mark = name;
		}
		else if (star != nullptr) {
			pat = star + 1;
			name = ++mark;
		}
		else {
			return false;
		}
	}
	while (*pat == L'*') pat++;
	return *pat == L'\0';
}

void walk(const UnicodeString &dir, const Query &q, Result &out, int &budget)
{
	if (budget <= 0) return;

	const UnicodeString base = IncludeTrailingPathDelimiter(dir);
	TSearchRec sr;
	if (FindFirst(base + "*", faAnyFile, sr) != 0) return;

	do {
		if (budget <= 0) break;
		if (SameStr(sr.Name, ".") || SameStr(sr.Name, "..")) continue;
		if (!view_state::IsListedByAttr(sr.Attr, q.show_hidden, q.show_system)) continue;

		budget--;
		out.scanned++;

		const bool is_dir = ((sr.Attr & faDirectory) != 0);
		const bool want = (q.target == Target::Both)
			|| (is_dir? (q.target == Target::Directories) : (q.target == Target::Files));

		if (want && MatchesMask(sr.Name, q.mask)) {
			if (static_cast<int>(out.items.size()) >= kMaxResults) {
				out.truncated_hits = true;
			}
			else {
				FileItem it;
				it.name = sr.Name;
				it.full_path = base + sr.Name;
				it.attr = sr.Attr;
				it.is_dir = is_dir;
				it.size = is_dir? -1 : sr.Size;
				it.stamp = sr.TimeStamp;
				out.items.push_back(it);
			}
		}

		if (is_dir && q.recursive) walk(base + sr.Name, q, out, budget);
	} while (FindNext(sr) == 0);
	FindClose(sr);
}

}  // namespace

//---------------------------------------------------------------------------
bool MatchesMask(const UnicodeString &name, const UnicodeString &mask)
{
	if (mask.IsEmpty()) return true;

	UnicodeString rest = mask;
	while (!rest.IsEmpty()) {
		UnicodeString one = get_tkn(rest, ';');
		rest = get_tkn_r(rest, ';');
		one = Trim(one);
		if (one.IsEmpty()) {
			if (rest.IsEmpty()) break;
			continue;
		}
		if (match_one(name.c_str(), one.c_str())) return true;
		// get_tkn_r は区切りが無いと同じ文字列を返すので、そこで止める
		if (SameStr(rest, mask)) break;
	}
	return false;
}

//---------------------------------------------------------------------------
Result Search(const UnicodeString &root, const Query &query)
{
	Result out;
	int budget = kMaxScanFiles;
	walk(root, query, out, budget);
	out.truncated_scan = (budget <= 0);
	return out;
}

}  // namespace find_files
