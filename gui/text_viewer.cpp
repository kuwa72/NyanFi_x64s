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
#include "usr_cmdlist.h"
#include "gui/text_display.h"
#include "gui/find_txt_dialog.h"

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
	marks_.clear();
	last_search_ = EmptyStr;
	find_options_ = find_txt::Options();
	find_options_.keyword = EmptyStr;
	last_error_ = EmptyStr;
	forced_code_page_ = 0;

	UpdateLineNoCols();
	RebuildWrap();
	Refresh();
	return true;
}

//---------------------------------------------------------------------------
int TextViewer::GutterWidth() const
{
	if (!show_line_no_) return 0;
	return (line_no_cols_ + 1) * char_width_;
}

int TextViewer::TextAreaCols() const
{
	// SetWidth で折り返し幅が指定されていればウィンドウ幅ではなくそれを使う
	// (VCL の ViewFoldFitWin=false + ViewFoldWidth と同じ)
	if (fold_width_ > 0) return fold_width_;
	return std::max(1, (GetClientSize().x - GutterWidth() - left_margin_) / char_width_);
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
			const std::size_t rows = text_viewer_core::WrapLine(doc_.lines[static_cast<std::size_t>(i)], width, tab_width_).size();
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
	// VCL は TxtViewer.cpp:5396-5399 で FindText を利用可能判定し、
	// MainFrm.cpp:19167-19171 から TFindTextDlg を表示する。wx では
	// 同じダイアログを gui/find_txt_dialog に移し、判定は find_txt.h へ渡す。
	find_txt::Options options = find_options_;
	options.keyword = last_search_;
	if (!find_txt_dialog::Run(this, doc_.is_binary, options)) return;

	find_options_ = options;
	last_search_ = options.keyword;
	if (last_search_.IsEmpty()) return;
	if (options.bytes || options.migemo) {
		wxMessageBox(to_wx(_T("バイト列/Migemo 検索の実処理は未実装です")),
		             to_wx(_T("検索")), wxOK | wxICON_INFORMATION, this);
		return;
	}

	const bool found = options.direction == find_txt::Direction::Up
		? SearchBackward(last_search_, current_line_, options.direction)
		: SearchForward(last_search_, current_line_, options.direction);
	if (!found) {
		wxMessageBox(to_wx(_T("見つかりませんでした")), to_wx(_T("検索")), wxOK | wxICON_INFORMATION, this);
	}
	else if (options.close_after && on_close_) {
		on_close_();
	}
}

bool TextViewer::SearchForward(const UnicodeString &kwd, int from_line,
                               find_txt::Direction direction)
{
	find_txt::Options options = find_options_;
	options.keyword = kwd;
	const int found = find_txt::FindNextLine(doc_.lines, options, from_line, direction);
	if (found == -1) return false;
	current_line_ = found;
	EnsureCursorVisible();
	Refresh();
	return true;
}

bool TextViewer::SearchBackward(const UnicodeString &kwd, int from_line,
                                find_txt::Direction direction)
{
	find_txt::Options options = find_options_;
	options.keyword = kwd;
	const int found = find_txt::FindNextLine(doc_.lines, options, from_line, direction);
	if (found == -1) return false;
	current_line_ = found;
	EnsureCursorVisible();
	Refresh();
	return true;
}

//---------------------------------------------------------------------------
/**
 * @details VCL 版 (TTxtViewer::ExeCommand / TNyanFiForm::ExeCommandV) の頻度上位
 *          コマンドを行単位ビューア向けに単純化したもの。選択系 (Sel系)・
 *          バイナリ/CSV/画像プレビュー系 (ChangeViewMode/BitmapView等)・
 *          外部連携系 (TagJump/OpenURL/WebSearch等) は対象外
 */
bool TextViewer::Execute(const UnicodeString &full_command)
{
	const UnicodeString command = get_CmdStr(full_command);
	const UnicodeString param = get_PrmStr(full_command);
	last_error_ = EmptyStr;

	if (SameStr(command, _T("CursorUp"))) {
		CmdCursorUp(param);
	}
	else if (SameStr(command, _T("CursorDown"))) {
		CmdCursorDown(param);
	}
	else if (SameStr(command, _T("PageUp"))) {
		CmdPageUp();
	}
	else if (SameStr(command, _T("PageDown"))) {
		CmdPageDown();
	}
	else if (SameStr(command, _T("TextTop"))) {
		CmdTextTop();
	}
	else if (SameStr(command, _T("TextEnd"))) {
		CmdTextEnd();
	}
	else if (SameStr(command, _T("LineTop"))) {
		CmdLineTop();
	}
	else if (SameStr(command, _T("LineEnd"))) {
		CmdLineEnd();
	}
	else if (SameStr(command, _T("CursorLeft"))) {
		CmdCursorLeft(param);
	}
	else if (SameStr(command, _T("CursorRight"))) {
		CmdCursorRight(param);
	}
	else if (SameStr(command, _T("FindText"))) {
		CmdFindText(param);
	}
	else if (SameStr(command, _T("FindDown"))) {
		if (!CmdFindDown(param) && param.IsEmpty() && last_search_.IsEmpty()) {
			last_error_ = _T("検索文字列がありません");
		}
	}
	else if (SameStr(command, _T("FindUp"))) {
		if (!CmdFindUp(param) && param.IsEmpty() && last_search_.IsEmpty()) {
			last_error_ = _T("検索文字列がありません");
		}
	}
	else if (SameStr(command, _T("JumpLine"))) {
		if (!CmdJumpLine(param)) last_error_ = _T("行番号が範囲外です");
	}
	else if (SameStr(command, _T("Mark"))) {
		CmdMark();
	}
	else if (SameStr(command, _T("ClearMark"))) {
		CmdClearMark();
	}
	else if (SameStr(command, _T("FindMarkDown"))) {
		if (!CmdFindMarkDown()) last_error_ = _T("下方向にマークがありません");
	}
	else if (SameStr(command, _T("FindMarkUp"))) {
		if (!CmdFindMarkUp()) last_error_ = _T("上方向にマークがありません");
	}
	else if (SameStr(command, _T("ChangeCodePage"))) {
		CmdChangeCodePage(param);
	}
	else if (SameStr(command, _T("ReloadFile"))) {
		CmdReload();
	}
	else if (SameStr(command, _T("ShowLineNo"))) {
		CmdShowLineNo(param);
	}
	else if (SameStr(command, _T("ShowRuler"))) {
		CmdShowRuler(param);
	}
	else if (SameStr(command, _T("ShowTAB"))) {
		CmdShowTAB(param);
	}
	else if (SameStr(command, _T("ShowCR"))) {
		CmdShowCR(param);
	}
	else if (SameStr(command, _T("SetTab"))) {
		CmdSetTab(param);
	}
	else if (SameStr(command, _T("SetWidth"))) {
		CmdSetWidth(param);
	}
	else if (SameStr(command, _T("SetMargin"))) {
		CmdSetMargin(param);
	}
	else if (SameStr(command, _T("Close"))) {
		CmdClose();
	}
	else {
		return false;  // 未実装
	}
	return true;
}

//---------------------------------------------------------------------------
void TextViewer::CmdCursorUp(const UnicodeString &param)
{
	if (doc_.lines.empty() || doc_.is_binary) return;
	current_line_ = text_viewer_core::StepLine(
		current_line_, -text_viewer_core::ParseMoveCount(param, VisibleRows()),
		static_cast<int>(doc_.lines.size()));
	EnsureCursorVisible();
	Refresh();
}

void TextViewer::CmdCursorDown(const UnicodeString &param)
{
	if (doc_.lines.empty() || doc_.is_binary) return;
	current_line_ = text_viewer_core::StepLine(
		current_line_, text_viewer_core::ParseMoveCount(param, VisibleRows()),
		static_cast<int>(doc_.lines.size()));
	EnsureCursorVisible();
	Refresh();
}

void TextViewer::CmdPageUp()
{
	if (doc_.lines.empty() || doc_.is_binary) return;
	MoveCursor(-text_viewer_core::PageStepLines(VisibleRows()));
}

void TextViewer::CmdPageDown()
{
	if (doc_.lines.empty() || doc_.is_binary) return;
	MoveCursor(text_viewer_core::PageStepLines(VisibleRows()));
}

void TextViewer::CmdTextTop()
{
	GotoTop();
}

void TextViewer::CmdTextEnd()
{
	GotoEnd();
}

void TextViewer::CmdLineTop()
{
	if (doc_.is_binary) return;
	h_offset_chars_ = 0;
	Refresh();
}

void TextViewer::CmdLineEnd()
{
	if (doc_.is_binary || wrap_) return;
	int max_len = 0;
	for (const auto &ln : doc_.lines) max_len = std::max(max_len, ln.Length());
	h_offset_chars_ = std::max(0, max_len - TextAreaCols() + 1);
	Refresh();
}

void TextViewer::CmdCursorLeft(const UnicodeString &param)
{
	ScrollHorizontal(param.IsEmpty() ? -4 : -text_viewer_core::ParseMoveCount(param, VisibleRows()));
}

void TextViewer::CmdCursorRight(const UnicodeString &param)
{
	ScrollHorizontal(param.IsEmpty() ? 4 : text_viewer_core::ParseMoveCount(param, VisibleRows()));
}

void TextViewer::CmdFindText(const UnicodeString &param)
{
	if (param.IsEmpty()) {
		PromptSearch();
		return;
	}
	last_search_ = param;
	find_options_.keyword = param;
	if (doc_.is_binary) {
		// バイナリ検索ダイアログ自体は開けるが、wx 版の実検索は未実装。
		wxMessageBox(to_wx(_T("バイト列検索の実処理は未実装です")), to_wx(_T("検索")),
		             wxOK | wxICON_INFORMATION, this);
		return;
	}
	if (!SearchForward(last_search_, current_line_, find_options_.direction)) {
		wxMessageBox(to_wx(_T("見つかりませんでした")), to_wx(_T("検索")), wxOK | wxICON_INFORMATION, this);
	}
}

bool TextViewer::CmdFindDown(const UnicodeString &param)
{
	if (doc_.is_binary) return false;
	if (!param.IsEmpty()) {
		last_search_ = param;
		find_options_.keyword = param;
	}
	if (last_search_.IsEmpty()) return false;
	if (!SearchForward(last_search_, current_line_, find_txt::Direction::Down)) {
		wxMessageBox(to_wx(_T("見つかりませんでした")), to_wx(_T("検索")), wxOK | wxICON_INFORMATION, this);
		return false;
	}
	return true;
}

bool TextViewer::CmdFindUp(const UnicodeString &param)
{
	if (doc_.is_binary) return false;
	if (!param.IsEmpty()) {
		last_search_ = param;
		find_options_.keyword = param;
	}
	if (last_search_.IsEmpty()) return false;
	if (!SearchBackward(last_search_, current_line_, find_txt::Direction::Up)) {
		wxMessageBox(to_wx(_T("見つかりませんでした")), to_wx(_T("検索")), wxOK | wxICON_INFORMATION, this);
		return false;
	}
	return true;
}

bool TextViewer::CmdJumpLine(const UnicodeString &param)
{
	if (doc_.is_binary || doc_.lines.empty()) return false;
	if (param.IsEmpty()) {
		UnicodeString cap;
		cap.cat_sprintf(_T("行番号 (1～%d)"), static_cast<int>(doc_.lines.size()));
		wxTextEntryDialog dlg(this, to_wx(cap), to_wx(_T("指定行へ移動")));
		if (dlg.ShowModal() != wxID_OK) return true;  // キャンセルは処理済み扱い
		const int target = text_viewer_core::ParseJumpLine(
			to_us(dlg.GetValue()), current_line_, static_cast<int>(doc_.lines.size()));
		if (target == -1) return false;
		GotoLine(target);
		return true;
	}
	const int target = text_viewer_core::ParseJumpLine(
		param, current_line_, static_cast<int>(doc_.lines.size()));
	if (target == -1) return false;
	GotoLine(target);
	return true;
}

void TextViewer::CmdMark()
{
	if (doc_.is_binary || doc_.lines.empty()) return;
	marks_ = text_viewer_core::ToggleMark(marks_, current_line_);
	Refresh();
}

void TextViewer::CmdClearMark()
{
	marks_ = text_viewer_core::ClearMarkList(marks_);
	Refresh();
}

bool TextViewer::CmdFindMarkDown()
{
	const int found = text_viewer_core::FindMarkNext(marks_, current_line_, true);
	if (found == -1) return false;
	GotoLine(found);
	return true;
}

bool TextViewer::CmdFindMarkUp()
{
	const int found = text_viewer_core::FindMarkNext(marks_, current_line_, false);
	if (found == -1) return false;
	GotoLine(found);
	return true;
}

void TextViewer::CmdChangeCodePage(const UnicodeString &param)
{
	if (path_.IsEmpty() || doc_.is_binary) return;
	int cp;
	if (param.IsEmpty()) {
		cp = text_viewer_core::NextCodePage(doc_.code_page);
	}
	else {
		cp = text_viewer_core::ParseCodePageParam(param, doc_.code_page);
		if (cp == 0) {
			last_error_ = _T("対応していない文字コードです: ") + param;
			wxMessageBox(to_wx(last_error_), to_wx(_T("文字コード変更")),
				wxOK | wxICON_INFORMATION, this);
			return;
		}
	}
	forced_code_page_ = cp;
	CmdReload();
}

void TextViewer::CmdReload()
{
	if (path_.IsEmpty()) return;
	UnicodeString error;
	text_viewer_core::LoadResult r =
		text_viewer_core::LoadForView(path_, text_viewer_core::kMaxViewBytes, forced_code_page_);
	if (!r.ok) {
		wxMessageBox(to_wx(r.error), to_wx(_T("開けませんでした")), wxOK | wxICON_ERROR, this);
		return;
	}
	const int keep = current_line_;
	doc_ = std::move(r);
	current_line_ = std::clamp(keep, 0, std::max(0, static_cast<int>(doc_.lines.size()) - 1));
	// マークは再読込で行数が変わるとずれるため、範囲外だけ落とす
	marks_.erase(std::remove_if(marks_.begin(), marks_.end(), [&](int m) {
		return m < 0 || m >= static_cast<int>(doc_.lines.size());
	}), marks_.end());
	UpdateLineNoCols();
	RebuildWrap();
	EnsureCursorVisible();
	Refresh();
}

void TextViewer::CmdClose()
{
	if (on_close_) on_close_();
}

//---------------------------------------------------------------------------
void TextViewer::CmdShowLineNo(const UnicodeString &param)
{
	show_line_no_ = text_display::ToggleValue(show_line_no_, param);
	Refresh();
}

void TextViewer::CmdShowRuler(const UnicodeString &param)
{
	show_ruler_ = text_display::ToggleValue(show_ruler_, param);
	Refresh();
}

void TextViewer::CmdShowTAB(const UnicodeString &param)
{
	show_tab_ = text_display::ToggleValue(show_tab_, param);
	Refresh();
}

void TextViewer::CmdShowCR(const UnicodeString &param)
{
	show_cr_ = text_display::ToggleValue(show_cr_, param);
	Refresh();
}

void TextViewer::CmdSetTab(const UnicodeString &param)
{
	// VCL は空で入力ボックスを出す。ここに入力UIは無いので無視する
	if (param.IsEmpty()) return;
	tab_width_ = text_display::ParseTabWidth(param, tab_width_);
	RebuildWrap();
	Refresh();
}

void TextViewer::CmdSetWidth(const UnicodeString &param)
{
	// VCL は空で入力ボックスを出す。ここに入力UIは無いので無視する
	if (param.IsEmpty()) return;
	fold_width_ = text_display::ParseFoldWidth(param, fold_width_);
	RebuildWrap();
	Refresh();
}

void TextViewer::CmdSetMargin(const UnicodeString &param)
{
	// VCL は空で SetActionAbort (何もしない)。同じく無視する
	if (param.IsEmpty()) return;
	left_margin_ = text_display::ParseMargin(param, left_margin_);
	RebuildWrap();
	Refresh();
}

//---------------------------------------------------------------------------
/**
 * @details コマンド名とキーは src/Global.cpp の既定キー表 (ScrModeIdStr "V")
 * に極力合わせた。実装済み: Q=Close (閉じる)、F=FindText (検索)。
 * それ以外 (B=ChangeViewMode 等) は Phase 2 骨格のスコープ外の機能
 * (バイナリ/CSV/画像プレビュー切替等) のため対応せず、代わりに
 * W (折り返し切替) を独自に割り当てた (推測・要検証。既定キー表に
 * 折り返し専用のキーが見当たらなかったため)。
 * G/M/N/P/R/C/J は Execute と同じ処理へのショートカット (推測のキー。
 * 既定キー表に記載が無いため。J=JumpLine、GはVCLのGrepと紛らわしいが
 * ビューア表示中はGrepに回さない)。
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
	case 'M':          CmdMark(); return true;
	case 'N':          CmdFindDown(EmptyStr); return true;
	case 'P':          CmdFindUp(EmptyStr); return true;
	case 'J':          CmdJumpLine(EmptyStr); return true;
	case 'R':          CmdReload(); return true;
	case 'C':          CmdChangeCodePage(EmptyStr); return true;
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
	if (!show_line_no_) s += _T("  行番:OFF");
	if (!marks_.empty()) s.cat_sprintf(_T("  栞:%d"), static_cast<int>(marks_.size()));
	if (!last_error_.IsEmpty()) s += _T("  ") + last_error_;
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
	const int text_x = gutter_w + left_margin_;
	if (gutter_w > 0) {
		dc.SetBrush(wxBrush(gutter_bg));
		dc.SetPen(*wxTRANSPARENT_PEN);
		dc.DrawRectangle(0, HeaderHeight(), gutter_w, client.y - HeaderHeight());
	}

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

		// 行番号 (折り返しの継続行は空欄にする、一般的なエディタと同じ表現。
		// ShowLineNo=OFF では欄自体を描かない)
		if (sub == 0 && gutter_w > 0) {
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
				text_viewer_core::WrapLine(doc_.lines[static_cast<std::size_t>(line)], TextAreaCols(), tab_width_);
			if (sub >= 0 && sub < static_cast<int>(segs.size())) text = segs[static_cast<std::size_t>(sub)];
		}
		else {
			const UnicodeString &full = doc_.lines[static_cast<std::size_t>(line)];
			if (h_offset_chars_ < full.Length()) text = full.SubString(h_offset_chars_ + 1);
		}

		dc.DrawText(to_wx(text), text_x, y + 1);
	}
}
