/**
 * @file gui/tabs.cpp
 * @brief gui/tabs.h の実装 (wx 非依存)
 */
#include "gui/tabs.h"

#include "UIniFile.h"

namespace {

/// このクラス専用のセクション名 (gui/settings.cpp の kSection と同じ考え方。
/// VCL 版の TabList/タブグループ書式とは互換性の無い新規設計)
const UnicodeString kSection = _T("WxGuiTabs");

int sort_key_to_int(SortKey key)
{
	return static_cast<int>(key);
}

SortKey int_to_sort_key(int v)
{
	switch (v) {
	case static_cast<int>(SortKey::Ext):  return SortKey::Ext;
	case static_cast<int>(SortKey::Date): return SortKey::Date;
	case static_cast<int>(SortKey::Size): return SortKey::Size;
	case static_cast<int>(SortKey::Attr): return SortKey::Attr;
	default: return SortKey::Name;
	}
}

}  // namespace

//---------------------------------------------------------------------------
TabManager::TabManager()
{
	tabs_.emplace_back();  // 最初の1本 (空の状態。MutableCurrent() で埋める)
}

//---------------------------------------------------------------------------
int TabManager::AddTab(const TabState &state)
{
	tabs_.push_back(state);
	current_ = static_cast<int>(tabs_.size()) - 1;
	return current_;
}

//---------------------------------------------------------------------------
bool TabManager::CloseTabAt(int index)
{
	if (tabs_.size() <= 1) return false;  // 最後のタブは閉じない (要件)
	if (index < 0 || index >= static_cast<int>(tabs_.size())) return false;

	tabs_.erase(tabs_.begin() + index);

	if (index < current_) {
		--current_;  // 現在のタブより前を閉じたので、添字を1つ詰める
	}
	else if (index == current_) {
		// VCL 版 (DelTabActionExecute → UpdateTabBar(idx)) と同じく、
		// 削除後は同じ添字 (無ければ末尾) のタブへ移る
		if (current_ >= static_cast<int>(tabs_.size())) current_ = static_cast<int>(tabs_.size()) - 1;
	}
	// index > current_ (現在のタブより後ろを閉じた) 場合は current_ はそのまま

	return true;
}

//---------------------------------------------------------------------------
void TabManager::NextTab()
{
	if (tabs_.size() <= 1) return;
	current_ = (current_ + 1 < static_cast<int>(tabs_.size())) ? current_ + 1 : 0;
}

//---------------------------------------------------------------------------
void TabManager::PrevTab()
{
	if (tabs_.size() <= 1) return;
	current_ = (current_ > 0) ? current_ - 1 : static_cast<int>(tabs_.size()) - 1;
}

//---------------------------------------------------------------------------
bool TabManager::SelectAt(int index)
{
	if (index < 0 || index >= static_cast<int>(tabs_.size())) return false;
	current_ = index;
	return true;
}

//---------------------------------------------------------------------------
UnicodeString TabManager::CaptionAt(int index) const
{
	if (index < 0 || index >= static_cast<int>(tabs_.size())) return EmptyStr;

	// 左ペインのディレクトリ末尾要素名を使う (VCL 版 SetTabStr の
	// `def_if_empty(itm_buf[2], get_DirNwlName(itm_buf[0]))` の簡易版。
	// Phase 2 骨格はカスタムキャプション (itm_buf[2] 相当) を持たないため
	// 常にディレクトリ名から作る)
	const UnicodeString dir = tabs_[static_cast<std::size_t>(index)].panes[0].directory;
	if (dir.IsEmpty()) return _T("(無題)");

	const UnicodeString leaf = ExtractFileName(ExcludeTrailingPathDelimiter(dir));
	return leaf.IsEmpty() ? dir : leaf;  // ドライブのルートは leaf が空になるためパスそのものを使う
}

//---------------------------------------------------------------------------
std::vector<UnicodeString> TabManager::Captions() const
{
	std::vector<UnicodeString> result;
	result.reserve(tabs_.size());
	for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) result.push_back(CaptionAt(i));
	return result;
}

//---------------------------------------------------------------------------
void TabManager::SaveToIni(UsrIniFile &ini) const
{
	ini.WriteInteger(kSection, _T("Count"), static_cast<int>(tabs_.size()));
	ini.WriteInteger(kSection, _T("Current"), current_);

	for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
		const TabState &tab = tabs_[static_cast<std::size_t>(i)];
		UnicodeString prefix;
		prefix.sprintf(_T("Tab%02d_"), i);

		for (int p = 0; p < 2; ++p) {
			const PaneTabState &pane = tab.panes[p];
			UnicodeString key;

			ini.WriteString(kSection, key.sprintf(_T("%sDir%d"), prefix.c_str(), p), pane.directory);
			ini.WriteInteger(kSection, key.sprintf(_T("%sSortKey%d"), prefix.c_str(), p),
			                  sort_key_to_int(pane.sort_key));
			ini.WriteBool(kSection, key.sprintf(_T("%sSortDesc%d"), prefix.c_str(), p), pane.sort_descending);
			ini.WriteBool(kSection, key.sprintf(_T("%sDirsFirst%d"), prefix.c_str(), p), pane.dirs_first);
		}
	}
}

//---------------------------------------------------------------------------
void TabManager::LoadFromIni(UsrIniFile &ini)
{
	const int count = ini.ReadInteger(kSection, _T("Count"), 0);
	if (count <= 0) return;  // セクションが無い/空なら既定の1タブのまま呼び出し元に任せる

	std::vector<TabState> loaded;
	loaded.reserve(static_cast<std::size_t>(count));

	for (int i = 0; i < count; ++i) {
		TabState tab;
		UnicodeString prefix;
		prefix.sprintf(_T("Tab%02d_"), i);

		for (int p = 0; p < 2; ++p) {
			PaneTabState &pane = tab.panes[p];
			UnicodeString key;

			pane.directory = ini.ReadString(kSection, key.sprintf(_T("%sDir%d"), prefix.c_str(), p), EmptyStr);
			pane.sort_key = int_to_sort_key(
				ini.ReadInteger(kSection, key.sprintf(_T("%sSortKey%d"), prefix.c_str(), p), 0));
			pane.sort_descending =
				ini.ReadBool(kSection, key.sprintf(_T("%sSortDesc%d"), prefix.c_str(), p), false);
			pane.dirs_first =
				ini.ReadBool(kSection, key.sprintf(_T("%sDirsFirst%d"), prefix.c_str(), p), true);
		}
		loaded.push_back(tab);
	}

	if (loaded.empty()) return;

	tabs_ = std::move(loaded);
	const int saved_current = ini.ReadInteger(kSection, _T("Current"), 0);
	current_ = (saved_current >= 0 && saved_current < static_cast<int>(tabs_.size())) ? saved_current : 0;
}

//---------------------------------------------------------------------------
bool TabManager::MoveCurrentTab(int direction)
{
	if (Count() < 2 || direction == 0) return false;

	const int from = current_;
	// VCL は末尾から先頭へ回る (MainFrm.cpp:37429-37430)
	const int to = (direction > 0)? ((from < Count() - 1)? from + 1 : 0)
	                              : ((from > 0)? from - 1 : Count() - 1);
	if (from == to) return false;

	const TabState moved = tabs_[static_cast<std::size_t>(from)];
	tabs_.erase(tabs_.begin() + from);
	tabs_.insert(tabs_.begin() + to, moved);
	current_ = to;  // 移動後もそのタブが選ばれたまま
	return true;
}

//---------------------------------------------------------------------------
int TabManager::CloseOtherTabs()
{
	if (Count() < 2) return 0;

	const TabState keep = Current();
	const int closed = Count() - 1;
	tabs_.clear();
	tabs_.push_back(keep);
	current_ = 0;
	return closed;
}

//---------------------------------------------------------------------------
int TabManager::GoHome(bool all)
{
	int n = 0;
	for (int i = 0; i < Count(); i++) {
		if (!all && i != current_) continue;

		TabState &t = tabs_[static_cast<std::size_t>(i)];
		bool changed = false;
		for (int k = 0; k < 2; k++) {
			// VCL は home が**空でないときだけ**書き戻す (MainFrm.cpp:37597-37598)
			if (t.panes[k].home.IsEmpty()) continue;
			if (t.panes[k].directory != t.panes[k].home) changed = true;
			t.panes[k].directory = t.panes[k].home;
		}
		if (changed) n++;
	}
	return n;
}

//---------------------------------------------------------------------------
bool TabManager::SelectByParam(const UnicodeString &param)
{
	if (param.IsEmpty()) return false;

	// VCL はまず数値として解釈する (MainFrm.cpp:37464)。番号は 1 起点
	const int num = param.ToIntDef(-1);
	if (num != -1) return SelectAt(num - 1);

	// 数値でなければキャプションの完全一致 (大文字小文字を区別しない)
	for (int i = 0; i < Count(); i++) {
		if (SameText(CaptionAt(i), param)) return SelectAt(i);
	}
	return false;
}
