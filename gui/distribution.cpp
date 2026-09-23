/**
 * @file gui/distribution.cpp
 * @brief gui/distribution.h の実装
 */
#include "gui/distribution.h"

#include <algorithm>

namespace distribution {

//---------------------------------------------------------------------------
Rule ParseRule(const UnicodeString &record)
{
	Rule rule;
	const TStringDynArray fields = get_csv_array(record, 4, true);
	if (fields.Length < 4) return rule;
	rule.title = fields[0];
	rule.enabled = SameText(fields[1], _T("1")) || SameText(fields[1], _T("true"));
	rule.mask = fields[2];
	rule.destination = fields[3];
	return rule;
}

//---------------------------------------------------------------------------
UnicodeString MakeRuleRecord(const Rule &rule)
{
	return make_csv_rec_str({rule.title, rule.enabled? _T("1") : _T("0"),
	                         rule.mask, rule.destination});
}

//---------------------------------------------------------------------------
bool IsRegexMask(const UnicodeString &mask)
{
	return is_regex_slash(mask);
}

//---------------------------------------------------------------------------
bool IsValidMask(const UnicodeString &mask, UnicodeString &error_out)
{
	if (mask.IsEmpty()) {
		error_out = _T("マスクを入力してください");
		return false;
	}
	if (IsRegexMask(mask)) {
		const UnicodeString body = exclude_top_end(mask);
		if (!chk_RegExPtn(body)) {
			error_out = _T("正規表現が不正です");
			return false;
		}
	}
	error_out = EmptyStr;
	return true;
}

//---------------------------------------------------------------------------
bool MatchMask(const UnicodeString &mask, const UnicodeString &name)
{
	if (mask.IsEmpty()) return false;
	if (IsRegexMask(mask)) {
		const UnicodeString body = exclude_top_end(mask);
		if (!chk_RegExPtn(body)) return false;
		TRegExOptions opt;
		opt << roIgnoreCase;
		return TRegEx::Match(name, body, opt).Success;
	}
	return str_match(mask, name);
}

//---------------------------------------------------------------------------
UnicodeString FormatDestination(const UnicodeString &format, const UnicodeString &source,
                                const UnicodeString &opposite_path)
{
	const UnicodeString base = get_base_name(ExcludeTrailingPathDelimiter(source));
	const UnicodeString ext = get_extension(source);
	UnicodeString ext_no_dot = ext;
	remove_top_s(ext_no_dot, _T("."));

	UnicodeString name;
	int i = 1;
	while (i <= format.Length()) {
		if (format[i] != _T('\\')) {
			name += format[i];
			++i;
			continue;
		}

		UnicodeString token = format.SubString(i);
		if (StartsStr(_T("\\L("), token)) {
			const int n = get_in_paren(token).ToIntDef(0);
			if (n > 0) name += base.SubString(1, n);
			i += token.Length();
		}
		else if (StartsStr(_T("\\S("), token)) {
			const int m = get_tkn_m(token, _T("("), _T(",")).ToIntDef(0);
			const int n = get_tkn_m(token, _T(","), _T(")")).ToIntDef(0);
			if (m > 0 && n > 0) name += base.SubString(m, n);
			i += token.Length();
		}
		else if (StartsStr(_T("\\R("), token)) {
			const int n = get_in_paren(token).ToIntDef(0);
			if (n > 0) name += base.SubString(std::max(1, base.Length() - n + 1), n);
			i += token.Length();
		}
		else if (StartsStr(_T("\\A"), token)) {
			name += base;
			i += 2;
		}
		else if (StartsStr(_T("\\E"), token)) {
			name += ext_no_dot;
			i += 2;
		}
		else if (StartsStr(_T("\\C"), token)) {
			name += get_dir_name(ExtractFilePath(ExcludeTrailingPathDelimiter(source)));
			i += 2;
		}
		else {
			// \DT/\TS/\XT/\Z などは既存の実処理に依存するため、明示的に未実装。
			name += token.SubString(1, 2);
			i += 2;
		}
	}

	if (name.IsEmpty()) return opposite_path;
	if (StartsStr(_T("\\"), name) || ContainsStr(name, _T(":"))) return name;
	return IncludeTrailingPathDelimiter(opposite_path) + name;
}

//---------------------------------------------------------------------------
file_ops::ConflictPolicy ConflictPolicyFor(CopyMode mode)
{
	switch (mode) {
	case CopyMode::Overwrite:  return file_ops::ConflictPolicy::Overwrite;
	case CopyMode::Newest:     return file_ops::ConflictPolicy::NewestWins;
	case CopyMode::AutoRename: return file_ops::ConflictPolicy::AutoRename;
	case CopyMode::Skip:
	default:                   return file_ops::ConflictPolicy::SkipExisting;
	}
}

//---------------------------------------------------------------------------
Preview BuildPreview(const std::vector<Input> &items, const std::vector<Rule> &rules,
                     const Options &options, const DirectoryExists &directory_exists)
{
	Preview result;
	result.items.reserve(items.size());

	for (const Input &item : items) {
		const UnicodeString name = get_dir_name(item.path);
		const Rule *matched_rule = nullptr;
		UnicodeString destination;
		for (const Rule &rule : rules) {
			if (!rule.enabled || !MatchMask(rule.mask, name)) continue;
			matched_rule = &rule;
			destination = FormatDestination(rule.destination, item.path, options.opposite_path);
			break;
		}
		if (matched_rule == nullptr) continue;

		PreviewItem row;
		row.source = item.path;
		row.destination = destination;
		row.is_dir = item.is_dir;
		const UnicodeString source_dir = ExtractFilePath(ExcludeTrailingPathDelimiter(item.path));
		const UnicodeString dest_dir = ExtractFilePath(ExcludeTrailingPathDelimiter(destination));
		row.skipped = SameText(source_dir, dest_dir);
		if (!row.skipped && !options.create_directories && directory_exists &&
		    !directory_exists(IncludeTrailingPathDelimiter(destination))) {
			row.skipped = true;
		}

		result.items.push_back(row);
		++result.matched;
		if (row.is_dir) ++result.directories; else ++result.files;
		if (row.skipped) ++result.skipped;
	}
	return result;
}

//---------------------------------------------------------------------------
bool CanAddRule(bool registration_enabled, const UnicodeString &title,
                const UnicodeString &mask, const UnicodeString &destination,
                int rule_count, UnicodeString &error_out)
{
	(void)destination;
	if (!registration_enabled) {
		error_out = _T("この画面では登録を変更できません");
		return false;
	}
	if (title.IsEmpty()) {
		error_out = _T("タイトルを入力してください");
		return false;
	}
	if (mask.IsEmpty()) {
		error_out = _T("マスクを入力してください");
		return false;
	}
	if (rule_count >= 200) {
		error_out = _T("登録は200件までです");
		return false;
	}
	return IsValidMask(mask, error_out);
}

//---------------------------------------------------------------------------
bool HasDuplicateRule(const std::vector<Rule> &rules, const UnicodeString &mask,
                      const UnicodeString &destination)
{
	for (const Rule &rule : rules) {
		if (SameText(rule.mask, mask) && SameText(rule.destination, destination)) return true;
	}
	return false;
}

//---------------------------------------------------------------------------
std::vector<bool> GroupChecked(const std::vector<Rule> &rules, int selected,
                               bool checked)
{
	std::vector<bool> values;
	values.reserve(rules.size());
	UnicodeString title;
	if (selected >= 0 && selected < static_cast<int>(rules.size())) {
		title = rules[static_cast<std::size_t>(selected)].title;
	}
	for (const Rule &rule : rules) values.push_back(!title.IsEmpty() && SameText(rule.title, title)? checked : false);
	return values;
}

}  // namespace distribution
