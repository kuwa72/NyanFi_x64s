/**
 * @file gui/cv_enc_dialog.cpp
 * @brief gui/cv_enc_dialog.h の実装
 */
#include "gui/cv_enc_dialog.h"

#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/statline.h>
#include <wx/stattext.h>

namespace cv_enc_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<std::size_t>(s.Length()));
}

class CvEncDialog final : public wxDialog {
public:
	CvEncDialog(wxWindow *parent, const Options &options)
		: wxDialog(parent, wxID_ANY,
		           to_wx(_T("文字コードの変換") + options.title_suffix),
		           wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
		wxFlexGridSizer *grid = new wxFlexGridSizer(2, 2, 8);
		grid->AddGrowableCol(1, 1);

		wxArrayString encodings;
		for (const UnicodeString &name : cv_enc::EncodingNames()) encodings.Add(to_wx(name));
		code_ctrl_ = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                            wxDefaultSize, encodings, wxCB_DROPDOWN | wxCB_READONLY);
		code_ctrl_->SetSelection(cv_enc::NormalizeSelection(
			options.code_index, static_cast<int>(cv_enc::EncodingNames().size())));
		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("出力コード"))), wxSizerFlags().CentreVertical());
		grid->Add(code_ctrl_, wxSizerFlags(1).Expand());

		wxArrayString breaks;
		breaks.Add(to_wx(_T("CR/LF")));
		breaks.Add(to_wx(_T("LF")));
		breaks.Add(to_wx(_T("CR")));
		line_ctrl_ = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                            wxDefaultSize, breaks, wxCB_DROPDOWN | wxCB_READONLY);
		line_ctrl_->SetSelection(options.line_break_index == 1? 1 : options.line_break_index == 2? 2 : 0);
		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("改行"))), wxSizerFlags().CentreVertical());
		grid->Add(line_ctrl_, wxSizerFlags(1).Expand());
		top->Add(grid, wxSizerFlags().Expand().Border(wxALL, 8));

		bom_ctrl_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("BOM を付ける")));
		bom_ctrl_->SetValue(options.with_bom);
		top->Add(bom_ctrl_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		wxBoxSizer *buttons = new wxBoxSizer(wxHORIZONTAL);
		buttons->AddStretchSpacer();
		buttons->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("キャンセル"))), wxSizerFlags().Border(wxLEFT, 6));
		ok_ctrl_ = new wxButton(this, wxID_OK, to_wx(_T("開始")));
		buttons->Add(ok_ctrl_);
		top->Add(buttons, wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();
		Bind(wxEVT_COMBOBOX, &CvEncDialog::OnCodeChanged, this, code_ctrl_->GetId());
		Bind(wxEVT_BUTTON, &CvEncDialog::OnOk, this, wxID_OK);
		UpdateEnabled();
		code_ctrl_->SetFocus();
	}

	const Options &Result() const { return result_; }

private:
	void UpdateEnabled()
	{
		bom_ctrl_->Enable(cv_enc::BomAvailable(
			cv_enc::EncodingNames()[static_cast<std::size_t>(code_ctrl_->GetSelection())]));
		ok_ctrl_->Enable(cv_enc::CanSubmit(code_ctrl_->GetSelection()));
	}

	void OnCodeChanged(wxCommandEvent &event) { UpdateEnabled(); event.Skip(); }

	void OnOk(wxCommandEvent &)
	{
		if (!cv_enc::CanSubmit(code_ctrl_->GetSelection())) return;
		result_.code_index = code_ctrl_->GetSelection();
		result_.line_break_index = line_ctrl_->GetSelection();
		result_.with_bom = bom_ctrl_->GetValue();
		EndModal(wxID_OK);
	}

	wxComboBox *code_ctrl_ = nullptr;
	wxComboBox *line_ctrl_ = nullptr;
	wxCheckBox *bom_ctrl_ = nullptr;
	wxButton *ok_ctrl_ = nullptr;
	Options result_;
};

}  // namespace

bool Run(wxWindow *parent, Options &options)
{
	CvEncDialog dialog(parent, options);
	if (dialog.ShowModal() != wxID_OK) return false;
	const Options result = dialog.Result();
	options.code_index = result.code_index;
	options.line_break_index = result.line_break_index;
	options.with_bom = result.with_bom;
	options.title_suffix = EmptyStr;
	return true;
}

}  // namespace cv_enc_dialog
