/**
 * @file gui/regdir.cpp
 * @brief gui/regdir.h の実装 (wx 非依存)
 */
#include "gui/regdir.h"

#include "UIniFile.h"

namespace regdir {

namespace {

// このクラス専用のセクション名 (gui/tabs.cpp の kSection と同じ考え方)
const UnicodeString kSection = _T("WxGuiRegDir");

/// フィルタを空白区切りで割る (空語は除く)
std::vector<UnicodeString> split_words(const UnicodeString &filter)
{
	std::vector<UnicodeString> words;
	UnicodeString cur;
	for (int i = 1; i <= filter.Length(); i++) {
		const wchar_t c = filter[i];
		if (c == L' ' || c == L'\t') {
			if (!cur.IsEmpty()) {
				words.push_back(cur);
				cur = EmptyStr;
			}
		}
		else {
			cur += c;
		}
	}
	if (!cur.IsEmpty()) words.push_back(cur);
	return words;
}

}  // namespace

//---------------------------------------------------------------------------
RegDirItem ParseRecord(const UnicodeString &record)
{
	// force_size=true で足りない分は空で埋められる (VCL と同じ)
	TStringDynArray itm = get_csv_array(record, kCsvItemCount, true);

	RegDirItem r;
	r.key = itm[0];
	r.title = itm[1];
	r.path = itm[2];
	r.user = itm[3];
	return r;
}

//---------------------------------------------------------------------------
UnicodeString FormatRecord(const RegDirItem &item)
{
	return make_csv_rec_str({item.key, item.title, item.path, item.user});
}

//---------------------------------------------------------------------------
bool IsSeparator(const RegDirItem &item)
{
	return is_separator(item.title);
}

//---------------------------------------------------------------------------
UnicodeString SelectablePath(const RegDirItem &item)
{
	// VCL の GetCurDirItem: is_separator の行では dnam を空のまま返す
	if (IsSeparator(item)) return EmptyStr;
	return item.path;
}

//---------------------------------------------------------------------------
bool MoveTop(std::vector<RegDirItem> &items, int idx)
{
	// VCL の move_top_RegDirItem: if (idx>0 && idx<Count)
	if (idx <= 0 || idx >= static_cast<int>(items.size())) return false;

	// セパレータをまたがないよう、属するグループの先頭を探す
	int top = 0;
	for (int i = idx; i > 0; i--) {
		if (IsSeparator(items[static_cast<std::size_t>(i)])) {
			top = i + 1;
			break;
		}
	}

	RegDirItem moved = items[static_cast<std::size_t>(idx)];
	items.erase(items.begin() + idx);
	items.insert(items.begin() + top, moved);
	return true;
}

//---------------------------------------------------------------------------
std::vector<int> KeyMatches(const std::vector<RegDirItem> &items, const UnicodeString &key)
{
	std::vector<int> hit;
	if (key.IsEmpty()) return hit;

	// VCL の RegDirListBoxKeyPress: SameText(k, itm_buf[0]) で数える
	for (int i = 0; i < static_cast<int>(items.size()); i++) {
		if (SameText(key, items[static_cast<std::size_t>(i)].key)) hit.push_back(i);
	}
	return hit;
}

//---------------------------------------------------------------------------
bool MatchesFilter(const RegDirItem &item, const UnicodeString &filter, bool and_mode)
{
	const std::vector<UnicodeString> words = split_words(filter);
	if (words.empty()) return true;

	// VCL の UpdateSpDirList: contains_upper なら soCaseSens を付ける
	const bool case_sens = contains_upper(filter);

	// 当てる先は表示されるタイトル・パスとキー
	const UnicodeString target = item.title + _T(" ") + item.path + _T(" ") + item.key;

	for (const UnicodeString &w : words) {
		const bool hit = case_sens ? ContainsStr(target, w) : ContainsText(target, w);
		if (hit && !and_mode) return true;   // OR: 1語でも当たれば通す
		if (!hit && and_mode) return false;  // AND: 1語でも外せば落とす
	}
	return and_mode;  // OR ですべて外し→false、AND ですべて当たり→true
}

//---------------------------------------------------------------------------
void RegDirStore::SaveToIni(UsrIniFile &ini) const
{
	ini.WriteInteger(kSection, _T("Count"), static_cast<int>(items_.size()));
	for (int i = 0; i < static_cast<int>(items_.size()); i++) {
		UnicodeString key;
		ini.WriteString(kSection, key.sprintf(_T("Item%02d"), i),
		                FormatRecord(items_[static_cast<std::size_t>(i)]));
	}
}

//---------------------------------------------------------------------------
void RegDirStore::LoadFromIni(UsrIniFile &ini)
{
	const int count = ini.ReadInteger(kSection, _T("Count"), 0);
	if (count <= 0) return;  // セクションが無い/空なら何もしない

	std::vector<RegDirItem> loaded;
	loaded.reserve(static_cast<std::size_t>(count));
	for (int i = 0; i < count; i++) {
		UnicodeString key;
		// 値は CSV 形式で自前のクォートを持つため、ini 側の del_quot は切る
		// (既定 true だと全体の外側クォートが剥がれて先頭末尾の項目が壊れる)
		const UnicodeString rec =
			ini.ReadString(kSection, key.sprintf(_T("Item%02d"), i), EmptyStr, false);
		if (rec.IsEmpty()) continue;
		loaded.push_back(ParseRecord(rec));
	}
	if (loaded.empty()) return;

	items_ = std::move(loaded);
}

}  // namespace regdir
