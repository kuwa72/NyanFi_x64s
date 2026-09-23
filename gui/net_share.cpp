/**
 * @file gui/net_share.cpp
 * @brief gui/net_share.h の実装
 */
#include "gui/net_share.h"

#include <algorithm>

namespace net_share {

namespace {

ValidationResult Failure(const UnicodeString &error)
{
	ValidationResult result;
	result.error = error;
	return result;
}

bool HasInvalidComputerChar(wchar_t ch)
{
	if (ch < 0x20) return true;
	switch (ch) {
	case L'<':
	case L'>':
	case L':':
	case L'"':
	case L'/':
	case L'\\':
	case L'|':
	case L'?':
	case L'*':
	case L' ':
	case L'\t':
		return true;
	default:
		return false;
	}
}

bool HasInvalidPathChar(wchar_t ch)
{
	if (ch < 0x20) return true;
	switch (ch) {
	case L'<':
	case L'>':
	case L':':
	case L'"':
	case L'/':
	case L'|':
	case L'?':
	case L'*':
		return true;
	default:
		return false;
	}
}

UnicodeString StripTrailingSlashes(UnicodeString value)
{
	while (value.Length() >= 2 && value[value.Length()] == L'\\') value.SetLength(value.Length() - 1);
	return value;
}

UnicodeString StripLeadingSlashes(UnicodeString value)
{
	int pos = 1;
	while (pos <= value.Length() && value[pos] == L'\\') ++pos;
	return value.SubString(pos);
}

bool ContainsIgnoreCase(const UnicodeString &text, const UnicodeString &needle_upper)
{
	return !needle_upper.IsEmpty() && text.UpperCase().Pos(needle_upper) > 0;
}

}  // namespace

//---------------------------------------------------------------------------
ValidationResult NormalizeComputer(const UnicodeString &input)
{
	UnicodeString value = input.Trim();
	if (value.IsEmpty()) return Failure(_T("コンピュータ名を入力してください"));

	if (value.Length() >= 2 && value[1] == L'\\' && value[2] == L'\\') {
		value = value.SubString(3);
	}
	else if (value[1] == L'\\' || value[1] == L'/') {
		return Failure(_T("コンピュータ名は host または \\\\host で入力してください"));
	}
	value = StripTrailingSlashes(StripLeadingSlashes(value.Trim()));
	if (value.IsEmpty()) return Failure(_T("コンピュータ名を入力してください"));
	if (value == _T(".") || value == _T(".."))
		return Failure(_T("コンピュータ名が不正です"));
	if (value.Length() > 255)
		return Failure(_T("コンピュータ名が長すぎます"));

	for (int i = 1; i <= value.Length(); ++i) {
		if (HasInvalidComputerChar(value[i]))
			return Failure(_T("コンピュータ名に無効な文字があります"));
	}

	ValidationResult result;
	result.ok = true;
	result.value = _T("\\\\") + value;
	return result;
}

//---------------------------------------------------------------------------
ValidationResult NormalizeUncPath(const UnicodeString &input)
{
	UnicodeString value = input.Trim();
	if (value.Length() < 2 || value[1] != L'\\' || value[2] != L'\\')
		return Failure(_T("UNC形式で入力してください"));

	value = StripTrailingSlashes(value);
	if (value.Length() <= 2)
		return Failure(_T("UNC共有名を指定してください"));

	int component_count = 0;
	UnicodeString component;
	for (int i = 3; i <= value.Length(); ++i) {
		const wchar_t ch = value[i];
		if (ch != L'\\') {
			component += ch;
			continue;
		}

		if (component.IsEmpty())
			return Failure(_T("UNCパスに空の区切りがあります"));
		if (component == _T(".") || component == _T(".."))
			return Failure(_T("UNCパスに相対名があります"));
		for (int j = 1; j <= component.Length(); ++j) {
			const bool invalid = component_count == 0
				? HasInvalidPathChar(component[j]) || component[j] == L' ' || component[j] == L'\t'
				: HasInvalidPathChar(component[j]);
			if (invalid) return Failure(_T("UNCパスに無効な文字があります"));
		}
		component = EmptyStr;
		++component_count;
	}

	if (component.IsEmpty())
		return Failure(_T("UNC共有名を指定してください"));
	if (component == _T(".") || component == _T(".."))
		return Failure(_T("UNCパスに相対名があります"));
	for (int j = 1; j <= component.Length(); ++j) {
		if (HasInvalidPathChar(component[j]))
			return Failure(_T("UNCパスに無効な文字があります"));
	}
	++component_count;
	if (component_count < 2)
		return Failure(_T("UNC共有名を指定してください"));

	ValidationResult result;
	result.ok = true;
	result.value = value;
	return result;
}

//---------------------------------------------------------------------------
ConnectionAction ResolveConnectionAction(ShareListResult list_result,
                                         ConnectResult connect_result)
{
	if (list_result == ShareListResult::Success) return ConnectionAction::UseExisting;
	if (connect_result == ConnectResult::NotAttempted) return ConnectionAction::Connect;
	if (connect_result == ConnectResult::Success) return ConnectionAction::UseExisting;
	if (connect_result == ConnectResult::Cancelled) return ConnectionAction::Cancel;
	return ConnectionAction::ShowError;
}

//---------------------------------------------------------------------------
UnicodeString FormatComputerCaption(const UnicodeString &computer)
{
	const ValidationResult normalized = NormalizeComputer(computer);
	if (!normalized.ok) return EmptyStr;
	UnicodeString value = normalized.value;
	for (int i = 1; i <= value.Length(); ++i) {
		if (value[i] == L'\\') value[i] = L'/';
	}
	return value;
}

//---------------------------------------------------------------------------
UnicodeString MakeUncSharePath(const UnicodeString &computer, const UnicodeString &share)
{
	const ValidationResult host = NormalizeComputer(computer);
	if (!host.ok) return EmptyStr;

	UnicodeString share_name = StripTrailingSlashes(
		StripLeadingSlashes(share.Trim()).Trim()).Trim();
	if (share_name.IsEmpty()) return EmptyStr;
	for (int i = 1; i <= share_name.Length(); ++i) {
		if (HasInvalidPathChar(share_name[i]) || share_name[i] == L'\\') return EmptyStr;
	}
	if (share_name == _T(".") || share_name == _T("..")) return EmptyStr;

	const ValidationResult path = NormalizeUncPath(host.value + _T("\\") + share_name);
	return path.ok ? path.value : EmptyStr;
}

//---------------------------------------------------------------------------
UnicodeString FormatShareRow(const UnicodeString &computer, const ShareItem &item)
{
	UnicodeString row = MakeUncSharePath(computer, item.name);
	if (row.IsEmpty()) return EmptyStr;
	if (!item.local_path.IsEmpty()) row += _T("  ") + item.local_path;
	if (!item.remark.IsEmpty()) row += _T("  (") + item.remark + _T(")");
	return row;
}

//---------------------------------------------------------------------------
std::vector<ShareItem> SortFilterShares(const std::vector<ShareItem> &items,
                                        const UnicodeString &filter)
{
	const UnicodeString filter_upper = filter.Trim().UpperCase();
	std::vector<ShareItem> result;
	result.reserve(items.size());
	for (const ShareItem &item : items) {
		const UnicodeString name = item.name.Trim();
		// VCL ShareDlg.cpp:183-186 と同じく管理共有 (末尾 $) は出さない。
		if (name.IsEmpty() || EndsStr(_T("$"), name)) continue;
		if (!filter_upper.IsEmpty() &&
		    !ContainsIgnoreCase(name, filter_upper) &&
		    !ContainsIgnoreCase(item.local_path, filter_upper) &&
		    !ContainsIgnoreCase(item.remark, filter_upper)) {
			continue;
		}
		ShareItem copy = item;
		copy.name = name;
		result.push_back(copy);
	}

	std::sort(result.begin(), result.end(), [](const ShareItem &a, const ShareItem &b) {
		return CompareText(a.name, b.name) < 0;
	});
	return result;
}

}  // namespace net_share
