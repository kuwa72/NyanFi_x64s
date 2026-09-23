/**
 * @file gui/function_list.cpp
 * @brief gui/function_list.h の実装
 */
#include "gui/function_list.h"

#include "gui/f_misc_ops.h"

namespace function_list {

//---------------------------------------------------------------------------
Mode NormalizeMode(int mode)
{
	if (mode < 0 || mode > 2) return Mode::Function;
	return static_cast<Mode>(mode);
}

//---------------------------------------------------------------------------
UnicodeString ModeTitle(Mode mode)
{
	switch (NormalizeMode(static_cast<int>(mode))) {
	case Mode::UserDefined: return _T("ユーザ定義文字列一覧");
	case Mode::MarkLine:     return _T("マーク行一覧");
	case Mode::Function:
	default:                  return _T("関数一覧");
	}
}

//---------------------------------------------------------------------------
CommandOptions ParseCommandOptions(const UnicodeString &param)
{
	CommandOptions result;
	result.to_filter = f_misc_ops::HasParamToken(param, _T("FF"));
	result.fuzzy = f_misc_ops::HasParamToken(param, _T("FZ"));
	return result;
}

//---------------------------------------------------------------------------
bool IsAvailable(Mode mode, bool is_text, bool has_marks)
{
	if (!is_text) return false;
	if (NormalizeMode(static_cast<int>(mode)) == Mode::MarkLine && !has_marks) return false;
	return true;
}

//---------------------------------------------------------------------------
namespace {

bool LooksLikeFunction(const UnicodeString &line, bool is_dfm)
{
	const UnicodeString trimmed = Trim(line);
	if (trimmed.IsEmpty()) return false;
	if (is_dfm) return StartsText(_T("object "), trimmed) || StartsText(_T("inherited "), trimmed) ||
	                         ContainsStr(trimmed, _T(" = "));
	// VCL の GetFuncPtns が拡張子依存で返すパターンを、固定された小さな
	// 規則に置き換える。名前+括弧、或いは DFM の定義行だけを候補にする。
	return ContainsStr(trimmed, _T("(")) && !ContainsStr(trimmed, _T("="));
}

bool MatchesPattern(const UnicodeString &text, const UnicodeString &pattern, bool regex)
{
	if (pattern.IsEmpty()) return true;
	TRegExOptions opt;
	opt << roIgnoreCase;
	if (regex) return chk_RegExPtn(pattern) && TRegEx::IsMatch(text, pattern, opt);
	return TRegEx::IsMatch(text, TRegEx::Escape(pattern), opt);
}

}  // namespace

//---------------------------------------------------------------------------
std::vector<Entry> BuildEntries(const Source &source, Mode mode)
{
	std::vector<Entry> result;
	const Mode normalized = NormalizeMode(static_cast<int>(mode));
	if (normalized == Mode::MarkLine) {
		for (const int line : source.marks) {
			if (line < 0 || line >= static_cast<int>(source.lines.size())) continue;
			result.push_back({source.lines[static_cast<std::size_t>(line)], line});
		}
		return result;
	}

	for (std::size_t i = 0; i < source.lines.size(); ++i) {
		const UnicodeString line = source.lines[i];
		if (normalized == Mode::Function) {
			const bool match = source.function_pattern.IsEmpty()
				? LooksLikeFunction(line, source.is_dfm)
				: MatchesPattern(line, source.function_pattern, true);
			if (match) result.push_back({TrimRight(line), static_cast<int>(i)});
		}
		else {
			const bool match = MatchesPattern(line, source.user_pattern, source.user_regex);
			if (match) result.push_back({TrimRight(line), static_cast<int>(i)});
		}
	}
	return result;
}

//---------------------------------------------------------------------------
std::vector<Entry> FilterEntries(const std::vector<Entry> &entries,
                                 const UnicodeString &filter,
                                 const FilterOptions &options)
{
	if (filter.IsEmpty()) return entries;
	std::vector<Entry> result;
	for (const Entry &entry : entries) {
		bool ok = false;
		if (options.fuzzy) {
			ok = contains_fuzzy_word(entry.text, filter, options.case_sensitive);
		}
		else if (options.regex) {
			if (chk_RegExPtn(filter)) {
				TRegExOptions opt;
				if (!options.case_sensitive) opt << roIgnoreCase;
				ok = TRegEx::IsMatch(entry.text, filter, opt);
			}
		}
		else {
			ok = options.case_sensitive? ContainsStr(entry.text, filter)
			                           : ContainsText(entry.text, filter);
		}
		if (ok) result.push_back(entry);
	}
	return result;
}

//---------------------------------------------------------------------------
UnicodeString NameOnlyText(const UnicodeString &text, bool name_only,
                           const UnicodeString &name_pattern)
{
	if (!name_only || name_pattern.IsEmpty() || !chk_RegExPtn(name_pattern)) return text;
	TRegExOptions opt;
	opt << roIgnoreCase;
	const TMatch match = TRegEx::Match(text, name_pattern, opt);
	if (!match.Success) return text;
	UnicodeString result = Trim(match.Value);
	while (!result.IsEmpty() && (result[result.Length()] == _T('(') ||
	                              result[result.Length()] == _T('{') ||
	                              result[result.Length()] == _T('[') ||
	                              result[result.Length()] == _T(':'))) {
		result.Delete(result.Length(), 1);
	}
	return result;
}

//---------------------------------------------------------------------------
int SelectNearest(const std::vector<Entry> &entries, int line_no)
{
	if (entries.empty()) return -1;
	int selected = 0;
	for (std::size_t i = 0; i < entries.size(); ++i) {
		if (entries[i].line_no <= line_no) selected = static_cast<int>(i);
		else break;
	}
	return selected;
}

}  // namespace function_list
