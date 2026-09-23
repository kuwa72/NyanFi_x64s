/**
 * @file gui/exe_cmd_dialog.cpp
 * @brief gui/exe_cmd_dialog.h の実装
 */
#include "gui/exe_cmd_dialog.h"

#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/filedlg.h>
#include <wx/statbox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "gui/new_file.h"

namespace exe_cmd_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<std::size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

class ExeCmdDialog final : public wxDialog {
public:
	ExeCmdDialog(wxWindow *parent, const exe_cmd::Options &options,
	             const exe_cmd::CommandSeed &seed)
		: wxDialog(parent, wxID_ANY, to_wx(_T("コマンドラインの実行")),
		           wxDefaultPosition, wxSize(650, 390), wxDEFAULT_DIALOG_STYLE),
		  result_(options)
	{
		result_.command = seed.text;
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		wxArrayString history;
		for (const UnicodeString &entry : options.history) history.Add(to_wx(entry));
		command_ctrl_ = new wxComboBox(this, wxID_ANY, to_wx(seed.text), wxDefaultPosition,
		                               wxDefaultSize, history, wxTE_PROCESS_ENTER);
		command_ctrl_->SetToolTip(to_wx(_T("実行するコマンドライン")));
		wxBoxSizer *command_box = new wxBoxSizer(wxHORIZONTAL);
		command_box->Add(command_ctrl_, wxSizerFlags(1).Expand().Border(wxRIGHT, 6));
		command_box->Add(new wxButton(this, ID_DELETE, to_wx(_T("履歴から削除"))),
		                 wxSizerFlags().Border(wxRIGHT, 6));
		command_box->Add(new wxButton(this, ID_CLEAR, to_wx(_T("履歴を消去"))));
		top->Add(command_box, wxSizerFlags().Expand().Border(wxALL, 8));

		wxStaticBox *output_box = new wxStaticBox(this, wxID_ANY, to_wx(_T("オプションの出力")));
		wxBoxSizer *output_layout = new wxBoxSizer(wxVERTICAL);
		wxBoxSizer *checks = new wxBoxSizer(wxHORIZONTAL);
		log_ctrl_ = new wxCheckBox(output_box, wxID_ANY, to_wx(_T("ログに出力")));
		log_ctrl_->SetValue(options.log_stdout);
		copy_ctrl_ = new wxCheckBox(output_box, wxID_ANY, to_wx(_T("クリップボードにコピー")));
		copy_ctrl_->SetValue(options.copy_stdout);
		save_ctrl_ = new wxCheckBox(output_box, wxID_ANY, to_wx(_T("ファイルに保存")));
		save_ctrl_->SetValue(options.save_stdout);
		list_ctrl_ = new wxCheckBox(output_box, wxID_ANY, to_wx(_T("一覧で表示")));
		list_ctrl_->SetValue(options.list_stdout);
		checks->Add(log_ctrl_, wxSizerFlags().Border(wxRIGHT, 10));
		checks->Add(copy_ctrl_, wxSizerFlags().Border(wxRIGHT, 10));
		checks->Add(save_ctrl_, wxSizerFlags().Border(wxRIGHT, 10));
		checks->Add(list_ctrl_);
		output_layout->Add(checks, wxSizerFlags().Expand().Border(wxALL, 6));

		wxBoxSizer *save_box = new wxBoxSizer(wxHORIZONTAL);
		save_name_ctrl_ = new wxTextCtrl(output_box, wxID_ANY, to_wx(options.save_name));
		save_browse_ctrl_ = new wxButton(output_box, ID_SAVE_BROWSE, to_wx(_T("...")));
		save_box->Add(new wxStaticText(output_box, wxID_ANY, to_wx(_T("保存先"))),
		              wxSizerFlags().CentreVertical().Border(wxRIGHT, 6));
		save_box->Add(save_name_ctrl_, wxSizerFlags(1).Expand().Border(wxRIGHT, 6));
		save_box->Add(save_browse_ctrl_);
		output_layout->Add(save_box, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 6));
		output_box->SetSizer(output_layout);
		top->Add(output_box, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		run_as_ctrl_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("管理者として実行")));
		run_as_ctrl_->SetValue(options.run_as);
		uac_ctrl_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("UAC ダイアログを表示")));
		uac_ctrl_->Enable(false);
		uac_ctrl_->SetToolTip(to_wx(_T("未移植 (未実装扱い): ForcedElevation を指定する専用動作")));
		top->Add(run_as_ctrl_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		top->Add(uac_ctrl_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		wxBoxSizer *buttons = new wxBoxSizer(wxHORIZONTAL);
		buttons->AddStretchSpacer();
		buttons->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("キャンセル"))), wxSizerFlags().Border(wxLEFT, 6));
		ok_ctrl_ = new wxButton(this, wxID_OK, to_wx(_T("実行")));
		buttons->Add(ok_ctrl_);
		top->Add(buttons, wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		Bind(wxEVT_BUTTON, &ExeCmdDialog::OnDeleteHistory, this, ID_DELETE);
		Bind(wxEVT_BUTTON, &ExeCmdDialog::OnClearHistory, this, ID_CLEAR);
		Bind(wxEVT_BUTTON, &ExeCmdDialog::OnSaveBrowse, this, ID_SAVE_BROWSE);
		Bind(wxEVT_BUTTON, &ExeCmdDialog::OnOk, this, wxID_OK);
		Bind(wxEVT_TEXT, &ExeCmdDialog::OnChanged, this, command_ctrl_->GetId());
		Bind(wxEVT_COMBOBOX, &ExeCmdDialog::OnChanged, this, command_ctrl_->GetId());
		Bind(wxEVT_TEXT, &ExeCmdDialog::OnChanged, this, save_name_ctrl_->GetId());
		Bind(wxEVT_CHECKBOX, &ExeCmdDialog::OnChanged, this);

		UpdateEnabled();
		command_ctrl_->SetFocus();
		command_ctrl_->SetSelection(seed.caret, seed.caret);
	}

	const exe_cmd::Options &Result() const { return result_; }

private:
	enum {
		ID_DELETE = wxID_HIGHEST + 1,
		ID_CLEAR,
		ID_SAVE_BROWSE
	};

	exe_cmd::Options CurrentOptions() const
	{
		exe_cmd::Options out = result_;
		out.command = to_us(command_ctrl_->GetValue());
		out.log_stdout = log_ctrl_->GetValue();
		out.copy_stdout = copy_ctrl_->GetValue();
		out.save_stdout = save_ctrl_->GetValue();
		out.list_stdout = list_ctrl_->GetValue();
		out.save_name = to_us(save_name_ctrl_->GetValue());
		out.run_as = run_as_ctrl_->GetValue();
		out.uac_dialog = false;
		return out;
	}

	void UpdateEnabled()
	{
		const exe_cmd::Options options = CurrentOptions();
		const bool output_enabled = exe_cmd::OutputOptionsEnabled(options);
		log_ctrl_->Enable(output_enabled);
		copy_ctrl_->Enable(output_enabled);
		save_ctrl_->Enable(output_enabled);
		list_ctrl_->Enable(output_enabled);
		save_name_ctrl_->Enable(output_enabled && options.save_stdout);
		save_browse_ctrl_->Enable(output_enabled && options.save_stdout);
		ok_ctrl_->Enable(exe_cmd::CanSubmit(options));
	}

	void RebuildHistory()
	{
		wxString command = command_ctrl_->GetValue();
		command_ctrl_->Freeze();
		command_ctrl_->Clear();
		for (const UnicodeString &entry : result_.history) command_ctrl_->Append(to_wx(entry));
		command_ctrl_->ChangeValue(command);
		command_ctrl_->Thaw();
	}

	void OnDeleteHistory(wxCommandEvent &)
	{
		const UnicodeString entry = to_us(command_ctrl_->GetValue());
		for (std::size_t i = 0; i < result_.history.size();) {
			if (SameStr(result_.history[i], entry))
				result_.history.erase(result_.history.begin() + static_cast<std::ptrdiff_t>(i));
			else ++i;
		}
		command_ctrl_->ChangeValue(wxEmptyString);
		RebuildHistory();
		UpdateEnabled();
	}

	void OnClearHistory(wxCommandEvent &)
	{
		result_.history.clear();
		RebuildHistory();
	}

	void OnSaveBrowse(wxCommandEvent &)
	{
		UnicodeString dir = new_file::TemplateDirectory(to_us(save_name_ctrl_->GetValue()));
		wxFileDialog dlg(this, to_wx(_T("標準出力の保存先")), to_wx(dir), _T("out.txt"),
		                 to_wx(_T("Text files (*.txt)|*.txt|All files (*.*)|*.*")),
		                 wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
		if (dlg.ShowModal() == wxID_OK) save_name_ctrl_->ChangeValue(dlg.GetPath());
	}

	void OnChanged(wxCommandEvent &event) { UpdateEnabled(); event.Skip(); }

	void OnOk(wxCommandEvent &)
	{
		result_ = CurrentOptions();
		if (!exe_cmd::CanSubmit(result_)) return;
		exe_cmd::PromoteHistory(result_.history, Trim(result_.command));
		EndModal(wxID_OK);
	}

	wxComboBox *command_ctrl_ = nullptr;
	wxCheckBox *log_ctrl_ = nullptr;
	wxCheckBox *copy_ctrl_ = nullptr;
	wxCheckBox *save_ctrl_ = nullptr;
	wxCheckBox *list_ctrl_ = nullptr;
	wxTextCtrl *save_name_ctrl_ = nullptr;
	wxButton *save_browse_ctrl_ = nullptr;
	wxCheckBox *run_as_ctrl_ = nullptr;
	wxCheckBox *uac_ctrl_ = nullptr;
	wxButton *ok_ctrl_ = nullptr;
	exe_cmd::Options result_;
};

}  // namespace

bool Run(wxWindow *parent, exe_cmd::Options &options, const exe_cmd::CommandSeed &seed)
{
	ExeCmdDialog dialog(parent, options, seed);
	if (dialog.ShowModal() != wxID_OK) return false;
	options = dialog.Result();
	return true;
}

}  // namespace exe_cmd_dialog
