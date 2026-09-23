/**
 * @file gui/edit_hist_dialog.cpp
 * @brief gui/edit_hist_dialog.h の実装
 */
#include "gui/edit_hist_dialog.h"

#include <algorithm>
#include <vector>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/listctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>

#include "usr_file_ex.h"
#include "usr_str.h"

namespace edit_hist_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

UnicodeString stamp_of(const UnicodeString &path)
{
	if (path.IsEmpty() || (!file_exists(path) && !dir_exists(path))) return EmptyStr;
	return DateTimeToStr(get_file_age(path));
}

//---------------------------------------------------------------------------
/**
 * @brief 編集履歴の一覧画面
 * @details VCL の `UpdateList`/`UpdateGrid`/`del_HistItem` の主要ブロックを
 *          wxListCtrl に移した。削除は HistoryList に対して即時反映し、
 *          MainFrame が終了時に SaveToIni できる状態で返す。
 */
class EditHistoryDialog final : public wxDialog {
public:
	EditHistoryDialog(wxWindow *parent, history::HistoryList &history, const Input &input)
		: wxDialog(parent, wxID_ANY, to_wx(edit_hist::ModeTitle(input.preferences.mode)),
		           wxDefaultPosition, wxSize(860, 560),
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
		  history_(history),
		  input_(input),
		  result_(Result{})
	{
		result_.preferences = input.preferences;
		CreateControls();
		BindEvents();
		RefreshRows();
		SetEscapeId(wxID_CANCEL);

		if (input_.focus_filter) {
			filter_ctrl_->SetFocus();
			filter_ctrl_->SelectAll();
		}
		else if (list_->GetItemCount() > 0) {
			list_->SetFocus();
		}
		CentreOnParent();
	}

	Result ResultValue()
	{
		SyncPreferences();
		return result_;
	}

private:
	enum {
		ID_DELETE = wxID_HIGHEST + 1,
		ID_CLEAR,
		ID_OPEN,
		ID_EXCLUDED,
	};

	void CreateControls()
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		list_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		                       wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxBORDER_SIMPLE);
		list_->AppendColumn(_T(""), wxLIST_FORMAT_LEFT, 48);
		list_->AppendColumn(_T("名前"), wxLIST_FORMAT_LEFT, 230);
		list_->AppendColumn(_T("更新日時"), wxLIST_FORMAT_LEFT, 150);
		list_->AppendColumn(_T("場所"), wxLIST_FORMAT_LEFT, 390);
		top->Add(list_, wxSizerFlags(1).Expand().Border(wxALL, 8));

		wxBoxSizer *filter_row = new wxBoxSizer(wxHORIZONTAL);
		filter_row->Add(new wxStaticText(this, wxID_ANY, _T("フィルタ")),
		                wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		const int width = std::clamp(input_.preferences.filter_width, 60, 1000);
		filter_ctrl_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                              wxSize(width, -1), wxTE_PROCESS_ENTER);
		filter_row->Add(filter_ctrl_, wxSizerFlags(1).Expand().Border(wxRIGHT, 8));
		migemo_chk_ = new wxCheckBox(this, wxID_ANY, _T("Migemo"));
		migemo_chk_->SetValue(input_.preferences.migemo);
		filter_row->Add(migemo_chk_, wxSizerFlags().CentreVertical().Border(wxRIGHT, 12));
		mode_ctrl_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		                           3);
		mode_ctrl_->Append(_T("すべて表示"));
		mode_ctrl_->Append(_T("現在の場所以下"));
		mode_ctrl_->Append(_T("現在の場所のみ"));
		mode_ctrl_->SetSelection(edit_hist::ModeIndex(input_.preferences.mode));
		filter_row->Add(mode_ctrl_, wxSizerFlags().CentreVertical().Border(wxRIGHT, 12));
		status_chk_ = new wxCheckBox(this, wxID_ANY, _T("ステータス"));
		status_chk_->SetValue(input_.preferences.status_bar);
		filter_row->Add(status_chk_, wxSizerFlags().CentreVertical());
		top->Add(filter_row, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxBoxSizer *tools = new wxBoxSizer(wxHORIZONTAL);
		delete_btn_ = new wxButton(this, ID_DELETE, _T("選択項目を削除"));
		tools->Add(delete_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		clear_btn_ = new wxButton(this, ID_CLEAR, _T("履歴をすべて削除"));
		tools->Add(clear_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		open_btn_ = new wxButton(this, ID_OPEN, _T("エディタで開く"));
		tools->Add(open_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		excluded_btn_ = new wxButton(this, ID_EXCLUDED, _T("表示しないパス..."));
		tools->Add(excluded_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		tools->AddStretchSpacer();
		tools->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags());
		top->Add(tools, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		status_ctrl_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		top->Add(status_ctrl_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));

		wxStaticText *note = new wxStaticText(
			this, wxID_ANY,
			_T("未移植 (未実装扱い): Migemo辞書、OwnerDraw色/アイコン、")
			_T("列幅保存・ヘッダソート、ファイル情報/プロパティ、")
			_T("RecentList/栞マーク/リポジトリ/タグ画面"));
		note->Wrap(800);
		top->Add(note, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		SetSizerAndFit(top);
		SetMinSize(wxSize(680, 430));
	}

	void BindEvents()
	{
		filter_ctrl_->Bind(wxEVT_TEXT, &EditHistoryDialog::OnFilterChanged, this);
		migemo_chk_->Bind(wxEVT_CHECKBOX, &EditHistoryDialog::OnFilterChanged, this);
		mode_ctrl_->Bind(wxEVT_CHOICE, &EditHistoryDialog::OnFilterChanged, this);
		status_chk_->Bind(wxEVT_CHECKBOX, &EditHistoryDialog::OnStatusChanged, this);
		list_->Bind(wxEVT_LIST_ITEM_SELECTED, &EditHistoryDialog::OnSelectionChanged, this);
		list_->Bind(wxEVT_LIST_ITEM_FOCUSED, &EditHistoryDialog::OnSelectionChanged, this);
		list_->Bind(wxEVT_LIST_ITEM_ACTIVATED, &EditHistoryDialog::OnActivated, this);
		delete_btn_->Bind(wxEVT_BUTTON, &EditHistoryDialog::OnDelete, this);
		clear_btn_->Bind(wxEVT_BUTTON, &EditHistoryDialog::OnClearAll, this);
		open_btn_->Bind(wxEVT_BUTTON, &EditHistoryDialog::OnOpen, this);
		excluded_btn_->Bind(wxEVT_BUTTON, &EditHistoryDialog::OnExcludedPaths, this);
		Bind(wxEVT_BUTTON, &EditHistoryDialog::OnOk, this, wxID_OK);
		Bind(wxEVT_BUTTON, &EditHistoryDialog::OnCancel, this, wxID_CANCEL);
	}

	void SyncPreferences()
	{
		result_.preferences.mode = edit_hist::ModeFromIndex(mode_ctrl_->GetSelection());
		result_.preferences.migemo = migemo_chk_->GetValue();
		result_.preferences.status_bar = status_chk_->GetValue();
		result_.preferences.filter_width = filter_ctrl_->GetSize().GetWidth();
	}

	edit_hist::Context ContextFromControls() const
	{
		edit_hist::Context context;
		context.current_path = input_.current_path;
		context.mode = edit_hist::ModeFromIndex(mode_ctrl_->GetSelection());
		context.filter = to_us(filter_ctrl_->GetValue());
		context.migemo = migemo_chk_->GetValue();
		return context;
	}

	int SelectedRow() const
	{
		const long row = list_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		return row < 0? -1 : static_cast<int>(row);
	}

	const edit_hist::Entry *SelectedEntry() const
	{
		const int row = SelectedRow();
		return row < 0 || static_cast<std::size_t>(row) >= rows_.size()
		           ? nullptr : &rows_[static_cast<std::size_t>(row)];
	}

	void SelectPath(const UnicodeString &path)
	{
		for (std::size_t i = 0; i < rows_.size(); ++i) {
			if (!SameText(rows_[i].path, path)) continue;
			const long row = static_cast<long>(i);
			list_->SetItemState(row, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
			                    wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
			list_->EnsureVisible(row);
			return;
		}
		if (!rows_.empty()) {
			list_->SetItemState(0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
			                    wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
		}
		UpdateStatus();
	}

	void RefreshRows()
	{
		const UnicodeString old_path = SelectedEntry() != nullptr ? SelectedEntry()->path : EmptyStr;
		rows_ = edit_hist::BuildEntries(history_, ContextFromControls());
		list_->DeleteAllItems();
		for (std::size_t i = 0; i < rows_.size(); ++i) {
			const edit_hist::Entry &entry = rows_[i];
			UnicodeString number;
			if (i < 10) number.sprintf(_T("%d"), static_cast<int>((i + 1) % 10));
			const long row = list_->InsertItem(static_cast<long>(i), to_wx(number));
			list_->SetItem(row, 1, to_wx(entry.name));
			list_->SetItem(row, 2, to_wx(stamp_of(entry.path)));
			list_->SetItem(row, 3, to_wx(entry.location));
		}
		SelectPath(old_path);
		UpdateStatus();
	}

	void UpdateStatus()
	{
		const int selected = SelectedRow() >= 0 ? 1 : 0;
		status_ctrl_->SetLabel(to_wx(edit_hist::StatusText(static_cast<int>(rows_.size()),
		                                                          static_cast<int>(history_.Entries().size()),
		                                                          selected)));
		status_ctrl_->Show(status_chk_->GetValue());
		delete_btn_->Enable(edit_hist::CanDelete(SelectedRow(), static_cast<int>(rows_.size())));
		clear_btn_->Enable(edit_hist::CanClearAll(static_cast<int>(history_.Entries().size())));
		open_btn_->Enable(SelectedEntry() != nullptr);
		Layout();
	}

	void OnFilterChanged(wxCommandEvent &event)
	{
		RefreshRows();
		event.Skip();
	}

	void OnStatusChanged(wxCommandEvent &event)
	{
		UpdateStatus();
		event.Skip();
	}

	void OnSelectionChanged(wxListEvent &event)
	{
		UpdateStatus();
		event.Skip();
	}

	void OnDelete(wxCommandEvent &event)
	{
		const edit_hist::Entry *entry = SelectedEntry();
		if (entry == nullptr) {
			wxBell();
			return;
		}
		if (wxMessageBox(to_wx(_T("選択項目を編集履歴から削除しますか?")),
		                 to_wx(_T("編集履歴の削除")), wxYES_NO | wxICON_QUESTION, this) != wxYES) {
			return;
		}
		result_.changed |= edit_hist::RemoveEntry(history_, entry->path) > 0;
		RefreshRows();
		event.Skip();
	}

	void OnClearAll(wxCommandEvent &event)
	{
		if (history_.Entries().empty()) return;
		if (wxMessageBox(to_wx(_T("編集履歴をすべて削除しますか?")),
		                 to_wx(_T("編集履歴の削除")), wxYES_NO | wxICON_QUESTION, this) != wxYES) {
			return;
		}
		history_.Clear();
		result_.changed = true;
		RefreshRows();
		event.Skip();
	}

	void OnExcludedPaths(wxCommandEvent &event)
	{
		wxTextEntryDialog dlg(this, _T("表示しないパスの設定 (部分一致、; 区切り)"),
		                       _T("編集履歴の設定"), to_wx(result_.preferences.excluded_paths));
		if (dlg.ShowModal() != wxID_OK) return;
		result_.preferences.excluded_paths = to_us(dlg.GetValue()).Trim();
		result_.changed |= edit_hist::ApplyExcludedPaths(history_, result_.preferences.excluded_paths) > 0;
		RefreshRows();
		event.Skip();
	}

	void Finish(Action action)
	{
		const edit_hist::Entry *entry = SelectedEntry();
		if (entry == nullptr) {
			wxBell();
			return;
		}
		SyncPreferences();
		result_.action = action;
		result_.path = entry->path;
		result_.accepted = true;
		EndModal(wxID_OK);
	}

	void OnActivated(wxListEvent &event)
	{
		Finish(Action::Open);
		event.Skip();
	}

	void OnOpen(wxCommandEvent &event)
	{
		Finish(Action::Open);
		event.Skip();
	}

	void OnOk(wxCommandEvent &event)
	{
		Finish(Action::Move);
		event.Skip();
	}

	void OnCancel(wxCommandEvent &event)
	{
		SyncPreferences();
		result_.accepted = false;
		result_.action = Action::None;
		result_.path = EmptyStr;
		EndModal(wxID_CANCEL);
		event.Skip();
	}

	history::HistoryList &history_;
	Input input_;
	Result result_;
	std::vector<edit_hist::Entry> rows_;
	wxListCtrl *list_ = nullptr;
	wxTextCtrl *filter_ctrl_ = nullptr;
	wxCheckBox *migemo_chk_ = nullptr;
	wxCheckBox *status_chk_ = nullptr;
	wxChoice *mode_ctrl_ = nullptr;
	wxButton *delete_btn_ = nullptr;
	wxButton *clear_btn_ = nullptr;
	wxButton *open_btn_ = nullptr;
	wxButton *excluded_btn_ = nullptr;
	wxStaticText *status_ctrl_ = nullptr;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, history::HistoryList &history, const Input &input, Result &out)
{
	EditHistoryDialog dlg(parent, history, input);
	dlg.ShowModal();
	out = dlg.ResultValue();
	return out.accepted;
}

}  // namespace edit_hist_dialog
