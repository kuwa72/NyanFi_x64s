/**
 * @file gui/tag.cpp
 * @brief gui/tag.h の実装
 */
#include "gui/tag.h"

#include <algorithm>
#include <memory>

#include "UIniFile.h"
#include "usr_file_ex.h"

namespace tag {

namespace {

UnicodeString normalized_param(const UnicodeString &param)
{
	UnicodeString out = param;
	out = ReplaceStr(out, _T("|"), _T(";"));
	return Trim(out);
}

}  // namespace

//---------------------------------------------------------------------------
std::vector<UnicodeString> SplitTags(const UnicodeString &text)
{
	std::vector<UnicodeString> out;
	UnicodeString rest = text;
	while (!rest.IsEmpty()) {
		const UnicodeString part = Trim(split_tkn(rest, _T(";")));
		if (!part.IsEmpty()) out.push_back(part);
	}
	return out;
}

//---------------------------------------------------------------------------
UnicodeString JoinTags(const std::vector<UnicodeString> &tags, bool trailing_semicolon)
{
	UnicodeString out;
	for (const UnicodeString &raw : tags) {
		const UnicodeString tag = Trim(raw);
		if (tag.IsEmpty()) continue;

		bool exists = false;
		for (const UnicodeString &added : SplitTags(out)) {
			if (SameText(added, tag)) {
				exists = true;
				break;
			}
		}
		if (exists) continue;
		if (!out.IsEmpty()) out += _T(";");
		out += tag;
	}
	if (trailing_semicolon && !out.IsEmpty()) out += _T(";");
	return out;
}

//---------------------------------------------------------------------------
InputPlan ResolveInput(Mode mode, const UnicodeString &param, const UnicodeString &initial_tags)
{
	InputPlan plan;
	const UnicodeString value = normalized_param(param);
	plan.and_match = !ContainsStr(param, _T("|"));

	if (value.IsEmpty() || SameText(value, _T(";"))) {
		plan.show_dialog = true;
		plan.tags = SameText(value, _T(";")) ? initial_tags : EmptyStr;
		return plan;
	}

	plan.show_dialog = false;
	plan.tags = value;
	if ((mode == Mode::Find || mode == Mode::Select) && SameText(value, _T("*"))) {
		plan.match_all = true;
	}
	return plan;
}

//---------------------------------------------------------------------------
UnicodeString Title(Mode mode, bool and_match)
{
	switch (mode) {
	case Mode::Add:   return _T("タグの追加");
	case Mode::Set:   return _T("タグの設定");
	case Mode::Find:  return and_match? _T("タグ検索 (AND)") : _T("タグ検索 (OR)");
	case Mode::Select:return _T("タグ選択");
	case Mode::FolderIcon: return _T("フォルダアイコン検索");
	}
	return _T("タグ");
}

//---------------------------------------------------------------------------
UnicodeString BuildSearchCommand(const UnicodeString &tags, bool and_match, bool opposite)
{
	UnicodeString keyword = tags;
	if (!and_match) keyword = ReplaceStr(keyword, _T(";"), _T("|"));

	UnicodeString out = _T(";タグ検索 [") + keyword + _T("]");
	out += _T("\r\n");
	if (opposite) out += _T("ToOpposite\r\n");
	out += _T("FindTag_") + keyword;
	return out;
}

//---------------------------------------------------------------------------
std::vector<UnicodeString> LoadFolderIcons(const UnicodeString &ini_path)
{
	std::vector<UnicodeString> out;
	if (ini_path.IsEmpty()) return out;

	UsrIniFile ini(ini_path);
	std::unique_ptr<TStringList> list(new TStringList());
	ini.ReadSection(_T("FolderIcon"), list.get());
	for (int i = 0; i < list->Count; ++i) {
		const UnicodeString icon = to_absolute_name(list->ValueFromIndex[i]);
		if (!icon.IsEmpty()) out.push_back(icon);
	}
	std::sort(out.begin(), out.end(),
	          [](const UnicodeString &a, const UnicodeString &b) { return CompareText(a, b) < 0; });
	out.erase(std::unique(out.begin(), out.end(),
	                      [](const UnicodeString &a, const UnicodeString &b) { return SameText(a, b); }),
	          out.end());
	return out;
}

}  // namespace tag
