/**
 * @file gui/main_frame.cpp
 * @brief メインウィンドウの実装
 */
#include "gui/main_frame.h"

#include <wx/clipbrd.h>

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <vector>

#include <wx/choicdlg.h>
#include <wx/dcbuffer.h>
#include <wx/radiobox.h>
#include <wx/settings.h>
#include <wx/statline.h>
#include <wx/textdlg.h>

#include "gui/file_info_panel.h"
#include "gui/file_open.h"
#include "gui/archive.h"
#include "gui/clipboard_files.h"
#include "gui/compare.h"
#include "gui/text_ops.h"
#include "gui/text_viewer_core.h"
#include "gui/file_ops.h"
#include "gui/grep_dialog.h"
#include "gui/image_load.h"
#include "gui/rename_dialog.h"
#include "gui/selection.h"
#include "gui/view_state.h"
#include "usr_cmdlist.h"
#include "usr_file_ex.h"
#include "usr_file_inf.h"
#include "usr_str.h"

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

/// wxString → UnicodeString (wxTextEntryDialog の戻り値用。MSW では両方 UTF-16)
inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

/// 起動時に開くディレクトリ
UnicodeString initial_path()
{
	const UnicodeString cur = IncludeTrailingPathDelimiter(GetCurrentDir());
	return dir_exists(cur) ? cur : UnicodeString(_T("C:\\"));
}

/// 破壊的操作 (コピー・移動・削除) の前に出す確認ダイアログ
/// @details 件数と、対象の先頭数件の名前、コピー/移動先 (削除なら空) を明示する
bool ConfirmItems(wxWindow *parent, const UnicodeString &title, const UnicodeString &verb,
                   const std::vector<UnicodeString> &names, const UnicodeString &dest)
{
	UnicodeString text;
	text.sprintf(_T("%d 件を%sします。よろしいですか?\n\n"), static_cast<int>(names.size()), verb.c_str());

	const std::size_t show = std::min<std::size_t>(names.size(), 8);
	for (std::size_t i = 0; i < show; ++i) text += _T("・") + names[i] + _T("\n");
	if (names.size() > show) text.cat_sprintf(_T("...ほか %d 件\n"), static_cast<int>(names.size() - show));

	if (!dest.IsEmpty()) text += _T("\n宛先: ") + dest;

	return wxMessageBox(to_wx(text), to_wx(title), wxYES_NO | wxICON_QUESTION, parent) == wxYES;
}

/// 実行可能ファイルを開く前の確認 (誤って起動してしまう事故を防ぐ)
bool ConfirmExecute(wxWindow *parent, const UnicodeString &full_path)
{
	const UnicodeString text = _T("実行可能ファイルです。開いてもよろしいですか?\n\n") + full_path;
	return wxMessageBox(to_wx(text), to_wx(_T("実行の確認")), wxYES_NO | wxICON_WARNING, parent) == wxYES;
}

}  // namespace

//---------------------------------------------------------------------------
/**
 * @brief タブバー (自前描画)
 * @details gui/main_frame.h から `class TabBar;` として前方宣言されるため、
 * (無名名前空間だと別の型として扱われ食い違ってしまう) 無名名前空間の外
 * (このファイルのグローバルスコープ) に置いてある。
 *
 * 色は wxSystemSettings から取る (gui/file_pane.cpp と同じ考え方。Windows の
 * ライト/ダークモードに追従する)。VCL 版のタブの閉じるボタン (DelTabBtn) に
 * 相当するものとして各タブの右端に "x" を描き、そこをクリックすると
 * OnCloseTab を呼ぶ。末尾の "+" (VCL 版に対応するボタンは無い、Phase 2 骨格
 * 向けの新規 UI) をクリックすると OnAddTab を呼ぶ
 */
class TabBar : public wxWindow {
public:
	explicit TabBar(wxWindow *parent) : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, 24))
	{
		SetBackgroundStyle(wxBG_STYLE_PAINT);
		Bind(wxEVT_PAINT, &TabBar::OnPaint, this);
		Bind(wxEVT_LEFT_DOWN, &TabBar::OnLeftDown, this);
	}

	std::function<void(int)> OnSelect;    //!< タブ本体をクリックしたとき (タブへ切り替え)
	std::function<void(int)> OnCloseTab;  //!< "x" をクリックしたとき (そのタブを閉じる)
	std::function<void()> OnAddTab;       //!< 末尾の "+" をクリックしたとき (タブを追加)

	/// 表示するキャプション一覧と、現在アクティブなタブの添字を設定する
	void SetTabs(const std::vector<UnicodeString> &captions, int current)
	{
		captions_ = captions;
		current_ = current;
		Refresh();
	}

private:
	/// タブ・"x"・"+" それぞれの矩形 (OnPaint と OnLeftDown で同じ計算をするための共通処理)
	struct Layout {
		std::vector<wxRect> tab_rects;
		std::vector<wxRect> close_rects;
		wxRect add_rect;
	};

	Layout BuildLayout() const
	{
		Layout lay;
		int x = 2;
		const int h = std::max(1, GetClientSize().y);

		wxClientDC dc(const_cast<TabBar *>(this));
		for (const UnicodeString &cap : captions_) {
			const wxSize ext = dc.GetTextExtent(to_wx(cap));
			const int w = ext.x + 28;  // 文字幅 + 左右の余白 + "x" の幅
			lay.tab_rects.push_back(wxRect(x, 1, w, h - 2));
			lay.close_rects.push_back(wxRect(x + w - 18, 1, 16, h - 2));
			x += w + 2;
		}
		lay.add_rect = wxRect(x, 1, h - 2, h - 2);  // 正方形の "+" ボタン
		return lay;
	}

	void OnPaint(wxPaintEvent & /*event*/)
	{
		wxAutoBufferedPaintDC dc(this);

		const wxColour bg = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
		const wxColour active_bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
		const wxColour fg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
		const wxColour frame_col = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW);

		dc.SetBackground(wxBrush(bg));
		dc.Clear();
		dc.SetTextForeground(fg);

		const Layout lay = BuildLayout();
		for (std::size_t i = 0; i < captions_.size(); ++i) {
			const bool is_current = (static_cast<int>(i) == current_);
			dc.SetPen(wxPen(frame_col));
			dc.SetBrush(wxBrush(is_current ? active_bg : bg));
			dc.DrawRectangle(lay.tab_rects[i]);
			dc.DrawText(to_wx(captions_[i]), lay.tab_rects[i].x + 6, lay.tab_rects[i].y + 3);
			dc.DrawText("x", lay.close_rects[i].x + 4, lay.close_rects[i].y + 3);
		}

		dc.SetPen(wxPen(frame_col));
		dc.SetBrush(wxBrush(bg));
		dc.DrawRectangle(lay.add_rect);
		dc.DrawText("+", lay.add_rect.x + lay.add_rect.width / 2 - 4, lay.add_rect.y + 3);
	}

	void OnLeftDown(wxMouseEvent &event)
	{
		const wxPoint pos = event.GetPosition();
		const Layout lay = BuildLayout();

		for (std::size_t i = 0; i < captions_.size(); ++i) {
			if (lay.close_rects[i].Contains(pos)) {
				if (OnCloseTab) OnCloseTab(static_cast<int>(i));
				return;
			}
			if (lay.tab_rects[i].Contains(pos)) {
				if (OnSelect) OnSelect(static_cast<int>(i));
				return;
			}
		}
		if (lay.add_rect.Contains(pos) && OnAddTab) OnAddTab();
	}

	std::vector<UnicodeString> captions_;
	int current_ = 0;
};

//---------------------------------------------------------------------------
MainFrame::MainFrame()
	: wxFrame(nullptr, wxID_ANY, "NyanFi (wxWidgets port)", wxDefaultPosition, wxSize(1000, 640))
{
	wxPanel *root = new wxPanel(this);
	wxBoxSizer *columns = new wxBoxSizer(wxHORIZONTAL);

	for (int i = 0; i < 2; ++i) {
		wxBoxSizer *column = new wxBoxSizer(wxVERTICAL);

		headers_[i] = new wxStaticText(root, wxID_ANY, wxEmptyString);
		panes_[i] = new FilePane(root, wxID_ANY);

		column->Add(headers_[i], wxSizerFlags().Expand().Border(wxLEFT | wxTOP, 4));
		column->Add(panes_[i], wxSizerFlags(1).Expand().Border(wxALL, 2));
		columns->Add(column, wxSizerFlags(1).Expand());
	}

	root->SetSizer(columns);
	root_ = root;
	columns_ = columns;

	// タブバー (gui/tabs.h の TabManager の見た目)。root_ の上に横一列で置く
	// (レイアウトの詳細は MainFrame::OnSize を参照)
	tab_bar_ = new TabBar(this);
	tab_bar_->OnSelect = [this](int index) {
		if (index == tabs_.CurrentIndex()) return;
		StoreCurrentTabState();
		if (tabs_.SelectAt(index)) ApplyTabState(tabs_.Current());
		RefreshTabBar();
	};
	tab_bar_->OnCloseTab = [this](int index) {
		// 表示中のタブ以外を閉じても、表示中のタブ自体は切り替わらない
		// (TabManager::CloseTabAt を参照)
		const bool was_current = (index == tabs_.CurrentIndex());
		if (!tabs_.CloseTabAt(index)) {
			wxBell();  // 最後の1枚は閉じない
			return;
		}
		if (was_current) ApplyTabState(tabs_.Current());
		RefreshTabBar();
	};
	tab_bar_->OnAddTab = [this]() { CmdAddTab(); };

	// テキストビューア。root_ と同じ領域に重ねて置き、開いていないときは隠す
	// (ShowViewer で切り替える)。root_/viewer_ 双方の実サイズは
	// MainFrame::OnSize で明示的に GetClientSize() へ合わせる
	viewer_ = new TextViewer(this, wxID_ANY);
	viewer_->Hide();
	viewer_->SetOnClose([this]() { ShowViewer(false); });

	// 画像ビューア。viewer_ と同じく root_ に重ねて置き、閉じている間は隠す
	image_viewer_ = new ImageViewer(this, wxID_ANY);
	image_viewer_->Hide();
	image_viewer_->SetOnClose([this]() { ShowImageViewer(false); });
	image_viewer_->SetOnNavigate([this](int direction) { CmdImageNavigate(direction); });

	CreateStatusBar(2);
	SetStatusWidths(2, std::array<int, 2>{-3, -1}.data());

	// キー割り当て: VCL 版と同じ ini (<exe名>.ini) に "KeyFuncList" セクションが
	// あれば、既定の割り当て (key_map.cpp の LoadDefaults) を上書きする。
	// 読み込みのみで書き込みは行わない (settings_ とは別ファイル。理由は
	// gui/settings.h と gui/key_map.h のコメントを参照)
	keymap_.LoadFromIni(ChangeFileExt(Application->ExeName, _T(".ini")));

	// ウィンドウ位置・ペインのディレクトリを復元する (無ければ既定)
	LoadSettings();
	SetActivePane(0);

	Bind(wxEVT_CHAR_HOOK, &MainFrame::OnCharHook, this);
	Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
	Bind(wxEVT_SIZE, &MainFrame::OnSize, this);
}

//---------------------------------------------------------------------------
/// tab_bar_ をクライアント領域の最上段に、root_ (2ペイン)・viewer_ (テキスト
/// ビューア)・image_viewer_ (画像ビューア) をその下いっぱいに揃える
void MainFrame::OnSize(wxSizeEvent &event)
{
	const wxSize sz = GetClientSize();
	const int tab_bar_h = (tab_bar_ != nullptr) ? tab_bar_->GetSize().y : 0;

	if (tab_bar_ != nullptr) tab_bar_->SetSize(0, 0, sz.x, tab_bar_h);

	const wxRect body(0, tab_bar_h, sz.x, std::max(0, sz.y - tab_bar_h));
	if (root_ != nullptr) root_->SetSize(body);
	if (viewer_ != nullptr) viewer_->SetSize(body);
	if (image_viewer_ != nullptr) image_viewer_->SetSize(body);
	event.Skip();
}

//---------------------------------------------------------------------------
/**
 * @brief 起動時: settings_ (nyanfi_wx.ini) からウィンドウ位置・タブを復元する
 * @details タブの永続化は tabs_ (gui/tabs.h) が担い、settings_ と同じ ini
 * ファイル (settings_.Ini()) の別セクションに書く (gui/tabs.h の解説を参照)。
 * 保存されたタブが無い (初回起動、または本機能より前の _wx.ini) 場合は、
 * 旧来の settings_.LeftDir/RightDir (無ければ起動時のカレントディレクトリ)
 * から1本だけのタブを作る
 */
void MainFrame::LoadSettings()
{
	const UnicodeString start = initial_path();

	tabs_.LoadFromIni(settings_.Ini());
	if (tabs_.Count() == 0) {
		// 通常は起こらない (TabManager は常に1本以上持つ) が、念のため
		TabState fallback;
		fallback.panes[0].directory = start;
		fallback.panes[1].directory = start;
		tabs_.AddTab(fallback);
	}
	else if (tabs_.Current().panes[0].directory.IsEmpty() && tabs_.Current().panes[1].directory.IsEmpty()) {
		// ini にタブの記録が無かった場合 (TabManager の既定のまま)。
		// 旧来の LeftDir/RightDir から1本だけのタブを組み立てる
		TabState &cur = tabs_.MutableCurrent();
		const UnicodeString left_dir  = settings_.LeftDir;
		const UnicodeString right_dir = settings_.RightDir;
		cur.panes[0].directory = (!left_dir.IsEmpty()  && dir_exists(left_dir))  ? left_dir  : start;
		cur.panes[1].directory = (!right_dir.IsEmpty() && dir_exists(right_dir)) ? right_dir : start;
	}

	// タブの記録を両ペインへ適用する (初回なので record_history はどちらでも
	// 差が出ないが、ApplyTabState と同じ経路を通すため false のまま統一する)
	for (int i = 0; i < 2; ++i) {
		const PaneTabState &pane_state = tabs_.Current().panes[i];
		panes_[i]->SetSortSettings(pane_state.sort_key, pane_state.sort_descending, pane_state.dirs_first);
		panes_[i]->SetPath(!pane_state.directory.IsEmpty() && dir_exists(pane_state.directory)
		                        ? pane_state.directory : start,
		                    /*record_history=*/false);
	}
	RefreshTabBar();

	const WindowState &w = settings_.Window;
	SetSize(wxSize(w.width, w.height));
	if (w.left != -1 && w.top != -1) SetPosition(wxPoint(w.left, w.top));
	if (w.maximized) Maximize(true);
}

//---------------------------------------------------------------------------
/// 終了時: 現在のウィンドウ位置・タブを settings_ 経由で保存する
void MainFrame::SaveSettings()
{
	StoreCurrentTabState();
	tabs_.SaveToIni(settings_.Ini());

	// 後方互換 (本機能より前の _wx.ini を読む古いビルドとの橋渡し)。
	// タブが無い/未対応のビルドでも最後に開いていたディレクトリだけは復元できる
	settings_.LeftDir  = panes_[0]->GetPath();
	settings_.RightDir = panes_[1]->GetPath();

	settings_.Window.maximized = IsMaximized();
	// 最大化中は座標・サイズを更新しない (次回、通常サイズに戻したときの
	// 位置・サイズが復元できるようにするため)
	if (!settings_.Window.maximized) {
		const wxPoint pos  = GetPosition();
		const wxSize  size = GetSize();
		settings_.Window.left	 = pos.x;
		settings_.Window.top	 = pos.y;
		settings_.Window.width	 = size.x;
		settings_.Window.height = size.y;
	}

	settings_.Save();
}

//---------------------------------------------------------------------------
void MainFrame::SetActivePane(int index)
{
	active_ = (index == 0) ? 0 : 1;
	panes_[active_]->SetActive(true);
	panes_[1 - active_]->SetActive(false);
	panes_[active_]->SetFocus();
	UpdateStatus();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// 選択操作の受け渡し (判断は gui/selection.h の純関数が持つ。規約8)
//---------------------------------------------------------------------------
void MainFrame::SetStatusWarning(const UnicodeString &text)
{
	SetStatusText(to_wx(text), 0);
}

//---------------------------------------------------------------------------
bool MainFrame::ApplySelection(FilePane *pane, const std::function<void(std::vector<FileItem> &)> &fn)
{
	std::vector<FileItem> items = pane->VisibleItems();
	fn(items);
	pane->ApplyMarks(items);
	return selection::MarkedCount(items) > 0;
}

//---------------------------------------------------------------------------
void MainFrame::MarkCurrentAndMove(FilePane *pane, int delta)
{
	// VCL の SelectUpActionExecute (MainFrm.cpp:25031) と同じ順序。
	// 「今の位置を反転してから動く」なので、動いた先は反転されない
	pane->ToggleMarkNoMove();
	pane->MoveCursor(delta);
}

//---------------------------------------------------------------------------
void MainFrame::MarkBetween(FilePane *pane, int from, int to)
{
	std::vector<FileItem> items = pane->VisibleItems();
	// 移動元と移動先の**両方を含む**範囲を選択する。
	// selection::MarkRange は [from, to) なので、下限・上限を作って +1 する
	const int lo = (from < to)? from : to;
	const int hi = (from < to)? to : from;
	selection::MarkRange(items, lo, hi + 1);
	pane->ApplyMarks(items);
}

//---------------------------------------------------------------------------
void MainFrame::CmdMatchSelect()
{
	const wxString word = wxGetTextFromUser(
		to_wx(_T("名前に含む文字列を入力してください")), to_wx(_T("指定文字列を含むファイルを選択")),
		wxEmptyString, this);
	if (word.IsEmpty()) return;

	FilePane *pane = ActivePane();
	std::vector<FileItem> items = pane->VisibleItems();
	const int n = selection::SelectMatching(items, to_us(word));
	pane->ApplyMarks(items);

	if (n == 0) SetStatusWarning(_T("一致する項目がありません"));
	else SetStatusWarning(UnicodeString().sprintf(_T("%d 件を選択しました"), n));
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// 表示の切り替え (判断は gui/view_state.h の純関数が持つ。規約8)
//---------------------------------------------------------------------------
void MainFrame::SetBorderRatio(double ratio)
{
	border_ratio_ = view_state::ClampRatio(ratio);
	if (columns_ == nullptr) return;

	// sizer の比率は整数なので、100 分率にして割り当てる
	const int left = static_cast<int>(border_ratio_ * 100.0 + 0.5);
	columns_->GetItem(static_cast<size_t>(0))->SetProportion(left);
	columns_->GetItem(static_cast<size_t>(1))->SetProportion(100 - left);
	root_->Layout();
}

//---------------------------------------------------------------------------
void MainFrame::ToggleBothPanes(const std::function<void(FilePane *)> &fn, bool reload)
{
	// VCL も MAX_FILELIST を回して両方に効かせる (MainFrm.cpp:25904)
	for (int i = 0; i < 2; ++i) {
		fn(panes_[i]);
		if (reload) panes_[i]->Reload();
	}
}

//---------------------------------------------------------------------------
void MainFrame::CmdSwapLR()
{
	// VCL の SwapLRActionExecute (MainFrm.cpp:26531) は CurPath とカーソルを入れ替える
	const UnicodeString left = panes_[0]->GetPath();
	const UnicodeString right = panes_[1]->GetPath();
	const int csr0 = panes_[0]->GetCursor();
	const int csr1 = panes_[1]->GetCursor();

	panes_[0]->SetPath(right);
	panes_[1]->SetPath(left);
	panes_[0]->MoveCursorTo(csr1);
	panes_[1]->MoveCursorTo(csr0);
	UpdateStatus();  // ヘッダのパス表示もここで更新される
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// ディレクトリ移動 (判断は gui/navigation.h の純関数・クラスが持つ。規約8)
//---------------------------------------------------------------------------
void MainFrame::CmdToRoot()
{
	FilePane *pane = ActivePane();
	const UnicodeString root = get_drive_str(pane->GetPath());
	if (root.IsEmpty()) { SetStatusWarning(_T("ルートを特定できません")); return; }
	pane->SetPath(root);
	UpdateStatus();
}

//---------------------------------------------------------------------------
void MainFrame::CmdCopyPath(bool to_opp)
{
	// CurrToOpp: カレントのパスを反対側にも開く (MainFrm.cpp:16073)
	// CurrFromOpp: その逆
	const int cur = active_;
	const int opp = 1 - active_;
	const int from = to_opp? cur : opp;
	const int to = to_opp? opp : cur;

	panes_[to]->SetPath(panes_[from]->GetPath());
	UpdateStatus();
}

//---------------------------------------------------------------------------
void MainFrame::CmdCsrDirToOpp()
{
	FilePane *pane = ActivePane();
	const FileItem *itm = pane->GetCurrentItem();
	if (itm == nullptr || !itm->is_dir || itm->is_parent) {
		SetStatusWarning(_T("カーソル位置がディレクトリではありません"));
		return;
	}
	panes_[1 - active_]->SetPath(IncludeTrailingPathDelimiter(pane->GetPath()) + itm->name);
	UpdateStatus();
}

//---------------------------------------------------------------------------
void MainFrame::CmdToOppSameItem()
{
	FilePane *pane = ActivePane();
	const FileItem *itm = pane->GetCurrentItem();
	if (itm == nullptr) return;

	FilePane *opp = panes_[1 - active_];
	const std::vector<UnicodeString> names = opp->VisibleNames();
	for (std::size_t i = 0; i < names.size(); ++i) {
		if (SameText(names[i], itm->name)) {
			opp->MoveCursorTo(static_cast<int>(i));
			UpdateStatus();
			return;
		}
	}
	SetStatusWarning(_T("反対側に同名の項目がありません"));
}

//---------------------------------------------------------------------------
void MainFrame::CmdParentOn(int index)
{
	if (index < 0 || index > 1) return;
	panes_[index]->GoParent();
	UpdateStatus();
}

//---------------------------------------------------------------------------
void MainFrame::CmdCycleDrive(bool forward)
{
	std::unique_ptr<TStringList> drives(new TStringList());
	get_available_drive_list(drives.get());

	std::vector<UnicodeString> list;
	for (int i = 0; i < drives->Count; ++i) list.push_back(drives->Strings[i]);

	FilePane *pane = ActivePane();
	const UnicodeString next = NextDriveOf(list, get_drive_str(pane->GetPath()), forward);
	if (next.IsEmpty()) { SetStatusWarning(_T("利用可能なドライブがありません")); return; }

	if (!pane->SetPath(next)) SetStatusWarning(_T("ドライブを開けません: ") + next);
	UpdateStatus();
}

//---------------------------------------------------------------------------
void MainFrame::CmdPushDir()
{
	FilePane *pane = ActivePane();
	dir_stack_.Push(pane->GetPath(), pane->GetCursor());
	SetStatusWarning(UnicodeString().sprintf(_T("ディレクトリを積みました (%d 件)"), dir_stack_.Count()));
}

//---------------------------------------------------------------------------
void MainFrame::CmdPopDir()
{
	DirStack::Entry e;
	// 存在しなくなったディレクトリは DirStack::Pop が読み飛ばす
	if (!dir_stack_.Pop(e, [](const UnicodeString &p) { return dir_exists(p); })) {
		SetStatusWarning(_T("積んであるディレクトリがありません"));
		return;
	}
	FilePane *pane = ActivePane();
	pane->SetPath(e.path);
	pane->MoveCursorTo(e.cursor);
	UpdateStatus();
}

//---------------------------------------------------------------------------
void MainFrame::CmdShowDirStack()
{
	// VCL も空なら何も出さない (MainFrm.cpp:16780)
	if (dir_stack_.IsEmpty()) { SetStatusWarning(_T("積んであるディレクトリがありません")); return; }

	wxArrayString choices;
	for (const DirStack::Entry &e : dir_stack_.Entries()) choices.Add(to_wx(e.path));

	const int sel = wxGetSingleChoiceIndex(to_wx(_T("移動先を選んでください")),
	                                       to_wx(_T("ディレクトリ・スタック")), choices, this);
	if (sel < 0) return;

	FilePane *pane = ActivePane();
	pane->SetPath(dir_stack_.Entries()[static_cast<std::size_t>(sel)].path);
	UpdateStatus();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// タブ操作 (判断は gui/tabs.h の TabManager が持つ。規約8)
//---------------------------------------------------------------------------
void MainFrame::CmdMoveTab(int direction)
{
	StoreCurrentTabState();
	if (!tabs_.MoveCurrentTab(direction)) { SetStatusWarning(_T("タブが1枚しかありません")); return; }
	RefreshTabBar();
}

//---------------------------------------------------------------------------
void MainFrame::CmdSoloTab()
{
	StoreCurrentTabState();
	const int n = tabs_.CloseOtherTabs();
	if (n == 0) { SetStatusWarning(_T("タブが1枚しかありません")); return; }
	RefreshTabBar();
	SetStatusWarning(UnicodeString().sprintf(_T("%d 枚のタブを閉じました"), n));
}

//---------------------------------------------------------------------------
void MainFrame::CmdTabHome(bool all)
{
	StoreCurrentTabState();
	const int n = tabs_.GoHome(all);
	if (n == 0) { SetStatusWarning(_T("ホームが設定されていません")); return; }
	ApplyTabState(tabs_.Current());
	RefreshTabBar();
}

//---------------------------------------------------------------------------
void MainFrame::CmdToTab()
{
	// VCL は ActionParam を受けるが、Phase 2 骨格にはコマンドへ引数を渡す
	// 仕組みが無いので、番号かキャプションを訊く形にした
	const wxString param = wxGetTextFromUser(
		to_wx(_T("タブの番号 (1 起点) またはキャプションを入力してください")),
		to_wx(_T("指定のタブへ")), wxEmptyString, this);
	if (param.IsEmpty()) return;

	StoreCurrentTabState();
	if (!tabs_.SelectByParam(to_us(param))) {
		SetStatusWarning(_T("該当するタブがありません: ") + to_us(param));
		return;
	}
	ApplyTabState(tabs_.Current());
	RefreshTabBar();
}

//---------------------------------------------------------------------------
void MainFrame::CmdSubDirList()
{
	FilePane *pane = ActivePane();
	const UnicodeString base = IncludeTrailingPathDelimiter(pane->GetPath());

	wxArrayString choices;
	std::vector<UnicodeString> dirs;
	TSearchRec sr;
	if (FindFirst(base + "*", faDirectory, sr) == 0) {
		do {
			if (SameStr(sr.Name, ".") || SameStr(sr.Name, "..")) continue;
			if ((sr.Attr & faDirectory) == 0) continue;
			dirs.push_back(sr.Name);
			choices.Add(to_wx(sr.Name));
		} while (FindNext(sr) == 0);
		FindClose(sr);
	}

	if (dirs.empty()) { SetStatusWarning(_T("サブディレクトリがありません")); return; }

	const int sel = wxGetSingleChoiceIndex(to_wx(_T("移動先を選んでください")),
	                                       to_wx(_T("サブディレクトリ一覧")), choices, this);
	if (sel < 0) return;
	pane->SetPath(base + dirs[static_cast<std::size_t>(sel)]);
	UpdateStatus();
}

//---------------------------------------------------------------------------
void MainFrame::CmdSpecialDirList()
{
	// 移植済みの get_SpecialFolder() (usr_file_ex.h) が Windows の
	// 既知フォルダを引く。VCL 版の SpecialDirList と同じ顔ぶれに寄せた
	struct Entry { const wchar_t *label; int csidl; };
	static const Entry kEntries[] = {
		{L"デスクトップ", CSIDL_DESKTOPDIRECTORY},
		{L"ドキュメント", CSIDL_PERSONAL},
		{L"ダウンロード", -1},  // CSIDL には無いのでプロファイル配下から作る
		{L"ピクチャ", CSIDL_MYPICTURES},
		{L"ミュージック", CSIDL_MYMUSIC},
		{L"ビデオ", CSIDL_MYVIDEO},
		{L"アプリケーション データ", CSIDL_APPDATA},
		{L"プログラム ファイル", CSIDL_PROGRAM_FILES},
		{L"Windows", CSIDL_WINDOWS},
		{L"システム", CSIDL_SYSTEM},
	};

	wxArrayString choices;
	std::vector<UnicodeString> paths;
	for (const Entry &e : kEntries) {
		UnicodeString path;
		if (e.csidl == -1) {
			const UnicodeString prof = GetEnvironmentVariable(_T("USERPROFILE"));
			if (!prof.IsEmpty()) path = IncludeTrailingPathDelimiter(prof) + _T("Downloads");
		}
		else {
			wchar_t buf[MAX_PATH] = {};
			if (SUCCEEDED(::SHGetFolderPathW(NULL, e.csidl, NULL, 0, buf))) path = buf;
		}
		if (path.IsEmpty() || !dir_exists(path)) continue;

		paths.push_back(path);
		choices.Add(to_wx(UnicodeString(e.label) + _T("  ") + path));
	}

	if (paths.empty()) { SetStatusWarning(_T("特殊フォルダを取得できません")); return; }

	const int sel = wxGetSingleChoiceIndex(to_wx(_T("移動先を選んでください")),
	                                       to_wx(_T("特殊フォルダ一覧")), choices, this);
	if (sel < 0) return;
	ActivePane()->SetPath(paths[static_cast<std::size_t>(sel)]);
	UpdateStatus();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// ファイル操作 (機能群5)
//---------------------------------------------------------------------------
void MainFrame::CmdCopyMoveTo(bool move)
{
	FilePane *pane = ActivePane();
	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	const UnicodeString verb = move? _T("移動") : _T("コピー");
	if (names.empty()) { SetStatusWarning(verb + _T("対象がありません")); return; }

	const wxString input = wxGetTextFromUser(
		to_wx(verb + _T("先のディレクトリを入力してください")), to_wx(verb + _T("先の指定")),
		to_wx(OppositePane()->GetPath()), this);
	if (input.IsEmpty()) return;

	UnicodeString dst;
	if (!ResolveDirectoryInput(to_us(input), pane->GetPath(), dst)) {
		wxMessageBox(to_wx(_T("ディレクトリが見つかりません: ") + to_us(input)),
		             to_wx(verb), wxOK | wxICON_WARNING, this);
		return;
	}

	// 自分自身や配下への操作を弾く (規約: 破壊的な機能を足すとき)。
	// file_ops 側でも見るが、確認ダイアログを出す前に落としたい
	for (const UnicodeString &name : names) {
		if (file_ops::IsSameOrInside(pane->GetPath() + name, dst)) {
			wxMessageBox(to_wx(_T("自分自身または配下のディレクトリへは") + verb + _T("できません: ") + name),
			             to_wx(verb), wxOK | wxICON_WARNING, this);
			return;
		}
	}

	if (!ConfirmItems(this, verb, verb, names, dst)) return;

	std::vector<UnicodeString> paths;
	for (const UnicodeString &name : names) paths.push_back(pane->GetPath() + name);

	const file_ops::FileOpResult result = move? file_ops::MoveItems(paths, dst)
	                                          : file_ops::CopyItems(paths, dst);
	panes_[0]->Reload();
	panes_[1]->Reload();
	wxMessageBox(to_wx(file_ops::Summarize(result)), to_wx(verb + _T("の結果")),
	             wxOK | wxICON_INFORMATION, this);
	UpdateStatus();
}

//---------------------------------------------------------------------------
void MainFrame::CmdChangeNameCase(file_ops::NameCase how)
{
	FilePane *pane = ActivePane();
	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	const UnicodeString verb = (how == file_ops::NameCase::Upper)? _T("大文字化") : _T("小文字化");
	if (names.empty()) { SetStatusWarning(verb + _T("の対象がありません")); return; }

	// 改名も元に戻しにくい操作なので確認する (規約: 破壊的操作の前に必ず確認)
	if (!ConfirmItems(this, verb, verb, names, pane->GetPath())) return;

	const file_ops::FileOpResult result = file_ops::ChangeNameCase(pane->GetPath(), names, how);
	pane->Reload();
	wxMessageBox(to_wx(file_ops::Summarize(result)), to_wx(verb + _T("の結果")),
	             wxOK | wxICON_INFORMATION, this);
	UpdateStatus();
}

//---------------------------------------------------------------------------
void MainFrame::CmdCopyFileName(bool full_path)
{
	FilePane *pane = ActivePane();
	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	if (names.empty()) { SetStatusWarning(_T("対象がありません")); return; }

	std::vector<UnicodeString> paths;
	for (const UnicodeString &name : names) paths.push_back(pane->GetPath() + name);

	const UnicodeString text = file_ops::FormatFileNames(paths, full_path);
	if (wxTheClipboard->Open()) {
		wxTheClipboard->SetData(new wxTextDataObject(to_wx(text)));
		wxTheClipboard->Close();
		SetStatusWarning(UnicodeString().sprintf(_T("%d 件の名前をコピーしました"),
		                                         static_cast<int>(names.size())));
	}
	else {
		SetStatusWarning(_T("クリップボードを開けません"));
	}
}

//---------------------------------------------------------------------------
void MainFrame::CmdNewFile()
{
	FilePane *pane = ActivePane();
	const wxString input = wxGetTextFromUser(to_wx(_T("作成するファイル名を入力してください")),
	                                          to_wx(_T("新規ファイルの作成")), wxEmptyString, this);
	if (input.IsEmpty()) return;

	const UnicodeString path = IncludeTrailingPathDelimiter(pane->GetPath()) + to_us(input);
	if (file_exists(path) || dir_exists(path)) {
		// 既存があれば上書きしない (規約: 上書きを既定にしない)
		wxMessageBox(to_wx(_T("同名のファイルまたはディレクトリが既にあります")),
		             to_wx(_T("新規ファイルの作成")), wxOK | wxICON_WARNING, this);
		return;
	}

	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW,
	                         FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		wxMessageBox(to_wx(_T("作成できませんでした: ") + to_us(input)),
		             to_wx(_T("新規ファイルの作成")), wxOK | wxICON_ERROR, this);
		return;
	}
	::CloseHandle(h);

	pane->Reload();
	// 作ったファイルにカーソルを合わせる
	const std::vector<UnicodeString> names = pane->VisibleNames();
	for (std::size_t i = 0; i < names.size(); ++i) {
		if (SameText(names[i], to_us(input))) { pane->MoveCursorTo(static_cast<int>(i)); break; }
	}
	UpdateStatus();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// クリップボード経由のファイル操作
//
// エクスプローラと相互運用するため、Win32 の CF_HDROP と
// CFSTR_PREFERREDDROPEFFECT をそのまま扱う。wx の wxFileDataObject は
// CF_HDROP は載せられるが「コピーか切り取りか」を載せられないため、
// ここだけ Win32 API を直接使う (VCL 版も同じ2つの形式を使っている。
// MainFrm.cpp:28677-28704)。
//---------------------------------------------------------------------------
namespace {

/// CF_HDROP を作る (DROPFILES + ワイド文字のパス列 + 終端の二重 NUL)
HGLOBAL make_hdrop(const std::vector<UnicodeString> &paths)
{
	std::size_t chars = 1;  // 終端の余分な NUL
	for (const UnicodeString &p : paths) chars += static_cast<std::size_t>(p.Length()) + 1;

	const std::size_t bytes = sizeof(DROPFILES) + chars * sizeof(wchar_t);
	HGLOBAL h = ::GlobalAlloc(GHND, bytes);
	if (h == NULL) return NULL;

	BYTE *base = static_cast<BYTE *>(::GlobalLock(h));
	DROPFILES *df = reinterpret_cast<DROPFILES *>(base);
	df->pFiles = sizeof(DROPFILES);
	df->fWide = TRUE;

	wchar_t *w = reinterpret_cast<wchar_t *>(base + sizeof(DROPFILES));
	for (const UnicodeString &p : paths) {
		::wcscpy(w, p.c_str());
		w += p.Length() + 1;
	}
	*w = L'\0';  // 二重 NUL で終端

	::GlobalUnlock(h);
	return h;
}

/// DWORD ひとつだけのグローバルメモリ (Preferred DropEffect 用)
HGLOBAL make_dword(DWORD value)
{
	HGLOBAL h = ::GlobalAlloc(GHND, sizeof(DWORD));
	if (h == NULL) return NULL;
	*static_cast<DWORD *>(::GlobalLock(h)) = value;
	::GlobalUnlock(h);
	return h;
}

}  // namespace

//---------------------------------------------------------------------------
void MainFrame::CmdFilesToClip(bool cut)
{
	FilePane *pane = ActivePane();
	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	if (names.empty()) { SetStatusWarning(_T("対象がありません")); return; }

	std::vector<UnicodeString> paths;
	for (const UnicodeString &name : names) paths.push_back(pane->GetPath() + name);

	if (!::OpenClipboard(static_cast<HWND>(GetHandle()))) {
		SetStatusWarning(_T("クリップボードを開けません"));
		return;
	}
	::EmptyClipboard();

	HGLOBAL hdrop = make_hdrop(paths);
	if (hdrop != NULL) ::SetClipboardData(CF_HDROP, hdrop);

	// VCL と同じ形式で「コピーか切り取りか」を載せる
	const UINT fmt = ::RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
	HGLOBAL heff = make_dword(cut? DROPEFFECT_MOVE : DROPEFFECT_COPY);
	if (fmt != 0 && heff != NULL) ::SetClipboardData(fmt, heff);

	::CloseClipboard();
	SetStatusWarning(UnicodeString().sprintf(cut? _T("%d 件を切り取りました") : _T("%d 件をコピーしました"),
	                                         static_cast<int>(paths.size())));
}

//---------------------------------------------------------------------------
void MainFrame::CmdPaste()
{
	if (!::IsClipboardFormatAvailable(CF_HDROP)) {
		SetStatusWarning(_T("クリップボードにファイルがありません"));
		return;
	}
	if (!::OpenClipboard(static_cast<HWND>(GetHandle()))) {
		SetStatusWarning(_T("クリップボードを開けません"));
		return;
	}

	std::vector<UnicodeString> paths;
	bool is_move = false;
	{
		HDROP dp = static_cast<HDROP>(::GetClipboardData(CF_HDROP));
		if (dp != NULL) {
			const UINT count = ::DragQueryFileW(dp, 0xFFFFFFFF, NULL, 0);
			for (UINT i = 0; i < count; ++i) {
				wchar_t buf[MAX_PATH * 2] = {};
				if (::DragQueryFileW(dp, i, buf, MAX_PATH * 2) > 0) paths.push_back(UnicodeString(buf));
			}
		}
		const UINT fmt = ::RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
		if (fmt != 0) {
			const DWORD *ep = static_cast<const DWORD *>(::GetClipboardData(fmt));
			if (ep != NULL) is_move = clipboard_files::IsMoveEffect(*ep);
		}
	}
	::CloseClipboard();

	if (paths.empty()) { SetStatusWarning(_T("クリップボードにファイルがありません")); return; }

	FilePane *pane = ActivePane();
	const UnicodeString dst = pane->GetPath();
	const UnicodeString verb = is_move? _T("移動") : _T("コピー");

	// 弾いた分は黙って捨てず、必ず見せる (規約: 破壊的操作)
	const clipboard_files::PasteCheck check = clipboard_files::ValidatePasteTargets(paths, dst);
	if (!check.rejected.empty()) {
		UnicodeString msg = _T("次の項目は貼り付けられません:\r\n");
		for (const UnicodeString &r : check.rejected) msg += _T("  ") + r + _T("\r\n");
		if (check.accepted.empty()) {
			wxMessageBox(to_wx(msg), to_wx(_T("貼り付け")), wxOK | wxICON_WARNING, this);
			return;
		}
		msg += _T("\r\n残りを") + verb + _T("しますか?");
		if (wxMessageBox(to_wx(msg), to_wx(_T("貼り付け")), wxYES_NO | wxICON_WARNING, this) != wxYES) return;
	}

	std::vector<UnicodeString> names;
	for (const UnicodeString &p : check.accepted) names.push_back(ExtractFileName(ExcludeTrailingPathDelimiter(p)));
	if (!ConfirmItems(this, _T("貼り付け"), verb, names, dst)) return;

	const file_ops::FileOpResult result = is_move? file_ops::MoveItems(check.accepted, dst)
	                                             : file_ops::CopyItems(check.accepted, dst);
	panes_[0]->Reload();
	panes_[1]->Reload();
	wxMessageBox(to_wx(file_ops::Summarize(result)), to_wx(_T("貼り付けの結果")),
	             wxOK | wxICON_INFORMATION, this);
	UpdateStatus();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// リンク・属性 (機能群6)
//---------------------------------------------------------------------------
void MainFrame::CmdCreateLinks(links::LinkKind kind)
{
	FilePane *pane = ActivePane();
	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	const UnicodeString verb = (kind == links::LinkKind::Shortcut)? _T("ショートカット")
	                         : (kind == links::LinkKind::Hard)?     _T("ハードリンク")
	                                                              : _T("シンボリックリンク");
	if (names.empty()) { SetStatusWarning(verb + _T("の対象がありません")); return; }

	// VCL と同じく**反対ペインのディレクトリ**に作る (MainFrm.cpp:15873)
	const UnicodeString dst = OppositePane()->GetPath();

	if (kind == links::LinkKind::Hard) {
		// 同一ボリュームかつ NTFS でなければ作れない (MainFrm.cpp:15707)
		wchar_t fs[32] = {};
		const UnicodeString dst_root = get_drive_str(dst);
		::GetVolumeInformationW(dst_root.c_str(), NULL, 0, NULL, NULL, NULL, fs, 32);
		if (!links::CanCreateHardLink(get_drive_str(pane->GetPath()), dst_root, UnicodeString(fs))) {
			wxMessageBox(to_wx(_T("ハードリンクは同じボリュームの NTFS 上にしか作れません")),
			             to_wx(verb), wxOK | wxICON_WARNING, this);
			return;
		}
	}

	if (!ConfirmItems(this, verb + _T("の作成"), verb + _T("を作成")   , names, dst)) return;

	std::vector<UnicodeString> paths;
	for (const UnicodeString &name : names) paths.push_back(pane->GetPath() + name);

	const file_ops::FileOpResult result = links::CreateLinks(paths, dst, kind);
	OppositePane()->Reload();
	wxMessageBox(to_wx(file_ops::Summarize(result)), to_wx(verb + _T("の結果")),
	             wxOK | wxICON_INFORMATION, this);
	UpdateStatus();
}

//---------------------------------------------------------------------------
void MainFrame::CmdSetDirTime()
{
	FilePane *pane = ActivePane();
	const std::vector<UnicodeString> names = pane->GetSelectedNames();

	std::vector<UnicodeString> dirs;
	for (const UnicodeString &name : names) {
		const UnicodeString p = pane->GetPath() + name;
		if (dir_exists(p)) dirs.push_back(p);
	}
	if (dirs.empty()) { SetStatusWarning(_T("対象のディレクトリがありません")); return; }

	// タイムスタンプの変更も元に戻せないので確認する
	if (!ConfirmItems(this, _T("タイムスタンプの変更"), _T("配下の最新に合わせ"),
	                  names, pane->GetPath())) return;

	int done = 0;
	for (const UnicodeString &d : dirs) {
		// 表示の設定 (隠し/システム) をそのまま渡す。VCL も同じものを見る
		if (static_cast<double>(links::SetDirTimeRecursive(d, pane->GetShowHidden(),
		                                                   pane->GetShowSystem())) > 0.0) done++;
	}
	pane->Reload();
	SetStatusWarning(UnicodeString().sprintf(_T("%d 件のディレクトリの日時を変更しました"), done));
	UpdateStatus();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// 書庫 (機能群7)
//
// 実体は移植済みの src/usr_arc.cpp。**外部の書庫 DLL** (7-zip32.dll など) を
// 読むので、入っていない環境では「利用できない」と理由を出して中止する。
// VCL 版も同じ前提。
//---------------------------------------------------------------------------
void MainFrame::CmdListArchive()
{
	FilePane *pane = ActivePane();
	const FileItem *itm = pane->GetCurrentItem();
	if (itm == nullptr || itm->is_dir) { SetStatusWarning(_T("カーソル位置が書庫ではありません")); return; }

	const UnicodeString path = pane->GetPath() + itm->name;
	std::vector<archive::Entry> entries;
	UnicodeString error;
	if (!archive::ListEntries(path, entries, error)) {
		wxMessageBox(to_wx(error), to_wx(_T("書庫の一覧")), wxOK | wxICON_WARNING, this);
		return;
	}

	UnicodeString text;
	text.sprintf(_T("%s  (%d 項目)\r\n\r\n"), itm->name.c_str(), static_cast<int>(entries.size()));
	int shown = 0;
	for (const archive::Entry &e : entries) {
		if (shown++ >= 200) { text += _T("...\r\n(以下省略)\r\n"); break; }
		text += _T("  ") + e.name + _T("\r\n");
	}
	wxMessageBox(to_wx(text), to_wx(_T("書庫の一覧")), wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
void MainFrame::CmdTestArchive()
{
	FilePane *pane = ActivePane();
	const FileItem *itm = pane->GetCurrentItem();
	if (itm == nullptr || itm->is_dir) { SetStatusWarning(_T("カーソル位置が書庫ではありません")); return; }

	UnicodeString error;
	if (archive::TestArchive(pane->GetPath() + itm->name, error)) {
		wxMessageBox(to_wx(_T("問題は見つかりませんでした")), to_wx(_T("書庫の検査")),
		             wxOK | wxICON_INFORMATION, this);
	}
	else {
		wxMessageBox(to_wx(error), to_wx(_T("書庫の検査")), wxOK | wxICON_WARNING, this);
	}
}

//---------------------------------------------------------------------------
void MainFrame::CmdUnPack(bool to_current)
{
	FilePane *pane = ActivePane();
	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	if (names.empty()) { SetStatusWarning(_T("展開する書庫がありません")); return; }

	// VCL と同じく既定は反対ペイン、ToCurrent 付きならカレント
	const UnicodeString dst = to_current? pane->GetPath() : OppositePane()->GetPath();
	if (!ConfirmItems(this, _T("展開"), _T("展開"), names, dst)) return;

	int ok = 0;
	std::vector<UnicodeString> failures;
	for (const UnicodeString &name : names) {
		UnicodeString error;
		if (archive::Extract(pane->GetPath() + name, dst, error)) ok++;
		else failures.push_back(name + _T(": ") + error);
	}

	panes_[0]->Reload();
	panes_[1]->Reload();

	UnicodeString msg;
	msg.sprintf(_T("%d 件を展開しました"), ok);
	for (const UnicodeString &f : failures) msg += _T("\r\n") + f;
	wxMessageBox(to_wx(msg), to_wx(_T("展開の結果")), wxOK | wxICON_INFORMATION, this);
	UpdateStatus();
}

//---------------------------------------------------------------------------
void MainFrame::CmdPack(bool to_current)
{
	FilePane *pane = ActivePane();
	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	if (names.empty()) { SetStatusWarning(_T("書庫に詰めるものがありません")); return; }

	const FileItem *cur = pane->GetCurrentItem();
	const UnicodeString base = archive::DefaultArchiveBaseName(
		(cur != nullptr && !cur->is_parent)? cur->name : EmptyStr, names);

	const wxString input = wxGetTextFromUser(
		to_wx(_T("作る書庫の名前を入力してください (拡張子で形式が決まります)")),
		to_wx(_T("書庫の作成")), to_wx(base + _T(".zip")), this);
	if (input.IsEmpty()) return;

	const UnicodeString dst_dir = to_current? pane->GetPath() : OppositePane()->GetPath();
	const UnicodeString arc = IncludeTrailingPathDelimiter(dst_dir) + to_us(input);

	if (!ConfirmItems(this, _T("書庫の作成"), _T("書庫に追加"), names, arc)) return;

	UnicodeString error;
	if (!archive::Create(arc, pane->GetPath(), names, error)) {
		wxMessageBox(to_wx(error), to_wx(_T("書庫の作成")), wxOK | wxICON_WARNING, this);
		return;
	}

	panes_[0]->Reload();
	panes_[1]->Reload();
	SetStatusWarning(_T("書庫を作成しました: ") + to_us(input));
	UpdateStatus();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// 比較・ハッシュ (機能群8)
//
// ハッシュの計算は移植済みの get_HashStr() (src/usr_file_inf.h) が持つ。
// ここは「どう比べるか」(gui/compare.h) と受け渡しだけ。
//---------------------------------------------------------------------------
namespace {

/// 既定のハッシュ方式。VCL も既定は MD5 (MainFrm.cpp:14804 ほか)
const wchar_t *const kDefaultHashId = L"MD5";

}  // namespace

void MainFrame::CmdGetHash()
{
	FilePane *pane = ActivePane();
	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	if (names.empty()) { SetStatusWarning(_T("対象がありません")); return; }

	UnicodeString text;
	for (const UnicodeString &name : names) {
		const UnicodeString p = pane->GetPath() + name;
		if (dir_exists(p)) continue;  // ディレクトリは対象外
		const UnicodeString h = get_HashStr(p, UnicodeString(kDefaultHashId));
		text += name + _T("\r\n  ") + (h.IsEmpty()? _T("(取得できません)") : h) + _T("\r\n");
	}
	if (text.IsEmpty()) { SetStatusWarning(_T("対象のファイルがありません")); return; }

	wxMessageBox(to_wx(text), to_wx(UnicodeString(kDefaultHashId) + _T(" ハッシュ値")),
	             wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
void MainFrame::CmdCompareHash()
{
	FilePane *pane = ActivePane();
	const FileItem *itm = pane->GetCurrentItem();
	if (itm == nullptr || itm->is_dir) { SetStatusWarning(_T("カーソル位置がファイルではありません")); return; }

	const UnicodeString here = pane->GetPath() + itm->name;
	const UnicodeString there = OppositePane()->GetPath() + itm->name;
	if (!file_exists(there)) {
		wxMessageBox(to_wx(_T("反対側に同名のファイルがありません: ") + itm->name),
		             to_wx(_T("ハッシュの比較")), wxOK | wxICON_WARNING, this);
		return;
	}

	const UnicodeString a = get_HashStr(here, UnicodeString(kDefaultHashId));
	const UnicodeString b = get_HashStr(there, UnicodeString(kDefaultHashId));
	if (a.IsEmpty() || b.IsEmpty()) {
		wxMessageBox(to_wx(_T("ハッシュ値を取得できません")), to_wx(_T("ハッシュの比較")),
		             wxOK | wxICON_WARNING, this);
		return;
	}

	UnicodeString msg = itm->name + _T("\r\n\r\n  ") + a + _T("\r\n  ") + b + _T("\r\n\r\n");
	msg += SameText(a, b)? _T("一致しました") : _T("**一致しません**");
	wxMessageBox(to_wx(msg), to_wx(_T("ハッシュの比較")), wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
void MainFrame::CmdSelOnlyCur()
{
	FilePane *pane = ActivePane();
	std::vector<FileItem> here = pane->VisibleItems();
	const std::vector<FileItem> there = OppositePane()->VisibleItems();

	const std::vector<int> idx = compare::IndicesOnlyHere(here, there, compare::MatchBy::Name);
	for (FileItem &it : here) it.marked = false;
	for (int i : idx) here[static_cast<std::size_t>(i)].marked = true;
	pane->ApplyMarks(here);

	SetStatusWarning(UnicodeString().sprintf(_T("カレント側だけにある %d 件を選択しました"),
	                                         static_cast<int>(idx.size())));
}

//---------------------------------------------------------------------------
void MainFrame::CmdToOppSameHash()
{
	FilePane *pane = ActivePane();
	const FileItem *itm = pane->GetCurrentItem();
	if (itm == nullptr || itm->is_dir) { SetStatusWarning(_T("カーソル位置がファイルではありません")); return; }

	const UnicodeString target = get_HashStr(pane->GetPath() + itm->name,
	                                          UnicodeString(kDefaultHashId));
	if (target.IsEmpty()) { SetStatusWarning(_T("ハッシュ値を取得できません")); return; }

	FilePane *opp = OppositePane();
	const std::vector<FileItem> items = opp->VisibleItems();
	for (std::size_t i = 0; i < items.size(); ++i) {
		if (items[i].is_dir || items[i].is_parent) continue;
		// サイズが違えば計算するまでもない (大きな一覧で全部計算しないための枝刈り)
		if (items[i].size != itm->size) continue;

		const UnicodeString h = get_HashStr(opp->GetPath() + items[i].name,
		                                     UnicodeString(kDefaultHashId));
		if (SameText(h, target)) {
			opp->MoveCursorTo(static_cast<int>(i));
			UpdateStatus();
			return;
		}
	}
	SetStatusWarning(_T("反対側に同じ内容のファイルがありません"));
}

//---------------------------------------------------------------------------
void MainFrame::CmdDiffDir()
{
	const std::vector<compare::DiffRow> rows = compare::DiffDirectories(
		panes_[0]->VisibleItems(), panes_[1]->VisibleItems(), compare::MatchBy::NameSize);

	if (rows.empty()) {
		wxMessageBox(to_wx(_T("違いは見つかりませんでした (名前とサイズで比較)")),
		             to_wx(_T("ディレクトリの比較")), wxOK | wxICON_INFORMATION, this);
		return;
	}

	UnicodeString text = _T("名前とサイズで比較しました\r\n\r\n");
	int shown = 0;
	for (const compare::DiffRow &r : rows) {
		if (shown++ >= 200) { text += _T("...\r\n(以下省略)\r\n"); break; }
		const UnicodeString mark = r.differs? _T("[異] ") : (r.in_left? _T("[左] ") : _T("[右] "));
		text += mark + r.name + _T("\r\n");
	}
	wxMessageBox(to_wx(text), to_wx(_T("ディレクトリの比較")), wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// テキスト操作 (機能群9)
//---------------------------------------------------------------------------
void MainFrame::CmdCountLines()
{
	FilePane *pane = ActivePane();
	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	if (names.empty()) { SetStatusWarning(_T("対象がありません")); return; }

	UnicodeString text = _T("ファイル                        全行     空白   空白以外\r\n");
	int t_total = 0, t_blank = 0;
	int counted = 0;

	for (const UnicodeString &name : names) {
		const UnicodeString p = pane->GetPath() + name;
		if (dir_exists(p)) continue;

		const text_viewer_core::LoadResult r = text_viewer_core::LoadForView(p);
		if (!r.ok || r.is_binary) {
			text += name + _T("  (テキストではありません)\r\n");
			continue;
		}
		const text_ops::LineStats st = text_ops::CountLines(r.lines);
		text.cat_sprintf(_T("%-28s %8d %8d %8d\r\n"), name.c_str(), st.total, st.blank, st.non_blank);
		t_total += st.total;
		t_blank += st.blank;
		counted++;
	}

	if (counted == 0) { SetStatusWarning(_T("テキストファイルがありません")); return; }
	text.cat_sprintf(_T("\r\n合計 (%d ファイル) %8d %8d %8d\r\n"),
	                 counted, t_total, t_blank, t_total - t_blank);
	// VCL はコメント行も分けて数えるが、その定義はユーザ設定 (UserHighlight) 側に
	// あり、まだリンク対象に入っていない。**内訳を出さないことを明示する**
	text += _T("\r\n(コメント行の内訳は未対応)");

	wxMessageBox(to_wx(text), to_wx(_T("行数のカウント")), wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
void MainFrame::CmdJoinText()
{
	FilePane *pane = ActivePane();
	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	if (names.size() < 2) { SetStatusWarning(_T("2件以上を選んでください")); return; }

	const wxString input = wxGetTextFromUser(
		to_wx(_T("出力するファイル名を入力してください (UTF-8 で書きます)")),
		to_wx(_T("テキストの結合")), to_wx(_T("joined.txt")), this);
	if (input.IsEmpty()) return;

	const UnicodeString out = IncludeTrailingPathDelimiter(pane->GetPath()) + to_us(input);
	if (!ConfirmItems(this, _T("テキストの結合"), _T("結合"), names, out)) return;

	std::vector<UnicodeString> paths;
	for (const UnicodeString &name : names) paths.push_back(pane->GetPath() + name);

	const text_ops::JoinResult r = text_ops::JoinTextFiles(paths, out);
	pane->Reload();

	UnicodeString msg;
	msg.sprintf(_T("%d 件を結合しました"), r.joined);
	for (const UnicodeString &f : r.failures) msg += _T("\r\n") + f;
	wxMessageBox(to_wx(msg), to_wx(_T("テキストの結合")), wxOK | wxICON_INFORMATION, this);
	UpdateStatus();
}

//---------------------------------------------------------------------------
void MainFrame::CmdConvertTextEnc()
{
	FilePane *pane = ActivePane();
	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	if (names.empty()) { SetStatusWarning(_T("対象がありません")); return; }

	wxArrayString choices;
	choices.Add(to_wx(_T("UTF-8 (BOM 無し)")));
	choices.Add(to_wx(_T("UTF-8 (BOM 付き)")));
	choices.Add(to_wx(_T("Shift_JIS")));
	const int sel = wxGetSingleChoiceIndex(to_wx(_T("変換先の文字コードを選んでください")),
	                                       to_wx(_T("文字コードの変換")), choices, this);
	if (sel < 0) return;

	const int cp = (sel == 2)? 932 : CP_UTF8;
	const bool bom = (sel == 1);

	// **その場で書き換える破壊的な操作**なので必ず確認する
	if (!ConfirmItems(this, _T("文字コードの変換"), _T("変換"), names, pane->GetPath())) return;

	int ok = 0;
	std::vector<UnicodeString> failures;
	for (const UnicodeString &name : names) {
		const UnicodeString p = pane->GetPath() + name;
		if (dir_exists(p)) continue;
		UnicodeString error;
		if (text_ops::ConvertEncoding(p, cp, bom, error)) ok++;
		else failures.push_back(name + _T(": ") + error);
	}

	pane->Reload();
	UnicodeString msg;
	msg.sprintf(_T("%d 件を変換しました"), ok);
	for (const UnicodeString &f : failures) msg += _T("\r\n") + f;
	wxMessageBox(to_wx(msg), to_wx(_T("文字コードの変換")), wxOK | wxICON_INFORMATION, this);
	UpdateStatus();
}

//---------------------------------------------------------------------------
void MainFrame::CmdListFileName()
{
	FilePane *pane = ActivePane();
	const std::vector<FileItem> items = pane->VisibleItems();

	UnicodeString text;
	int shown = 0;
	for (const FileItem &it : items) {
		if (it.is_parent) continue;
		if (shown++ >= 500) { text += _T("...\r\n(以下省略)\r\n"); break; }
		text += it.name + (it.is_dir? _T("\\") : EmptyStr) + _T("\r\n");
	}
	if (text.IsEmpty()) { SetStatusWarning(_T("項目がありません")); return; }

	wxMessageBox(to_wx(text), to_wx(_T("ファイル名の一覧")), wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// 外部連携 (機能群10)
//---------------------------------------------------------------------------
namespace {

/// LaunchSpec のとおりに起動する
bool launch(const external::LaunchSpec &spec, HWND owner)
{
	const HINSTANCE r = ::ShellExecuteW(
		owner, L"open", spec.file.c_str(),
		spec.parameters.IsEmpty()? NULL : spec.parameters.c_str(),
		spec.directory.IsEmpty()? NULL : spec.directory.c_str(), SW_SHOWNORMAL);
	// ShellExecute は成功時に 32 より大きい値を返す
	return reinterpret_cast<INT_PTR>(r) > 32;
}

}  // namespace

void MainFrame::CmdLaunchShell(external::ShellKind kind)
{
	const external::LaunchSpec spec = external::ShellLaunchSpec(kind, ActivePane()->GetPath());
	if (!launch(spec, static_cast<HWND>(GetHandle()))) {
		SetStatusWarning(_T("起動できません: ") + spec.file);
	}
}

//---------------------------------------------------------------------------
void MainFrame::CmdOpenByExplorer()
{
	FilePane *pane = ActivePane();
	const FileItem *itm = pane->GetCurrentItem();

	// カーソルが有効ならその項目を選択した状態で、そうでなければカレントを開く
	const bool has_item = (itm != nullptr && !itm->is_parent);
	const UnicodeString path = has_item? (pane->GetPath() + itm->name) : pane->GetPath();
	const external::LaunchSpec spec =
		external::ExplorerLaunchSpec(path, has_item? itm->is_dir : true);

	if (!launch(spec, static_cast<HWND>(GetHandle()))) {
		SetStatusWarning(_T("エクスプローラを開けません"));
	}
}

//---------------------------------------------------------------------------
void MainFrame::CmdContextMenu()
{
	// **移植済みの UserShell::ShowContextMenu を使えていない。**
	// src/usr_shell.cpp はコンパイルは通るが、同じファイルにあるアイコン取得の
	// 経路が Graphics::TIcon::SetSize / TGraphic::LoadFromFile / TBitmap::SetHandle
	// を呼んでおり、それらは規約4 に従って宣言のみ。--gc-sections はシンボル解決の
	// 後に走るのでリンクエラーになる (CLAUDE.md 規約4 の注意書き)。
	// アイコン処理を実装したら、こちらを消して UserShell に寄せること (報告書 §20)。
	FilePane *pane = ActivePane();
	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	if (names.empty()) { SetStatusWarning(_T("対象がありません")); return; }

	const UnicodeString dir = ExcludeTrailingPathDelimiter(pane->GetPath());

	IShellFolder *desktop = NULL;
	if (FAILED(::SHGetDesktopFolder(&desktop)) || desktop == NULL) {
		SetStatusWarning(_T("シェルを取得できません"));
		return;
	}

	LPITEMIDLIST dir_pidl = NULL;
	IShellFolder *folder = NULL;
	std::vector<LPCITEMIDLIST> child_pidls;
	std::vector<LPITEMIDLIST> owned;
	IContextMenu *menu = NULL;

	auto cleanup = [&] {
		if (menu != NULL) menu->Release();
		for (LPITEMIDLIST p : owned) ::CoTaskMemFree(p);
		if (folder != NULL) folder->Release();
		if (dir_pidl != NULL) ::CoTaskMemFree(dir_pidl);
		if (desktop != NULL) desktop->Release();
	};

	if (FAILED(desktop->ParseDisplayName(NULL, NULL, const_cast<LPWSTR>(dir.c_str()),
	                                      NULL, &dir_pidl, NULL))
	    || FAILED(desktop->BindToObject(dir_pidl, NULL, IID_IShellFolder,
	                                     reinterpret_cast<void **>(&folder)))) {
		cleanup();
		SetStatusWarning(_T("ディレクトリを解決できません"));
		return;
	}

	for (const UnicodeString &name : names) {
		LPITEMIDLIST p = NULL;
		if (SUCCEEDED(folder->ParseDisplayName(NULL, NULL, const_cast<LPWSTR>(name.c_str()),
		                                        NULL, &p, NULL)) && p != NULL) {
			owned.push_back(p);
			child_pidls.push_back(p);
		}
	}
	if (child_pidls.empty()) { cleanup(); SetStatusWarning(_T("対象を解決できません")); return; }

	if (FAILED(folder->GetUIObjectOf(static_cast<HWND>(GetHandle()),
	                                  static_cast<UINT>(child_pidls.size()), child_pidls.data(),
	                                  IID_IContextMenu, NULL, reinterpret_cast<void **>(&menu)))
	    || menu == NULL) {
		cleanup();
		SetStatusWarning(_T("コンテキストメニューを取得できません"));
		return;
	}

	HMENU hmenu = ::CreatePopupMenu();
	if (hmenu == NULL) { cleanup(); return; }

	// 1 から始めるのは、0 を「選ばれなかった」と区別するため
	const UINT kFirst = 1;
	const UINT kLast = 0x7FFF;
	if (SUCCEEDED(menu->QueryContextMenu(hmenu, 0, kFirst, kLast, CMF_NORMAL))) {
		::POINT pt = {};
		::GetCursorPos(&pt);
		const int cmd = ::TrackPopupMenu(hmenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
		                                 pt.x, pt.y, 0, static_cast<HWND>(GetHandle()), NULL);
		if (cmd > 0) {
			CMINVOKECOMMANDINFO ici = {};
			ici.cbSize = sizeof(ici);
			ici.hwnd = static_cast<HWND>(GetHandle());
			ici.lpVerb = MAKEINTRESOURCEA(cmd - kFirst);
			ici.nShow = SW_SHOWNORMAL;
			menu->InvokeCommand(&ici);
		}
	}
	::DestroyMenu(hmenu);
	cleanup();

	// メニューからの操作でファイルが変わりうるので読み直す
	panes_[0]->Reload();
	panes_[1]->Reload();
	UpdateStatus();
}

//---------------------------------------------------------------------------
void MainFrame::CmdOpenTrash()
{
	external::LaunchSpec spec;
	spec.file = _T("explorer.exe");
	spec.parameters = _T("shell:RecycleBinFolder");
	if (!launch(spec, static_cast<HWND>(GetHandle()))) SetStatusWarning(_T("ごみ箱を開けません"));
}

//---------------------------------------------------------------------------
void MainFrame::CmdOpenCtrlPanel()
{
	external::LaunchSpec spec;
	spec.file = _T("control.exe");
	if (!launch(spec, static_cast<HWND>(GetHandle()))) {
		SetStatusWarning(_T("コントロールパネルを開けません"));
	}
}

//---------------------------------------------------------------------------
void MainFrame::CmdFileRun()
{
	// Windows の「ファイル名を指定して実行」を出す。
	// VCL は自前で組み立てているが、シェルの標準ダイアログで足りる
	typedef void(WINAPI * RunFileDlgW)(HWND, HICON, LPCWSTR, LPCWSTR, LPCWSTR, UINT);
	HMODULE h = ::LoadLibraryW(L"shell32.dll");
	if (h != NULL) {
		RunFileDlgW fn = reinterpret_cast<RunFileDlgW>(
			reinterpret_cast<void *>(::GetProcAddress(h, MAKEINTRESOURCEA(61))));
		if (fn != NULL) {
			fn(static_cast<HWND>(GetHandle()), NULL,
			   ExcludeTrailingPathDelimiter(ActivePane()->GetPath()).c_str(), NULL, NULL, 0);
			::FreeLibrary(h);
			return;
		}
		::FreeLibrary(h);
	}
	// **序数 61 は文書化されていない**ので、取れなければ入力で代替する
	const wxString input = wxGetTextFromUser(to_wx(_T("実行するコマンドを入力してください")),
	                                          to_wx(_T("ファイル名を指定して実行")),
	                                          wxEmptyString, this);
	if (input.IsEmpty()) return;

	external::LaunchSpec spec;
	spec.file = to_us(input);
	spec.directory = ExcludeTrailingPathDelimiter(ActivePane()->GetPath());
	if (!launch(spec, static_cast<HWND>(GetHandle()))) SetStatusWarning(_T("実行できません"));
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// 情報系 (機能群11/12)
//---------------------------------------------------------------------------
void MainFrame::CmdCalcDirSize(bool all)
{
	FilePane *pane = ActivePane();

	std::vector<UnicodeString> targets;
	if (all) {
		// 一覧にあるディレクトリを全部
		for (const FileItem &it : pane->VisibleItems()) {
			if (it.is_dir && !it.is_parent) targets.push_back(it.name);
		}
	}
	else {
		for (const UnicodeString &n : pane->GetSelectedNames()) {
			if (dir_exists(pane->GetPath() + n)) targets.push_back(n);
		}
	}
	if (targets.empty()) { SetStatusWarning(_T("対象のディレクトリがありません")); return; }

	UnicodeString text;
	Int64 grand = 0;
	bool any_truncated = false;
	for (const UnicodeString &n : targets) {
		const dir_info::DirSize r = dir_info::CalcDirSize(pane->GetPath() + n,
		                                                   pane->GetShowHidden(),
		                                                   pane->GetShowSystem());
		grand += r.bytes;
		any_truncated = any_truncated || r.truncated;
		text.cat_sprintf(_T("%-28s %14s  (%d ファイル)\r\n"), n.c_str(),
		                 get_size_str_B(r.bytes, 14).Trim().c_str(), r.files);
	}
	text.cat_sprintf(_T("\r\n合計 %s\r\n"), get_size_str_B(grand, 14).Trim().c_str());
	// 打ち切ったら黙っていない (規約: 上限を超えたことを明示する)
	if (any_truncated) {
		text.cat_sprintf(_T("\r\n※ %d ファイルで打ち切りました。数字は途中までです"),
		                 dir_info::kMaxScanFiles);
	}

	wxMessageBox(to_wx(text), to_wx(_T("ディレクトリ容量")), wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
void MainFrame::CmdFileExtList()
{
	FilePane *pane = ActivePane();
	bool truncated = false;
	const auto stats = dir_info::CalcExtStats(pane->GetPath(), false, pane->GetShowHidden(),
	                                           pane->GetShowSystem(), truncated);
	if (stats.empty()) { SetStatusWarning(_T("ファイルがありません")); return; }

	UnicodeString text = _T("拡張子            件数           容量\r\n");
	for (const dir_info::ExtStat &st : stats) {
		text.cat_sprintf(_T("%-16s %6d %14s\r\n"), st.ext.c_str(), st.count,
		                 get_size_str_B(st.bytes, 14).Trim().c_str());
	}
	if (truncated) text += _T("\r\n※ 上限に達して打ち切りました");

	wxMessageBox(to_wx(text), to_wx(_T("拡張子別一覧")), wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
void MainFrame::CmdListTree()
{
	FilePane *pane = ActivePane();
	bool truncated = false;
	// 深さの上限はこちらの判断。深いところまで一気に出すと読めなくなる
	const auto lines = dir_info::BuildTree(pane->GetPath(), 4, pane->GetShowHidden(),
	                                        pane->GetShowSystem(), truncated);
	if (lines.empty()) { SetStatusWarning(_T("サブディレクトリがありません")); return; }

	UnicodeString text = pane->GetPath() + _T("\r\n");
	int shown = 0;
	for (const dir_info::TreeLine &l : lines) {
		if (shown++ >= 500) { text += _T("...\r\n(以下省略)\r\n"); break; }
		for (int i = 0; i <= l.depth; ++i) text += _T("  ");
		text += l.name + _T("\r\n");
	}
	if (truncated) text += _T("\r\n※ 上限に達して打ち切りました");

	wxMessageBox(to_wx(text), to_wx(_T("ディレクトリ構造")), wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
void MainFrame::CmdAbout()
{
	UnicodeString text = _T("NyanFi (wxWidgets 版)\r\n\r\n");
	text += _T("キーボード操作主体の2画面ファイラ NyanFi を、C++Builder / VCL から\r\n");
	text += _T("OSS のツールチェイン (mingw-w64 + wxWidgets) へ移植したものです。\r\n\r\n");
	text += _T("本家: https://nyanfi.dip.jp/\r\n");
	text += _T("移植: https://github.com/kuwa72/NyanFi_x64s\r\n\r\n");

	// 実行ファイルのバージョン情報を出す (無ければ出さない)
	unsigned mj = 0, mi = 0, bl = 0;
	if (GetProductVersion(Application->ExeName, mj, mi, bl)) {
		text.cat_sprintf(_T("バージョン: %u.%u.%u\r\n"), mj, mi, bl);
	}
	text.cat_sprintf(_T("wxWidgets: %d.%d.%d\r\n"), wxMAJOR_VERSION, wxMINOR_VERSION,
	                 wxRELEASE_NUMBER);

	wxMessageBox(to_wx(text), to_wx(_T("バージョン情報")), wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
void MainFrame::UpdateStatus()
{
	for (int i = 0; i < 2; ++i) {
		const UnicodeString mark = (i == active_) ? _T("● ") : _T("　 ");

		// パス + 並べ替えキー/方向 + (絞り込み中なら) マスクをヘッダに出す。
		// マスク中は絞り込み状態が見えないと危険なので必ず表示する
		UnicodeString label = mark + panes_[i]->GetPath() + _T("  [") + panes_[i]->GetSortSummary() + _T("]");
		if (panes_[i]->HasMask()) label += _T("  マスク: ") + panes_[i]->GetMask();
		headers_[i]->SetLabel(to_wx(label));
	}

	FilePane *pane = ActivePane();

	// インクリメンタルサーチ中はサーチ文字列と一致件数を表示する (どこにも
	// 出さないと今の状態が分からなくなるため。VCL 版のステータス表示に相当)
	if (incsearch_.IsActive()) {
		UnicodeString text = _T("サーチ: ") + incsearch_.Word();
		const int match_cnt = pane->GetMatchedCount();
		if (match_cnt > 0) text.cat_sprintf(_T("  (%d 件一致)"), match_cnt);
		SetStatusText(to_wx(text), 0);
	}
	else {
		SetStatusText(to_wx(pane->GetSummary()), 0);
	}

	const FileItem *itm = pane->GetCurrentItem();
	SetStatusText(itm != nullptr ? to_wx(itm->name) : wxString(), 1);
}

//---------------------------------------------------------------------------
void MainFrame::OnCharHook(wxKeyEvent &event)
{
	// 画像ビューアが開いている間は、キー入力を丸ごとビューアに渡す
	// (I モードのキー割り当ては KeyMap では扱わない。gui/image_viewer.cpp を参照)
	if (image_viewer_ != nullptr && image_viewer_->IsShown()) {
		if (!image_viewer_->HandleKey(event)) event.Skip();
		return;
	}

	// テキストビューアが開いている間は、キー入力を丸ごとビューアに渡す
	// (V モードのキー割り当ては KeyMap では扱わない。gui/key_map.cpp を参照)
	if (viewer_ != nullptr && viewer_->IsShown()) {
		if (!viewer_->HandleKey(event)) event.Skip();
		return;
	}

	// インクリメンタルサーチ中は、KeyMap を経由せずキー入力を丸ごと横取りする
	// (VCL 版の CurStt->is_IncSea が true の間、FileListIncSearch にキーを
	// 渡して Key=0 にするのと同じ。gui/key_map.h 冒頭のコメントも参照)
	if (incsearch_.IsActive()) {
		HandleIncSearchKey(event);
		return;
	}

	const UnicodeString key_str = KeyMap::KeyStrOf(event);
	const UnicodeString command = keymap_.Lookup(key_str);

	if (command.IsEmpty() || !Execute(command)) {
		event.Skip();  // 割り当ての無いキーは通常処理へ
		return;
	}
	UpdateStatus();
}

//---------------------------------------------------------------------------
void MainFrame::OnClose(wxCloseEvent &event)
{
	SaveSettings();
	event.Skip();
}

//---------------------------------------------------------------------------
/**
 * @brief コマンド名で処理を実行する
 * @details コマンド名の綴りは usr_cmdlist.cpp のコマンド表に合わせている。
 *          未実装のコマンドは false を返し、キーは通常処理に流す。
 */
bool MainFrame::Execute(const UnicodeString &command)
{
	FilePane *pane = ActivePane();

	if (SameStr(command, _T("CursorUp"))) {
		pane->MoveCursor(-1);
	}
	else if (SameStr(command, _T("CursorDown"))) {
		pane->MoveCursor(1);
	}
	else if (SameStr(command, _T("PageUp"))) {
		pane->PageMove(-1);
	}
	else if (SameStr(command, _T("PageDown"))) {
		pane->PageMove(1);
	}
	else if (SameStr(command, _T("CursorTop"))) {
		pane->CursorTop();
	}
	else if (SameStr(command, _T("CursorEnd"))) {
		pane->CursorEnd();
	}
	else if (SameStr(command, _T("OpenStandard"))) {
		if (!pane->EnterCurrent()) CmdOpenStandard();  // ディレクトリでなければ関連付けで開く
	}
	else if (SameStr(command, _T("OpenByApp"))) {
		CmdOpenByApp();
	}
	else if (SameStr(command, _T("PropertyDlg"))) {
		CmdPropertyDlg();
	}
	else if (SameStr(command, _T("ToParent")) || SameStr(command, _T("ToLeft"))) {
		pane->GoParent();
	}
	else if (SameStr(command, _T("ToRight"))) {
		pane->EnterCurrent();
	}
	else if (SameStr(command, _T("ToOpposite"))) {
		SetActivePane(1 - active_);
	}
	else if (SameStr(command, _T("ReloadList"))) {
		pane->Reload();
	}
	else if (SameStr(command, _T("Select"))) {
		pane->ToggleMark();
	}
	else if (SameStr(command, _T("SelAllItem"))) {
		// VCL の SelAllItemActionExecute (MainFrm.cpp:24846) と同じ判定。
		// 「選択が1件も無ければ全選択、1件でもあれば全解除」のトグル
		pane->MarkAll(pane->GetMarkedCount() == 0);
	}
	else if (SameStr(command, _T("ClearAll"))) {
		pane->MarkAll(false);
	}
	//-- 選択操作 -------------------------------------------------------------
	// 判断は gui/selection.h の純関数が持ち、ここは受け渡しだけにする (規約8)
	else if (SameStr(command, _T("SelAllFile"))) {
		ApplySelection(pane, [](std::vector<FileItem> &v) { selection::ToggleAllFiles(v); });
	}
	else if (SameStr(command, _T("SelReverse"))) {
		ApplySelection(pane, [](std::vector<FileItem> &v) { selection::ReverseFiles(v); });
	}
	else if (SameStr(command, _T("SelReverseAll"))) {
		ApplySelection(pane, [](std::vector<FileItem> &v) { selection::ReverseAll(v); });
	}
	else if (SameStr(command, _T("SelSameExt"))) {
		const int csr = pane->GetCursor();
		if (!ApplySelection(pane, [csr](std::vector<FileItem> &v) { selection::SelectSameExt(v, csr); }))
			SetStatusWarning(_T("カーソル位置がファイルではありません"));
	}
	else if (SameStr(command, _T("SelSameName"))) {
		const int csr = pane->GetCursor();
		if (!ApplySelection(pane, [csr](std::vector<FileItem> &v) { selection::SelectSameName(v, csr); }))
			SetStatusWarning(_T("カーソル位置がファイルではありません"));
	}
	else if (SameStr(command, _T("MatchSelect"))) {
		CmdMatchSelect();
	}
	else if (SameStr(command, _T("NextSelItem")) || SameStr(command, _T("PrevSelItem"))) {
		const bool forward = SameStr(command, _T("NextSelItem"));
		const std::vector<FileItem> v = pane->VisibleItems();
		const int idx = selection::FindNextMarked(v, pane->GetCursor(), forward);
		if (idx == -1) SetStatusWarning(_T("選択項目がありません"));
		else pane->MoveCursorTo(idx);
	}
	else if (SameStr(command, _T("SelectUp"))) {
		// VCL の SelectUpActionExecute (MainFrm.cpp:25031): 選択を反転してから上へ
		MarkCurrentAndMove(pane, -1);
	}
	//-- 選択しながらカーソル移動 (Shift+↑↓ など) -----------------------------
	else if (SameStr(command, _T("CursorUpSel"))) {
		MarkCurrentAndMove(pane, -1);
	}
	else if (SameStr(command, _T("CursorDownSel"))) {
		MarkCurrentAndMove(pane, 1);
	}
	else if (SameStr(command, _T("PageUpSel")) || SameStr(command, _T("PageDownSel"))) {
		const int from = pane->GetCursor();
		pane->PageMove(SameStr(command, _T("PageUpSel"))? -1 : 1);
		MarkBetween(pane, from, pane->GetCursor());
	}
	else if (SameStr(command, _T("CursorTopSel")) || SameStr(command, _T("CursorEndSel"))) {
		const int from = pane->GetCursor();
		if (SameStr(command, _T("CursorTopSel"))) pane->CursorTop(); else pane->CursorEnd();
		MarkBetween(pane, from, pane->GetCursor());
	}
	//-- 表示の切り替え -------------------------------------------------------
	else if (SameStr(command, _T("ShowHideAtr"))) {
		// 列挙のしかたが変わるので読み直しが要る (MainFrm.cpp:25992 の ReloadList)
		const bool v = !pane->GetShowHidden();
		ToggleBothPanes([v](FilePane *p) { p->SetShowHidden(v); }, true);
		SetStatusWarning(v? _T("隠しファイルを表示します") : _T("隠しファイルを隠します"));
	}
	else if (SameStr(command, _T("ShowSystemAtr"))) {
		const bool v = !pane->GetShowSystem();
		ToggleBothPanes([v](FilePane *p) { p->SetShowSystem(v); }, true);
		SetStatusWarning(v? _T("システムファイルを表示します") : _T("システムファイルを隠します"));
	}
	else if (SameStr(command, _T("ShowByteSize"))) {
		// 表示だけなので再描画で足りる (MainFrm.cpp:25901)
		const bool v = !pane->GetByteSize();
		ToggleBothPanes([v](FilePane *p) { p->SetByteSize(v); }, false);
	}
	else if (SameStr(command, _T("HideSizeTime"))) {
		const bool v = !pane->GetHideSizeTime();
		ToggleBothPanes([v](FilePane *p) { p->SetHideSizeTime(v); }, false);
	}
	//-- 左右の境界 -----------------------------------------------------------
	else if (SameStr(command, _T("BorderLeft"))) {
		SetBorderRatio(view_state::MoveBorder(border_ratio_, -1));
	}
	else if (SameStr(command, _T("BorderRight"))) {
		SetBorderRatio(view_state::MoveBorder(border_ratio_, 1));
	}
	else if (SameStr(command, _T("BorderCenter")) || SameStr(command, _T("EqualListWidth"))) {
		// EqualListWidth は WidenCurList に "50"/"Left" を渡したもの (MainFrm.cpp:17244)
		SetBorderRatio(view_state::WidenSide(true, 0.5));
	}
	else if (SameStr(command, _T("WidenCurList"))) {
		SetBorderRatio(view_state::WidenSide(active_ == 0));
	}
	else if (SameStr(command, _T("SwapLR"))) {
		CmdSwapLR();
	}
	//-- ウィンドウ -----------------------------------------------------------
	else if (SameStr(command, _T("WinMaximize"))) {
		Maximize(true);
	}
	else if (SameStr(command, _T("WinMinimize"))) {
		Iconize(true);
	}
	else if (SameStr(command, _T("WinNormal"))) {
		if (IsMaximized()) Maximize(false);
		if (IsIconized()) Iconize(false);
	}
	else if (SameStr(command, _T("ShowStatusBar"))) {
		wxStatusBar *sb = GetStatusBar();
		if (sb != nullptr) { sb->Show(!sb->IsShown()); Layout(); }
	}
	else if (SameStr(command, _T("ShowTabBar"))) {
		if (tab_bar_ != nullptr) { tab_bar_->Show(!tab_bar_->IsShown()); Layout(); }
	}
	//-- ディレクトリ移動 -----------------------------------------------------
	else if (SameStr(command, _T("ToRoot"))) {
		CmdToRoot();
	}
	else if (SameStr(command, _T("CurrToOpp"))) {
		CmdCopyPath(true);
	}
	else if (SameStr(command, _T("CurrFromOpp"))) {
		CmdCopyPath(false);
	}
	else if (SameStr(command, _T("CsrDirToOpp"))) {
		CmdCsrDirToOpp();
	}
	else if (SameStr(command, _T("ToOppSameItem"))) {
		CmdToOppSameItem();
	}
	else if (SameStr(command, _T("ToParentOnLeft"))) {
		CmdParentOn(0);
	}
	else if (SameStr(command, _T("ToParentOnRight"))) {
		CmdParentOn(1);
	}
	else if (SameStr(command, _T("NextDrive"))) {
		CmdCycleDrive(true);
	}
	else if (SameStr(command, _T("PrevDrive"))) {
		CmdCycleDrive(false);
	}
	else if (SameStr(command, _T("PushDir"))) {
		CmdPushDir();
	}
	else if (SameStr(command, _T("PopDir"))) {
		CmdPopDir();
	}
	else if (SameStr(command, _T("DirStack"))) {
		CmdShowDirStack();
	}
	else if (SameStr(command, _T("SyncLR"))) {
		// VCL はトグルで、有効にした直後に反対側を同名項目へ合わせる
		// (MainFrm.cpp:26706 の `if (SyncLR) ExeCommandAction("ToOppSameItem", "NO")`)
		sync_lr_ = !sync_lr_;
		SetStatusWarning(sync_lr_? _T("左右の同期: 有効") : _T("左右の同期: 解除"));
		if (sync_lr_) CmdToOppSameItem();
	}
	//-- タブ操作 -------------------------------------------------------------
	else if (SameStr(command, _T("MoveTab"))) {
		CmdMoveTab(1);
	}
	else if (SameStr(command, _T("SoloTab"))) {
		CmdSoloTab();
	}
	else if (SameStr(command, _T("TabHome"))) {
		CmdTabHome(false);
	}
	else if (SameStr(command, _T("ToTab"))) {
		CmdToTab();
	}
	else if (SameStr(command, _T("SubDirList"))) {
		CmdSubDirList();
	}
	else if (SameStr(command, _T("SpecialDirList"))) {
		CmdSpecialDirList();
	}
	//-- ファイル操作 ---------------------------------------------------------
	else if (SameStr(command, _T("CopyTo"))) {
		CmdCopyMoveTo(false);
	}
	else if (SameStr(command, _T("MoveTo"))) {
		CmdCopyMoveTo(true);
	}
	else if (SameStr(command, _T("NameToUpper"))) {
		CmdChangeNameCase(file_ops::NameCase::Upper);
	}
	else if (SameStr(command, _T("NameToLower"))) {
		CmdChangeNameCase(file_ops::NameCase::Lower);
	}
	else if (SameStr(command, _T("CopyFileName"))) {
		CmdCopyFileName(true);
	}
	else if (SameStr(command, _T("NewFile")) || SameStr(command, _T("NewTextFile"))) {
		CmdNewFile();
	}
	else if (SameStr(command, _T("CopyToClip"))) {
		CmdFilesToClip(false);
	}
	else if (SameStr(command, _T("CutToClip"))) {
		CmdFilesToClip(true);
	}
	else if (SameStr(command, _T("Paste"))) {
		CmdPaste();
	}
	//-- リンク・属性 ---------------------------------------------------------
	else if (SameStr(command, _T("CreateShortcut"))) {
		CmdCreateLinks(links::LinkKind::Shortcut);
	}
	else if (SameStr(command, _T("CreateHardLink"))) {
		CmdCreateLinks(links::LinkKind::Hard);
	}
	else if (SameStr(command, _T("CreateSymLink"))) {
		CmdCreateLinks(links::LinkKind::Symbolic);
	}
	else if (SameStr(command, _T("SetDirTime"))) {
		CmdSetDirTime();
	}
	//-- 書庫 -----------------------------------------------------------------
	else if (SameStr(command, _T("ListArchive"))) {
		CmdListArchive();
	}
	else if (SameStr(command, _T("TestArchive"))) {
		CmdTestArchive();
	}
	else if (SameStr(command, _T("UnPack"))) {
		CmdUnPack(false);
	}
	else if (SameStr(command, _T("UnPackToCurr"))) {
		CmdUnPack(true);
	}
	else if (SameStr(command, _T("Pack"))) {
		CmdPack(false);
	}
	else if (SameStr(command, _T("PackToCurr"))) {
		CmdPack(true);
	}
	//-- 比較・ハッシュ -------------------------------------------------------
	else if (SameStr(command, _T("GetHash"))) {
		CmdGetHash();
	}
	else if (SameStr(command, _T("CompareHash"))) {
		CmdCompareHash();
	}
	else if (SameStr(command, _T("SelOnlyCur"))) {
		CmdSelOnlyCur();
	}
	else if (SameStr(command, _T("ToOppSameHash"))) {
		CmdToOppSameHash();
	}
	else if (SameStr(command, _T("DiffDir"))) {
		CmdDiffDir();
	}
	//-- テキスト操作 ---------------------------------------------------------
	else if (SameStr(command, _T("CountLines"))) {
		CmdCountLines();
	}
	else if (SameStr(command, _T("JoinText"))) {
		CmdJoinText();
	}
	else if (SameStr(command, _T("ConvertTextEnc"))) {
		CmdConvertTextEnc();
	}
	else if (SameStr(command, _T("ListFileName"))) {
		CmdListFileName();
	}
	//-- 外部連携 -------------------------------------------------------------
	else if (SameStr(command, _T("CommandPrompt"))) {
		CmdLaunchShell(external::ShellKind::CommandPrompt);
	}
	else if (SameStr(command, _T("PowerShell"))) {
		CmdLaunchShell(external::ShellKind::PowerShell);
	}
	else if (SameStr(command, _T("WinTerminal"))) {
		CmdLaunchShell(external::ShellKind::WindowsTerminal);
	}
	else if (SameStr(command, _T("OpenByExp"))) {
		CmdOpenByExplorer();
	}
	else if (SameStr(command, _T("ContextMenu"))) {
		CmdContextMenu();
	}
	else if (SameStr(command, _T("OpenTrash"))) {
		CmdOpenTrash();
	}
	else if (SameStr(command, _T("OpenCtrlPanel"))) {
		CmdOpenCtrlPanel();
	}
	else if (SameStr(command, _T("FileRun"))) {
		CmdFileRun();
	}
	//-- 情報系 ---------------------------------------------------------------
	else if (SameStr(command, _T("CalcDirSize"))) {
		CmdCalcDirSize(false);
	}
	else if (SameStr(command, _T("CalcDirSizeAll"))) {
		CmdCalcDirSize(true);
	}
	else if (SameStr(command, _T("FileExtList"))) {
		CmdFileExtList();
	}
	else if (SameStr(command, _T("ListTree"))) {
		CmdListTree();
	}
	else if (SameStr(command, _T("AboutNyanFi"))) {
		CmdAbout();
	}
	else if (SameStr(command, _T("KeyList"))) {
		ShowKeyList();
	}
	else if (SameStr(command, _T("ShowCmdList"))) {
		ShowCmdList();
	}
	else if (SameStr(command, _T("SortDlg"))) {
		ShowSortDialog();
	}
	else if (SameStr(command, _T("SetPathMask"))) {
		ShowMaskDialog();
	}
	else if (SameStr(command, _T("ClearMask"))) {
		pane->SetMask(EmptyStr);
	}
	else if (SameStr(command, _T("Copy"))) {
		CmdCopy();
	}
	else if (SameStr(command, _T("Move"))) {
		CmdMove();
	}
	else if (SameStr(command, _T("Delete"))) {
		CmdDelete();
	}
	else if (SameStr(command, _T("CreateDir"))) {
		CmdCreateDir();
	}
	else if (SameStr(command, _T("RenameDlg"))) {
		CmdRenameDlg();
	}
	else if (SameStr(command, _T("TextViewer"))) {
		CmdTextViewer();
	}
	else if (SameStr(command, _T("ImageViewer"))) {
		CmdImageViewer();
	}
	else if (SameStr(command, _T("Grep"))) {
		CmdGrep();
	}
	else if (SameStr(command, _T("IncSearch"))) {
		StartIncSearch();
	}
	else if (SameStr(command, _T("BackDirHist"))) {
		pane->GoBackDirHistory();  // 履歴が無ければ何もしない (VCL 版はアクション自体が無効化される)
	}
	else if (SameStr(command, _T("ForwardDirHist"))) {
		pane->GoForwardDirHistory();
	}
	else if (SameStr(command, _T("DirHistory"))) {
		ShowDirHistoryDialog();
	}
	else if (SameStr(command, _T("DriveList"))) {
		ShowDriveListDialog();
	}
	else if (SameStr(command, _T("InputDir"))) {
		ShowInputDirDialog();
	}
	else if (SameStr(command, _T("AddTab"))) {
		CmdAddTab();
	}
	else if (SameStr(command, _T("DelTab"))) {
		CmdDelTab();
	}
	else if (SameStr(command, _T("NextTab"))) {
		CmdNextTab();
	}
	else if (SameStr(command, _T("PrevTab"))) {
		CmdPrevTab();
	}
	else if (SameStr(command, _T("PopupTab"))) {
		ShowTabListDialog();
	}
	else if (SameStr(command, _T("Exit"))) {
		Close(true);
	}
	else {
		return false;  // 未実装
	}

	// 現在のタブの記録 (ディレクトリ・並べ替え設定) を、いま実際にペインが
	// 開いている状態に合わせておく。VCL 版が SetCurStt 等の広い箇所から
	// SetCurTab を呼んでタブのキャプションを常に最新に保つのに対応する簡易版
	// (個々のコマンドごとに呼び分けず、実行された全コマンドの後にまとめて行う)
	StoreCurrentTabState();
	RefreshTabBar();
	return true;
}

//---------------------------------------------------------------------------
/// 現在のキー割り当てを一覧表示する (F1)
void MainFrame::ShowKeyList()
{
	const TStringList *entries = keymap_.Entries();

	UnicodeString text = _T("カーソルキー (UP/DOWN/LEFT/RIGHT) は get_CsrKeyCmd() の割り当てを使用\n\n");
	for (int i = 0; i < entries->Count; ++i) {
		text += entries->Strings[i] + _T("\n");
	}

	wxMessageBox(to_wx(text), to_wx(_T("キー割り当て")), wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
/// 移植済みのコマンド表の規模と、いま実装済みのコマンドを表示する (F12)
void MainFrame::ShowCmdList()
{
	// set_CmdList は VCL 版と同じコマンド表を作る。GUI が未実装でも表は生きている
	std::unique_ptr<TStringList> cmds(new TStringList());
	std::unique_ptr<TStringList> ids(new TStringList());
	set_CmdList(cmds.get(), ids.get());

	UnicodeString sample;
	for (int i = 0; i < cmds->Count && i < 12; ++i) {
		sample += cmds->Strings[i] + _T("\n");
	}

	// 実装済みのコマンドは keymap の割り当て先から拾う (ここで Execute は呼ばない)
	const TStringList *keys = keymap_.Entries();
	UnicodeString impl;
	for (int i = 0; i < keys->Count; ++i) {
		impl += keys->Strings[i] + _T("\n");
	}

	UnicodeString text;
	text.sprintf(_T("コマンド表: %d 件 (usr_cmdlist.cpp より)\n\n先頭 12 件:\n%s\n")
	             _T("Phase 2 骨格で実装済みのキー割り当て:\n%s"),
	             static_cast<int>(cmds->Count), sample.c_str(), impl.c_str());
	wxMessageBox(to_wx(text), to_wx(_T("コマンド一覧")), wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
/**
 * @brief ソートダイアログ (S)
 * @details SrtModDlg.cpp (ソートダイアログ) の簡略版。並べ替えキー (名前/拡張子/
 *          日時/サイズ/属性。SortModeRadioGroup と同じ並び)・降順・ディレクトリを
 *          先に集めるか、の3点だけを選ぶ。SrtModDlg.cpp にある「両側に適用」
 *          「拡張子リストの優先順」「記号順」などの拡張オプションは対象外
 *          (Phase 2 骨格のスコープ外)
 */
void MainFrame::ShowSortDialog()
{
	FilePane *pane = ActivePane();

	wxDialog dlg(this, wxID_ANY, to_wx(_T("ソート")));

	wxArrayString choices;
	choices.Add(to_wx(_T("名前(&F)")));
	choices.Add(to_wx(_T("拡張子(&E)")));
	choices.Add(to_wx(_T("日時(&D)")));
	choices.Add(to_wx(_T("サイズ(&S)")));
	choices.Add(to_wx(_T("属性(&A)")));

	wxRadioBox *key_box = new wxRadioBox(&dlg, wxID_ANY, to_wx(_T("並べ替えキー")), wxDefaultPosition,
	                                      wxDefaultSize, choices, 1, wxRA_SPECIFY_COLS);
	key_box->SetSelection(static_cast<int>(pane->GetSortKey()));

	wxCheckBox *desc_box = new wxCheckBox(&dlg, wxID_ANY, to_wx(_T("降順")));
	desc_box->SetValue(pane->IsSortDescending());

	wxCheckBox *dirs_box = new wxCheckBox(&dlg, wxID_ANY, to_wx(_T("ディレクトリを先に集める")));
	dirs_box->SetValue(pane->IsDirsFirst());

	wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
	top->Add(key_box, wxSizerFlags().Expand().Border(wxALL, 8));
	top->Add(desc_box, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
	top->Add(dirs_box, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
	top->Add(dlg.CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));
	dlg.SetSizerAndFit(top);
	dlg.CentreOnParent();

	if (dlg.ShowModal() != wxID_OK) return;

	pane->SetSortSettings(static_cast<SortKey>(key_box->GetSelection()), desc_box->GetValue(), dirs_box->GetValue());
}

//---------------------------------------------------------------------------
/**
 * @brief パスマスクの入力 (Ctrl+M)
 * @details MainFrm.cpp の ApplyPathMask/SplitMasksFD と同じ書式
 *          (";" 区切り、末尾 "\\" でディレクトリ用、先頭 "!" で除外) を
 *          gui/file_item.cpp::MatchPathMask がそのまま解釈する。
 *          空文字列で OK すると絞り込みを解除する (ClearMask と同じ効果)
 */
void MainFrame::ShowMaskDialog()
{
	FilePane *pane = ActivePane();

	wxTextEntryDialog dlg(this, to_wx(_T("ファイル名マスク (例: *.txt;*.doc;!*.tmp)\n空にすると解除")),
	                       to_wx(_T("パスマスクを設定")), to_wx(pane->GetMask()));
	if (dlg.ShowModal() != wxID_OK) return;

	pane->SetMask(to_us(dlg.GetValue()));
}

//---------------------------------------------------------------------------
/**
 * @brief インクリメンタルサーチを開始する (F。src/Global.cpp の既定キー表
 * "F:F=IncSearch" と同じ)
 * @details 状態は incsearch_ (gui/navigation.h の IncrementalSearch) が持つ。
 * 開始直後はキーワードが空なのでハイライトは付かない
 */
void MainFrame::StartIncSearch()
{
	incsearch_.Start();
	ActivePane()->ClearIncSearchHighlight();
	UpdateStatus();
}

//---------------------------------------------------------------------------
/**
 * @brief インクリメンタルサーチ中の1キー分の処理 (OnCharHook から横取りされる)
 * @details VCL 版 (MainFrm.cpp::FileListIncSearch) と同様、サーチ中は
 * ほぼ全てのキーをこのメソッドが飲み込む (event.Skip() を呼ばない)。
 * ESC・Enter でサーチを抜ける動作は VCL 版の "S:Enter=IncSearchExit" と
 * equal_ESC() チェックに相当する。上下キーは一致項目間の移動 (通常の
 * カーソル移動1行分ではない)。それ以外の印字可能な文字は
 * wxKeyEvent::GetUnicodeKey() から直接取り、VCL 版のような JP/US
 * キーボードごとの Shift+記号変換表は持たない (gui/navigation.h の
 * IncrementalSearch のコメントを参照。wx が OS 翻訳済みの文字を渡してくる
 * ため不要と判断した。ただし実機のキーボード入力での確認はできていない
 * ため、報告に未検証として明記する)
 */
void MainFrame::HandleIncSearchKey(wxKeyEvent &event)
{
	const UnicodeString key_str = KeyMap::KeyStrOf(event);

	if (SameText(key_str, _T("ESC")) || SameText(key_str, _T("ENTER"))) {
		ExitIncSearch();
		return;
	}
	if (SameText(key_str, _T("BKSP")) || SameText(key_str, _T("Ctrl+H"))) {
		HandleIncSearchBackspace();
		return;
	}
	if (SameText(key_str, _T("UP"))) {
		const std::vector<UnicodeString> names = ActivePane()->VisibleNames();
		const int idx = FindIncrementalSearchMatch(names, incsearch_.Word(), ActivePane()->GetCursor(), false);
		if (idx != -1) ActivePane()->MoveCursorTo(idx);
		UpdateStatus();
		return;
	}
	if (SameText(key_str, _T("DOWN"))) {
		const std::vector<UnicodeString> names = ActivePane()->VisibleNames();
		const int idx = FindIncrementalSearchMatch(names, incsearch_.Word(), ActivePane()->GetCursor(), true);
		if (idx != -1) ActivePane()->MoveCursorTo(idx);
		UpdateStatus();
		return;
	}

	const wchar_t uc = static_cast<wchar_t>(event.GetUnicodeKey());
	if (uc != WXK_NONE) HandleIncSearchChar(uc);

	// マッチしない特殊キー (Ctrl+X 等) は無視する。VCL 版と同じくサーチ中は
	// 通常のキー割り当てへフォールスルーさせない
}

//---------------------------------------------------------------------------
/// サーチモードを抜ける (Esc/Enter。VCL 版の ExitIncSearch 相当)
void MainFrame::ExitIncSearch()
{
	incsearch_.Exit();
	ActivePane()->ClearIncSearchHighlight();
	UpdateStatus();
}

//---------------------------------------------------------------------------
/**
 * @brief 1文字追加する
 * @details VCL 版の FileListIncSearch の「見つからなかった場合: 警告表示 +
 * beep + delete_end(キーワード末尾を戻す)」に相当する動作。一致が1件も
 * 無くなる文字は入力できない (見つかる直前の状態に戻る)
 */
void MainFrame::HandleIncSearchChar(wchar_t ch)
{
	FilePane *pane = ActivePane();

	incsearch_.Append(ch);
	pane->ApplyIncSearchHighlight(incsearch_.Word());

	if (pane->GetMatchedCount() == 0) {
		incsearch_.Backspace();
		pane->ApplyIncSearchHighlight(incsearch_.Word());
		wxBell();
	}
	else {
		JumpToNearestIncSearchMatch();
	}
	UpdateStatus();
}

//---------------------------------------------------------------------------
/// 1文字削除する (BackSpace)
void MainFrame::HandleIncSearchBackspace()
{
	FilePane *pane = ActivePane();
	if (!incsearch_.Backspace()) return;  // キーワードが元々空なら何もしない

	pane->ApplyIncSearchHighlight(incsearch_.Word());
	if (pane->GetMatchedCount() > 0) JumpToNearestIncSearchMatch();
	UpdateStatus();
}

//---------------------------------------------------------------------------
/// 現在のカーソル位置から最も近い (自分自身を含む) 一致項目へ移動する
void MainFrame::JumpToNearestIncSearchMatch()
{
	FilePane *pane = ActivePane();
	const std::vector<UnicodeString> names = pane->VisibleNames();
	// カーソル自身も候補に含めたいので、開始位置を1つ手前にする
	const int idx = FindIncrementalSearchMatch(names, incsearch_.Word(), pane->GetCursor() - 1, true);
	if (idx != -1) pane->MoveCursorTo(idx);
}

//---------------------------------------------------------------------------
/**
 * @brief ディレクトリ履歴の一覧から選ぶ (H。src/Global.cpp の既定キー表
 * "F:H=DirHistory" と同じ)
 * @details VCL 版 (HistDlg.cpp::TDirHistoryDlg) の簡略版。一覧の並び順
 * (新しい順に表示する) は VCL 版の実際の表示順を確認できておらず、
 * Phase 2 骨格として使いやすさを優先して決めたもの (推測。要検証)
 */
void MainFrame::ShowDirHistoryDialog()
{
	FilePane *pane = ActivePane();
	const std::vector<UnicodeString> &entries = pane->DirHistoryEntries();

	if (entries.empty()) {
		wxMessageBox(to_wx(_T("ディレクトリ履歴がありません")), to_wx(_T("ディレクトリ履歴")),
		             wxOK | wxICON_INFORMATION, this);
		return;
	}

	wxArrayString choices;
	for (int i = static_cast<int>(entries.size()) - 1; i >= 0; --i) {
		choices.Add(to_wx(entries[static_cast<std::size_t>(i)]));
	}

	const int picked = wxGetSingleChoiceIndex(to_wx(_T("移動先のディレクトリを選んでください")),
	                                           to_wx(_T("ディレクトリ履歴")), choices, this);
	if (picked == -1) return;

	const int index = static_cast<int>(entries.size()) - 1 - picked;
	pane->GoDirHistoryIndex(index);
	UpdateStatus();
}

//---------------------------------------------------------------------------
/**
 * @brief ドライブの一覧から選んで移動する (L。src/Global.cpp の既定キー表
 * "F:L=DriveList" と同じ)
 * @details 移植済みの get_available_drive_list() (usr_file_ex.h) で
 * 利用可能なドライブを列挙し、get_drive_type() の種別を DriveTypeLabel()
 * (gui/navigation.h。Global.cpp の type_str と同じ文言) で添えて表示する。
 * VCL 版 (DriveDlg.cpp::TSelDriveDlg) の空き容量・ボリューム名表示は
 * Phase 2 骨格のスコープ外
 */
void MainFrame::ShowDriveListDialog()
{
	FilePane *pane = ActivePane();

	std::unique_ptr<TStringList> drives(new TStringList());
	get_available_drive_list(drives.get());

	if (drives->Count == 0) {
		wxMessageBox(to_wx(_T("利用可能なドライブがありません")), to_wx(_T("ドライブ一覧")),
		             wxOK | wxICON_INFORMATION, this);
		return;
	}

	wxArrayString choices;
	std::vector<UnicodeString> paths;
	for (int i = 0; i < drives->Count; ++i) {
		const UnicodeString drv = drives->Strings[i];
		const UnicodeString type_label = DriveTypeLabel(get_drive_type(drv));

		UnicodeString label = drv;
		if (!type_label.IsEmpty()) label += _T("  ") + type_label;

		choices.Add(to_wx(label));
		paths.push_back(drv);
	}

	const int picked = wxGetSingleChoiceIndex(to_wx(_T("移動先のドライブを選んでください")),
	                                           to_wx(_T("ドライブ一覧")), choices, this);
	if (picked == -1) return;

	pane->SetPath(paths[static_cast<std::size_t>(picked)]);
	UpdateStatus();
}

//---------------------------------------------------------------------------
/**
 * @brief パスを直接入力して移動する (Ctrl+G、推測のキー。gui/key_map.cpp を参照)
 * @details 環境変数の展開・相対パスの解決は gui/navigation.h の
 * ResolveDirectoryInput() (移植済みの get_actual_path()/to_absolute_name()
 * を使う) に委ねる。VCL 版 (MainFrm.cpp::InputDirActionExecute) にある
 * UNC パスの疎通確認・クリップボードからの入力・ユーザー名指定は
 * Phase 2 骨格のスコープ外
 */
void MainFrame::ShowInputDirDialog()
{
	FilePane *pane = ActivePane();

	wxTextEntryDialog dlg(this, to_wx(_T("移動先のディレクトリ (環境変数 %VAR% が使えます)")),
	                       to_wx(_T("ディレクトリを開く")), to_wx(pane->GetPath()));
	if (dlg.ShowModal() != wxID_OK) return;

	const UnicodeString input = to_us(dlg.GetValue());
	UnicodeString resolved;
	if (!ResolveDirectoryInput(input, pane->GetPath(), resolved)) {
		wxMessageBox(to_wx(_T("ディレクトリが見つかりません:\n") + input),
		             to_wx(_T("ディレクトリを開く")), wxOK | wxICON_ERROR, this);
		return;
	}

	pane->SetPath(resolved);
	UpdateStatus();
}

//---------------------------------------------------------------------------
/**
 * @brief コピー (C)
 * @details 対象はアクティブペインの選択項目 (マーク済み、無ければカーソル位置の
 * 1件)。宛先は反対側のペインの現在のディレクトリ固定 (指定ディレクトリへの
 * コピー(CopyTo)は対象外)。実行前に必ず確認ダイアログを出し、実行後は必ず
 * 結果 (成功/スキップ/失敗の件数) を表示する。ディレクトリの再帰コピー・
 * 上書き回避の判断は gui/file_ops.h を参照
 */
void MainFrame::CmdCopy()
{
	FilePane *pane = ActivePane();
	FilePane *dst_pane = OppositePane();

	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	if (names.empty()) {
		wxMessageBox(to_wx(_T("コピー対象がありません")), to_wx(_T("コピー")), wxOK | wxICON_INFORMATION, this);
		return;
	}

	const UnicodeString dst_dir = dst_pane->GetPath();
	if (!ConfirmItems(this, _T("コピー"), _T("コピー"), names, dst_dir)) return;

	std::vector<UnicodeString> paths;
	for (const UnicodeString &name : names) paths.push_back(pane->GetPath() + name);

	const file_ops::FileOpResult result = file_ops::CopyItems(paths, dst_dir);

	pane->Reload();
	dst_pane->Reload();

	wxMessageBox(to_wx(file_ops::Summarize(result)), to_wx(_T("コピーの結果")), wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
/**
 * @brief 移動 (M)
 * @details Copy と同じ対象の決め方・確認・結果表示。ディレクトリの移動が
 * ボリュームを跨ぐ場合は失敗として報告する (gui/file_ops.h を参照)
 */
void MainFrame::CmdMove()
{
	FilePane *pane = ActivePane();
	FilePane *dst_pane = OppositePane();

	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	if (names.empty()) {
		wxMessageBox(to_wx(_T("移動対象がありません")), to_wx(_T("移動")), wxOK | wxICON_INFORMATION, this);
		return;
	}

	const UnicodeString dst_dir = dst_pane->GetPath();
	if (!ConfirmItems(this, _T("移動"), _T("移動"), names, dst_dir)) return;

	std::vector<UnicodeString> paths;
	for (const UnicodeString &name : names) paths.push_back(pane->GetPath() + name);

	const file_ops::FileOpResult result = file_ops::MoveItems(paths, dst_dir);

	pane->Reload();
	dst_pane->Reload();

	wxMessageBox(to_wx(file_ops::Summarize(result)), to_wx(_T("移動の結果")), wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
/**
 * @brief 削除 (D)
 * @details 完全削除ではなく、SHFileOperationW (FOF_ALLOWUNDO) でゴミ箱へ送る
 * だけ。delete_Dir/delete_Dirs (完全削除、しかも delete_Dirs はファイルを
 * 削除しない既知の不具合がある) は使わない
 */
void MainFrame::CmdDelete()
{
	FilePane *pane = ActivePane();

	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	if (names.empty()) {
		wxMessageBox(to_wx(_T("削除対象がありません")), to_wx(_T("削除")), wxOK | wxICON_INFORMATION, this);
		return;
	}

	if (!ConfirmItems(this, _T("削除"), _T("ゴミ箱へ移動"), names, EmptyStr)) return;

	std::vector<UnicodeString> paths;
	for (const UnicodeString &name : names) paths.push_back(pane->GetPath() + name);

	UnicodeString error;
	const bool ok = file_ops::SendToTrash(paths, error, static_cast<HWND>(GetHandle()));

	pane->Reload();

	if (ok) {
		UnicodeString msg;
		msg.sprintf(_T("%d 件をゴミ箱へ送りました"), static_cast<int>(paths.size()));
		wxMessageBox(to_wx(msg), to_wx(_T("削除の結果")), wxOK | wxICON_INFORMATION, this);
	}
	else {
		wxMessageBox(to_wx(error), to_wx(_T("削除に失敗しました")), wxOK | wxICON_ERROR, this);
	}
}

//---------------------------------------------------------------------------
/// ディレクトリの作成 (K)
void MainFrame::CmdCreateDir()
{
	FilePane *pane = ActivePane();

	wxTextEntryDialog dlg(this, to_wx(_T("作成するディレクトリ名")), to_wx(_T("ディレクトリの作成")));
	if (dlg.ShowModal() != wxID_OK) return;

	const UnicodeString name = to_us(dlg.GetValue());
	UnicodeString error;
	if (!file_ops::MakeDirectory(pane->GetPath(), name, error)) {
		wxMessageBox(to_wx(error), to_wx(_T("ディレクトリを作成できません")), wxOK | wxICON_ERROR, this);
		return;
	}

	pane->Reload();
}

//---------------------------------------------------------------------------
/**
 * @brief 一括リネーム (R)
 * @details 対象はアクティブペインの選択項目 (マーク済み、無ければカーソル位置の
 * 1件。CmdCopy 等と同じ `GetSelectedItems()`)。実際の方式選択・プレビュー・
 * 確認・実行は gui/rename_dialog.h に委ねる (正規表現置換・連番付与・
 * 大文字小文字変換の3方式。詳細は gui/rename.h のコメントを参照)
 */
void MainFrame::CmdRenameDlg()
{
	FilePane *pane = ActivePane();

	const std::vector<FileItem> items = pane->GetSelectedItems();
	if (items.empty()) {
		wxMessageBox(to_wx(_T("対象がありません")), to_wx(_T("一括リネーム")), wxOK | wxICON_INFORMATION, this);
		return;
	}

	std::vector<rename_core::RenameTarget> targets;
	targets.reserve(items.size());
	for (const FileItem &itm : items) {
		rename_core::RenameTarget t;
		t.name = itm.name;
		t.is_dir = itm.is_dir;
		targets.push_back(t);
	}

	if (rename_dialog::Run(this, pane->GetPath(), targets)) pane->Reload();
}

//---------------------------------------------------------------------------
/**
 * @brief 関連付けで開く (ENTER。カーソル位置がディレクトリでなかった場合のみ呼ばれる)
 * @details 実行可能ファイル (test_ExeExt、FEXT_EXECUTE) は誤って起動する事故を
 * 防ぐため必ず確認ダイアログを出す。関連付けが無い拡張子の場合は
 * ShellExecuteExW 自身が Windows 標準の「開く方法を選んでください」
 * ダイアログを出す (gui/file_open.h を参照)
 */
void MainFrame::CmdOpenStandard()
{
	FilePane *pane = ActivePane();
	const FileItem *itm = pane->GetCurrentItem();
	if (itm == nullptr || itm->is_parent || itm->is_dir) return;

	const UnicodeString full_path = pane->GetPath() + itm->name;
	if (test_ExeExt(get_extension(full_path)) && !ConfirmExecute(this, full_path)) return;

	UnicodeString error;
	if (!file_open::OpenStandard(full_path, error, static_cast<HWND>(GetHandle())) && !error.IsEmpty()) {
		wxMessageBox(to_wx(error), to_wx(_T("開けませんでした")), wxOK | wxICON_ERROR, this);
	}
}

//---------------------------------------------------------------------------
/// アプリケーションから開く (Ctrl+Enter)。ディレクトリでも呼べる (関連付けダイアログ側の判断に任せる)
void MainFrame::CmdOpenByApp()
{
	FilePane *pane = ActivePane();
	const FileItem *itm = pane->GetCurrentItem();
	if (itm == nullptr || itm->is_parent) return;

	const UnicodeString full_path = pane->GetPath() + itm->name;

	UnicodeString error;
	if (!file_open::OpenWithDialog(full_path, error, static_cast<HWND>(GetHandle())) && !error.IsEmpty()) {
		wxMessageBox(to_wx(error), to_wx(_T("アプリケーションから開く")), wxOK | wxICON_ERROR, this);
	}
}

//---------------------------------------------------------------------------
/**
 * @brief ファイル情報ダイアログ (Alt+Enter、推測のキー。gui/key_map.cpp 参照)
 * @details ディレクトリでもファイルでも表示できる (usr_cmdlist.cpp の
 * "FVI:PropertyDlg" は種別を問わない)
 */
void MainFrame::CmdPropertyDlg()
{
	FilePane *pane = ActivePane();
	const FileItem *itm = pane->GetCurrentItem();
	if (itm == nullptr || itm->is_parent) return;

	const UnicodeString full_path = pane->GetPath() + itm->name;
	ShowFileInfoDialog(this, full_path, *itm);
}

//---------------------------------------------------------------------------
/**
 * @brief テキストビューアで開く (V。src/Global.cpp の既定キー表 "F:V=TextViewer" と同じ)
 * @details ディレクトリと ".." は対象外 (無視する)。文字コード判定・行分割は
 * gui/text_viewer_core.h (移植済みの get_MemoryCodePage を使う) を参照
 */
void MainFrame::CmdTextViewer()
{
	FilePane *pane = ActivePane();
	const FileItem *itm = pane->GetCurrentItem();
	if (itm == nullptr || itm->is_parent || itm->is_dir) return;

	const UnicodeString full_path = pane->GetPath() + itm->name;

	UnicodeString error;
	if (!viewer_->LoadFile(full_path, error)) {
		wxMessageBox(to_wx(error), to_wx(_T("開けませんでした")), wxOK | wxICON_ERROR, this);
		return;
	}

	ShowViewer(true);
}

//---------------------------------------------------------------------------
/**
 * @brief ビューアの表示/非表示を切り替える
 * @details root_ (2ペイン) と viewer_ は同じ領域に重ねてあり、常に片方だけを
 * 表示する。閉じたときはアクティブペインへフォーカスを戻す
 * (TextViewer::HandleKey の Q/ESC → SetOnClose のコールバックから呼ばれる)
 */
void MainFrame::ShowViewer(bool show)
{
	if (show && image_viewer_ != nullptr && image_viewer_->IsShown()) image_viewer_->Show(false);
	if (root_ != nullptr) root_->Show(!show);
	viewer_->Show(show);

	if (show) {
		// タブバー分を除いた領域に合わせる (MainFrame::OnSize と同じ計算)
		const wxSize sz = GetClientSize();
		const int tab_bar_h = (tab_bar_ != nullptr) ? tab_bar_->GetSize().y : 0;
		viewer_->SetSize(wxRect(0, tab_bar_h, sz.x, std::max(0, sz.y - tab_bar_h)));
		viewer_->SetFocus();
	}
	else {
		ActivePane()->SetFocus();
	}
	Layout();
	UpdateStatus();
}

//---------------------------------------------------------------------------
/**
 * @brief 画像ビューアで開く (G。src/Global.cpp の既定キー表 "F:G=ImageViewer" と同じ)
 * @details ディレクトリと ".." は対象外 (無視する。gui/text_viewer.cpp の
 * CmdTextViewer と同じ判断)。画像として認識できない/デコードに失敗しても
 * ビューア自体は開き、エラーメッセージを表示する (gui/image_viewer.h の
 * LoadFile のコメントを参照。VCL 版の imgv_thread.cpp が壊れた画像1件で
 * ビューアを閉じないのと同じ考え方)。前後の画像への移動 (Left/Right) の
 * 対象は BuildImageNavList が作る一覧を参照
 */
void MainFrame::CmdImageViewer()
{
	FilePane *pane = ActivePane();
	const FileItem *itm = pane->GetCurrentItem();
	if (itm == nullptr || itm->is_parent || itm->is_dir) return;

	BuildImageNavList(itm->name);

	const UnicodeString full_path = pane->GetPath() + itm->name;
	image_viewer_->LoadFile(full_path);
	ShowImageViewer(true);
}

//---------------------------------------------------------------------------
/**
 * @brief 画像ビューアを開いた時点のディレクトリ内で、対応拡張子のファイルだけを
 * 表示順 (アクティブペインの現在の並べ替え/マスク絞り込みを反映した順序) で
 * 集めた一覧を作る
 * @details FilePane::ItemAtVisible() で項目を引き、ディレクトリを除外してから
 *          拡張子で判定する。拡張子だけで見ると "photo.jpg" という名前の
 *          フォルダを画像として拾ってしまうため。
 */
void MainFrame::BuildImageNavList(const UnicodeString &current_name)
{
	FilePane *pane = ActivePane();

	image_nav_dir_ = pane->GetPath();
	image_nav_list_.clear();
	image_nav_index_ = -1;

	// ディレクトリを除外する。拡張子だけで判定すると "photo.jpg" という名前の
	// フォルダを画像として拾ってしまう
	const int count = pane->GetItemCount();
	for (int i = 0; i < count; ++i) {
		const FileItem *itm = pane->ItemAtVisible(i);
		if (itm == nullptr || itm->is_dir || itm->is_parent) continue;
		if (!image_load::IsSupportedExt(itm->name)) continue;

		if (SameText(itm->name, current_name)) image_nav_index_ = static_cast<int>(image_nav_list_.size());
		image_nav_list_.push_back(itm->name);
	}
}

//---------------------------------------------------------------------------
/**
 * @brief 前後の画像へ移動する (Left/Right、推測のキー。gui/image_viewer.h の
 * SetOnNavigate から呼ばれる)
 * @details 一覧の端に達したら何もしない (src/Global.cpp の既定値
 * LoopFilerCursor=false と同じ。src/Global.cpp::to_NextFile/to_PrevFile が
 * ループしないのと同じ挙動。実測)
 */
void MainFrame::CmdImageNavigate(int direction)
{
	if (image_nav_list_.empty() || image_nav_index_ == -1) return;

	const int next = image_nav_index_ + direction;
	if (next < 0 || next >= static_cast<int>(image_nav_list_.size())) return;

	image_nav_index_ = next;
	const UnicodeString full_path = image_nav_dir_ + image_nav_list_[static_cast<std::size_t>(next)];
	image_viewer_->LoadFile(full_path);
}

//---------------------------------------------------------------------------
/**
 * @brief 画像ビューアの表示/非表示を切り替える
 * @details root_ (2ペイン)・viewer_ (テキストビューア)・image_viewer_
 * (画像ビューア) は同じ領域に重ねてあり、常にどれか1つだけを表示する
 */
void MainFrame::ShowImageViewer(bool show)
{
	if (show && viewer_ != nullptr && viewer_->IsShown()) viewer_->Show(false);
	if (root_ != nullptr) root_->Show(!show);
	image_viewer_->Show(show);

	if (show) {
		// タブバー分を除いた領域に合わせる (MainFrame::OnSize と同じ計算)
		const wxSize sz = GetClientSize();
		const int tab_bar_h = (tab_bar_ != nullptr) ? tab_bar_->GetSize().y : 0;
		image_viewer_->SetSize(wxRect(0, tab_bar_h, sz.x, std::max(0, sz.y - tab_bar_h)));
		image_viewer_->SetFocus();
	}
	else {
		ActivePane()->SetFocus();
	}
	Layout();
	UpdateStatus();
}

//---------------------------------------------------------------------------
/**
 * @brief 文字列検索 (GREP、Ctrl+F、推測のキー。gui/key_map.cpp を参照)
 * @details アクティブペインの現在のディレクトリを対象にする (要件1)。
 * 検索条件の入力・進捗表示・結果一覧からの選択は gui/grep_dialog.h に
 * まとめてあり、ここは選ばれたマッチをテキストビューアで開くだけ。
 * grep_thread.cpp のような別スレッドは使っておらず、grep_dialog::Run が
 * 同期的に走査する (wxProgressDialog でユーザーの中断を受け付ける)
 */
void MainFrame::CmdGrep()
{
	FilePane *pane = ActivePane();

	grep_core::GrepMatch selected;
	if (!grep_dialog::Run(this, pane->GetPath(), pane->GetMask(), selected)) return;

	UnicodeString error;
	if (!viewer_->LoadFile(selected.file, error)) {
		wxMessageBox(to_wx(error), to_wx(_T("開けませんでした")), wxOK | wxICON_ERROR, this);
		return;
	}

	// GrepMatch::line は1始まり、GotoLine は0始まりの行番号を取る
	viewer_->GotoLine(selected.line - 1);
	ShowViewer(true);
}

//---------------------------------------------------------------------------
/**
 * @brief タブを追加する (Ctrl+T、推測のキー。usr_cmdlist.cpp の "F:AddTab=タブを追加"
 * と同じコマンド名)
 * @details VCL 版 (AddTabActionExecute) の既定 (ActionParam 無し) と同じく、
 * 現在の両ペインのディレクトリ・並べ替え設定を複製した新しいタブを末尾に
 * 追加し、そこへ切り替える (ブラウザの「新しいタブ」に近い。VCL 版のような
 * 空ディレクトリでの新規タブは無い)
 */
void MainFrame::CmdAddTab()
{
	StoreCurrentTabState();  // 現在のタブの記録を最新化してから複製する

	TabState state = tabs_.Current();
	// タブを作った時点のディレクトリをホームにする (TabHome の戻り先)。
	// VCL は TabList の CSV に home0/home1 として持つ
	for (int i = 0; i < 2; ++i) state.panes[i].home = state.panes[i].directory;
	tabs_.AddTab(state);
	ApplyTabState(tabs_.Current());  // 内容は複製なので実際にはペインは変化しない
	RefreshTabBar();
}

//---------------------------------------------------------------------------
/**
 * @brief 現在のタブを閉じる (Ctrl+W、推測のキー。"F:DelTab=タブを削除" と同じコマンド名)
 * @details 最後の1枚は閉じない (要件。TabManager::CloseCurrentTab を参照)
 */
void MainFrame::CmdDelTab()
{
	if (!tabs_.CloseCurrentTab()) {
		wxBell();  // 最後の1枚は閉じない
		return;
	}
	ApplyTabState(tabs_.Current());
	RefreshTabBar();
}

//---------------------------------------------------------------------------
/// 次のタブへ (Ctrl+Tab、推測のキー。"F:NextTab=次のタブへ" と同じコマンド名)
void MainFrame::CmdNextTab()
{
	if (tabs_.Count() <= 1) return;
	StoreCurrentTabState();
	tabs_.NextTab();
	ApplyTabState(tabs_.Current());
	RefreshTabBar();
}

//---------------------------------------------------------------------------
/// 前のタブへ (Shift+Ctrl+Tab、推測のキー。"F:PrevTab=前のタブへ" と同じコマンド名)
void MainFrame::CmdPrevTab()
{
	if (tabs_.Count() <= 1) return;
	StoreCurrentTabState();
	tabs_.PrevTab();
	ApplyTabState(tabs_.Current());
	RefreshTabBar();
}

//---------------------------------------------------------------------------
/**
 * @brief タブの一覧から選ぶ (Ctrl+E、推測のキー。"F:PopupTab=タブ選択メニューを表示"
 * と同じコマンド名)
 * @details gui/main_frame.cpp::ShowDirHistoryDialog と同じ、wxGetSingleChoiceIndex
 * によるシンプルな一覧選択。タブが1本しか無くても (選んでも変化が無いだけなので)表示する
 */
void MainFrame::ShowTabListDialog()
{
	const std::vector<UnicodeString> captions = tabs_.Captions();

	wxArrayString choices;
	for (const UnicodeString &cap : captions) choices.Add(to_wx(cap));

	const int picked = wxGetSingleChoiceIndex(to_wx(_T("切り替え先のタブを選んでください")),
	                                           to_wx(_T("タブの一覧")), choices, this);
	if (picked == -1 || picked == tabs_.CurrentIndex()) return;

	StoreCurrentTabState();
	if (tabs_.SelectAt(picked)) ApplyTabState(tabs_.Current());
	RefreshTabBar();
}

//---------------------------------------------------------------------------
/// 現在のタブの記録を、いま実際に両ペインが開いている状態で上書きする
/// (VCL 版の StoreTabStt/SetCurTab 相当)
void MainFrame::StoreCurrentTabState()
{
	TabState &cur = tabs_.MutableCurrent();
	for (int i = 0; i < 2; ++i) {
		PaneTabState &pane_state = cur.panes[i];
		pane_state.directory       = panes_[i]->GetPath();
		pane_state.sort_key        = panes_[i]->GetSortKey();
		pane_state.sort_descending = panes_[i]->IsSortDescending();
		pane_state.dirs_first      = panes_[i]->IsDirsFirst();
	}
}

//---------------------------------------------------------------------------
/**
 * @brief タブの記録を両ペインへ適用する (VCL 版の TabControl1Change 相当)
 * @details 並べ替え設定を先に適用してからディレクトリを開く (FilePane::SetPath
 * は開き直す際に現在の並べ替え設定でソートし直すため、この順序でないと
 * 一瞬古い並び順で読み込んでしまう)。ディレクトリが現在のペインと同じ場合は
 * SetPath を呼ばない (カーソル位置を 0 に戻してしまわないため。VCL 版は
 * sel_list で選択状態ごと復元するが Phase 2 骨格では未対応。報告に明記する)。
 * ディレクトリ履歴への記録はしない (VCL 版の InhDirHist++/-- と同じ意図。
 * タブの切り替えは「新しい場所への移動」ではないため)
 */
void MainFrame::ApplyTabState(const TabState &state)
{
	for (int i = 0; i < 2; ++i) {
		const PaneTabState &pane_state = state.panes[i];
		panes_[i]->SetSortSettings(pane_state.sort_key, pane_state.sort_descending, pane_state.dirs_first);

		if (!pane_state.directory.IsEmpty() && !SameText(pane_state.directory, panes_[i]->GetPath())
		    && dir_exists(pane_state.directory)) {
			panes_[i]->SetPath(pane_state.directory, /*record_history=*/false);
		}
	}
	UpdateStatus();
}

//---------------------------------------------------------------------------
/// tab_bar_ の表示 (キャプション一覧・現在位置) を更新する
void MainFrame::RefreshTabBar()
{
	if (tab_bar_ != nullptr) tab_bar_->SetTabs(tabs_.Captions(), tabs_.CurrentIndex());
}
