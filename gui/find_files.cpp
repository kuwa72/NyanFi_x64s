/**
 * @file gui/find_files.cpp
 * @brief ファイル名検索の実装 (設計は gui/find_files.h)
 */
#include "gui/find_files.h"

#include <map>

#include "gui/view_state.h"
#include "usr_file_inf.h"
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

//---------------------------------------------------------------------------
DuplicateResult FindDuplicates(const UnicodeString &root, DuplicateBy how,
                               bool show_hidden, bool show_system)
{
	DuplicateResult out;

	// まず全ファイルを集める
	Query q;
	q.target = Target::Files;
	q.recursive = true;
	q.show_hidden = show_hidden;
	q.show_system = show_system;
	const Result all = Search(root, q);
	out.truncated_scan = all.truncated_scan;

	// サイズで束ねる。サイズが違えば内容も違うので、ここで落とせる分は落とす
	std::map<Int64, std::vector<FileItem>> by_size;
	for (const FileItem &it : all.items) {
		if (it.size <= 0) continue;  // 空ファイルは対象外 (互いに「同じ」になってしまう)
		by_size[it.size].push_back(it);
	}

	for (auto &kv : by_size) {
		std::vector<FileItem> &group = kv.second;
		if (group.size() < 2) continue;

		if (how == DuplicateBy::NameSize) {
			// 名前も同じものだけを重複とする
			std::map<UnicodeString, std::vector<FileItem>> by_name;
			for (const FileItem &it : group) by_name[it.name.UpperCase()].push_back(it);
			for (auto &nk : by_name) {
				if (nk.second.size() < 2) continue;
				out.groups++;
				for (const FileItem &it : nk.second) out.items.push_back(it);
			}
			continue;
		}

		// 内容で比べる。ここまで来たものだけハッシュを取る
		std::map<UnicodeString, std::vector<FileItem>> by_hash;
		for (const FileItem &it : group) {
			const UnicodeString h = get_HashStr(it.full_path, _T("MD5"));
			out.hashed++;
			if (h.IsEmpty()) continue;  // 読めないものは重複判定から外す
			by_hash[h].push_back(it);
		}
		for (auto &hk : by_hash) {
			if (hk.second.size() < 2) continue;
			out.groups++;
			for (const FileItem &it : hk.second) out.items.push_back(it);
		}
	}
	return out;
}

}  // namespace find_files
