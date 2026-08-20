/**
 * @file gui/file_pane.cpp
 * @brief ファイル一覧ペインの実装
 */
#include "gui/file_pane.h"

#include <wx/dcbuffer.h>
#include <wx/settings.h>

#include <algorithm>
#include <memory>

#include "usr_file_ex.h"
#include "usr_file_inf.h"
#include "usr_str.h"

namespace {

/// wxString への変換 (UnicodeString は UTF-16、wxString も MSW では UTF-16)
inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

/// 並べ替えキーの表示名 (ソートダイアログ・列見出し共通)
UnicodeString sort_key_name(SortKey key)
{
	switch (key) {
	case SortKey::Name: return _T("名前");
	case SortKey::Ext:  return _T("拡張子");
	case SortKey::Date: return _T("日時");
	case SortKey::Size: return _T("サイズ");
	case SortKey::Attr: return _T("属性");
	}
	return UnicodeString();
}

}  // namespace

//---------------------------------------------------------------------------
FilePane::FilePane(wxWindow *parent, wxWindowID id)
	: wxWindow(parent, id, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
{
	SetBackgroundStyle(wxBG_STYLE_PAINT);

	// 等幅フォント。ファイル名の桁揃えが崩れると2画面ファイラとして使えない
	font_ = wxFont(wxFontInfo(10).FaceName("Consolas").Family(wxFONTFAMILY_TELETYPE));
	if (!font_.IsOk()) font_ = wxFont(wxFontInfo(10).Family(wxFONTFAMILY_TELETYPE));

	Bind(wxEVT_PAINT, &FilePane::OnPaint, this);
	Bind(wxEVT_SIZE, &FilePane::OnSize, this);
	Bind(wxEVT_LEFT_DOWN, &FilePane::OnLeftDown, this);
	Bind(wxEVT_LEFT_DCLICK, &FilePane::OnLeftDClick, this);
	Bind(wxEVT_MOUSEWHEEL, &FilePane::OnMouseWheel, this);
	Bind(wxEVT_SET_FOCUS, &FilePane::OnSetFocus, this);

	UpdateMetrics();
}

//---------------------------------------------------------------------------
void FilePane::UpdateMetrics()
{
	wxClientDC dc(this);
	dc.SetFont(font_);
	const wxSize ext = dc.GetTextExtent("M");
	char_width_ = std::max(1, ext.x);
	row_height_ = std::max(1, ext.y + 2);
}

//---------------------------------------------------------------------------
bool FilePane::SetPath(const UnicodeString &path)
{
	const UnicodeString newpath = IncludeTrailingPathDelimiter(path);
	if (!dir_exists(newpath)) return false;

	path_ = newpath;
	cursor_ = 0;
	top_ = 0;
	Collect();
	Refresh();
	return true;
}

//---------------------------------------------------------------------------
void FilePane::Reload()
{
	// カーソル位置は名前で復元する (削除やリネームで行番号がずれるため)
	UnicodeString cur;
	if (const FileItem *itm = GetCurrentItem()) cur = itm->name;

	Collect();

	RestoreCursorByName(cur);
	MoveCursorTo(cursor_);
	Refresh();
}

//---------------------------------------------------------------------------
void FilePane::Collect()
{
	all_items_.clear();

	if (!is_root_dir(path_)) {
		FileItem up;
		up.name = "..";
		up.is_dir = true;
		up.is_parent = true;
		up.size = -1;
		all_items_.push_back(up);
	}

	TSearchRec sr;
	if (FindFirst(path_ + "*", faAnyFile, sr) == 0) {
		do {
			if (SameStr(sr.Name, ".") || SameStr(sr.Name, "..")) continue;

			FileItem itm;
			itm.name = sr.Name;
			itm.attr = sr.Attr;
			itm.is_dir = (sr.Attr & faDirectory) != 0;
			itm.size = itm.is_dir ? -1 : sr.Size;
			itm.stamp = sr.TimeStamp;
			all_items_.push_back(itm);
		} while (FindNext(sr) == 0);
		FindClose(sr);
	}

	ApplyFilterAndSort();

	if (cursor_ >= GetItemCount()) cursor_ = GetItemCount() - 1;
	if (cursor_ < 0) cursor_ = 0;
}

//---------------------------------------------------------------------------
/**
 * @details all_items_ (ディスクの実体) から、マスクに一致する項目だけを選んで
 * order_ (表示順の添字列) を作り直す。マーク状態は all_items_ 側にしか
 * 持たせていないので、マスクや並べ替えを変えてもマークは消えない
 * (Collect() でディレクトリを読み直したときだけリセットされる、既存の挙動のまま)。
 */
void FilePane::ApplyFilterAndSort()
{
	order_.clear();
	order_.reserve(all_items_.size());

	for (std::size_t i = 0; i < all_items_.size(); ++i) {
		const FileItem &itm = all_items_[i];
		// ".." はマスクに関わらず常に表示する
		if (itm.is_parent || !HasMask() || MatchPathMask(mask_, itm.name, itm.is_dir)) {
			order_.push_back(i);
		}
	}

	std::sort(order_.begin(), order_.end(), [this](std::size_t ia, std::size_t ib) {
		return CompareFileItems(all_items_[ia], all_items_[ib], sort_key_, sort_descending_, dirs_first_) < 0;
	});
}

//---------------------------------------------------------------------------
void FilePane::RestoreCursorByName(const UnicodeString &name)
{
	cursor_ = 0;
	if (name.IsEmpty()) return;

	for (int i = 0; i < GetItemCount(); ++i) {
		if (SameStr(ItemAt(i).name, name)) {
			cursor_ = i;
			break;
		}
	}
}

//---------------------------------------------------------------------------
const FileItem *FilePane::GetCurrentItem() const
{
	if (cursor_ < 0 || cursor_ >= GetItemCount()) return nullptr;
	return &ItemAt(cursor_);
}

//---------------------------------------------------------------------------
void FilePane::MoveCursor(int delta)
{
	MoveCursorTo(cursor_ + delta);
}

void FilePane::MoveCursorTo(int index)
{
	const int last = GetItemCount() - 1;
	cursor_ = std::clamp(index, 0, std::max(0, last));
	EnsureVisible();
	Refresh();
}

void FilePane::PageMove(int direction)
{
	MoveCursor(direction * std::max(1, VisibleRows() - 1));
}

//---------------------------------------------------------------------------
void FilePane::ToggleMark()
{
	if (cursor_ < 0 || cursor_ >= GetItemCount()) return;
	FileItem &itm = ItemAt(cursor_);
	if (itm.is_parent) return;  // ".." はマークできない
	itm.marked = !itm.marked;
	MoveCursor(1);              // NyanFi と同じく、マークしたらカーソルを進める
}

void FilePane::MarkAll(bool marked)
{
	// 表示されている (マスクを通った) 項目だけを対象にする
	for (int i = 0; i < GetItemCount(); ++i) {
		FileItem &itm = ItemAt(i);
		if (!itm.is_parent) itm.marked = marked;
	}
	Refresh();
}

int FilePane::GetMarkedCount() const
{
	int n = 0;
	for (int i = 0; i < GetItemCount(); ++i) {
		if (ItemAt(i).marked) ++n;
	}
	return n;
}

std::vector<UnicodeString> FilePane::GetSelectedNames() const
{
	std::vector<UnicodeString> names;
	for (int i = 0; i < GetItemCount(); ++i) {
		const FileItem &itm = ItemAt(i);
		if (itm.marked && !itm.is_parent) names.push_back(itm.name);
	}

	// マークが無ければカーソル位置の1件を対象にする (".." は対象外)
	if (names.empty()) {
		const FileItem *cur = GetCurrentItem();
		if (cur != nullptr && !cur->is_parent) names.push_back(cur->name);
	}
	return names;
}

//---------------------------------------------------------------------------
void FilePane::SetActive(bool active)
{
	if (active_ == active) return;
	active_ = active;
	Refresh();
}

//---------------------------------------------------------------------------
bool FilePane::GoParent()
{
	if (is_root_dir(path_)) return false;

	const UnicodeString here = path_;
	const UnicodeString parent = get_parent_path(ExcludeTrailingPathDelimiter(path_));
	if (parent.IsEmpty() || !SetPath(parent)) return false;

	// 元いたディレクトリにカーソルを合わせる
	const UnicodeString leaf = ExtractFileName(ExcludeTrailingPathDelimiter(here));
	RestoreCursorByName(leaf);
	MoveCursorTo(cursor_);
	return true;
}

//---------------------------------------------------------------------------
bool FilePane::EnterCurrent()
{
	const FileItem *itm = GetCurrentItem();
	if (itm == nullptr || !itm->is_dir) return false;

	if (itm->is_parent) return GoParent();
	return SetPath(path_ + itm->name);
}

//---------------------------------------------------------------------------
UnicodeString FilePane::GetSummary() const
{
	int dirs = 0, files = 0;
	Int64 total = 0;
	for (int i = 0; i < GetItemCount(); ++i) {
		const FileItem &itm = ItemAt(i);
		if (itm.is_parent) continue;
		if (itm.is_dir) {
			++dirs;
		}
		else {
			++files;
			total += itm.size;
		}
	}

	UnicodeString s;
	s.sprintf(_T("%d dir / %d file"), dirs, files);
	if (files > 0) s += _T("  ") + get_size_str_G(total, 10, 1).Trim();
	const int marked = GetMarkedCount();
	if (marked > 0) s.cat_sprintf(_T("   [%d marked]"), marked);
	return s;
}

//---------------------------------------------------------------------------
void FilePane::SetSortSettings(SortKey key, bool descending, bool dirs_first)
{
	const UnicodeString cur = (GetCurrentItem() != nullptr) ? GetCurrentItem()->name : UnicodeString();

	sort_key_ = key;
	sort_descending_ = descending;
	dirs_first_ = dirs_first;
	ApplyFilterAndSort();

	RestoreCursorByName(cur);
	MoveCursorTo(cursor_);
	Refresh();
}

//---------------------------------------------------------------------------
UnicodeString FilePane::GetSortSummary() const
{
	UnicodeString s = sort_key_name(sort_key_) + (sort_descending_ ? _T(" 降順") : _T(" 昇順"));
	if (!dirs_first_) s += _T(" (Dir混在)");
	return s;
}

//---------------------------------------------------------------------------
void FilePane::SetMask(const UnicodeString &mask)
{
	const UnicodeString cur = (GetCurrentItem() != nullptr) ? GetCurrentItem()->name : UnicodeString();

	mask_ = mask;
	ApplyFilterAndSort();

	RestoreCursorByName(cur);
	MoveCursorTo(cursor_);
	Refresh();
}

//---------------------------------------------------------------------------
int FilePane::VisibleRows() const
{
	return std::max(1, (GetClientSize().y - HeaderHeight()) / row_height_);
}

void FilePane::EnsureVisible()
{
	const int rows = VisibleRows();
	if (cursor_ < top_) top_ = cursor_;
	if (cursor_ >= top_ + rows) top_ = cursor_ - rows + 1;
	if (top_ < 0) top_ = 0;
}

//---------------------------------------------------------------------------
void FilePane::OnSize(wxSizeEvent &event)
{
	EnsureVisible();
	Refresh();
	event.Skip();
}

void FilePane::OnSetFocus(wxFocusEvent &event)
{
	SetActive(true);
	event.Skip();
}

void FilePane::OnLeftDown(wxMouseEvent &event)
{
	SetFocus();
	const int y = event.GetY() - HeaderHeight();
	if (y >= 0) {
		const int row = top_ + y / row_height_;
		if (row >= 0 && row < GetItemCount()) MoveCursorTo(row);
	}
	event.Skip();
}

void FilePane::OnLeftDClick(wxMouseEvent &event)
{
	OnLeftDown(event);
	EnterCurrent();
}

void FilePane::OnMouseWheel(wxMouseEvent &event)
{
	const int lines = event.GetWheelRotation() > 0 ? -3 : 3;
	top_ = std::clamp(top_ + lines, 0, std::max(0, GetItemCount() - 1));
	Refresh();
}

//---------------------------------------------------------------------------
void FilePane::OnPaint(wxPaintEvent &)
{
	wxAutoBufferedPaintDC dc(this);
	dc.SetFont(font_);

	// 色はシステムから取る。これで Windows のライト/ダークに自動追従する
	const wxColour bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
	const wxColour fg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
	const wxColour sel_bg = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
	const wxColour sel_fg = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT);
	const wxColour dir_fg = wxSystemSettings::GetColour(wxSYS_COLOUR_HOTLIGHT);
	const wxColour hdr_bg = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
	const wxColour hdr_fg = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT);
	const wxColour mark_fg = wxColour(0xE0, 0x60, 0x30);

	const wxSize client = GetClientSize();
	dc.SetBrush(wxBrush(bg));
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.DrawRectangle(0, 0, client.x, client.y);

	const int size_col = client.x - char_width_ * 32;
	const int date_col = client.x - char_width_ * 22;
	const int attr_col = client.x - char_width_ * 5;

	// 列見出し行: 名前 / サイズ / 日時 / 属性。現在の並べ替えキーには矢印を付ける
	// (拡張子キーは専用の列を持たないため、名前列の矢印で代用する)
	{
		dc.SetBrush(wxBrush(hdr_bg));
		dc.SetPen(*wxTRANSPARENT_PEN);
		dc.DrawRectangle(0, 0, client.x, HeaderHeight());
		dc.SetTextForeground(hdr_fg);

		const UnicodeString mark = sort_descending_ ? _T(" ▼") : _T(" ▲");
		const bool name_active = (sort_key_ == SortKey::Name || sort_key_ == SortKey::Ext);

		UnicodeString name_hdr = _T("名前");
		if (name_active) name_hdr += mark;
		dc.DrawText(to_wx(name_hdr), char_width_ / 2, 1);

		UnicodeString size_hdr = _T("サイズ");
		if (sort_key_ == SortKey::Size) size_hdr += mark;
		dc.DrawText(to_wx(size_hdr), size_col, 1);

		UnicodeString date_hdr = _T("日時");
		if (sort_key_ == SortKey::Date) date_hdr += mark;
		dc.DrawText(to_wx(date_hdr), date_col, 1);

		UnicodeString attr_hdr = _T("属性");
		if (sort_key_ == SortKey::Attr) attr_hdr += mark;
		dc.DrawText(to_wx(attr_hdr), attr_col, 1);
	}

	const int rows = VisibleRows();

	for (int i = 0; i < rows; ++i) {
		const int index = top_ + i;
		if (index < 0 || index >= GetItemCount()) break;

		const FileItem &itm = ItemAt(index);
		const int y = HeaderHeight() + i * row_height_;
		const bool on_cursor = (index == cursor_);

		wxColour text_fg = itm.marked ? mark_fg : (itm.is_dir ? dir_fg : fg);

		if (on_cursor) {
			// 非アクティブ側のカーソルは枠だけにして、どちらが操作対象か分かるようにする
			if (active_) {
				dc.SetBrush(wxBrush(sel_bg));
				dc.SetPen(*wxTRANSPARENT_PEN);
				dc.DrawRectangle(0, y, client.x, row_height_);
				text_fg = itm.marked ? mark_fg : sel_fg;
			}
			else {
				dc.SetBrush(*wxTRANSPARENT_BRUSH);
				dc.SetPen(wxPen(sel_bg));
				dc.DrawRectangle(0, y, client.x, row_height_);
			}
		}

		dc.SetTextForeground(text_fg);

		// 名前 (右端の桁に重ならないところで切る)
		const int name_cols = std::max(4, (size_col - char_width_) / char_width_);
		UnicodeString name = itm.name;
		if (name.Length() > name_cols) name = name.SubString(1, name_cols - 1) + _T("…");
		dc.DrawText(to_wx(name), char_width_ / 2, y + 1);

		if (itm.is_dir) {
			dc.DrawText(itm.is_parent ? "<UP>" : "<DIR>", size_col, y + 1);
		}
		else {
			const wxString size_str = to_wx(get_size_str_B(itm.size, 14).Trim());
			dc.DrawText(size_str, size_col + char_width_ * 8 - dc.GetTextExtent(size_str).x, y + 1);
		}

		dc.DrawText(to_wx(FormatDateTime(_T("yyyy/mm/dd hh:nn"), itm.stamp)), date_col, y + 1);
		dc.DrawText(to_wx(get_file_attr_str(itm.attr)), attr_col, y + 1);
	}
}
