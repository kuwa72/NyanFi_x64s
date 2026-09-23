/**
 * @file gui/cmd_list.cpp
 * @brief gui/cmd_list.h の実装
 */
#include "gui/cmd_list.h"

#include "usr_file_ex.h"

namespace cmd_list {

//---------------------------------------------------------------------------
bool IsCommandFile(const UnicodeString &path)
{
	return SameText(get_extension(path), _T(".nbt"));
}

//---------------------------------------------------------------------------
UnicodeString NormalizeCommandPath(const UnicodeString &path)
{
	UnicodeString result = Trim(path);
	if (StartsStr(_T("@"), result)) result.Delete(1, 1);
	return exclude_quot(Trim(result));
}

//---------------------------------------------------------------------------
UnicodeString MakeExecutionCommand(const UnicodeString &path)
{
	return _T("ExeCommands_\"@") + NormalizeCommandPath(path) + _T("\"");
}

//---------------------------------------------------------------------------
UnicodeString MakeEditCommand(const UnicodeString &path)
{
	return _T("FileEdit_\"") + NormalizeCommandPath(path) + _T("\"");
}

//---------------------------------------------------------------------------
std::vector<Entry> FilterEntries(const std::vector<Entry> &entries,
                                 const UnicodeString &filter,
                                 const FilterOptions &options)
{
	if (filter.IsEmpty()) return entries;
	std::vector<Entry> result;
	for (const Entry &entry : entries) {
		const UnicodeString text = entry.name + _T(" ") + entry.description;
		bool ok = false;
		if (options.fuzzy) {
			ok = contains_fuzzy_word(text, filter, options.case_sensitive);
		}
		else if (options.regex) {
			if (chk_RegExPtn(filter)) {
				TRegExOptions opt;
				if (!options.case_sensitive) opt << roIgnoreCase;
				ok = TRegEx::IsMatch(text, filter, opt);
			}
		}
		else {
			ok = options.case_sensitive? ContainsStr(text, filter)
			                           : ContainsText(text, filter);
		}
		if (ok) result.push_back(entry);
	}
	return result;
}

//---------------------------------------------------------------------------
int CompareNatural(const Entry &a, const Entry &b)
{
	// VCL の NaturalOrder=true と同じ StrCmpLogicalW を使う。
	return StrCmpLogicalW(a.path.c_str(), b.path.c_str());
}

//---------------------------------------------------------------------------
int FindSelected(const std::vector<Entry> &entries, const UnicodeString &path)
{
	const UnicodeString wanted = NormalizeCommandPath(path);
	for (std::size_t i = 0; i < entries.size(); ++i) {
		if (SameText(NormalizeCommandPath(entries[i].path), wanted)) return static_cast<int>(i);
	}
	return -1;
}

//---------------------------------------------------------------------------
bool ShouldAutoExecute(bool select_only, bool confirm_execute, bool filter_focused,
                       bool filter_empty, int visible_count)
{
	return !select_only && confirm_execute && filter_focused && !filter_empty && visible_count == 1;
}

}  // namespace cmd_list
