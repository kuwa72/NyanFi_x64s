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

		if (want && MatchesMask(sr.Name, q.mask)
			&& MatchesQuery(sr.Name, sr.TimeStamp, is_dir ? 0 : sr.Size, sr.Attr, is_dir, q)) {
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
bool MatchesQuery(const UnicodeString &name, TDateTime stamp, Int64 size, int attr,
                  bool is_dir, const Query &query)
{
	//キーワード (src/Global.cpp check_file_std と同じ順序・同じ意味)
	if (!query.keyword.IsEmpty()) {
		UnicodeString kwd = query.keyword;
		bool is_regex = query.use_regex;
		//ダブルクォーテーションで囲まれていたら空白を含む語として正規表現で
		if (is_quot(kwd)) {
			kwd = TRegEx::Escape(exclude_quot(kwd));
			if (ContainsStr(kwd, " ")) kwd = ReplaceStr(kwd, " ", "\\s");
			is_regex = true;
		}
		if (is_regex) {
			try {
				TRegExOptions opt;
				if (!query.case_sensitive) opt << roIgnoreCase;
				if (!TRegEx::IsMatch(name, kwd, opt)) return false;
			}
			catch (...) {
				return false;  // VCL は事前チェックで弾く。ここでは念のため偽
			}
		}
		else {
			if (!find_mlt(kwd, name, query.match_all, false, query.case_sensitive)) return false;
		}
	}
	//タイムスタンプ (日付だけ見て時刻は見ない)
	if (query.date_mode != DateMode::None) {
		const TValueRelationship res = System::Dateutils::CompareDate(stamp, query.date_value);
		switch (query.date_mode) {
		case DateMode::Same:   if (res != EqualsValue) return false; break;
		case DateMode::Before: if (res != EqualsValue && res != LessThanValue) return false; break;
		case DateMode::After:  if (res != EqualsValue && res != GreaterThanValue) return false; break;
		default: break;
		}
	}
	//サイズ (VCL と同じくディレクトリは対象外)
	if (!is_dir && query.size_mode != SizeMode::None) {
		switch (query.size_mode) {
		case SizeMode::AtMost:  if (!(size <= query.size_value)) return false; break;
		case SizeMode::AtLeast: if (!(size >= query.size_value)) return false; break;
		default: break;
		}
	}
	//属性
	if (query.attr_mode != AttrMode::None) {
		switch (query.attr_mode) {
		case AttrMode::HasAny:  if (!(attr & query.attr_bits)) return false; break;
		case AttrMode::HasNone: if ((attr & query.attr_bits)) return false; break;
		default: break;
		}
	}

	return true;
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
DuplicateResult FindDuplicates(const UnicodeString &root, const DuplicateOptions &opt,
                               bool show_hidden, bool show_system)
{
	DuplicateResult out;

	// まず全ファイルを集める
	Query q;
	q.target = Target::Files;
	q.recursive = opt.recursive;
	q.mask = opt.mask;
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

		if (opt.how == DuplicateBy::NameSize) {
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
