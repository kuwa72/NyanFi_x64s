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

//---------------------------------------------------------------------------
int SelectByMask(std::vector<FileItem> &items, const UnicodeString &mask)
{
	int n = 0;
	for (std::size_t i = 0; i < items.size(); ++i) {
		FileItem &itm = items[i];
		if (!is_selectable(itm) || itm.is_separator) continue;
		itm.marked = MatchPathMask(mask, itm.name, itm.is_dir);
		if (itm.marked) ++n;
	}
	return n;
}

//---------------------------------------------------------------------------
int SelectByNames(std::vector<FileItem> &items, const std::vector<UnicodeString> &names)
{
	int n = 0;
	for (std::size_t i = 0; i < items.size(); ++i) {
		FileItem &itm = items[i];
		if (!is_selectable(itm) || itm.is_separator) continue;

		bool hit = false;
		for (std::size_t j = 0; j < names.size() && !hit; ++j) {
			hit = SameText(itm.name, names[j]);
		}
		itm.marked = hit;
		if (hit) ++n;
	}
	return n;
}

//---------------------------------------------------------------------------
namespace {

/// `format_Date` (src/UserFunc.cpp:438) と同じ
UnicodeString format_date(const TDateTime &dt)
{
	return FormatDateTime(_T("yyyy'/'mm'/'dd"), dt);
}

/**
 * @brief 日付条件を解釈する
 * @param prm 条件文字列
 * @param dt [out] 境界の日付
 * @param ct `CP` (カーソル位置) のときに使う日付
 * @return 1 = より古い / 2 = 同じ日 / 3 = より新しい。条件なしは 0、エラーは -1
 * @details **`get_DateCond` (src/UserFunc.cpp:464) の書き写し。**
 *          `UserFunc.cpp` はコンパイルは通るがリンクできない
 *          (VCL コントロールを触る関数が宣言のみのシムを呼ぶ。報告書 §24)
 *          ので、必要な2つだけをここに写した。**中身は1行ずつ突き合わせてある。**
 *          元を直したらこちらも直すこと
 */
int date_cond_of(UnicodeString prm, TDateTime &dt, const TDateTime &ct)
{
	int cnd = 0;
	if (prm.IsEmpty()) return 0;

	if      (SameText(prm, _T("TD"))) prm = _T("=") + format_date(Date());
	else if (SameText(prm, _T("CP"))) prm = _T("=") + format_date(ct);

	cnd = UnicodeString(_T("<=>")).Pos(prm[1]);
	if (cnd <= 0) return -1;

	prm.Delete(1, 1);
	// 絶対指定 (yyyy/mm/dd)
	if (TRegEx::IsMatch(prm, _T("^\\d{4}/\\d{2}/\\d{2}$"))) {
		dt = str_to_DateTime(prm);
		return cnd;
	}

	// 相対指定 (-30D / 6M / 1Y)
	if (prm.IsEmpty()) return -1;
	const UnicodeString unit = prm.SubString(prm.Length(), 1).UpperCase();
	if (!ContainsText(_T("DMY"), unit)) return -1;
	delete_end(prm);

	dt = Date();
	const int dn = prm.ToIntDef(0);
	if (dn != 0) {
		switch (idx_of_word_i(_T("D|M|Y"), unit)) {
		case 0: dt = IncDay(dt, dn);   break;
		case 1: dt = IncMonth(dt, dn); break;
		case 2: dt = IncYear(dt, dn);  break;
		}
	}
	return cnd;
}

/// `test_DateCond` (src/UserFunc.cpp:510) と同じ。**日付だけを比べる** (時刻は見ない)
bool test_date_cond(int cnd, const TDateTime &dt, const TDateTime &dt_r)
{
	const TValueRelationship res = System::Dateutils::CompareDate(dt, dt_r);
	switch (cnd) {
	case 1: return res == LessThanValue;
	case 2: return res == EqualsValue;
	case 3: return res == GreaterThanValue;
	}
	return false;
}

}  // namespace

//---------------------------------------------------------------------------
int SelectByDateCondition(std::vector<FileItem> &items, const UnicodeString &cond,
                          const TDateTime &cursor_time, UnicodeString &error)
{
	TDateTime border;
	// 解釈は date_cond_of (get_DateCond の書き写し) に任せる。0 以下なら不正
	const int how = date_cond_of(cond, border, cursor_time);
	if (how <= 0) {
		error = _T("日付条件が正しくありません");
		return -1;
	}

	int n = 0;
	for (std::size_t i = 0; i < items.size(); ++i) {
		FileItem &itm = items[i];
		if (!is_selectable(itm) || itm.is_separator) continue;
		// VCL はディレクトリを常に非選択にする (MainFrm.cpp:16292)
		itm.marked = !itm.is_dir && test_date_cond(how, itm.stamp, border);
		if (itm.marked) ++n;
	}
	return n;
}

//---------------------------------------------------------------------------
int FindNextSameName(const std::vector<FileItem> &items, int cursor)
{
	const int n = static_cast<int>(items.size());
	if (cursor < 0 || cursor >= n) return -1;
	if (items[static_cast<std::size_t>(cursor)].is_dir) return -1;

	const UnicodeString base = get_base_name(items[static_cast<std::size_t>(cursor)].name);
	int wrap = -1;

	for (int i = 0; i < n; ++i) {
		if (i <= cursor && wrap != -1) continue;
		const FileItem &itm = items[static_cast<std::size_t>(i)];
		if (itm.is_dir || itm.is_separator) continue;
		if (!SameText(base, get_base_name(itm.name))) continue;
		if (i <= cursor) wrap = i; else return i;
	}
	// 動かないなら「見つからなかった」と同じ扱いにする (VCL も
	// new_idx==c_idx のときは SetActionAbort する)
	return (wrap == cursor)? -1 : wrap;
}

//---------------------------------------------------------------------------
namespace {

/// 名前を ";" で繋いでマスクにする
UnicodeString join_mask(const std::vector<UnicodeString> &names)
{
	UnicodeString mask;
	for (std::size_t i = 0; i < names.size(); ++i) {
		if (!mask.IsEmpty()) mask += _T(";");
		mask += names[i];
	}
	return mask;
}

}  // namespace

//---------------------------------------------------------------------------
UnicodeString MaskOfMarked(const std::vector<FileItem> &items)
{
	std::vector<UnicodeString> names;
	for (std::size_t i = 0; i < items.size(); ++i) {
		const FileItem &itm = items[i];
		if (!is_selectable(itm) || itm.is_separator || !itm.marked) continue;
		names.push_back(itm.name);
	}
	return join_mask(names);
}

//---------------------------------------------------------------------------
UnicodeString MaskExcludingMarked(const std::vector<FileItem> &items)
{
	std::vector<UnicodeString> names;
	for (std::size_t i = 0; i < items.size(); ++i) {
		const FileItem &itm = items[i];
		if (!is_selectable(itm) || itm.is_separator || itm.marked) continue;
		names.push_back(itm.name);
	}
	return join_mask(names);
}

}  // namespace selection
