/**
 * @file gui/selection.cpp
 * @brief 一覧の選択操作の実装 (設計と VCL の該当行は gui/selection.h を参照)
 */
#include "gui/selection.h"

#include "usr_file_ex.h"
#include "usr_str.h"

namespace selection {

namespace {

/// 選択の対象にしてよい項目か。VCL の `is_selectable(fp) && !fp->is_dummy` 相当。
/// `..` は選択できない
bool is_selectable(const FileItem &item)
{
	return !item.is_parent;
}

/// 拡張子 (先頭の '.' を含まない。無ければ空)
UnicodeString ext_of(const UnicodeString &name)
{
	const UnicodeString e = get_extension(name);
	return StartsStr(_T("."), e)? e.SubString(2) : e;
}

/// 拡張子を除いたファイル名主部
UnicodeString base_of(const UnicodeString &name)
{
	return ChangeFileExt(name, EmptyStr);
}

}  // namespace

//---------------------------------------------------------------------------
int MarkedCount(const std::vector<FileItem> &items)
{
	int n = 0;
	for (const FileItem &it : items) {
		if (it.marked) n++;
	}
	return n;
}

//---------------------------------------------------------------------------
void ReverseAll(std::vector<FileItem> &items)
{
	for (FileItem &it : items) {
		if (is_selectable(it)) it.marked = !it.marked;
	}
}

//---------------------------------------------------------------------------
void ReverseFiles(std::vector<FileItem> &items)
{
	for (FileItem &it : items) {
		if (is_selectable(it) && !it.is_dir) it.marked = !it.marked;
	}
}

//---------------------------------------------------------------------------
void ToggleAllFiles(std::vector<FileItem> &items)
{
	const bool not_sel = (MarkedCount(items) == 0);
	for (FileItem &it : items) {
		if (!is_selectable(it)) continue;
		// ディレクトリは常に解除 (MainFrm.cpp:24833 の `!fp->is_dir? not_sel : false`)
		it.marked = it.is_dir? false : not_sel;
	}
}

//---------------------------------------------------------------------------
void ToggleAllItems(std::vector<FileItem> &items)
{
	const bool not_sel = (MarkedCount(items) == 0);
	for (FileItem &it : items) {
		if (is_selectable(it)) it.marked = not_sel;
	}
}

//---------------------------------------------------------------------------
void ClearAll(std::vector<FileItem> &items)
{
	for (FileItem &it : items) it.marked = false;
}

//---------------------------------------------------------------------------
bool SelectSameExt(std::vector<FileItem> &items, int cursor)
{
	if (cursor < 0 || cursor >= static_cast<int>(items.size())) return false;
	const FileItem &cur = items[cursor];
	if (cur.is_dir || cur.is_parent) return false;

	const UnicodeString target = ext_of(cur.name);
	for (FileItem &it : items) {
		if (!is_selectable(it) || it.is_dir) continue;
		// 追加ではなく「一致するものだけを選択し直す」(MainFrm.cpp:25337)
		it.marked = SameText(ext_of(it.name), target);
	}
	return true;
}

//---------------------------------------------------------------------------
bool SelectSameName(std::vector<FileItem> &items, int cursor)
{
	if (cursor < 0 || cursor >= static_cast<int>(items.size())) return false;
	const FileItem &cur = items[cursor];
	if (cur.is_dir || cur.is_parent) return false;

	const UnicodeString target = base_of(cur.name);
	for (FileItem &it : items) {
		if (!is_selectable(it) || it.is_dir) continue;
		it.marked = SameText(base_of(it.name), target);
	}
	return true;
}

//---------------------------------------------------------------------------
int SelectMatching(std::vector<FileItem> &items, const UnicodeString &word)
{
	if (word.IsEmpty()) return 0;

	int n = 0;
	for (FileItem &it : items) {
		if (!is_selectable(it)) continue;
		it.marked = ContainsText(it.name, word);
		if (it.marked) n++;
	}
	return n;
}

//---------------------------------------------------------------------------
int SelectByDate(std::vector<FileItem> &items, const TDateTime &border, DateCompare how)
{
	int n = 0;
	for (FileItem &it : items) {
		if (!is_selectable(it)) continue;

		bool hit = false;
		switch (how) {
		case DateCompare::Before: hit = (it.stamp < border); break;
		case DateCompare::After:  hit = (it.stamp > border); break;
		case DateCompare::Same:
			// 「同じ日」の比較。時刻は見ない
			hit = IsSameDay(it.stamp, border);
			break;
		}
		it.marked = hit;
		if (hit) n++;
	}
	return n;
}

//---------------------------------------------------------------------------
int FindNextMarked(const std::vector<FileItem> &items, int cursor, bool forward)
{
	const int n = static_cast<int>(items.size());
	if (forward) {
		for (int i = cursor + 1; i < n; i++) {
			if (items[i].marked) return i;
		}
	}
	else {
		for (int i = cursor - 1; i >= 0; i--) {
			if (items[i].marked) return i;
		}
	}
	return -1;  // 巡回しない
}

//---------------------------------------------------------------------------
void MarkRange(std::vector<FileItem> &items, int from, int to)
{
	const int n = static_cast<int>(items.size());
	int lo = (from < to)? from : to;
	int hi = (from < to)? to : from;
	if (lo < 0) lo = 0;
	if (hi > n) hi = n;

	for (int i = lo; i < hi; i++) {
		if (is_selectable(items[i])) items[i].marked = true;
	}
}

}  // namespace selection
