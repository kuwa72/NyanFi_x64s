/**
 * @file gui/new_file_dialog.cpp
 * @brief gui/new_file_dialog.h の実装
 */
#include "gui/new_file_dialog.h"

#include <wx/combobox.h>
#include <wx/filedlg.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace new_file_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<std::size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

class NewFileDialog final : public wxDialog {
public:
	NewFileDialog(wxWindow *parent, const Options &options)
		: wxDialog(parent, wxID_ANY, to_wx(_T("ファイルの新規作成")),
		           wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
		  result_(options)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
		wxFlexGridSizer *grid = new wxFlexGridSizer(3, 2, 8);
		grid->AddGrowableCol(1, 1);

		wxArrayString history;
		for (const UnicodeString &path : options.template_history) history.Add(to_wx(path));
		template_ctrl_ = new wxComboBox(this, wxID_ANY, to_wx(options.template_path),
		                                wxDefaultPosition, wxDefaultSize, history, wxTE_PROCESS_ENTER);
		wxBoxSizer *template_box = new wxBoxSizer(wxHORIZONTAL);
		template_box->Add(template_ctrl_, wxSizerFlags(1).Expand().Border(wxRIGHT, 6));
		template_box->Add(new wxButton(this, ID_BROWSE, to_wx(_T("..."))), wxSizerFlags());
		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("テンプレート"))), wxSizerFlags().CentreVertical());
		grid->Add(template_box, wxSizerFlags(1).Expand());

		name_ctrl_ = new wxTextCtrl(this, wxID_ANY, to_wx(options.name));
		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("名前"))), wxSizerFlags().CentreVertical());
		grid->Add(name_ctrl_, wxSizerFlags(1).Expand());

		command_ctrl_ = new wxTextCtrl(this, wxID_ANY, to_wx(options.post_command));
		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("作成後に実行するコマンド"))),
		          wxSizerFlags().CentreVertical());
		grid->Add(command_ctrl_, wxSizerFlags(1).Expand());
		top->Add(grid, wxSizerFlags().Expand().Border(wxALL, 8));

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		wxBoxSizer *buttons = new wxBoxSizer(wxHORIZONTAL);
		buttons->AddStretchSpacer();
		buttons->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("キャンセル"))), wxSizerFlags().Border(wxLEFT, 6));
		ok_ctrl_ = new wxButton(this, wxID_OK, to_wx(_T("OK")));
		buttons->Add(ok_ctrl_);
		top->Add(buttons, wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		Bind(wxEVT_BUTTON, &NewFileDialog::OnBrowse, this, ID_BROWSE);
		Bind(wxEVT_BUTTON, &NewFileDialog::OnOk, this, wxID_OK);
		Bind(wxEVT_COMBOBOX, &NewFileDialog::OnTemplateChanged, this, template_ctrl_->GetId());
		Bind(wxEVT_TEXT, &NewFileDialog::OnTemplateChanged, this, template_ctrl_->GetId());
		Bind(wxEVT_TEXT, &NewFileDialog::OnChanged, this, name_ctrl_->GetId());

		if (template_ctrl_->GetValue().IsEmpty() && !history.IsEmpty()) {
			template_ctrl_->SetSelection(0);
			ApplyTemplate(to_us(history[0]));
		}
		else if (!template_ctrl_->GetValue().IsEmpty()) {
			ApplyTemplate(to_us(template_ctrl_->GetValue()));
		}
		UpdateEnabled();
		name_ctrl_->SetFocus();
	}

	const Options &Result() const { return result_; }

private:
	enum { ID_BROWSE = wxID_HIGHEST + 1 };

	void ApplyTemplate(const UnicodeString &path)
	{
		const UnicodeString name = new_file::NameFromTemplate(path);
		name_ctrl_->ChangeValue(to_wx(name));
		name_ctrl_->SetSelection(0, new_file::StemSelectionLength(name));
	}

	void OnTemplateChanged(wxCommandEvent &event)
	{
		ApplyTemplate(to_us(template_ctrl_->GetValue()));
		UpdateEnabled();
		event.Skip();
	}

	void OnChanged(wxCommandEvent &event) { UpdateEnabled(); event.Skip(); }

	void OnBrowse(wxCommandEvent &)
	{
		UnicodeString dir = template_ctrl_->GetValue().IsEmpty()
		                  ? result_.default_directory
		                  : new_file::TemplateDirectory(to_us(template_ctrl_->GetValue()));
		wxFileDialog dlg(this, to_wx(_T("テンプレートの選択")), to_wx(dir), wxEmptyString,
		                 to_wx(_T("All files (*.*)|*.*|Text files (*.txt)|*.txt")),
		                 wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() != wxID_OK) return;
		const UnicodeString path = to_us(dlg.GetPath());
		std::vector<UnicodeString> history = result_.template_history;
		new_file::PromoteHistory(history, path);
		template_ctrl_->Freeze();
		template_ctrl_->Clear();
		for (const UnicodeString &entry : history) template_ctrl_->Append(to_wx(entry));
		template_ctrl_->SetSelection(0);
		template_ctrl_->Thaw();
		result_.template_history = history;
		ApplyTemplate(path);
		UpdateEnabled();
	}

	void UpdateEnabled()
	{
		ok_ctrl_->Enable(new_file::CanSubmit(to_us(template_ctrl_->GetValue()),
		                                       to_us(name_ctrl_->GetValue())));
	}

	void OnOk(wxCommandEvent &)
	{
		const UnicodeString path = to_us(template_ctrl_->GetValue());
		if (!new_file::CanSubmit(path, to_us(name_ctrl_->GetValue()))) return;
		result_.template_path = path;
		result_.name = to_us(name_ctrl_->GetValue());
		result_.post_command = to_us(command_ctrl_->GetValue());
		new_file::PromoteHistory(result_.template_history, path);
		EndModal(wxID_OK);
	}

	wxComboBox *template_ctrl_ = nullptr;
	wxTextCtrl *name_ctrl_ = nullptr;
	wxTextCtrl *command_ctrl_ = nullptr;
	wxButton *ok_ctrl_ = nullptr;
	Options result_;
};

}  // namespace

bool Run(wxWindow *parent, Options &options)
{
	NewFileDialog dialog(parent, options);
	if (dialog.ShowModal() != wxID_OK) return false;
	options = dialog.Result();
	return true;
}

}  // namespace new_file_dialog
