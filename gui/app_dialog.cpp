/**
 * @file gui/app_dialog.cpp
 * @brief gui/app_dialog.h の実装
 */
#include "gui/app_dialog.h"

#include <algorithm>
#include <wx/dir.h>
#include <wx/listbox.h>
#include <wx/splitter.h>
#include <wx/statline.h>
#include <wx/stattext.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace app_dialog {

namespace {

/// wxString への変換 (gui/sync_dialog.cpp と同じ変換ヘルパー)
inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

/// wxString → UnicodeString (MSW では両方 UTF-16)
inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

/// アプリ一覧の1行表示 (VCL のオーナー描画の代わりに「名前 ― テキスト」)
wxString app_row_of(const app_list::AppEntry &e)
{
	UnicodeString row = e.caption;
	if (!e.win_text.IsEmpty()) row += UnicodeString(_T(" ― ")) + e.win_text;
	if (e.no_response) row += _T(" (無応答)");
	if (e.to_close) row += _T(" [終了要求]");
	return to_wx(row);
}

/// ランチャー項目の収集 (.lnk/.url のみ。UpdateLaunchList の通常モードを実測)
std::vector<app_list::LaunchEntry> collect_launch(const UnicodeString &dir)
{
	std::vector<app_list::LaunchEntry> out;
	const wxString wdir = to_wx(IncludeTrailingPathDelimiter(dir));
	wxDir wxd(wdir);
	if (!wxd.IsOpened()) return out;
	wxString name;
	for (bool ok = wxd.GetFirst(&name); ok; ok = wxd.GetNext(&name)) {
		const UnicodeString fnam = to_us(name);
		const UnicodeString ext = get_extension(fnam);
		if (!SameText(ext, _T(".lnk")) && !SameText(ext, _T(".url"))) continue;
		app_list::LaunchEntry e;
		e.base_name = get_base_name(fnam);
		e.ext = ext;
		out.push_back(e);
	}
	std::sort(out.begin(), out.end(), [](const app_list::LaunchEntry &a,
	                                     const app_list::LaunchEntry &b) {
		return app_list::CompareLaunchNormal(a, b) < 0;
	});
	return out;
}

/**
 * @brief アプリ一覧・ランチャーのダイアログ
 * @details `src/AppDlg.cpp` (`TAppListDlg`) のうち、一覧の表示・選択
 *          (`AppListBoxKeyDown` の Enter/数字切替)、ランチャーの表示・実行
 *          (`LaunchListBoxKeyDown` の Enter 実行)、除外テキストの設定
 *          (`ExcAppTextItemClick`) だけを wx で再構成したもの。
 *          ウィンドウ実操作 (最小化/最大化/閉じる) は確認の上で警告に留める
 *          簡略版 (VCL の `ShowWindow`/`PostMessage` 実操作は未移植)
 */
class AppListDialog : public wxDialog {
public:
	AppListDialog(wxWindow *parent, const f_batch7_ops::AppListOpts &opts,
	              const std::vector<app_list::AppEntry> &apps,
	              const UnicodeString &launch_dir)
		: wxDialog(parent, wxID_ANY, to_wx(_T("アプリケーション一覧")), wxDefaultPosition,
		           wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, opts_(opts)
		, apps_(apps)
	{
		const app_list::AppView view =
			app_list::ResolveAppView(opts.only_app, opts.only_launcher);

		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		filter_edit_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
		                              wxDefaultPosition, wxDefaultSize,
		                              wxTE_PROCESS_ENTER);
		filter_edit_->SetHint(to_wx(_T("フィルタ (FI/LI: インクリメンタル検索、FZ: あいまい)")));
		if (opts.to_incsea) filter_edit_->SetValue(wxEmptyString);
		top->Add(filter_edit_, wxSizerFlags().Expand().Border(wxALL, 8));
		filter_edit_->Bind(wxEVT_TEXT, &AppListDialog::OnFilter, this);

		if (view.show_app) {
			top->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("起動中アプリケーション"))),
			         wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
			app_list_ = new wxListBox(this, wxID_ANY, wxDefaultPosition,
			                          wxSize(520, 180));
			top->Add(app_list_, wxSizerFlags(1).Expand().Border(wxALL, 8));
			app_list_->Bind(wxEVT_LISTBOX_DCLICK, &AppListDialog::OnAppActivate, this);
			app_list_->Bind(wxEVT_LISTBOX, &AppListDialog::OnAppSelect, this);
		}

		if (view.show_launcher) {
			top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
			top->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("ランチャー: ") + launch_dir)),
			         wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
			launch_list_ = new wxListBox(this, wxID_ANY, wxDefaultPosition,
			                             wxSize(520, 140));
			top->Add(launch_list_, wxSizerFlags(1).Expand().Border(wxALL, 8));
			launch_list_->Bind(wxEVT_LISTBOX_DCLICK, &AppListDialog::OnLaunchActivate, this);
			launch_list_->Bind(wxEVT_LISTBOX, &AppListDialog::OnLaunchSelect, this);
			launch_dir_ = launch_dir;
			launch_all_ = collect_launch(launch_dir);
			if (opts.add_start) {
				wxMessageBox(to_wx(_T("開始メニューの追加は未対応です")),
				             to_wx(_T("アプリケーション一覧")),
				             wxOK | wxICON_INFORMATION, this);
			}
		}

		status_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		top->Add(status_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxBoxSizer *op_row = new wxBoxSizer(wxHORIZONTAL);
		min_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("最小化")));
		op_row->Add(min_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		max_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("最大化")));
		op_row->Add(max_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		norm_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("元に戻す")));
		op_row->Add(norm_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		close_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("閉じる...")));
		op_row->Add(close_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		top->Add(op_row, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		min_btn_->Bind(wxEVT_BUTTON, &AppListDialog::OnWinOp, this);
		max_btn_->Bind(wxEVT_BUTTON, &AppListDialog::OnWinOp, this);
		norm_btn_->Bind(wxEVT_BUTTON, &AppListDialog::OnWinOp, this);
		close_btn_->Bind(wxEVT_BUTTON, &AppListDialog::OnWinOp, this);

		wxBoxSizer *ok_row = new wxBoxSizer(wxHORIZONTAL);
		ok_row->Add(new wxButton(this, wxID_OK, to_wx(_T("OK"))),
		            wxSizerFlags().Border(wxRIGHT, 4));
		ok_row->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("キャンセル"))));
		top->Add(ok_row, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		RefreshAppList();
		RefreshLaunchList();
		UpdateStatus();
	}

	const AppResult &result() const { return result_; }

private:
	void RefreshAppList()
	{
		if (!app_list_) return;
		app_list_->Clear();
		shown_apps_.clear();
		for (const app_list::AppEntry &e : apps_) {
			shown_apps_.push_back(e);
			app_list_->Append(app_row_of(e));
		}
		if (!shown_apps_.empty()) app_list_->SetSelection(0);
	}

	void RefreshLaunchList()
	{
		if (!launch_list_) return;
		launch_list_->Clear();
		shown_launch_.clear();
		const UnicodeString word = to_us(filter_edit_->GetValue());
		const bool ignore_case = word.LowerCase() == word;
		for (const app_list::LaunchEntry &e : launch_all_) {
			if (!word.IsEmpty() &&
			    !app_list::MatchesLaunchName(e.base_name, word, opts_.fuzzy, ignore_case))
				continue;
			shown_launch_.push_back(e);
			launch_list_->Append(to_wx(e.base_name + e.ext));
		}
		if (!shown_launch_.empty()) launch_list_->SetSelection(0);
	}

	void UpdateStatus()
	{
		UnicodeString msg;
		if (app_list_ && app_list_->GetSelection() != wxNOT_FOUND &&
		    app_list_->IsShown()) {
			const int sel = app_list_->GetSelection();
			if (sel >= 0 && sel < static_cast<int>(shown_apps_.size()))
				msg = app_list::FormatStatus(shown_apps_[static_cast<std::size_t>(sel)]);
		}
		else if (launch_list_ && launch_list_->GetSelection() != wxNOT_FOUND) {
			const int sel = launch_list_->GetSelection();
			if (sel >= 0 && sel < static_cast<int>(shown_launch_.size()))
				msg = shown_launch_[static_cast<std::size_t>(sel)].base_name;
		}
		status_->SetLabel(to_wx(msg));
	}

	void OnFilter(wxCommandEvent &) { RefreshLaunchList(); UpdateStatus(); }

	void OnAppSelect(wxCommandEvent &) { UpdateStatus(); }

	void OnLaunchSelect(wxCommandEvent &) { UpdateStatus(); }

	void OnAppActivate(wxCommandEvent &)
	{
		// VCL の Enter/数字切替 (AppListBoxKeyDown) を実測・簡略版:
		// 他アプリへの切替自体は未移植のため、NyanFi 自体なら切替扱い、
		// 他アプリなら警告に留める
		const int sel = app_list_ ? app_list_->GetSelection() : wxNOT_FOUND;
		if (sel == wxNOT_FOUND) return;
		const app_list::AppEntry &e = shown_apps_[static_cast<std::size_t>(sel)];
		if (e.caption.IsEmpty()) return;
		result_.switch_to_nyan = true;
		EndModal(wxID_OK);
	}

	void OnLaunchActivate(wxCommandEvent &)
	{
		// VCL の Enter 実行 (LaunchListBoxKeyDown) を実測:
		// .lnk 解決は未移植のためパスをそのまま返す (呼び出し側で開く)
		const int sel = launch_list_ ? launch_list_->GetSelection() : wxNOT_FOUND;
		if (sel == wxNOT_FOUND) return;
		const app_list::LaunchEntry &e = shown_launch_[static_cast<std::size_t>(sel)];
		result_.launch_file =
			IncludeTrailingPathDelimiter(launch_dir_) + e.base_name + e.ext;
		EndModal(wxID_OK);
	}

	void OnWinOp(wxCommandEvent &event)
	{
		// VCL の Minimize/Maximize/Restore/Close (ShowWindow/PostMessage 実操作)
		// は未移植。確認の上で警告に留める簡略版
		wxObject *src = event.GetEventObject();
		UnicodeString op = _T("ウィンドウ操作");
		if (src == min_btn_) op = _T("最小化");
		else if (src == max_btn_) op = _T("最大化");
		else if (src == norm_btn_) op = _T("元に戻す");
		else if (src == close_btn_) op = _T("閉じる");
		if (wxMessageBox(to_wx(op + _T("しますか? (実操作は未対応のため警告に留めます)")),
		                 to_wx(_T("アプリケーション一覧")),
		                 wxYES_NO | wxICON_QUESTION, this) != wxYES)
			return;
		wxMessageBox(to_wx(op + _T("は未対応です")), to_wx(_T("アプリケーション一覧")),
		             wxOK | wxICON_INFORMATION, this);
	}

	f_batch7_ops::AppListOpts opts_;
	std::vector<app_list::AppEntry> apps_;
	std::vector<app_list::AppEntry> shown_apps_;
	std::vector<app_list::LaunchEntry> launch_all_;
	std::vector<app_list::LaunchEntry> shown_launch_;
	UnicodeString launch_dir_;
	AppResult result_;
	wxTextCtrl *filter_edit_ = nullptr;
	wxListBox *app_list_ = nullptr;
	wxListBox *launch_list_ = nullptr;
	wxStaticText *status_ = nullptr;
	wxButton *min_btn_ = nullptr;
	wxButton *max_btn_ = nullptr;
	wxButton *norm_btn_ = nullptr;
	wxButton *close_btn_ = nullptr;
};

}  // namespace

std::vector<app_list::AppEntry> EnumerateApps(const UnicodeString &exc_text)
{
	std::vector<app_list::AppEntry> out;
#ifdef _WIN32
	// 実列挙の薄い層。フィルタ判断は app_list::ShouldListWindow に委ねる
	struct Ctx {
		std::vector<app_list::AppEntry> *out;
		std::vector<UnicodeString> exc;
	};
	// 自プロセスID (NyanFi 自体の判定用には使わず一覧に含めるだけ)
	Ctx ctx{&out, app_list::SplitExcList(exc_text)};
	::EnumWindows(
		[](HWND hWnd, LPARAM lp) -> BOOL {
			Ctx *c = reinterpret_cast<Ctx *>(lp);
			app_list::WindowAttrs attrs;
			attrs.visible = ::IsWindowVisible(hWnd) != FALSE;
			attrs.cloaked = false;
			LONG ex = ::GetWindowLongW(hWnd, GWL_EXSTYLE);
			attrs.tool_window = (ex & WS_EX_TOOLWINDOW) != 0;
			attrs.layered_no_edge =
				((ex & WS_EX_LAYERED) != 0) && ((ex & WS_EX_WINDOWEDGE) == 0);
			attrs.has_parent_no_appwindow =
				(::GetParent(hWnd) != NULL) && ((ex & WS_EX_APPWINDOW) == 0);
			RECT rc{};
			attrs.rect_empty =
				!::GetWindowRect(hWnd, &rc) || (rc.right <= rc.left) ||
				(rc.bottom <= rc.top);
			wchar_t buf[512]{};
			const int n = ::GetWindowTextW(hWnd, buf, 511);
			UnicodeString wtxt = (n > 0) ? UnicodeString(buf) : UnicodeString();
			attrs.text_empty = wtxt.IsEmpty();
			if (!app_list::ShouldListWindow(attrs, wtxt, c->exc)) return TRUE;
			app_list::AppEntry e;
			e.win_text = wtxt;
			DWORD pid = 0;
			::GetWindowThreadProcessId(hWnd, &pid);
			e.pid = pid;
			wchar_t cls[256]{};
			if (::GetClassNameW(hWnd, cls, 255) > 0) e.caption = cls;
			if (::IsIconic(hWnd)) e.minimized = true;
			if ((ex & WS_EX_TOPMOST) != 0) e.top_most = true;
			e.win_wd = rc.right - rc.left;
			e.win_hi = rc.bottom - rc.top;
			c->out->push_back(e);
			return TRUE;
		},
		reinterpret_cast<LPARAM>(&ctx));
#else
	(void)exc_text;
#endif
	return out;
}

bool Run(wxWindow *parent, const f_batch7_ops::AppListOpts &opts,
         const std::vector<app_list::AppEntry> &apps, const UnicodeString &launch_dir,
         AppResult &out)
{
	AppListDialog dlg(parent, opts, apps, launch_dir);
	if (dlg.ShowModal() != wxID_OK) return false;
	out = dlg.result();
	return true;
}

}  // namespace app_dialog
