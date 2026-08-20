/**
 * @file gui/main_frame.cpp
 * @brief メインウィンドウの実装
 */
#include "gui/main_frame.h"

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
#include "gui/file_ops.h"
#include "gui/grep_dialog.h"
#include "gui/image_load.h"
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
	else if (SameStr(command, _T("UpDir")) || SameStr(command, _T("ToLeft"))) {
		pane->GoParent();
	}
	else if (SameStr(command, _T("ToRight"))) {
		pane->EnterCurrent();
	}
	else if (SameStr(command, _T("ChangePane"))) {
		SetActivePane(1 - active_);
	}
	else if (SameStr(command, _T("Refresh"))) {
		pane->Reload();
	}
	else if (SameStr(command, _T("MarkItem"))) {
		pane->ToggleMark();
	}
	else if (SameStr(command, _T("MarkAll"))) {
		pane->MarkAll(true);
	}
	else if (SameStr(command, _T("UnMarkAll"))) {
		pane->MarkAll(false);
	}
	else if (SameStr(command, _T("ShowKeyList"))) {
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
/// 名前等の変更 (R)。カーソル位置の1件のみ (複数選択には対応しない)
void MainFrame::CmdRenameDlg()
{
	FilePane *pane = ActivePane();
	const FileItem *itm = pane->GetCurrentItem();
	if (itm == nullptr || itm->is_parent) {
		wxMessageBox(to_wx(_T("対象がありません")), to_wx(_T("名前の変更")), wxOK | wxICON_INFORMATION, this);
		return;
	}

	wxTextEntryDialog dlg(this, to_wx(_T("新しい名前")), to_wx(_T("名前等の変更")), to_wx(itm->name));
	if (dlg.ShowModal() != wxID_OK) return;

	const UnicodeString new_name = to_us(dlg.GetValue());
	UnicodeString error;
	if (!file_ops::RenameItem(pane->GetPath(), itm->name, new_name, error)) {
		wxMessageBox(to_wx(error), to_wx(_T("名前を変更できません")), wxOK | wxICON_ERROR, this);
		return;
	}

	pane->Reload();
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
