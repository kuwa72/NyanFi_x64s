/**
 * @file gui/pack_dialog.cpp
 * @brief gui/pack_dialog.h の実装
 */
#include "gui/pack_dialog.h"

#include <array>

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/radiobut.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace pack_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

class PackInputDialog : public wxDialog {
public:
	PackInputDialog(wxWindow *parent, const pack_settings::Options &initial,
	                const Context &context)
		: wxDialog(parent, wxID_ANY, to_wx(_T("アーカイブの作成")), wxDefaultPosition,
		           wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, context_(context)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		wxBoxSizer *name_row = new wxBoxSizer(wxHORIZONTAL);
		name_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("アーカイブ名"))),
		              wxSizerFlags().CentreVertical().Border(wxRIGHT, 8));
		name_ = new wxTextCtrl(this, wxID_ANY,
		                       to_wx(initial.name.IsEmpty()? context.default_name : initial.name),
		                       wxDefaultPosition, wxSize(300, -1), wxTE_PROCESS_ENTER);
		ext_label_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		name_row->Add(name_, wxSizerFlags(1).Expand());
		name_row->Add(ext_label_, wxSizerFlags().CentreVertical().Border(wxLEFT, 8));
		top->Add(name_row, wxSizerFlags().Expand().Border(wxALL, 8));

		wxBoxSizer *format_row = new wxBoxSizer(wxHORIZONTAL);
		format_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("形式(&F)"))),
		                wxSizerFlags().CentreVertical().Border(wxRIGHT, 8));
		const wchar_t *labels[] = {_T("ZIP"), _T("7z"), _T("LHA"), _T("CAB"), _T("TAR")};
		for (int i = 0; i < 5; ++i) {
			formats_[i] = new wxRadioButton(this, wxID_ANY, to_wx(labels[i]),
			                                wxDefaultPosition, wxDefaultSize,
			                                i == 0 ? wxRB_GROUP : 0);
			format_row->Add(formats_[i], wxSizerFlags().Border(wxRIGHT, 8));
			formats_[i]->Bind(wxEVT_RADIOBUTTON, &PackInputDialog::OnFormatChanged, this);
		}
		top->Add(format_row, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxFlexGridSizer *options = new wxFlexGridSizer(2, 4, 8);
		options->AddGrowableCol(1);
		options->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("圧縮形式"))),
		             wxSizerFlags().CentreVertical());
		param_ = new wxChoice(this, wxID_ANY);
		options->Add(param_, wxSizerFlags(1).Expand());
		options->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("追加スイッチ"))),
		             wxSizerFlags().CentreVertical());
		switches_ = new wxTextCtrl(this, wxID_ANY, to_wx(initial.extra_switches),
		                            wxDefaultPosition, wxSize(280, -1));
		options->Add(switches_, wxSizerFlags(1).Expand());
		options->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("パスワード(&P)"))),
		             wxSizerFlags().CentreVertical());
		password_ = new wxTextCtrl(this, wxID_ANY, to_wx(initial.password),
		                            wxDefaultPosition, wxSize(280, -1), wxTE_PASSWORD);
		options->Add(password_, wxSizerFlags(1).Expand());
		top->Add(options, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		sfx_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("自己解凍")));
		sfx_->SetValue(initial.self_extract);
		per_dir_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("ディレクトリごとに作成")));
		per_dir_->SetValue(initial.per_directory);
		include_dir_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("トップディレクトリを含める")));
		include_dir_->SetValue(initial.include_top_directory);
		confirm_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("確認")));
		confirm_->SetValue(initial.confirm_existing);
		top->Add(sfx_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(per_dir_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(include_dir_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(confirm_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxBoxSizer *same_row = new wxBoxSizer(wxHORIZONTAL);
		same_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("同名アーカイブ"))),
		              wxSizerFlags().CentreVertical().Border(wxRIGHT, 8));
		append_ = new wxRadioButton(this, wxID_ANY, to_wx(_T("既存内容に追加")), wxDefaultPosition,
		                            wxDefaultSize, wxRB_GROUP);
		recreate_ = new wxRadioButton(this, wxID_ANY, to_wx(_T("削除して新規作成")));
		append_->SetValue(initial.existing_mode == pack_settings::ExistingMode::Append);
		recreate_->SetValue(initial.existing_mode == pack_settings::ExistingMode::Recreate);
		same_row->Add(append_, wxSizerFlags().Border(wxRIGHT, 12));
		same_row->Add(recreate_);
		top->Add(same_row, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		per_dir_->Enable(context.per_directory_available);
		include_dir_->Enable(context.per_directory_available);
		per_dir_->Bind(wxEVT_CHECKBOX, &PackInputDialog::OnPerDir, this);
		include_dir_->Bind(wxEVT_CHECKBOX, &PackInputDialog::OnPerDir, this);
		sfx_->Bind(wxEVT_CHECKBOX, &PackInputDialog::OnPerDir, this);
		param_->Bind(wxEVT_CHOICE, &PackInputDialog::OnParamChanged, this);

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));
		SetSizerAndFit(top);
		CentreOnParent();

		int selected = pack_settings::ToIndex(initial.format);
		if (selected < 0 || selected >= 5 || !pack_settings::IsAvailable(context.availability,
		                                                                    pack_settings::FromIndex(selected))) {
			selected = 0;
			for (int i = 0; i < 5; ++i) {
				if (pack_settings::IsAvailable(context.availability,
				                               pack_settings::FromIndex(i))) {
					selected = i;
					break;
				}
			}
		}
		formats_[selected]->SetValue(true);
		UpdateFormat(initial.compression);
	}

	pack_settings::Options Options() const
	{
		pack_settings::Options out;
		out.name = to_us(name_->GetValue()).Trim();
		for (int i = 0; i < 5; ++i) {
			if (formats_[i]->GetValue()) out.format = pack_settings::FromIndex(i);
		}
		const int p = param_->GetSelection();
		if (p >= 0) out.compression = pack_settings::CompressionFromUiIndex(out.format, p);
		else out.compression = pack_settings::CompressionFromUiIndex(out.format, 0);
		out.extra_switches = to_us(switches_->GetValue()).Trim();
		out.password = to_us(password_->GetValue());
		out.self_extract = sfx_->GetValue();
		out.per_directory = per_dir_->GetValue() && context_.per_directory_available;
		out.include_top_directory = include_dir_->GetValue() && context_.per_directory_available;
		out.confirm_existing = confirm_->GetValue();
		out.existing_mode = recreate_->GetValue() ? pack_settings::ExistingMode::Recreate
		                                           : pack_settings::ExistingMode::Append;
		return pack_settings::Normalize(out, context_.availability);
	}

private:
	int SelectedFormat() const
	{
		for (int i = 0; i < 5; ++i) if (formats_[i]->GetValue()) return i;
		return 0;
	}

	void OnFormatChanged(wxCommandEvent &event)
	{
		UpdateFormat(pack_settings::CompressionFromUiIndex(
			pack_settings::FromIndex(SelectedFormat()), 0));
		event.Skip();
	}

	void OnParamChanged(wxCommandEvent &event) { event.Skip(); }

	void UpdateFormat(int compression)
	{
		const int index = SelectedFormat();
		const pack_settings::Format format = pack_settings::FromIndex(index);
		for (int i = 0; i < 5; ++i) {
			formats_[i]->Enable(pack_settings::IsAvailable(context_.availability,
			                                               pack_settings::FromIndex(i)));
		}
		param_->Clear();
		switch (format) {
		case pack_settings::Format::Zip:
		case pack_settings::Format::SevenZip:
			param_->Append(to_wx(_T("無圧縮")));
			param_->Append(to_wx(_T("圧縮レベル1")));
			param_->Append(to_wx(_T("圧縮レベル3")));
			param_->Append(to_wx(_T("圧縮レベル5 (デフォルト)")));
			param_->Append(to_wx(_T("圧縮レベル7")));
			param_->Append(to_wx(_T("圧縮レベル9")));
			break;
		case pack_settings::Format::Cab:
			param_->Append(to_wx(_T("MSZIP形式 (デフォルト)")));
			for (int i = 15; i <= 22; ++i)
				param_->Append(to_wx(UnicodeString().sprintf(_T("LZX形式 圧縮レベル%d"), i)));
			break;
		case pack_settings::Format::Tar:
			param_->Append(to_wx(_T("無圧縮")));
			for (int i = 1; i <= 9; ++i)
				param_->Append(to_wx(UnicodeString().sprintf(_T("圧縮レベル gzip%d%s"), i,
				                                               i == 6 ? _T(" (デフォルト)") : _T(""))));
			break;
		case pack_settings::Format::Lha:
		default:
			break;
		}
		const int ui = pack_settings::UiIndexFromCompression(format, compression);
		param_->SetSelection(param_->GetCount() > 0 ? ui : wxNOT_FOUND);
		param_->Enable(param_->GetCount() > 0);
		ext_label_->SetLabel(to_wx(pack_settings::Extension(format)));

		const bool zip_family = format == pack_settings::Format::Zip ||
		                         format == pack_settings::Format::SevenZip;
		sfx_->Enable(format == pack_settings::Format::SevenZip);
		if (format != pack_settings::Format::SevenZip) sfx_->SetValue(false);
		password_->Enable(zip_family && !sfx_->GetValue());
		if (!password_->IsEnabled()) password_->Clear();
		UpdatePerDir();
	}

	void UpdatePerDir()
	{
		const bool enabled = context_.per_directory_available;
		per_dir_->Enable(enabled);
		include_dir_->Enable(enabled && per_dir_->GetValue());
	}

	void OnPerDir(wxCommandEvent &event)
	{
		UpdatePerDir();
		const pack_settings::Format format = pack_settings::FromIndex(SelectedFormat());
		const bool zip_family = format == pack_settings::Format::Zip ||
		                         format == pack_settings::Format::SevenZip;
		password_->Enable(zip_family && !sfx_->GetValue());
		if (!password_->IsEnabled()) password_->Clear();
		event.Skip();
	}

	Context context_;
	std::array<wxRadioButton *, 5> formats_{};
	wxTextCtrl *name_ = nullptr;
	wxStaticText *ext_label_ = nullptr;
	wxChoice *param_ = nullptr;
	wxTextCtrl *switches_ = nullptr;
	wxTextCtrl *password_ = nullptr;
	wxCheckBox *sfx_ = nullptr;
	wxCheckBox *per_dir_ = nullptr;
	wxCheckBox *include_dir_ = nullptr;
	wxCheckBox *confirm_ = nullptr;
	wxRadioButton *append_ = nullptr;
	wxRadioButton *recreate_ = nullptr;
};

}  // namespace

bool Run(wxWindow *parent, pack_settings::Options &options, const Context &context)
{
	PackInputDialog dlg(parent, options, context);
	if (dlg.ShowModal() != wxID_OK) return false;
	options = dlg.Options();
	const pack_settings::Resolved resolved = pack_settings::Resolve(options, context.availability);
	if (!resolved.available) {
		wxMessageBox(to_wx(resolved.unsupported_reason), to_wx(_T("アーカイブの作成")),
		             wxOK | wxICON_WARNING, parent);
		return false;
	}
	return true;
}

}  // namespace pack_dialog
