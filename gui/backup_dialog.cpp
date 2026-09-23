/**
 * @file gui/backup_dialog.cpp
 * @brief gui/backup_dialog.h の実装
 */
#include "gui/backup_dialog.h"

#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace backup_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

class BackupInputDialog : public wxDialog {
public:
	BackupInputDialog(wxWindow *parent, const Context &context,
	                  const backup_settings::Options &initial,
	                  std::vector<backup_settings::Setup> &setups, int selected_index)
		: wxDialog(parent, wxID_ANY, to_wx(_T("バックアップ")), wxDefaultPosition,
		           wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, context_(context)
		, setups_(setups)
		, selected_index_(selected_index)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		wxFlexGridSizer *paths = new wxFlexGridSizer(2, 4, 8);
		paths->AddGrowableCol(1);
		paths->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("バックアップ元"))),
		           wxSizerFlags().CentreVertical());
		source_ = new wxTextCtrl(this, wxID_ANY, to_wx(context.source_dir),
		                         wxDefaultPosition, wxSize(360, -1), wxTE_READONLY);
		paths->Add(source_, wxSizerFlags(1).Expand());
		paths->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("バックアップ先"))),
		           wxSizerFlags().CentreVertical());
		dest_ = new wxTextCtrl(this, wxID_ANY, to_wx(context.dest_dir),
		                       wxDefaultPosition, wxSize(360, -1), wxTE_READONLY);
		paths->Add(dest_, wxSizerFlags(1).Expand());
		top->Add(paths, wxSizerFlags().Expand().Border(wxALL, 8));

		wxBoxSizer *setup_row = new wxBoxSizer(wxHORIZONTAL);
		setup_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("設定"))),
		               wxSizerFlags().CentreVertical().Border(wxRIGHT, 8));
		setup_name_ = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                             wxSize(220, -1));
		setup_row->Add(setup_name_, wxSizerFlags(1).Expand().Border(wxRIGHT, 8));
		wxButton *save = new wxButton(this, wxID_ANY, to_wx(_T("保存")));
		wxButton *del = new wxButton(this, wxID_ANY, to_wx(_T("削除")));
		setup_row->Add(save, wxSizerFlags().Border(wxRIGHT, 4));
		setup_row->Add(del);
		top->Add(setup_row, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		save->Bind(wxEVT_BUTTON, &BackupInputDialog::OnSaveSetup, this);
		del->Bind(wxEVT_BUTTON, &BackupInputDialog::OnDeleteSetup, this);
		setup_name_->Bind(wxEVT_COMBOBOX, &BackupInputDialog::OnSetupSelected, this);

		wxFlexGridSizer *fields = new wxFlexGridSizer(2, 4, 8);
		fields->AddGrowableCol(1);
		fields->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("対象マスク"))),
		            wxSizerFlags().CentreVertical());
		include_ = new wxComboBox(this, wxID_ANY, to_wx(initial.include_mask),
		                            wxDefaultPosition, wxSize(300, -1), wxTE_PROCESS_ENTER);
		fields->Add(include_, wxSizerFlags(1).Expand());
		fields->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("除外マスク"))),
		            wxSizerFlags().CentreVertical());
		exclude_ = new wxComboBox(this, wxID_ANY, to_wx(initial.exclude_mask),
		                            wxDefaultPosition, wxSize(300, -1), wxTE_PROCESS_ENTER);
		fields->Add(exclude_, wxSizerFlags(1).Expand());
		fields->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("除外ディレクトリ"))),
		            wxSizerFlags().CentreVertical());
		skip_ = new wxTextCtrl(this, wxID_ANY, to_wx(initial.skip_dirs),
		                       wxDefaultPosition, wxSize(300, -1));
		fields->Add(skip_, wxSizerFlags(1).Expand());
		fields->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("日付条件"))),
		            wxSizerFlags().CentreVertical());
		date_ = new wxTextCtrl(this, wxID_ANY, to_wx(initial.date_condition),
		                       wxDefaultPosition, wxSize(180, -1));
		fields->Add(date_, wxSizerFlags().Expand());
		top->Add(fields, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		sub_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("サブディレクトリも対象にする")));
		mirror_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("ミラーを行う")));
		sync_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("同期コピー")));
		confirm_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("開始時に確認")));
		sub_->SetValue(initial.sub_dirs);
		mirror_->SetValue(initial.mirror);
		sync_->SetValue(initial.sync);
		confirm_->SetValue(initial.confirm);
		top->Add(sub_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(mirror_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(sync_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(confirm_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		sub_->Bind(wxEVT_CHECKBOX, &BackupInputDialog::OnEnabled, this);
		sync_->Bind(wxEVT_CHECKBOX, &BackupInputDialog::OnEnabled, this);

		sync_label_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		top->Add(sync_label_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxButton *command = new wxButton(this, wxID_ANY, to_wx(_T("コマンドファイルとして保存")));
		top->Add(command, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		command->Bind(wxEVT_BUTTON, &BackupInputDialog::OnMakeCommand, this);

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));
		SetSizerAndFit(top);
		CentreOnParent();

		RefreshSetups();
		UpdateEnabled();
		Bind(wxEVT_BUTTON, &BackupInputDialog::OnOk, this, wxID_OK);
	}

	int selected_index() const { return selected_index_; }

	backup_settings::Options Options() const
	{
		backup_settings::Options out;
		out.include_mask = to_us(include_->GetValue()).Trim();
		out.exclude_mask = to_us(exclude_->GetValue()).Trim();
		out.skip_dirs = to_us(skip_->GetValue()).Trim();
		out.sub_dirs = sub_->GetValue();
		out.mirror = mirror_->GetValue();
		out.sync = sync_->GetValue();
		out.date_condition = to_us(date_->GetValue()).Trim();
		out.confirm = confirm_->GetValue();
		return out;
	}

private:
	void RefreshSetups()
	{
		setup_name_->Clear();
		for (const backup_settings::Setup &s : setups_) setup_name_->Append(to_wx(s.name));
		if (selected_index_ >= 0 && selected_index_ < static_cast<int>(setups_.size())) {
			setup_name_->SetSelection(selected_index_);
			LoadSetup(selected_index_);
		}
		else {
			selected_index_ = -1;
		}
	}

	void LoadSetup(int index)
	{
		if (index < 0 || index >= static_cast<int>(setups_.size())) return;
		const backup_settings::Options &o = setups_[static_cast<std::size_t>(index)].options;
		include_->SetValue(to_wx(o.include_mask));
		exclude_->SetValue(to_wx(o.exclude_mask));
		skip_->SetValue(to_wx(o.skip_dirs));
		date_->SetValue(to_wx(o.date_condition));
		sub_->SetValue(o.sub_dirs);
		mirror_->SetValue(o.mirror);
		sync_->SetValue(o.sync);
		confirm_->SetValue(o.confirm);
		setup_name_->SetValue(to_wx(setups_[static_cast<std::size_t>(index)].name));
		UpdateEnabled();
	}

	void UpdateEnabled()
	{
		skip_->Enable(sub_->GetValue());
		const auto targets = backup_settings::ResolveDestinations(
			context_.dest_dir, sync_->GetValue(), context_.sync_settings);
		UnicodeString label = _T("(同期ディレクトリ: ");
		label += UnicodeString().sprintf(_T("%u"), static_cast<unsigned>(targets.empty() ? 0 : targets.size() - 1));
		label += _T(")");
		sync_label_->SetLabel(to_wx(label));
	}

	void OnEnabled(wxCommandEvent &event)
	{
		UpdateEnabled();
		event.Skip();
	}

	void OnSetupSelected(wxCommandEvent &event)
	{
		selected_index_ = setup_name_->GetSelection();
		if (selected_index_ >= 0) LoadSetup(selected_index_);
		event.Skip();
	}

	void OnSaveSetup(wxCommandEvent &)
	{
		const UnicodeString name = to_us(setup_name_->GetValue()).Trim();
		if (name.IsEmpty()) {
			wxMessageBox(to_wx(_T("設定名を入力してください")), to_wx(_T("バックアップ")),
			             wxOK | wxICON_WARNING, this);
			return;
		}
		backup_settings::UpsertSetup(setups_, name, Options());
		selected_index_ = backup_settings::FindSetupIndex(setups_, name);
		RefreshSetups();
	}

	void OnDeleteSetup(wxCommandEvent &)
	{
		const UnicodeString name = to_us(setup_name_->GetValue()).Trim();
		if (!backup_settings::DeleteSetup(setups_, name)) return;
		selected_index_ = setups_.empty() ? -1 : 0;
		RefreshSetups();
	}

	void OnMakeCommand(wxCommandEvent &)
	{
		// VCL はここでファイル保存ダイアログを出し、(src/BakDlg.cpp:172-190) の
		// 実保存を行う。移植版はファイル I/O を勝手に増やさず警告する。
		wxMessageBox(to_wx(_T("コマンドファイルの実保存は未移植 (未実装処理) です")),
		             to_wx(_T("バックアップ")), wxOK | wxICON_INFORMATION, this);
	}

	void OnOk(wxCommandEvent &event)
	{
		UnicodeString error;
		backup_settings::DateKind kind = backup_settings::DateKind::None;
		UnicodeString normalized;
		if (!backup_settings::ParseDateCondition(to_us(date_->GetValue()), kind, normalized, error)) {
			wxMessageBox(to_wx(error), to_wx(_T("バックアップ")), wxOK | wxICON_WARNING, this);
			return;
		}
		event.Skip();
	}

	Context context_;
	std::vector<backup_settings::Setup> &setups_;
	int selected_index_ = -1;
	wxTextCtrl *source_ = nullptr;
	wxTextCtrl *dest_ = nullptr;
	wxComboBox *setup_name_ = nullptr;
	wxComboBox *include_ = nullptr;
	wxComboBox *exclude_ = nullptr;
	wxTextCtrl *skip_ = nullptr;
	wxTextCtrl *date_ = nullptr;
	wxCheckBox *sub_ = nullptr;
	wxCheckBox *mirror_ = nullptr;
	wxCheckBox *sync_ = nullptr;
	wxCheckBox *confirm_ = nullptr;
	wxStaticText *sync_label_ = nullptr;
};

}  // namespace

bool Run(wxWindow *parent, const Context &context, backup_settings::Options &options,
         std::vector<backup_settings::Setup> &setups, int &selected_index)
{
	BackupInputDialog dlg(parent, context, options, setups, selected_index);
	if (dlg.ShowModal() != wxID_OK) return false;
	options = dlg.Options();
	selected_index = dlg.selected_index();
	return true;
}

}  // namespace backup_dialog
