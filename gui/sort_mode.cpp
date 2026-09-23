/**
 * @file gui/sort_mode.cpp
 * @brief gui/sort_mode.h の実装
 */
#include "gui/sort_mode.h"

#include <algorithm>

#include "usr_str.h"

namespace sort_mode {

namespace {

bool token_is(const UnicodeString &token, const _TCHAR *value)
{
	return SameText(token, value);
}

int mode_from_char(wchar_t ch)
{
	// src/Global.cpp:66 の SortIdStr = "FEDSAU"
	switch (ch) {
	case 'F': return 0;
	case 'E': return 1;
	case 'D': return 2;
	case 'S': return 3;
	case 'A': return 4;
	case 'U': return 5;
	default: return -1;
	}
}

int directory_from_char(wchar_t ch)
{
	// MainFrm.cpp:26295 の "NFDSAXI"
	switch (ch) {
	case 'N': return 0;
	case 'F': return 1;
	case 'D': return 2;
	case 'S': return 3;
	case 'A': return 4;
	case 'X': return 5;
	case 'I': return 6;
	default: return -1;
	}
}

}  // namespace

Mode FromIndex(int index)
{
	if (index < 0) return Mode::Name;
	if (index > static_cast<int>(Mode::None)) return Mode::None;
	return static_cast<Mode>(index);
}

DirectoryMode DirectoryFromIndex(int index)
{
	if (index < 0) return DirectoryMode::SameAsFile;
	if (index > static_cast<int>(DirectoryMode::Icon)) return DirectoryMode::Icon;
	return static_cast<DirectoryMode>(index);
}

int ToIndex(Mode mode)
{
	return static_cast<int>(mode);
}

int ToIndex(DirectoryMode mode)
{
	return static_cast<int>(mode);
}

bool IsSubModeEnabled(int primary, int secondary)
{
	primary = std::clamp(primary, 0, 5);
	secondary = std::clamp(secondary, 0, 5);
	// VCL: idx==0 なら i==5 だけを有効、それ以外なら同じ番号だけ無効
	if (primary == 0) return secondary == 5;
	return secondary != primary;
}

int NormalizeSubMode(int primary, int secondary)
{
	primary = std::clamp(primary, 0, 5);
	secondary = std::clamp(secondary, 0, 5);
	if (!IsSubModeEnabled(primary, secondary)) return 5;
	return secondary;
}

Options Normalize(Options options)
{
	options.mode = FromIndex(ToIndex(options.mode));
	options.dir_mode = DirectoryFromIndex(ToIndex(options.dir_mode));
	for (std::size_t i = 0; i < options.secondary.size(); ++i) {
		const int primary = static_cast<int>(i);
		options.secondary[i] = FromIndex(NormalizeSubMode(primary,
		                                                        ToIndex(options.secondary[i])));
	}
	options.extension_list = options.extension_list.Trim();
	return options;
}

ParamResult ApplyParam(const Options &current, const UnicodeString &param)
{
	ParamResult result;
	result.options = Normalize(current);
	if (param.Trim().IsEmpty()) return result;

	const TStringDynArray tokens = split_strings_semicolon(param, true);
	for (int i = 0; i < tokens.Length; ++i) {
		const UnicodeString token = tokens[i].Trim();
		if (token.IsEmpty()) continue;

		if (token_is(token, _T("IV"))) {
			result.recognized = true;
			switch (result.options.mode) {
			case Mode::Date: result.options.descending_old = !result.options.descending_old; break;
			case Mode::Size: result.options.descending_small = !result.options.descending_small; break;
			case Mode::Attribute:
				result.options.descending_attribute = !result.options.descending_attribute;
				break;
			default: result.options.descending_name = !result.options.descending_name; break;
			}
			result.changed = true;
			continue;
		}

		if (token_is(token, _T("IA"))) {
			result.recognized = true;
			result.options.descending_name = !result.options.descending_name;
			result.options.descending_old = !result.options.descending_old;
			result.options.descending_small = !result.options.descending_small;
			result.options.descending_attribute = !result.options.descending_attribute;
			result.changed = true;
			continue;
		}

		if (token_is(token, _T("XNX")) || token_is(token, _T("XNI"))) {
			result.recognized = true;
			const DirectoryMode target = token_is(token, _T("XNI"))
			                              ? DirectoryMode::Icon : DirectoryMode::Mixed;
			if (result.options.dir_mode == DirectoryMode::SameAsFile) {
				result.options.dir_mode = target;
			}
			else {
				result.options.dir_mode = DirectoryMode::SameAsFile;
			}
			result.changed = true;
			continue;
		}

		// 1文字の L は結果リストの場所順。FilePane には場所カラムがない。
		if (token_is(token, _T("L"))) {
			result.recognized = true;
			result.path_sort = true;
			continue;
		}

		if (token.Length() == 2 && StartsStr(_T("X"), token)) {
			const int dir = directory_from_char(token[2]);
			if (dir >= 0) {
				result.recognized = true;
				result.options.dir_mode = DirectoryFromIndex(dir);
				result.changed = true;
			}
			continue;
		}

		if (token.Length() == 2 && !StartsStr(_T("X"), token)) {
			const int first = mode_from_char(token[1]);
			const int second = mode_from_char(token[2]);
			if (first >= 0 && second >= 0) {
				result.recognized = true;
				const int old = ToIndex(result.options.mode);
				result.options.mode = FromIndex(old == first ? second : first);
				result.changed = true;
			}
			continue;
		}

		if (token.Length() == 1) {
			const int mode = mode_from_char(token[1]);
			if (mode >= 0) {
				result.recognized = true;
				result.options.mode = FromIndex(mode);
				result.changed = true;
			}
		}
	}

	result.options = Normalize(result.options);
	return result;
}

SortKey KeyForMode(Mode mode)
{
	switch (mode) {
	case Mode::Extension: return SortKey::Ext;
	case Mode::Date: return SortKey::Date;
	case Mode::Size: return SortKey::Size;
	case Mode::Attribute: return SortKey::Attr;
	case Mode::Name:
	case Mode::None:
	default: return SortKey::Name;
	}
}

bool DescendingForMode(const Options &options, Mode mode)
{
	switch (mode) {
	case Mode::Date: return options.descending_old;
	case Mode::Size: return options.descending_small;
	case Mode::Attribute: return options.descending_attribute;
	case Mode::Name:
	case Mode::Extension:
	case Mode::None:
	default: return options.descending_name;
	}
}

PaneSettings ToPaneSettings(const Options &options)
{
	const Options fixed = Normalize(options);
	PaneSettings result;
	result.key = KeyForMode(fixed.mode);
	result.descending = fixed.mode != Mode::None && DescendingForMode(fixed, fixed.mode);
	result.dirs_first = fixed.dir_mode != DirectoryMode::Mixed &&
	                    fixed.dir_mode != DirectoryMode::Icon;
	return result;
}

}  // namespace sort_mode
