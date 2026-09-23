/**
 * @file gui/gen_info.cpp
 * @brief gui/gen_info.h の実装
 */
#include "gui/gen_info.h"

#include <algorithm>
#include <set>

#include "usr_str.h"

namespace gen_info {

namespace {

std::vector<UnicodeString> split_words(const UnicodeString &text)
{
	std::vector<UnicodeString> words;
	UnicodeString word;
	for (int i = 1; i <= text.Length(); ++i) {
		const wchar_t ch = text[i];
		if (ch == L' ' || ch == L'\t') {
			if (!word.IsEmpty()) {
				words.push_back(word);
				word = EmptyStr;
			}
		}
		else {
			word += ch;
		}
	}
	if (!word.IsEmpty()) words.push_back(word);
	return words;
}

bool contains_text(const UnicodeString &text, const UnicodeString &part, bool case_sensitive)
{
	return case_sensitive ? ContainsStr(text, part) : ContainsText(text, part);
}

std::vector<UnicodeString> split_or_groups(const UnicodeString &text)
{
	std::vector<UnicodeString> groups;
	UnicodeString group;
	for (int i = 1; i <= text.Length(); ++i) {
		if (text[i] == L'|') {
			groups.push_back(group);
			group = EmptyStr;
		}
		else {
			group += text[i];
		}
	}
	groups.push_back(group);
	return groups;
}

bool matches_filter(const UnicodeString &text, Kind kind, const UnicodeString &keyword,
                    bool case_sensitive, bool any_term)
{
	const UnicodeString search_text = DisplayText(text, kind);
	if (!any_term) return contains_text(search_text, keyword, case_sensitive);

	// filter_List (Global.cpp:2845-2893) と同じ「| で OR、空白で AND」。
	const std::vector<UnicodeString> groups = split_or_groups(keyword);
	for (const UnicodeString &raw_group : groups) {
		const std::vector<UnicodeString> words = split_words(raw_group.Trim());
		if (words.empty()) continue;
		bool group_match = true;
		for (const UnicodeString &word : words) {
			if (!contains_text(search_text, word, case_sensitive)) {
				group_match = false;
				break;
			}
		}
		if (group_match) return true;
	}
	return false;
}

bool is_error_heading(const UnicodeString &line)
{
	// TGeneralInfoDlg::UpdateList は 1 始まりの文字 2/3/4 を直接見る。
	return line.Length() >= 4 && line[2] == L'>' && line[3] == L'E' && line[4] == L' ';
}

}  // namespace

//---------------------------------------------------------------------------
Kind DetectKind(const std::vector<UnicodeString> &lines, Kind requested)
{
	if (requested != Kind::Generic) return requested;
	if (lines.empty()) return Kind::Generic;

	// FormShow: isGit = 先頭が "$ git "
	if (StartsStr(_T("$ git "), lines.front())) return Kind::Git;

	// FormShow: 空行以外の各行の Names[i] に空白が無く、1件以上なら変数一覧。
	const bool all_name_value = !lines.empty() && std::all_of(
		lines.begin(), lines.end(), [](const UnicodeString &line) {
			if (line.IsEmpty()) return true;
			const int equal = line.Pos(_T("="));
			if (equal <= 1) return false;
			const UnicodeString name = line.SubString(1, equal - 1);
			return !name.IsEmpty() && !ContainsStr(name, _T(" ")) && !ContainsStr(name, _T("\t"));
		});
	return all_name_value ? Kind::Variable : Kind::Generic;
}

//---------------------------------------------------------------------------
std::vector<Entry> BuildEntries(const std::vector<UnicodeString> &lines, Kind kind,
                                const FilterOptions &options)
{
	std::vector<Entry> entries;
	entries.reserve(lines.size());

	if (kind == Kind::Log && options.errors_only) {
		bool in_error = false;
		for (std::size_t i = 0; i < lines.size(); ++i) {
			const UnicodeString &line = lines[i];
			if (is_error_heading(line)) {
				in_error = true;
				entries.push_back({line, static_cast<int>(i)});
			}
			else if (in_error && StartsStr(_T("    "), line)) {
				entries.push_back({line, static_cast<int>(i)});
			}
			else {
				in_error = false;
			}
		}
		// VCL の UpdateList は ErrOnly を通常のキーワード検索より先に扱い、
		// この場合はフィルタ文字列を適用しない。
		return entries;
	}

	for (std::size_t i = 0; i < lines.size(); ++i) {
		entries.push_back({lines[i], static_cast<int>(i)});
	}

	const UnicodeString keyword = options.keyword.Trim();
	if (keyword.IsEmpty()) return entries;

	std::vector<Entry> filtered;
	filtered.reserve(entries.size());
	// VCL は検索語に大文字が含まれる場合だけ soCaseSens を立てる。
	const bool case_sensitive = options.case_sensitive || contains_upper(keyword);
	for (const Entry &entry : entries) {
		if (matches_filter(entry.text, kind, keyword, case_sensitive, options.any_term)) {
			filtered.push_back(entry);
		}
	}
	return filtered;
}

//---------------------------------------------------------------------------
void SortEntries(std::vector<Entry> &entries, SortMode mode)
{
	if (mode == SortMode::Original) {
		std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
			return a.source_index < b.source_index;
		});
		return;
	}

	const bool descending = mode == SortMode::Descending;
	std::sort(entries.begin(), entries.end(), [descending](const Entry &a, const Entry &b) {
		const UnicodeString na = extract_top_num_str(a.text);
		const UnicodeString nb = extract_top_num_str(b.text);
		int cmp = 0;
		if (!na.IsEmpty() && !nb.IsEmpty()) {
			cmp = ::StrCmpLogicalW(na.c_str(), nb.c_str());
			if (descending) cmp = -cmp;
		}
		if (cmp == 0) {
			cmp = ::StrCmpLogicalW(a.text.c_str(), b.text.c_str());
			if (descending) cmp = -cmp;
		}
		// comp_AscendOrder/comp_DescendOrder と同じ.Objects 順。
		if (cmp == 0) cmp = a.source_index - b.source_index;
		return cmp < 0;
	});
}

//---------------------------------------------------------------------------
std::vector<Entry> RemoveDuplicates(const std::vector<Entry> &entries)
{
	std::vector<Entry> result;
	result.reserve(entries.size());
	std::set<std::wstring> seen;
	for (const Entry &entry : entries) {
		if (seen.insert(entry.text.wstr()).second) result.push_back(entry);
	}
	return result;
}

//---------------------------------------------------------------------------
UnicodeString DisplayText(const UnicodeString &line, Kind kind)
{
	return (kind == Kind::FileList || kind == Kind::Tree) ? get_pre_tab(line) : line;
}

//---------------------------------------------------------------------------
UnicodeString TargetText(const UnicodeString &line, Kind kind)
{
	if (kind == Kind::FileList || kind == Kind::Tree) {
		const int tab = line.Pos(_T("\t"));
		if (tab > 0) return line.SubString(tab + 1);
	}
	return line;
}

//---------------------------------------------------------------------------
UnicodeString ValueText(const UnicodeString &line)
{
	const int equal = line.Pos(_T("="));
	return equal > 0 ? line.SubString(equal + 1) : EmptyStr;
}

//---------------------------------------------------------------------------
UnicodeString JoinEntries(const std::vector<Entry> &entries, const std::vector<int> &indices,
                          Kind kind)
{
	UnicodeString text;
	bool first = true;
	for (int index : indices) {
		if (index < 0 || static_cast<std::size_t>(index) >= entries.size()) continue;
		if (!first) text += _T("\r\n");
		text += DisplayText(entries[static_cast<std::size_t>(index)].text, kind);
		first = false;
	}
	return text;
}

//---------------------------------------------------------------------------
int FindNext(const std::vector<Entry> &entries, int current, const UnicodeString &keyword,
             bool forward, bool case_sensitive)
{
	if (keyword.IsEmpty() || entries.empty()) return -1;
	int index = forward ? current + 1 : current - 1;
	for (; index >= 0 && static_cast<std::size_t>(index) < entries.size();
	     index += forward ? 1 : -1) {
		if (contains_text(entries[static_cast<std::size_t>(index)].text, keyword, case_sensitive)) {
			return index;
		}
	}
	return -1;
}

//---------------------------------------------------------------------------
UnicodeString StatusText(int visible_count, int total_count, int selected_count, int cursor,
                         bool filtered, bool errors_only)
{
	UnicodeString text;
	text.sprintf(_T("%s: %d"), errors_only ? _T("ERR") : _T("項目"), visible_count);
	if (filtered) text.cat_sprintf(_T("/%d"), total_count);
	if (cursor >= 0) text.cat_sprintf(_T("  -  %d"), cursor + 1);
	if (selected_count > 0) text.cat_sprintf(_T("    選択: %d"), selected_count);
	return text;
}

//---------------------------------------------------------------------------
UnicodeString CommandText(const UnicodeString &line)
{
	const UnicodeString head = get_pre_tab(line);
	const std::vector<UnicodeString> words = split_words(head);
	// wx 側が保持する素のコマンドはそのまま返す。
	if (words.size() < 3 || words[0].Pos(_T(":")) <= 0) return head;

	// AddCmdHistory: "hh:nn:ss.zzz <mode> <command>[_<param>]"
	if (SameStr(words[1], _T("-"))) return EmptyStr;
	UnicodeString command;
	for (std::size_t i = 2; i < words.size(); ++i) {
		if (!command.IsEmpty()) command += _T(" ");
		command += words[i];
	}
	return command;
}

}  // namespace gen_info
