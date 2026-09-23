/**
 * @file gui/key.cpp
 * @brief gui/key.h の実装
 */
#include "gui/key.h"

#include <algorithm>

#include "UIniFile.h"

namespace key {

namespace {
const wchar_t *const kSection = _T("WxGuiKeyList");
}

//---------------------------------------------------------------------------
UnicodeString TabLabel(Tab tab)
{
	switch (tab) {
	case Tab::File:   return _T("ファイラー");
	case Tab::Search: return _T("INC.サーチ");
	case Tab::Text:   return _T("テキストビューア");
	case Tab::Image:  return _T("イメージビューア");
	case Tab::List:   return _T("ログ");
	}
	return EmptyStr;
}

//---------------------------------------------------------------------------
bool ParseCommandRecord(const UnicodeString &record, UnicodeString &mode_out,
                        UnicodeString &command_out, UnicodeString &description_out)
{
	const int colon = record.Pos(_T(':'));
	const int equal = record.Pos(_T('='));
	if (colon <= 1 || equal <= colon + 1) return false;
	mode_out = record.SubString(1, colon - 1);
	command_out = record.SubString(colon + 1, equal - colon - 1);
	description_out = record.SubString(equal + 1);
	return !command_out.IsEmpty();
}

//---------------------------------------------------------------------------
bool ParseAssignment(const UnicodeString &line, const UnicodeString &description,
                     Entry &entry_out)
{
	const int equal = line.Pos(_T('='));
	if (equal <= 1 || equal >= line.Length()) return false;
	entry_out.key = line.SubString(1, equal - 1);
	entry_out.command = line.SubString(equal + 1);
	entry_out.description = description;
	return !entry_out.command.IsEmpty();
}

//---------------------------------------------------------------------------
UnicodeString FormatKeyForDisplay(const UnicodeString &key)
{
	UnicodeString out;
	for (int i = 1; i <= key.Length(); ++i) {
		const wchar_t ch = key[i];
		if ((ch == _T('+') || ch == _T('~')) && i > 1 && key[i - 1] != _T('_')) {
			if (!out.IsEmpty() && out[out.Length()] != _T(' ')) out += _T(' ');
			out += ch;
			out += _T(' ');
		}
		else {
			out += ch;
		}
	}
	return out;
}

//---------------------------------------------------------------------------
bool MatchesFilter(const Entry &entry, const UnicodeString &filter)
{
	if (filter.IsEmpty()) return true;
	UnicodeString all = entry.key + _T(" ") + entry.command + _T(" ") + entry.description;
	return contains_word_and_or(all, filter, false);
}

//---------------------------------------------------------------------------
void SortEntries(std::vector<Entry> &entries, SortMode mode)
{
	std::stable_sort(entries.begin(), entries.end(), [mode](const Entry &a, const Entry &b) {
		switch (mode) {
		case SortMode::Key:         return CompareText(a.key, b.key) < 0;
		case SortMode::Command:     return CompareText(a.command, b.command) < 0;
		case SortMode::Description: return CompareText(a.description, b.description) < 0;
		}
		return false;
	});
}

//---------------------------------------------------------------------------
std::vector<Entry> BuildRows(const std::vector<Entry> &assignments,
                             const std::vector<Entry> &commands,
                             bool show_all)
{
	std::vector<Entry> rows;
	for (const Entry &command : commands) {
		const Entry *assigned = nullptr;
		for (const Entry &candidate : assignments) {
			if (candidate.tab == command.tab && SameText(candidate.command, command.command)) {
				assigned = &candidate;
				break;
			}
		}
		if (assigned != nullptr) rows.push_back(*assigned);
		else if (show_all) rows.push_back(command);
	}

	// VCL は KeyFuncList にだけある孤立した行も表示する。
	for (const Entry &assignment : assignments) {
		bool found = false;
		for (const Entry &row : rows) {
			if (row.tab == assignment.tab && SameText(row.command, assignment.command)) {
				found = true;
				break;
			}
		}
		if (!found) rows.push_back(assignment);
	}
	return rows;
}

//---------------------------------------------------------------------------
std::vector<Entry> FilterAndSort(const std::vector<Entry> &entries,
                                 const UnicodeString &filter, SortMode mode)
{
	std::vector<Entry> out;
	for (const Entry &entry : entries) {
		if (MatchesFilter(entry, filter)) out.push_back(entry);
	}
	SortEntries(out, mode);
	return out;
}

//---------------------------------------------------------------------------
UnicodeString FormatList(const std::vector<Entry> &entries)
{
	UnicodeString text = _T("キー\tコマンド\t説明");
	for (const Entry &entry : entries) {
		text += _T("\r\n");
		text += entry.key + _T("\t") + entry.command + _T("\t") + entry.description;
	}
	return text;
}

//---------------------------------------------------------------------------
void StateStore::LoadFromIni(UsrIniFile &ini)
{
	tab = static_cast<Tab>(std::clamp(ini.ReadInteger(kSection, _T("Tab"), 0), 0, 4));
	sort_mode = static_cast<SortMode>(std::clamp(ini.ReadInteger(kSection, _T("Sort"), 0), 0, 2));
	show_all = ini.ReadBool(kSection, _T("ShowAll"), false);
	migemo = ini.ReadBool(kSection, _T("Migemo"), false);
	confirm_execute = ini.ReadBool(kSection, _T("Confirm"), false);
	filter = ini.ReadString(kSection, _T("Filter"), EmptyStr);
}

//---------------------------------------------------------------------------
void StateStore::SaveToIni(UsrIniFile &ini) const
{
	ini.WriteInteger(kSection, _T("Tab"), static_cast<int>(tab));
	ini.WriteInteger(kSection, _T("Sort"), static_cast<int>(sort_mode));
	ini.WriteBool(kSection, _T("ShowAll"), show_all);
	ini.WriteBool(kSection, _T("Migemo"), migemo);
	ini.WriteBool(kSection, _T("Confirm"), confirm_execute);
	ini.WriteString(kSection, _T("Filter"), filter);
}

}  // namespace key
