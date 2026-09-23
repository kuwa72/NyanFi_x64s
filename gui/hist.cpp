/**
 * @file gui/hist.cpp
 * @brief gui/hist.h の実装
 */
#include "gui/hist.h"

#include <algorithm>

#include "UIniFile.h"
#include "usr_file_ex.h"
#include "usr_str.h"

namespace hist {

namespace {

const wchar_t *const kAllSection = _T("WxGuiAllDirHistory");
const wchar_t *const kPrefSection = _T("WxGuiDirHistory");

UnicodeString normalized_directory(const UnicodeString &path)
{
	UnicodeString p = path.Trim();
	if (p.IsEmpty()) return EmptyStr;
	return IncludeTrailingPathDelimiter(p);
}

bool same_path(const UnicodeString &a, const UnicodeString &b)
{
	return SameText(ExcludeTrailingPathDelimiter(a), ExcludeTrailingPathDelimiter(b));
}

}  // namespace

//---------------------------------------------------------------------------
bool ParseMode(const UnicodeString &param, Mode &mode_out)
{
	if (SameText(param, _T("GA")) || SameText(param, _T("GS"))) {
		mode_out = Mode::All;
		return true;
	}
	if (SameText(param, _T("FM"))) {
		mode_out = Mode::Search;
		return true;
	}
	if (SameText(param, _T("RD"))) {
		mode_out = Mode::Recent;
		return true;
	}
	// AC/GC はダイアログを開かないコマンド (usr_cmdlist.cpp:715-723)。
	return false;
}

//---------------------------------------------------------------------------
UnicodeString ModeTitle(Mode mode, int shown, int total)
{
	UnicodeString title;
	switch (mode) {
	case Mode::Current:
		title = _T("ディレクトリ履歴 - 現在側");
		break;
	case Mode::All:
		title = _T("ディレクトリ履歴 - 全体");
		break;
	case Mode::Search:
		title.sprintf(_T("ディレクトリ履歴 - 全体検索 (%d/%d)"), shown, total);
		break;
	case Mode::Recent:
		title = _T("最近使ったディレクトリ");
		break;
	case Mode::Stack:
		title = _T("ディレクトリ・スタック");
		break;
	}
	return title;
}

//---------------------------------------------------------------------------
bool UsesFilter(Mode mode)
{
	return mode == Mode::Search;
}

//---------------------------------------------------------------------------
UnicodeString DisplayPath(const Entry &entry)
{
	if (!entry.path.IsEmpty()) return entry.path;
	return get_csv_item(entry.record, 0);
}

//---------------------------------------------------------------------------
bool MatchesFilter(const Entry &entry, const UnicodeString &filter, bool case_sensitive)
{
	if (filter.IsEmpty()) return true;
	return contains_word_and_or(DisplayPath(entry), filter, case_sensitive);
}

//---------------------------------------------------------------------------
View BuildView(const State &state, bool case_sensitive)
{
	View view;
	for (std::size_t i = 0; i < state.entries.size(); ++i) {
		if (!UsesFilter(state.mode) ||
		    MatchesFilter(state.entries[i], state.filter, case_sensitive)) {
			view.visible.push_back(i);
		}
	}
	return view;
}

//---------------------------------------------------------------------------
void RemoveVisible(std::vector<Entry> &entries, const std::vector<std::size_t> &visible)
{
	std::vector<std::size_t> sorted = visible;
	std::sort(sorted.begin(), sorted.end(), std::greater<std::size_t>());
	for (std::size_t index : sorted) {
		if (index < entries.size()) entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(index));
	}
}

//---------------------------------------------------------------------------
std::vector<UnicodeString> CopyLines(const State &state, const View &view)
{
	std::vector<UnicodeString> out;
	out.reserve(view.visible.size());
	for (std::size_t index : view.visible) {
		if (index < state.entries.size()) out.push_back(DisplayPath(state.entries[index]));
	}
	return out;
}

//---------------------------------------------------------------------------
void MergeDirectories(std::vector<Entry> &entries, const std::vector<UnicodeString> &directories)
{
	// 既存項目も VCL と同じく表示用には末尾区切りを付ける。record の CSV は
	// 壊さず、path 側だけを正規化する。
	for (Entry &entry : entries) {
		const UnicodeString normalized = normalized_directory(DisplayPath(entry));
		if (normalized.IsEmpty()) continue;
		entry.path = normalized;
		// path を正規化したら、先頭項目だけが古い record を残さないよう
		// VCL 互換 CSV の path も更新する (件数などの後続項目は保持)。
		TStringDynArray fields = get_csv_array(entry.record, 3, false);
		if (fields.Length == 0) {
			entry.record = make_csv_rec_str({normalized, _T("0")});
		}
		else {
			fields[0] = normalized;
			if (fields.Length < 2) fields.Length = 2;
			if (fields[1].IsEmpty()) fields[1] = _T("0");
			entry.record = make_csv_rec_str(fields);
		}
	}
	for (const UnicodeString &raw : directories) {
		const UnicodeString path = normalized_directory(raw);
		if (path.IsEmpty()) continue;
		bool exists = false;
		for (Entry &entry : entries) {
			if (same_path(DisplayPath(entry), path)) {
				exists = true;
				break;
			}
		}
		if (exists) continue;
		Entry entry;
		entry.path = path;
		entry.record = make_csv_rec_str({path, _T("0")});
		entries.push_back(entry);
	}
}

//---------------------------------------------------------------------------
UnicodeString DisplayWithAccelerator(const Entry &entry, int index)
{
	UnicodeString text;
	if (index >= 0 && index < 10) {
		text.sprintf(_T("%d "), (index + 1) % 10);
	}
	text += DisplayPath(entry);
	return text;
}

//---------------------------------------------------------------------------
bool CanClearAll(Mode mode, int count)
{
	return count > 0 && mode != Mode::Stack;
}

//---------------------------------------------------------------------------
bool CanClearFiltered(Mode mode, int shown, int total)
{
	return mode == Mode::Search && shown > 0 && shown < total;
}

//---------------------------------------------------------------------------
bool CanCopy(int count)
{
	return count > 0;
}

//---------------------------------------------------------------------------
bool CanProperty(int selected)
{
	return selected >= 0;
}

//---------------------------------------------------------------------------
void Store::LoadFromIni(UsrIniFile &ini)
{
	entries_.clear();
	const int count = std::max(0, ini.ReadInteger(kAllSection, _T("Count"), 0));
	for (int i = 0; i < count; ++i) {
		UnicodeString key;
		key.sprintf(_T("Item%02d"), i + 1);
		// CSV の外側クォートを残して読み込む。ReadString の既定 true は
		// CSV レコード全体を分解してしまうため false を明示する。
		const UnicodeString record = ini.ReadString(kAllSection, key, EmptyStr, false);
		if (record.IsEmpty()) continue;
		Entry entry;
		entry.record = record;
		entry.path = get_csv_item(record, 0);
		if (!entry.path.IsEmpty()) entries_.push_back(entry);
	}
}

//---------------------------------------------------------------------------
void Store::SaveToIni(UsrIniFile &ini) const
{
	ini.EraseSection(kAllSection);
	ini.WriteInteger(kAllSection, _T("Count"), static_cast<int>(entries_.size()));
	for (std::size_t i = 0; i < entries_.size(); ++i) {
		UnicodeString key;
		key.sprintf(_T("Item%02u"), static_cast<unsigned int>(i + 1));
		const Entry &entry = entries_[i];
		const UnicodeString record = entry.record.IsEmpty()
			? make_csv_rec_str({entry.path, _T("0")}) : entry.record;
		ini.WriteString(kAllSection, key, record);
	}
}

//---------------------------------------------------------------------------
void LoadPreferences(UsrIniFile &ini, Preferences &prefs)
{
	prefs.migemo = ini.ReadBool(kPrefSection, _T("Migemo"), false);
}

//---------------------------------------------------------------------------
void SavePreferences(UsrIniFile &ini, const Preferences &prefs)
{
	ini.WriteBool(kPrefSection, _T("Migemo"), prefs.migemo);
}

}  // namespace hist
