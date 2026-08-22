/**
 * @file gui/history.cpp
 * @brief 履歴リストの実装 (設計は gui/history.h)
 */
#include "gui/history.h"

#include <algorithm>
#include <memory>

#include "usr_file_ex.h"
#include "usr_str.h"

namespace history {

//---------------------------------------------------------------------------
HistoryList::HistoryList(int max_items) : max_items_(max_items <= 0 ? 1 : max_items)
{
}

//---------------------------------------------------------------------------
void HistoryList::Add(const UnicodeString &entry)
{
	if (entry.IsEmpty()) return;

	// 既にあれば取り除く (VCL の add_TextEditHistory と同じく SameText、
	// src/Global.cpp:11101-11102)
	Remove(entry);
	entries_.insert(entries_.begin(), entry);

	// 上限を超えたら一番古い項目 (末尾) から切り捨てる。VCL の
	// EditHistory/ViewHistory は保存時にしか切り詰めないが、ここでは
	// Add のたびに切り詰める (最終的に ini に残る内容は同じになる)
	if (static_cast<int>(entries_.size()) > max_items_) {
		entries_.resize(static_cast<std::size_t>(max_items_));
	}
}

//---------------------------------------------------------------------------
void HistoryList::Remove(const UnicodeString &entry)
{
	if (entry.IsEmpty()) return;

	std::size_t i = 0;
	while (i < entries_.size()) {
		if (SameText(entries_[i], entry)) entries_.erase(entries_.begin() + static_cast<long>(i));
		else ++i;
	}
}

//---------------------------------------------------------------------------
void HistoryList::Clear()
{
	entries_.clear();
}

//---------------------------------------------------------------------------
int HistoryList::DropMissingFiles()
{
	const std::size_t before = entries_.size();
	entries_.erase(
		std::remove_if(entries_.begin(), entries_.end(),
		                [](const UnicodeString &s) { return !file_exists(s) && !dir_exists(s); }),
		entries_.end());
	return static_cast<int>(before - entries_.size());
}

//---------------------------------------------------------------------------
void HistoryList::AssignLoaded(const std::vector<UnicodeString> &entries)
{
	entries_ = entries;
	if (static_cast<int>(entries_.size()) > max_items_) {
		entries_.resize(static_cast<std::size_t>(max_items_));
	}
}

//---------------------------------------------------------------------------
UnicodeString IniKeyOf(Kind kind)
{
	switch (kind) {
	case Kind::Edit:    return _T("TextEditHistory");   // VCL の ini キーと同名 (src/Global.cpp:2006)
	case Kind::View:    return _T("TextViewHistory");   // 同上 (src/Global.cpp:2005)
	case Kind::Recent:  return _T("RecentList");         // VCL に対応する ini は無い。Phase 2 の新規名
	case Kind::Command: return _T("CmdHistory");         // 同上 (VCL は ini 保存自体をしない)
	}
	return EmptyStr;
}

//---------------------------------------------------------------------------
void LoadFromIni(UsrIniFile &ini, Kind kind, HistoryList &out)
{
	std::unique_ptr<TStringList> lst(new TStringList());
	// del_quot=true。VCL の L:TextEditHistory=50,true / L:TextViewHistory=50,true
	// と同じ (src/Global.cpp:2005-2006、意味は src/UIniFile.h:106 の引数名の通り)
	ini.LoadListItems(IniKeyOf(kind), lst.get(), out.MaxItems(), /*del_quot=*/true);

	std::vector<UnicodeString> entries;
	entries.reserve(static_cast<std::size_t>(lst->Count));
	for (int i = 0; i < lst->Count; ++i) entries.push_back(lst->Strings[i]);

	out.AssignLoaded(entries);
}

//---------------------------------------------------------------------------
void SaveToIni(UsrIniFile &ini, Kind kind, const HistoryList &list)
{
	std::unique_ptr<TStringList> lst(new TStringList());
	const std::vector<UnicodeString> &entries = list.Entries();
	for (std::size_t i = 0; i < entries.size(); ++i) lst->Add(entries[i]);

	ini.SaveListItems(IniKeyOf(kind), lst.get(), list.MaxItems());
}

}  // namespace history
