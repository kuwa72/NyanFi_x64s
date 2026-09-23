/**
 * @file gui/pre_same_dialog.cpp
 * @brief gui/pre_same_dialog.h の実装
 */
#include "gui/pre_same_dialog.h"

#include <wx/radiobox.h>
#include <wx/statline.h>

namespace pre_same_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<std::size_t>(s.Length()));
}

class PreSameDialog final : public wxDialog {
public:
	PreSameDialog(wxWindow *parent, pre_same::Mode mode)
		: wxDialog(parent, wxID_ANY, to_wx(_T("同名時処理の事前指定")),
		           wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
		wxArrayString choices;
		for (int i = 0; i <= static_cast<int>(pre_same::Mode::AutoRename); ++i)
			choices.Add(to_wx(pre_same::ModeLabel(static_cast<pre_same::Mode>(i))));

		radio_ = new wxRadioBox(this, wxID_ANY, to_wx(_T("処理方法")),
		                        wxDefaultPosition, wxDefaultSize, choices, 1,
		                        wxRA_SPECIFY_ROWS);
		radio_->SetSelection(static_cast<int>(pre_same::NormalizeMode(static_cast<int>(mode))));
		top->Add(radio_, wxSizerFlags().Expand().Border(wxALL, 8));
		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));

		wxBoxSizer *buttons = new wxBoxSizer(wxHORIZONTAL);
		buttons->AddStretchSpacer();
		buttons->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("中断"))), wxSizerFlags().Border(wxLEFT, 6));
		buttons->Add(new wxButton(this, wxID_OK, _T("OK")));
		top->Add(buttons, wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();
		Bind(wxEVT_BUTTON, &PreSameDialog::OnOk, this, wxID_OK);
		radio_->SetFocus();
	}

	pre_same::Mode Mode() const
	{
		return pre_same::NormalizeMode(radio_->GetSelection());
	}

private:
	void OnOk(wxCommandEvent &event) { event.Skip(); EndModal(wxID_OK); }

	wxRadioBox *radio_ = nullptr;
};

}  // namespace

bool Run(wxWindow *parent, pre_same::Mode &mode)
{
	PreSameDialog dialog(parent, mode);
	if (dialog.ShowModal() != wxID_OK) return false;
	mode = dialog.Mode();
	return true;
}

}  // namespace pre_same_dialog
