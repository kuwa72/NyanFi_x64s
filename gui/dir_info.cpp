/**
 * @file gui/dir_info.cpp
 * @brief ディレクトリ集計の実装 (設計は gui/dir_info.h)
 */
#include "gui/dir_info.h"

#include <algorithm>
#include <map>

#include "gui/view_state.h"
#include "usr_file_ex.h"
#include "usr_str.h"

namespace dir_info {

namespace {

/// 拡張子を小文字で返す (先頭の '.' を含まない)。無ければ "(なし)"
UnicodeString ext_key(const UnicodeString &name)
{
	const UnicodeString e = get_extension(name);
	if (e.IsEmpty()) return _T("(なし)");
	return (StartsStr(_T("."), e)? e.SubString(2) : e).LowerCase();
}

void scan(const UnicodeString &dir, bool show_hidden, bool show_system, int &budget,
          DirSize *size, std::map<UnicodeString, ExtStat> *exts)
{
	if (budget <= 0) return;

	const UnicodeString base = IncludeTrailingPathDelimiter(dir);
	TSearchRec sr;
	if (FindFirst(base + "*", faAnyFile, sr) != 0) return;

	do {
		if (budget <= 0) break;
		if (SameStr(sr.Name, ".") || SameStr(sr.Name, "..")) continue;
		if (!view_state::IsListedByAttr(sr.Attr, show_hidden, show_system)) continue;

		if ((sr.Attr & faDirectory) != 0) {
			if (size != nullptr) size->dirs++;
			scan(base + sr.Name, show_hidden, show_system, budget, size, exts);
			continue;
		}

		budget--;
		if (size != nullptr) {
			size->files++;
			size->bytes += sr.Size;
		}
		if (exts != nullptr) {
			ExtStat &st = (*exts)[ext_key(sr.Name)];
			st.ext = ext_key(sr.Name);
			st.count++;
			st.bytes += sr.Size;
		}
	} while (FindNext(sr) == 0);
	FindClose(sr);
}

void walk_tree(const UnicodeString &dir, int depth, int max_depth, bool show_hidden,
               bool show_system, int &budget, std::vector<TreeLine> &out)
{
	if (depth > max_depth || budget <= 0) return;

	const UnicodeString base = IncludeTrailingPathDelimiter(dir);
	TSearchRec sr;
	if (FindFirst(base + "*", faDirectory, sr) != 0) return;

	std::vector<UnicodeString> names;
	do {
		if (SameStr(sr.Name, ".") || SameStr(sr.Name, "..")) continue;
		if ((sr.Attr & faDirectory) == 0) continue;
		if (!view_state::IsListedByAttr(sr.Attr, show_hidden, show_system)) continue;
		names.push_back(sr.Name);
	} while (FindNext(sr) == 0);
	FindClose(sr);

	// 名前順に並べる (FindFirst の順はファイルシステム任せで安定しない)
	std::sort(names.begin(), names.end(),
	          [](const UnicodeString &a, const UnicodeString &b) { return CompareText(a, b) < 0; });

	for (const UnicodeString &n : names) {
		if (budget <= 0) return;
		budget--;

		TreeLine line;
		line.depth = depth;
		line.name = n;
		out.push_back(line);
		walk_tree(base + n, depth + 1, max_depth, show_hidden, show_system, budget, out);
	}
}

}  // namespace

//---------------------------------------------------------------------------
DirSize CalcDirSize(const UnicodeString &dir, bool show_hidden, bool show_system)
{
	DirSize out;
	int budget = kMaxScanFiles;
	scan(dir, show_hidden, show_system, budget, &out, nullptr);
	out.truncated = (budget <= 0);
	return out;
}

//---------------------------------------------------------------------------
std::vector<ExtStat> CalcExtStats(const UnicodeString &dir, bool recursive,
                                  bool show_hidden, bool show_system, bool &truncated_out)
{
	std::map<UnicodeString, ExtStat> table;
	int budget = kMaxScanFiles;

	if (recursive) {
		scan(dir, show_hidden, show_system, budget, nullptr, &table);
	}
	else {
		// 直下だけ。scan は再帰するので、ここは自前で回す
		const UnicodeString base = IncludeTrailingPathDelimiter(dir);
		TSearchRec sr;
		if (FindFirst(base + "*", faAnyFile, sr) == 0) {
			do {
				if (budget <= 0) break;
				if (SameStr(sr.Name, ".") || SameStr(sr.Name, "..")) continue;
				if ((sr.Attr & faDirectory) != 0) continue;
				if (!view_state::IsListedByAttr(sr.Attr, show_hidden, show_system)) continue;

				budget--;
				ExtStat &st = table[ext_key(sr.Name)];
				st.ext = ext_key(sr.Name);
				st.count++;
				st.bytes += sr.Size;
			} while (FindNext(sr) == 0);
			FindClose(sr);
		}
	}
	truncated_out = (budget <= 0);

	std::vector<ExtStat> out;
	for (const auto &kv : table) out.push_back(kv.second);
	// 件数の多い順。同数なら拡張子名の順で安定させる
	std::sort(out.begin(), out.end(), [](const ExtStat &a, const ExtStat &b) {
		if (a.count != b.count) return a.count > b.count;
		return CompareText(a.ext, b.ext) < 0;
	});
	return out;
}

//---------------------------------------------------------------------------
std::vector<TreeLine> BuildTree(const UnicodeString &dir, int max_depth,
                                bool show_hidden, bool show_system, bool &truncated_out)
{
	std::vector<TreeLine> out;
	int budget = kMaxScanFiles;
	walk_tree(dir, 0, max_depth, show_hidden, show_system, budget, out);
	truncated_out = (budget <= 0);
	return out;
}

}  // namespace dir_info
