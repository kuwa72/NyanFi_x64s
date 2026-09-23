/**
 * @file gui/btn.cpp
 * @brief gui/btn.h の実装
 */
#include "gui/btn.h"

#include <algorithm>

#include "UIniFile.h"

namespace btn {

namespace {
const wchar_t *const kSection = _T("WxGuiToolButtons");
}

//---------------------------------------------------------------------------
Item ParseItem(const UnicodeString &record)
{
	Item item;
	const TStringDynArray fields = get_csv_array(record, 3, true);
	if (fields.Length >= 1) item.caption = fields[0];
	if (fields.Length >= 2) item.command = fields[1];
	if (fields.Length >= 3) item.icon = fields[2];
	return item;
}

//---------------------------------------------------------------------------
UnicodeString FormatItem(const Item &item)
{
	return make_csv_rec_str({item.caption, item.command, item.icon});
}

//---------------------------------------------------------------------------
bool IsSeparator(const Item &item)
{
	return SameText(item.caption, _T("-"));
}

//---------------------------------------------------------------------------
bool CanAdd(const Item &item)
{
	return !item.caption.IsEmpty() || !item.icon.IsEmpty();
}

//---------------------------------------------------------------------------
bool CanChange(const std::vector<Item> &items, int index, const Item &item)
{
	return index >= 0 && index < static_cast<int>(items.size()) && CanAdd(item);
}

//---------------------------------------------------------------------------
std::vector<UnicodeString> CommandChoices(Mode mode,
                                         const std::vector<UnicodeString> &all_commands,
                                         const std::vector<UnicodeString> &aliases)
{
	(void)mode;  // wx 版はモードの説明文を切り替えず、実在名を一括表示する
	std::vector<UnicodeString> out;
	auto append_unique = [&out](const UnicodeString &command) {
		if (command.IsEmpty()) return;
		for (const UnicodeString &old : out) {
			if (SameText(old, command)) return;
		}
		out.push_back(command);
	};
	for (const UnicodeString &command : all_commands) append_unique(command);
	for (const UnicodeString &alias : aliases) {
		if (alias.IsEmpty()) continue;
		append_unique(StartsText(_T("$"), alias) ? alias : _T("$") + alias);
	}
	return out;
}

//---------------------------------------------------------------------------
UnicodeString WithPathParameter(const UnicodeString &command, const UnicodeString &path)
{
	UnicodeString result;
	result.sprintf(_T("%s_\"%s\""), command.c_str(), path.c_str());
	return result;
}

//---------------------------------------------------------------------------
int ResolveIndex(const UnicodeString &param, int count)
{
	const int one_based = param.ToIntDef(0);
	if (one_based < 1 || one_based > count) return -1;
	return one_based - 1;
}

//---------------------------------------------------------------------------
bool Move(std::vector<Item> &items, int index, int delta)
{
	if (index < 0 || index >= static_cast<int>(items.size())) return false;
	const int target = index + (delta > 0 ? 1 : delta < 0 ? -1 : 0);
	if (target < 0 || target >= static_cast<int>(items.size())) return false;
	std::swap(items[static_cast<std::size_t>(index)], items[static_cast<std::size_t>(target)]);
	return true;
}

//---------------------------------------------------------------------------
void Store::LoadFromIni(UsrIniFile &ini)
{
	items_.clear();
	const int count = std::max(0, ini.ReadInteger(kSection, _T("Count"), 0));
	for (int i = 0; i < count; ++i) {
		UnicodeString key;
		key.sprintf(_T("Item%02d"), i + 1);
		const UnicodeString record = ini.ReadString(kSection, key, EmptyStr, false);
		if (record.IsEmpty()) continue;
		items_.push_back(ParseItem(record));
	}
}

//---------------------------------------------------------------------------
void Store::SaveToIni(UsrIniFile &ini) const
{
	ini.EraseSection(kSection);
	ini.WriteInteger(kSection, _T("Count"), static_cast<int>(items_.size()));
	for (std::size_t i = 0; i < items_.size(); ++i) {
		UnicodeString key;
		key.sprintf(_T("Item%02u"), static_cast<unsigned int>(i + 1));
		ini.WriteString(kSection, key, FormatItem(items_[i]));
	}
}

}  // namespace btn
