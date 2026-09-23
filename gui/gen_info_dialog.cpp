/**
 * @file gui/gen_info_dialog.cpp
 * @brief gui/gen_info_dialog.h の実装
 */
#include "gui/gen_info_dialog.h"

#include <algorithm>

#include <wx/checkbox.h>
#include <wx/clipbrd.h>
#include <wx/filedlg.h>
#include <wx/file.h>
#include <wx/listctrl.h>
#include <wx/menu.h>
#include <wx/statline.h>
#include <wx/statusbr.h>
#include <wx/textctrl.h>

namespace gen_info_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

class GenInfoDialog final : public wxDialog {
public:
	GenInfoDialog(wxWindow *parent, const Input &input)
		: wxDialog(parent, wxID_ANY, to_wx(input.title.IsEmpty()? _T("一覧") : input.title),
		           wxDefaultPosition, wxSize(900, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
		  original_(input.lines), kind_(gen_info::DetectKind(input.lines, input.kind)),
		  clear_source_(input.clear_source)
	{
		CreateControls();
		BindEvents();
		SetEscapeId(wxID_CANCEL);
		RefreshRows();
		if (input.focus_filter) {
			filter_ctrl_->SetFocus();
			filter_ctrl_->SelectAll();
		}
		else if (list_ctrl_->GetItemCount() > 0) list_ctrl_->SetFocus();
		CentreOnParent();
	}

	bool TakeResult(Result &out) const
	{
		out.primary = EmptyStr;
		out.selected.clear();
		const std::vector<int> selected = SelectedIndices();
		if (selected.empty()) return false;
		for (int index : selected) {
			if (index >= 0 && static_cast<std::size_t>(index) < entries_.size()) {
				out.selected.push_back(entries_[static_cast<std::size_t>(index)].text);
			}
		}
		if (out.selected.empty()) return false;
		const UnicodeString &line = entries_[static_cast<std::size_t>(selected.front())].text;
		out.primary = kind_ == gen_info::Kind::CommandHistory ? gen_info::CommandText(line) : line;
		return true;
	}

private:
	enum {
		ID_COPY = wxID_HIGHEST + 1,
		ID_COPY_VALUE,
		ID_COPY_COMMAND,
		ID_SELECT_ALL,
		ID_SAVE,
		ID_SORT_ASC,
		ID_SORT_DESC,
		ID_SORT_ORIGINAL,
		ID_DELETE_DUPLICATES,
		ID_RESTORE,
		ID_CLEAR,
	};

	void CreateControls()
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
		list_ctrl_ = new wxListView(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		                            wxLC_REPORT | wxLC_HRULES | wxBORDER_SIMPLE);
		list_ctrl_->AppendColumn(_T("行"), wxLIST_FORMAT_LEFT, 64);
		list_ctrl_->AppendColumn(_T("内容"), wxLIST_FORMAT_LEFT, 760);
		top->Add(list_ctrl_, wxSizerFlags(1).Expand().Border(wxALL, 8));
		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));

		wxBoxSizer *filter_box = new wxBoxSizer(wxHORIZONTAL);
		filter_box->Add(new wxStaticText(this, wxID_ANY, _T("フィルタ")), wxSizerFlags().CentreVertical());
		filter_ctrl_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                               wxSize(240, -1), wxTE_PROCESS_ENTER);
		filter_box->Add(filter_ctrl_, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT, 6));
		any_term_chk_ = new wxCheckBox(this, wxID_ANY, _T("AND/OR"));
		case_chk_ = new wxCheckBox(this, wxID_ANY, _T("大小文字を区別"));
		line_no_chk_ = new wxCheckBox(this, wxID_ANY, _T("行番号"));
		line_no_chk_->SetValue(true);
		filter_box->Add(any_term_chk_, wxSizerFlags().CentreVertical().Border(wxRIGHT, 8));
		filter_box->Add(case_chk_, wxSizerFlags().CentreVertical().Border(wxRIGHT, 8));
		filter_box->Add(line_no_chk_, wxSizerFlags().CentreVertical());
		if (kind_ == gen_info::Kind::Log) {
			errors_chk_ = new wxCheckBox(this, wxID_ANY, _T("エラー部分のみ"));
			filter_box->Add(errors_chk_, wxSizerFlags().CentreVertical().Border(wxLEFT, 8));
		}
		top->Add(filter_box, wxSizerFlags().Expand().Border(wxALL, 8));

		wxBoxSizer *actions = new wxBoxSizer(wxHORIZONTAL);
		actions->Add(new wxButton(this, ID_COPY, _T("コピー")), wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(new wxButton(this, ID_SAVE, _T("保存...")), wxSizerFlags().Border(wxRIGHT, 12));
		actions->Add(new wxButton(this, ID_SORT_ASC, _T("昇順")), wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(new wxButton(this, ID_SORT_DESC, _T("降順")), wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(new wxButton(this, ID_SORT_ORIGINAL, _T("元順")), wxSizerFlags().Border(wxRIGHT, 12));
		actions->Add(new wxButton(this, ID_DELETE_DUPLICATES, _T("重複除去")), wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(new wxButton(this, ID_RESTORE, _T("再構築")));
		if (clear_source_) actions->Add(new wxButton(this, ID_CLEAR, _T("履歴を消去")));
		actions->AddStretchSpacer();
		actions->Add(new wxButton(this, wxID_CANCEL, _T("キャンセル")));
		actions->Add(new wxButton(this, wxID_OK, _T("OK")));
		top->Add(actions, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		status_ctrl_ = new wxStatusBar(this, wxID_ANY);
		int widths[1] = {-1};
		status_ctrl_->SetStatusWidths(1, widths);
		top->Add(status_ctrl_, wxSizerFlags().Expand());
		SetSizerAndFit(top);
		SetMinSize(wxSize(640, 400));
	}

	void BindEvents()
	{
		list_ctrl_->Bind(wxEVT_LIST_ITEM_SELECTED, &GenInfoDialog::OnItemChanged, this);
		list_ctrl_->Bind(wxEVT_LIST_ITEM_DESELECTED, &GenInfoDialog::OnItemChanged, this);
		list_ctrl_->Bind(wxEVT_LIST_ITEM_FOCUSED, &GenInfoDialog::OnItemChanged, this);
		list_ctrl_->Bind(wxEVT_LIST_ITEM_ACTIVATED, &GenInfoDialog::OnActivate, this);
		list_ctrl_->Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &GenInfoDialog::OnRightClick, this);
		filter_ctrl_->Bind(wxEVT_TEXT, &GenInfoDialog::OnFilterChanged, this);
		any_term_chk_->Bind(wxEVT_CHECKBOX, &GenInfoDialog::OnFilterChanged, this);
		case_chk_->Bind(wxEVT_CHECKBOX, &GenInfoDialog::OnFilterChanged, this);
		if (errors_chk_) errors_chk_->Bind(wxEVT_CHECKBOX, &GenInfoDialog::OnFilterChanged, this);
		line_no_chk_->Bind(wxEVT_CHECKBOX, &GenInfoDialog::OnLineNumberChanged, this);

		Bind(wxEVT_BUTTON, &GenInfoDialog::OnCopy, this, ID_COPY);
		Bind(wxEVT_BUTTON, &GenInfoDialog::OnSave, this, ID_SAVE);
		Bind(wxEVT_BUTTON, &GenInfoDialog::OnSortAsc, this, ID_SORT_ASC);
		Bind(wxEVT_BUTTON, &GenInfoDialog::OnSortDesc, this, ID_SORT_DESC);
		Bind(wxEVT_BUTTON, &GenInfoDialog::OnSortOriginal, this, ID_SORT_ORIGINAL);
		Bind(wxEVT_BUTTON, &GenInfoDialog::OnDeleteDuplicates, this, ID_DELETE_DUPLICATES);
		Bind(wxEVT_BUTTON, &GenInfoDialog::OnRestore, this, ID_RESTORE);
		Bind(wxEVT_BUTTON, &GenInfoDialog::OnClear, this, ID_CLEAR);
		Bind(wxEVT_BUTTON, &GenInfoDialog::OnAccept, this, wxID_OK);
		Bind(wxEVT_MENU, &GenInfoDialog::OnCopy, this, ID_COPY);
		Bind(wxEVT_MENU, &GenInfoDialog::OnCopyValue, this, ID_COPY_VALUE);
		Bind(wxEVT_MENU, &GenInfoDialog::OnCopyCommand, this, ID_COPY_COMMAND);
		Bind(wxEVT_MENU, &GenInfoDialog::OnSelectAll, this, ID_SELECT_ALL);
		Bind(wxEVT_MENU, &GenInfoDialog::OnSave, this, ID_SAVE);
		Bind(wxEVT_MENU, &GenInfoDialog::OnSortAsc, this, ID_SORT_ASC);
		Bind(wxEVT_MENU, &GenInfoDialog::OnSortDesc, this, ID_SORT_DESC);
		Bind(wxEVT_MENU, &GenInfoDialog::OnSortOriginal, this, ID_SORT_ORIGINAL);
		Bind(wxEVT_MENU, &GenInfoDialog::OnDeleteDuplicates, this, ID_DELETE_DUPLICATES);
		Bind(wxEVT_MENU, &GenInfoDialog::OnRestore, this, ID_RESTORE);
		Bind(wxEVT_MENU, &GenInfoDialog::OnClear, this, ID_CLEAR);
	}

	void UpdateOptions()
	{
		options_.keyword = to_us(filter_ctrl_->GetValue());
		options_.any_term = any_term_chk_->GetValue();
		options_.case_sensitive = case_chk_->GetValue();
		options_.errors_only = errors_chk_ && errors_chk_->GetValue();
	}

	void PopulateList()
	{
		list_ctrl_->DeleteAllItems();
		list_ctrl_->SetColumnWidth(0, line_no_chk_->GetValue()? 64 : 0);
		for (std::size_t i = 0; i < entries_.size(); ++i) {
			const gen_info::Entry &entry = entries_[i];
			const wxString number = line_no_chk_->GetValue()
				? to_wx(UnicodeString().sprintf(_T("%d"), entry.source_index + 1)) : wxString();
			const long row = list_ctrl_->InsertItem(static_cast<long>(i), number);
			list_ctrl_->SetItem(row, 1, to_wx(gen_info::DisplayText(entry.text, kind_)));
		}
	}

	void RefreshRows()
	{
		const int old_focus = list_ctrl_->GetFocusedItem();
		const int old_source = old_focus >= 0 && static_cast<std::size_t>(old_focus) < entries_.size()
		                         ? entries_[static_cast<std::size_t>(old_focus)].source_index : -1;
		UpdateOptions();
		entries_ = gen_info::BuildEntries(original_, kind_, options_);
		gen_info::SortEntries(entries_, sort_mode_);
		PopulateList();
		int focus = 0;
		if (old_source >= 0) {
			for (std::size_t i = 0; i < entries_.size(); ++i) {
				if (entries_[i].source_index == old_source) { focus = static_cast<int>(i); break; }
			}
		}
		if (!entries_.empty()) {
			list_ctrl_->SetItemState(focus, wxLIST_STATE_FOCUSED | wxLIST_STATE_SELECTED,
			                          wxLIST_STATE_FOCUSED | wxLIST_STATE_SELECTED);
			list_ctrl_->EnsureVisible(focus);
		}
		UpdateStatus();
	}

	void UpdateStatus()
	{
		const bool filtered = options_.errors_only || !options_.keyword.Trim().IsEmpty();
		status_ctrl_->SetStatusText(to_wx(gen_info::StatusText(static_cast<int>(entries_.size()),
		                                                       static_cast<int>(original_.size()),
		                                                       list_ctrl_->GetSelectedItemCount(),
		                                                       list_ctrl_->GetFocusedItem(), filtered,
		                                                       options_.errors_only)), 0);
	}

	std::vector<int> SelectedIndices() const
	{
		std::vector<int> result;
		for (long i = list_ctrl_->GetFirstSelected(); i != -1; i = list_ctrl_->GetNextSelected(i)) {
			result.push_back(static_cast<int>(i));
		}
		return result;
	}

	bool PutClipboard(const UnicodeString &text) const
	{
		if (text.IsEmpty() || !wxTheClipboard->Open()) return false;
		const bool ok = wxTheClipboard->SetData(new wxTextDataObject(to_wx(text)));
		wxTheClipboard->Close();
		return ok;
	}

	void OnItemChanged(wxListEvent & /*event*/) { UpdateStatus(); }
	void OnFilterChanged(wxCommandEvent & /*event*/) { RefreshRows(); }
	void OnLineNumberChanged(wxCommandEvent & /*event*/) { RefreshRows(); }
	void OnActivate(wxListEvent & /*event*/) { AcceptSelected(); }
	void OnAccept(wxCommandEvent & /*event*/) { AcceptSelected(); }

	void AcceptSelected()
	{
		if (SelectedIndices().empty()) { wxBell(); return; }
		EndModal(wxID_OK);
	}

	void OnCopy(wxCommandEvent & /*event*/)
	{
		const std::vector<int> selected = SelectedIndices();
		if (selected.empty() || !PutClipboard(gen_info::JoinEntries(entries_, selected, kind_))) {
			wxBell();
		}
	}

	void OnCopyValue(wxCommandEvent & /*event*/)
	{
		const int index = list_ctrl_->GetFocusedItem();
		if (index < 0 || static_cast<std::size_t>(index) >= entries_.size()) { wxBell(); return; }
		const UnicodeString value = gen_info::ValueText(entries_[static_cast<std::size_t>(index)].text);
		if (value.IsEmpty() || !PutClipboard(value)) wxBell();
	}

	void OnCopyCommand(wxCommandEvent & /*event*/)
	{
		UnicodeString text;
		for (int index : SelectedIndices()) {
			const UnicodeString command = gen_info::CommandText(entries_[static_cast<std::size_t>(index)].text);
			if (command.IsEmpty()) continue;
			if (!text.IsEmpty()) text += _T("\r\n");
			text += command;
		}
		if (text.IsEmpty() || !PutClipboard(text)) wxBell();
	}

	void OnSelectAll(wxCommandEvent & /*event*/)
	{
		for (long i = 0; i < list_ctrl_->GetItemCount(); ++i) {
			list_ctrl_->SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		}
		UpdateStatus();
	}

	void OnSave(wxCommandEvent & /*event*/)
	{
		wxFileDialog dlg(this, _T("一覧をファイルに保存"), wxEmptyString, _T("list.txt"),
		                 _T("テキスト (*.txt)|*.txt|すべてのファイル (*.*)|*.*"),
		                 wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
		if (dlg.ShowModal() != wxID_OK) return;
		std::vector<int> all;
		for (std::size_t i = 0; i < entries_.size(); ++i) all.push_back(static_cast<int>(i));
		wxFile file;
		if (!file.Create(dlg.GetPath(), true) ||
		    !file.Write(wxString::FromUTF8("\xEF\xBB\xBF") +
		                to_wx(gen_info::JoinEntries(entries_, all, kind_)), wxConvUTF8)) {
			wxMessageBox(_T("保存できませんでした"), _T("保存"), wxOK | wxICON_ERROR, this);
		}
	}

	void SetSortMode(gen_info::SortMode mode) { sort_mode_ = mode; RefreshRows(); }
	void OnSortAsc(wxCommandEvent & /*event*/) { SetSortMode(gen_info::SortMode::Ascending); }
	void OnSortDesc(wxCommandEvent & /*event*/) { SetSortMode(gen_info::SortMode::Descending); }
	void OnSortOriginal(wxCommandEvent & /*event*/) { SetSortMode(gen_info::SortMode::Original); }

	void OnDeleteDuplicates(wxCommandEvent & /*event*/)
	{
		entries_ = gen_info::RemoveDuplicates(entries_);
		gen_info::SortEntries(entries_, sort_mode_);
		PopulateList();
		if (!entries_.empty()) {
			list_ctrl_->SetItemState(0, wxLIST_STATE_FOCUSED | wxLIST_STATE_SELECTED,
			                          wxLIST_STATE_FOCUSED | wxLIST_STATE_SELECTED);
		}
		UpdateStatus();
	}

	void OnRestore(wxCommandEvent & /*event*/)
	{
		sort_mode_ = gen_info::SortMode::Original;
		RefreshRows();
	}

	void OnClear(wxCommandEvent & /*event*/)
	{
		if (!clear_source_) return;
		if (wxMessageBox(_T("履歴をすべて消去しますか?"), _T("確認"),
		                 wxYES_NO | wxICON_QUESTION, this) != wxYES) return;
		clear_source_();
		original_.clear();
		entries_.clear();
		list_ctrl_->DeleteAllItems();
		UpdateStatus();
	}

	void OnRightClick(wxListEvent &event)
	{
		wxMenu menu;
		menu.Append(ID_COPY, _T("選択行をコピー"));
		if (kind_ == gen_info::Kind::Variable) menu.Append(ID_COPY_VALUE, _T("値をコピー"));
		if (kind_ == gen_info::Kind::CommandHistory) menu.Append(ID_COPY_COMMAND, _T("コマンドをコピー"));
		menu.AppendSeparator();
		menu.Append(ID_SELECT_ALL, _T("すべて選択"));
		menu.Append(ID_SORT_ASC, _T("昇順"));
		menu.Append(ID_SORT_DESC, _T("降順"));
		menu.Append(ID_SORT_ORIGINAL, _T("元の順序"));
		menu.Append(ID_DELETE_DUPLICATES, _T("重複行を削除"));
		menu.Append(ID_RESTORE, _T("一覧を再構築"));
		menu.AppendSeparator();
		menu.Append(ID_SAVE, _T("一覧を保存..."));
		if (clear_source_) menu.Append(ID_CLEAR, _T("履歴を消去"));
		const wxPoint screen = list_ctrl_->ClientToScreen(event.GetPoint());
		PopupMenu(&menu, ScreenToClient(screen));
	}

	std::vector<UnicodeString> original_;
	gen_info::Kind kind_ = gen_info::Kind::Generic;
	gen_info::FilterOptions options_;
	gen_info::SortMode sort_mode_ = gen_info::SortMode::Original;
	std::vector<gen_info::Entry> entries_;
	std::function<void()> clear_source_;
	wxListView *list_ctrl_ = nullptr;
	wxStatusBar *status_ctrl_ = nullptr;
	wxTextCtrl *filter_ctrl_ = nullptr;
	wxCheckBox *any_term_chk_ = nullptr;
	wxCheckBox *case_chk_ = nullptr;
	wxCheckBox *line_no_chk_ = nullptr;
	wxCheckBox *errors_chk_ = nullptr;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, const Input &input, Result &result_out)
{
	GenInfoDialog dlg(parent, input);
	if (dlg.ShowModal() != wxID_OK) return false;
	return dlg.TakeResult(result_out);
}

}  // namespace gen_info_dialog
