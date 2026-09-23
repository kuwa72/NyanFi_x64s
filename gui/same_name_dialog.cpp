/**
 * @file gui/same_name_dialog.cpp
 * @brief gui/same_name_dialog.h の実装
 */
#include "gui/same_name_dialog.h"

#include <wx/checkbox.h>
#include <wx/radiobut.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace same_name_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

class SameNameInputDialog : public wxDialog {
public:
	SameNameInputDialog(wxWindow *parent, const same_name::Context &context,
	                    const same_name::Options &initial)
		: wxDialog(parent, wxID_ANY, to_wx(_T("同名ファイルの処理")), wxDefaultPosition,
		           wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, context_(context)
		, initial_(same_name::NormalizeOptions(context, initial))
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		wxStaticText *heading = new wxStaticText(
			this, wxID_ANY, to_wx(context.task_no >= 0
			                       ? UnicodeString().sprintf(_T("タスク%u"), context.task_no + 1)
			                       : UnicodeString(_T("同名ファイル"))));
		top->Add(heading, wxSizerFlags().Border(wxALL, 8));

		modes_[0] = new wxRadioButton(this, wxID_ANY, to_wx(_T("上書き(&O)")), wxDefaultPosition,
		                              wxDefaultSize, wxRB_GROUP);
		modes_[1] = new wxRadioButton(this, wxID_ANY, to_wx(_T("更新日の新しい方だけ上書き(&N)")));
		modes_[2] = new wxRadioButton(this, wxID_ANY, to_wx(_T("スキップ(&S)")));
		modes_[3] = new wxRadioButton(this, wxID_ANY, to_wx(_T("自動的に名前を変更(&U)")));
		modes_[4] = new wxRadioButton(this, wxID_ANY, to_wx(_T("名前を変更(&R)")));
		for (int i = 0; i < 5; ++i) {
			top->Add(modes_[i], wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
			modes_[i]->Bind(wxEVT_RADIOBUTTON, &SameNameInputDialog::OnModeChanged, this);
		}
		modes_[static_cast<int>(initial_.mode)]->SetValue(true);

		rename_ = new wxTextCtrl(this, wxID_ANY, to_wx(initial_.rename_name),
		                         wxDefaultPosition, wxSize(330, -1));
		top->Add(rename_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		all_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("すべてに適用(&A)")));
		all_->SetValue(initial_.copy_all);
		top->Add(all_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		all_->Bind(wxEVT_CHECKBOX, &SameNameInputDialog::OnModeChanged, this);

		UnicodeString info;
		info += _T("元: ") + context.source + _T("\r\n");
		info += _T("先: ") + context.destination + _T("\r\n\r\n");
		info += same_name::SizeSummary(context.source_size, context.destination_size) + _T("\r\n");
		info += same_name::TimeSummary(context.source_time, context.destination_time);
		info_ = new wxTextCtrl(this, wxID_ANY, to_wx(info), wxDefaultPosition,
		                       wxSize(360, 125), wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
		top->Add(info_, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));
		SetSizerAndFit(top);
		CentreOnParent();
		UpdateEnabled();
	}

	same_name::Options Options() const
	{
		same_name::Options out;
		for (int i = 0; i < 5; ++i) {
			if (modes_[i]->GetValue()) out.mode = static_cast<same_name::Mode>(i);
		}
		out.copy_all = all_->GetValue();
		out.rename_name = to_us(rename_->GetValue()).Trim();
		return same_name::NormalizeOptions(context_, out);
	}

private:
	same_name::Mode SelectedMode() const
	{
		for (int i = 0; i < 5; ++i) if (modes_[i]->GetValue()) return static_cast<same_name::Mode>(i);
		return same_name::Mode::Overwrite;
	}

	void OnModeChanged(wxCommandEvent &event)
	{
		UpdateEnabled();
		event.Skip();
	}

	void UpdateEnabled()
	{
		if (all_->GetValue() && SelectedMode() == same_name::Mode::ManualRename)
			modes_[static_cast<int>(same_name::Mode::AutoRename)]->SetValue(true);
		const same_name::Mode mode = SelectedMode();
		for (int i = 0; i < 3; ++i) {
			modes_[i]->Enable(same_name::IsModeEnabled(static_cast<same_name::Mode>(i),
			                                               context_.same_path));
		}
		modes_[4]->Enable(!all_->GetValue());
		rename_->Enable(mode == same_name::Mode::ManualRename && !all_->GetValue());
	}

	same_name::Context context_;
	same_name::Options initial_;
	wxRadioButton *modes_[5]{};
	wxTextCtrl *rename_ = nullptr;
	wxCheckBox *all_ = nullptr;
	wxTextCtrl *info_ = nullptr;
};

}  // namespace

bool Run(wxWindow *parent, const same_name::Context &context, same_name::Options &options)
{
	SameNameInputDialog dlg(parent, context, options);
	if (dlg.ShowModal() != wxID_OK) return false;
	options = dlg.Options();
	return true;
}

}  // namespace same_name_dialog
