/**
 * @file gui/function_list_dialog.cpp
 * @brief gui/function_list_dialog.h の実装
 */
#include "gui/function_list_dialog.h"

#include <wx/checkbox.h>
#include <wx/listbox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace function_list_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<std::size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

class FunctionListInputDialog final : public wxDialog {
public:
	FunctionListInputDialog(wxWindow *parent, const function_list::Source &source,
	                        const function_list::Options &options)
		: wxDialog(parent, wxID_ANY, to_wx(function_list::ModeTitle(options.mode)),
		           wxDefaultPosition, wxDefaultSize,
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, source_(source)
		, options_(options)
	{
		source_.user_regex = options_.regex;
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		wxBoxSizer *filter_row = new wxBoxSizer(wxHORIZONTAL);
		filter_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("フィルタ"))),
		                wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		filter_edit_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(300, -1));
		filter_row->Add(filter_edit_, wxSizerFlags(1).Expand().Border(wxRIGHT, 8));
		migemo_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("Migemo")));
		name_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("名前だけ")));
		link_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("行に連動")));
		migemo_chk_->SetValue(options_.fuzzy);
		name_chk_->SetValue(options_.name_only);
		link_chk_->SetValue(options_.link);
		filter_row->Add(migemo_chk_, wxSizerFlags().Border(wxRIGHT, 8));
		filter_row->Add(name_chk_, wxSizerFlags().Border(wxRIGHT, 8));
		filter_row->Add(link_chk_);
		top->Add(filter_row, wxSizerFlags().Expand().Border(wxALL, 8));
		filter_edit_->Bind(wxEVT_TEXT, &FunctionListInputDialog::OnFilter, this);
		migemo_chk_->Bind(wxEVT_CHECKBOX, &FunctionListInputDialog::OnFilter, this);
		name_chk_->Bind(wxEVT_CHECKBOX, &FunctionListInputDialog::OnFilter, this);
		link_chk_->Bind(wxEVT_CHECKBOX, &FunctionListInputDialog::OnFilter, this);

		// VCL のユーザー定義欄は ListMode==1 のときだけ見える。
		if (options_.mode == function_list::Mode::UserDefined) {
			wxBoxSizer *user_row = new wxBoxSizer(wxHORIZONTAL);
			user_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("文字列"))),
			              wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
			user_edit_ = new wxTextCtrl(this, wxID_ANY, to_wx(source_.user_pattern),
			                             wxDefaultPosition, wxSize(260, -1));
			user_row->Add(user_edit_, wxSizerFlags(1).Expand().Border(wxRIGHT, 4));
			regex_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("正規表現")));
			regex_chk_->SetValue(options_.regex);
			user_row->Add(regex_chk_, wxSizerFlags().Border(wxRIGHT, 4));
			update_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("更新")));
			user_row->Add(update_btn_);
			top->Add(user_row, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
			user_edit_->Bind(wxEVT_TEXT_ENTER, &FunctionListInputDialog::OnUpdateUser, this);
			update_btn_->Bind(wxEVT_BUTTON, &FunctionListInputDialog::OnUpdateUser, this);
		}

		list_ = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(560, 300));
		top->Add(list_, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT, 8));
		list_->Bind(wxEVT_LISTBOX, &FunctionListInputDialog::OnSelect, this);
		list_->Bind(wxEVT_LISTBOX_DCLICK, &FunctionListInputDialog::OnActivate, this);

		status_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		top->Add(status_, wxSizerFlags().Expand().Border(wxALL, 8));

		wxBoxSizer *buttons = new wxBoxSizer(wxHORIZONTAL);
		edit_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("編集...")));
		buttons->Add(edit_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		buttons->AddStretchSpacer();
		buttons->Add(new wxButton(this, wxID_OK, to_wx(_T("OK"))),
		             wxSizerFlags().Border(wxRIGHT, 4));
		buttons->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("キャンセル"))));
		top->Add(buttons, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		edit_btn_->Bind(wxEVT_BUTTON, &FunctionListInputDialog::OnEdit, this);
		Bind(wxEVT_BUTTON, &FunctionListInputDialog::OnOk, this, wxID_OK);

		Bind(wxEVT_CHAR_HOOK, &FunctionListInputDialog::OnCharHook, this);
		SetSizerAndFit(top);
		CentreOnParent();
		RefreshList();
		if (options_.to_filter) {
			filter_edit_->SetFocus();
			filter_edit_->SelectAll();
		}
		else {
			list_->SetFocus();
		}
	}

	Result ResultValue() const { return result_; }

private:
	void RefreshList()
	{
		all_entries_ = function_list::BuildEntries(source_, options_.mode);
		function_list::FilterOptions filter;
		filter.fuzzy = migemo_chk_->GetValue();
		filter.regex = false;
		filter.case_sensitive = false;
		shown_entries_ = function_list::FilterEntries(all_entries_, to_us(filter_edit_->GetValue()), filter);
		list_->Clear();
		for (const function_list::Entry &entry : shown_entries_) {
			UnicodeString text = entry.text;
			if (name_chk_->GetValue()) text = function_list::NameOnlyText(text, true, source_.name_pattern);
			list_->Append(to_wx(text));
		}
		const int selected = function_list::SelectNearest(shown_entries_, source_.current_line);
		if (selected >= 0) list_->SetSelection(selected);
		status_->SetLabel(to_wx(UnicodeString().sprintf(_T("%u / %u 件"), static_cast<unsigned>(shown_entries_.size()),
		                                                   static_cast<unsigned>(all_entries_.size()))));
		edit_btn_->Enable(list_->GetSelection() != wxNOT_FOUND);
	}

	void OnFilter(wxCommandEvent &) { RefreshList(); }
	void OnSelect(wxCommandEvent &) { edit_btn_->Enable(list_->GetSelection() != wxNOT_FOUND); }
	void OnUpdateUser(wxCommandEvent &)
	{
		source_.user_pattern = to_us(user_edit_->GetValue());
		options_.regex = regex_chk_->GetValue();
		source_.user_regex = options_.regex;
		RefreshList();
	}
	void OnEdit(wxCommandEvent &)
	{
		if (list_->GetSelection() == wxNOT_FOUND) return;
		result_.request_edit = true;
		result_.user_pattern = source_.user_pattern;
		result_.name_only = name_chk_->GetValue();
		result_.link = link_chk_->GetValue();
		result_.regex = regex_chk_ != nullptr ? regex_chk_->GetValue() : options_.regex;
		EndModal(wxID_OK);
	}
	void OnActivate(wxCommandEvent &) { Finish(false); }
	void OnOk(wxCommandEvent &event) { Finish(false); event.Skip(); }
	void OnCharHook(wxKeyEvent &event)
	{
		if (event.GetKeyCode() == WXK_ESCAPE) {
			EndModal(wxID_CANCEL);
			return;
		}
		if (event.GetKeyCode() == WXK_RETURN && list_->GetSelection() != wxNOT_FOUND) {
			Finish(false);
			return;
		}
		event.Skip();
	}

	void Finish(bool /*unused*/)
	{
		const int selected = list_->GetSelection();
		if (selected < 0 || selected >= static_cast<int>(shown_entries_.size())) {
			wxMessageBox(to_wx(_T("項目を選んでください")), to_wx(_T("一覧")), wxOK | wxICON_INFORMATION, this);
			return;
		}
		result_.line_no = shown_entries_[static_cast<std::size_t>(selected)].line_no;
		result_.user_pattern = source_.user_pattern;
		result_.name_only = name_chk_->GetValue();
		result_.link = link_chk_->GetValue();
		result_.regex = regex_chk_ != nullptr ? regex_chk_->GetValue() : options_.regex;
		EndModal(wxID_OK);
	}

	function_list::Source source_;
	function_list::Options options_;
	std::vector<function_list::Entry> all_entries_;
	std::vector<function_list::Entry> shown_entries_;
	Result result_;
	wxTextCtrl *filter_edit_ = nullptr;
	wxCheckBox *migemo_chk_ = nullptr;
	wxCheckBox *name_chk_ = nullptr;
	wxCheckBox *link_chk_ = nullptr;
	wxTextCtrl *user_edit_ = nullptr;
	wxCheckBox *regex_chk_ = nullptr;
	wxButton *update_btn_ = nullptr;
	wxListBox *list_ = nullptr;
	wxStaticText *status_ = nullptr;
	wxButton *edit_btn_ = nullptr;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, const function_list::Source &source,
         const function_list::Options &options, Result &out)
{
	FunctionListInputDialog dlg(parent, source, options);
	if (dlg.ShowModal() != wxID_OK) return false;
	out = dlg.ResultValue();
	return true;
}

}  // namespace function_list_dialog
