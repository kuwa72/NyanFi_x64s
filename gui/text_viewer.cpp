/**
 * @file gui/text_viewer.cpp
 * @brief テキストビューアの実装
 */
#include "gui/text_viewer.h"

#include <algorithm>
#include <cstdlib>

#include <wx/dcbuffer.h>
#include <wx/settings.h>
#include <wx/textdlg.h>

#include "usr_str.h"

namespace {

/// wxString への変換 (gui/file_pane.cpp と同じ)
inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

/// wxString → UnicodeString (MSW では両方 UTF-16)
inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

}  // namespace

//---------------------------------------------------------------------------
TextViewer::TextViewer(wxWindow *parent, wxWindowID id)
	: wxWindow(parent, id, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
{
	SetBackgroundStyle(wxBG_STYLE_PAINT);

	// 等幅フォント。行番号・折り返し幅の計算がずれると表示が崩れる
	font_ = wxFont(wxFontInfo(10).FaceName("Consolas").Family(wxFONTFAMILY_TELETYPE));
	if (!font_.IsOk()) font_ = wxFont(wxFontInfo(10).Family(wxFONTFAMILY_TELETYPE));

	Bind(wxEVT_PAINT, &TextViewer::OnPaint, this);
	Bind(wxEVT_SIZE, &TextViewer::OnSize, this);
	Bind(wxEVT_MOUSEWHEEL, &TextViewer::OnMouseWheel, this);

	UpdateMetrics();
}

//---------------------------------------------------------------------------
void TextViewer::UpdateMetrics()
{
	wxClientDC dc(this);
	dc.SetFont(font_);
	const wxSize ext = dc.GetTextExtent("M");
	char_width_ = std::max(1, ext.x);
	row_height_ = std::max(1, ext.y + 2);
}

//---------------------------------------------------------------------------
void TextViewer::UpdateLineNoCols()
{
	int n = static_cast<int>(doc_.lines.size());
	int digits = 1;
	while (n >= 10) {
		n /= 10;
		++digits;
	}
	line_no_cols_ = std::max(3, digits);
}

//---------------------------------------------------------------------------
bool TextViewer::LoadFile(const UnicodeString &path, UnicodeString &error)
{
	text_viewer_core::LoadResult r = text_viewer_core::LoadForView(path);
	if (!r.ok) {
		error = r.error;
		return false;
	}

	doc_ = std::move(r);
	path_ = path;
	current_line_ = 0;
	top_row_ = 0;
	h_offset_chars_ = 0;
	wrap_ = false;

	UpdateLineNoCols();
	RebuildWrap();
	Refresh();
	return true;
}

//---------------------------------------------------------------------------
int TextViewer::GutterWidth() const
{
	return (line_no_cols_ + 1) * char_width_;
}

int TextViewer::TextAreaCols() const
{
	return std::max(1, (GetClientSize().x - GutterWidth()) / char_width_);
}

int TextViewer::VisibleRows() const
{
	return std::max(1, (GetClientSize().y - HeaderHeight()) / row_height_);
}

//---------------------------------------------------------------------------
void TextViewer::RebuildWrap()
{
	const int n = static_cast<int>(doc_.lines.size());
	wrap_rows_.assign(static_cast<std::size_t>(n), 1);

	if (wrap_) {
		const int width = TextAreaCols();
		for (int i = 0; i < n; ++i) {
			const std::size_t rows = text_viewer_core::WrapLine(doc_.lines[static_cast<std::size_t>(i)], width).size();
			wrap_rows_[static_cast<std::size_t>(i)] = std::max<int>(1, static_cast<int>(rows));
		}
	}

	prefix_rows_.assign(static_cast<std::size_t>(n) + 1, 0);
	for (int i = 0; i < n; ++i) {
		prefix_rows_[static_cast<std::size_t>(i) + 1] =
			prefix_rows_[static_cast<std::size_t>(i)] + wrap_rows_[static_cast<std::size_t>(i)];
	}
}

//---------------------------------------------------------------------------
Int64 TextViewer::DisplayRowOfLine(int line) const
{
	if (prefix_rows_.empty()) return 0;
	line = std::clamp(line, 0, static_cast<int>(prefix_rows_.size()) - 1);
	return prefix_rows_[static_cast<std::size_t>(line)];
}

int TextViewer::LineOfDisplayRow(Int64 row) const
{
	if (prefix_rows_.size() < 2) return 0;
	// prefix_rows_[i] = 行 i の先頭表示行。upper_bound で最初に row を超える
	// 位置を求め、その1つ手前が row を含む行になる
	auto it = std::upper_bound(prefix_rows_.begin(), prefix_rows_.end(), row);
	std::size_t idx = static_cast<std::size_t>(it - prefix_rows_.begin());
	if (idx == 0) idx = 1;
	if (idx >= prefix_rows_.size()) idx = prefix_rows_.size() - 1;
	return static_cast<int>(idx - 1);
}

//---------------------------------------------------------------------------
void TextViewer::EnsureCursorVisible()
{
	const Int64 row = DisplayRowOfLine(current_line_);
	const int rows = VisibleRows();
	if (row < top_row_) top_row_ = row;
	if (row >= top_row_ + rows) top_row_ = row - rows + 1;
	if (top_row_ < 0) top_row_ = 0;
}

//---------------------------------------------------------------------------
void TextViewer::MoveCursor(int delta)
{
	if (doc_.lines.empty()) return;
	const int n = static_cast<int>(doc_.lines.size());
	current_line_ = std::clamp(current_line_ + delta, 0, n - 1);
	EnsureCursorVisible();
	Refresh();
}

void TextViewer::PageMove(int direction)
{
	MoveCursor(direction * std::max(1, VisibleRows() - 1));
}

void TextViewer::GotoTop()
{
	if (doc_.lines.empty()) return;
	current_line_ = 0;
	top_row_ = 0;
	h_offset_chars_ = 0;
	Refresh();
}

void TextViewer::GotoEnd()
{
	if (doc_.lines.empty()) return;
	current_line_ = static_cast<int>(doc_.lines.size()) - 1;
	EnsureCursorVisible();
	Refresh();
}

//---------------------------------------------------------------------------
void TextViewer::GotoLine(int line)
{
	if (doc_.lines.empty()) return;
	const int n = static_cast<int>(doc_.lines.size());
	current_line_ = std::clamp(line, 0, n - 1);
	EnsureCursorVisible();
	Refresh();
}

void TextViewer::ScrollHorizontal(int delta)
{
	if (wrap_) return;  // 折り返し時は横スクロール不要 (禁則)
	h_offset_chars_ = std::max(0, h_offset_chars_ + delta);
	Refresh();
}

void TextViewer::ToggleWrap()
{
	wrap_ = !wrap_;
	h_offset_chars_ = 0;
	RebuildWrap();
	EnsureCursorVisible();
	Refresh();
}

//---------------------------------------------------------------------------
void TextViewer::PromptSearch()
{
	wxTextEntryDialog dlg(this, to_wx(_T("検索文字列")), to_wx(_T("検索")), to_wx(last_search_));
	if (dlg.ShowModal() != wxID_OK) return;

	const UnicodeString kwd = to_us(dlg.GetValue());
	if (kwd.IsEmpty()) return;

	last_search_ = kwd;
	if (!SearchForward(kwd, current_line_)) {
		wxMessageBox(to_wx(_T("見つかりませんでした")), to_wx(_T("検索")), wxOK | wxICON_INFORMATION, this);
	}
}

bool TextViewer::SearchForward(const UnicodeString &kwd, int from_line)
{
	const int n = static_cast<int>(doc_.lines.size());
	if (n == 0) return false;

	// 大小文字を区別しない (VCL 版の isCase 切替は非対応。要検証・簡略化)。
	// 次の行から探し、末尾まで行ったら先頭へ折り返す
	for (int step = 1; step <= n; ++step) {
		const int i = (from_line + step) % n;
		if (ContainsText(doc_.lines[static_cast<std::size_t>(i)], kwd)) {
			current_line_ = i;
			EnsureCursorVisible();
			Refresh();
			return true;
		}
	}
	return false;
}

//---------------------------------------------------------------------------
/**
 * @details コマンド名とキーは src/Global.cpp の既定キー表 (ScrModeIdStr "V")
 * に極力合わせた。実装済み: Q=Close (閉じる)、F=FindText (検索)。
 * それ以外 (B=ChangeViewMode 等) は Phase 2 骨格のスコープ外の機能
 * (バイナリ/CSV/画像プレビュー切替等) のため対応せず、代わりに
 * W (折り返し切替) を独自に割り当てた (推測・要検証。既定キー表に
 * 折り返し専用のキーが見当たらなかったため)。
 */
bool TextViewer::HandleKey(wxKeyEvent &event)
{
	const int code = event.GetKeyCode();

	// V:Q=Close (既定)。ESC は利便性のため追加 (要検証、既定キー表には無い)
	if (code == 'Q' || code == WXK_ESCAPE) {
		if (on_close_) on_close_();
		return true;
	}

	// バイナリ表示中はナビゲーションを行わない (閉じるキーのみ有効)
	if (doc_.is_binary) return true;

	switch (code) {
	case WXK_DOWN:     MoveCursor(1);  return true;
	case WXK_UP:       MoveCursor(-1); return true;
	case WXK_LEFT:     ScrollHorizontal(-4); return true;
	case WXK_RIGHT:    ScrollHorizontal(4);  return true;
	case WXK_PAGEDOWN: PageMove(1);  return true;
	case WXK_PAGEUP:   PageMove(-1); return true;
	// HOME/END は VCL 版の TextTop/TextEnd (先頭/末尾ジャンプ) 相当。
	// 行単位カーソルに単純化したため行内の桁移動は無い (要検証)
	case WXK_HOME:     GotoTop(); return true;
	case WXK_END:      GotoEnd(); return true;
	case 'W':          ToggleWrap(); return true;
	case 'F':          PromptSearch(); return true;
	default:           break;
	}
	return false;
}

//---------------------------------------------------------------------------
UnicodeString TextViewer::GetStatusSummary() const
{
	if (path_.IsEmpty()) return EmptyStr;

	UnicodeString s = path_;
	if (doc_.is_binary) {
		s += _T("  [バイナリファイル]");
		return s;
	}

	s += _T("  ") + get_NameOfCodePage(doc_.code_page, false, doc_.has_bom);
	s.cat_sprintf(_T("  %d/%d 行"), current_line_ + 1, static_cast<int>(doc_.lines.size()));
	s += wrap_ ? _T("  折返:ON") : _T("  折返:OFF");
	if (doc_.truncated) s += _T("  (先頭のみ表示 - サイズ制限)");
	return s;
}

//---------------------------------------------------------------------------
void TextViewer::OnSize(wxSizeEvent &event)
{
	RebuildWrap();  // 折り返し幅は表示幅に依存するため作り直す
	EnsureCursorVisible();
	Refresh();
	event.Skip();
}

void TextViewer::OnMouseWheel(wxMouseEvent &event)
{
	const int rows = (event.GetWheelRotation() > 0) ? -3 : 3;
	const Int64 total = TotalDisplayRows();
	top_row_ = std::clamp<Int64>(top_row_ + rows, 0, std::max<Int64>(0, total - 1));
	Refresh();
}

//---------------------------------------------------------------------------
void TextViewer::OnPaint(wxPaintEvent &)
{
	wxAutoBufferedPaintDC dc(this);
	dc.SetFont(font_);

	// 色はシステムから取る (gui/file_pane.cpp と同じ。ライト/ダークに自動追従)
	const wxColour bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
	const wxColour fg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
	const wxColour cursor_bg = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
	const wxColour cursor_fg = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT);
	const wxColour gutter_bg = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
	const wxColour gutter_fg = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT);
	const wxColour hdr_bg = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
	const wxColour hdr_fg = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT);

	const wxSize client = GetClientSize();
	dc.SetBrush(wxBrush(bg));
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.DrawRectangle(0, 0, client.x, client.y);

	// ヘッダ (ファイル名・コードページ・行数・折り返し状態)
	dc.SetBrush(wxBrush(hdr_bg));
	dc.DrawRectangle(0, 0, client.x, HeaderHeight());
	dc.SetTextForeground(hdr_fg);
	dc.DrawText(to_wx(GetStatusSummary()), char_width_ / 2, 2);

	if (path_.IsEmpty()) return;

	if (doc_.is_binary) {
		dc.SetTextForeground(fg);
		dc.DrawText(to_wx(_T("バイナリファイルです (テキストとして表示できません。Qで閉じます)")),
		            char_width_, HeaderHeight() + row_height_);
		return;
	}

	const int gutter_w = GutterWidth();
	dc.SetBrush(wxBrush(gutter_bg));
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.DrawRectangle(0, HeaderHeight(), gutter_w, client.y - HeaderHeight());

	const int rows = VisibleRows();
	const int n = static_cast<int>(doc_.lines.size());
	const Int64 total = TotalDisplayRows();

	for (int r = 0; r < rows; ++r) {
		const Int64 disp_row = top_row_ + r;
		if (disp_row >= total) break;

		const int line = LineOfDisplayRow(disp_row);
		if (line < 0 || line >= n) break;
		const int sub = static_cast<int>(disp_row - prefix_rows_[static_cast<std::size_t>(line)]);

		const int y = HeaderHeight() + r * row_height_;
		const bool on_cursor = (line == current_line_);

		if (on_cursor) {
			dc.SetBrush(wxBrush(cursor_bg));
			dc.SetPen(*wxTRANSPARENT_PEN);
			dc.DrawRectangle(0, y, client.x, row_height_);
		}

		// 行番号 (折り返しの継続行は空欄にする、一般的なエディタと同じ表現)
		if (sub == 0) {
			dc.SetTextForeground(on_cursor ? cursor_fg : gutter_fg);
			UnicodeString num;
			num.sprintf(_T("%*d"), line_no_cols_, line + 1);
			dc.DrawText(to_wx(num), char_width_ / 2, y + 1);
		}

		// 本文
		dc.SetTextForeground(on_cursor ? cursor_fg : fg);

		UnicodeString text;
		if (wrap_) {
			const std::vector<UnicodeString> segs =
				text_viewer_core::WrapLine(doc_.lines[static_cast<std::size_t>(line)], TextAreaCols());
			if (sub >= 0 && sub < static_cast<int>(segs.size())) text = segs[static_cast<std::size_t>(sub)];
		}
		else {
			const UnicodeString &full = doc_.lines[static_cast<std::size_t>(line)];
			if (h_offset_chars_ < full.Length()) text = full.SubString(h_offset_chars_ + 1);
		}

		dc.DrawText(to_wx(text), gutter_w, y + 1);
	}
}
