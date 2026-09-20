/**
 * @file gui/mask_dialog.cpp
 * @brief gui/mask_dialog.h の実装
 */
#include "gui/mask_dialog.h"

#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace mask_dialog {

namespace {

/// wxString への変換 (gui/grep_dialog.cpp と同じ変換ヘルパー)
inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

/// wxString → UnicodeString (MSW では両方 UTF-16)
inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

/**
 * @brief マスク/マッチ選択の入力ダイアログ
 * @details `src/MaskSelDlg.cpp` (`TMaskSelectDlg`) の入力部分だけを
 *          wx で再構成したもの。ヒント文言は VCL 版 (`MaskSelComboBox->Hint`)
 *          と同じにした
 */
class MaskInputDialog : public wxDialog {
public:
	MaskInputDialog(wxWindow *parent, Mode mode, const UnicodeString &initial)
		: wxDialog(parent, wxID_ANY,
		           to_wx(mode == Mode::Match ? _T("マッチ選択") : _T("マスク選択")),
		           wxDefaultPosition, wxDefaultSize,
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		top->Add(new wxStaticText(
			         this, wxID_ANY,
			         to_wx(mode == Mode::Match
			               ? _T("; で区切って複数指定可能、/～/ は正規表現")
			               : _T("; で区切って複数指定可能"))),
		         wxSizerFlags().Border(wxALL, 8));

		pattern_ctrl_ = new wxTextCtrl(this, wxID_ANY,
		                               to_wx(initial.IsEmpty() ? _T("*") : initial),
		                               wxDefaultPosition, wxSize(320, -1));
		top->Add(pattern_ctrl_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		pattern_ctrl_->SetFocus();
		pattern_ctrl_->SelectAll();

		Bind(wxEVT_BUTTON, &MaskInputDialog::OnOk, this, wxID_OK);
	}

	UnicodeString Pattern() const { return to_us(pattern_ctrl_->GetValue()); }

private:
	// 空での実行を止める (VCL は空で何もしない。MainFrm.cpp:21978 付近)
	void OnOk(wxCommandEvent & /*event*/)
	{
		if (to_us(pattern_ctrl_->GetValue()).Trim().IsEmpty()) {
			wxMessageBox(to_wx(_T("入力してください")),
			             to_wx(_T("マスク選択")), wxOK | wxICON_WARNING, this);
			return;
		}
		EndModal(wxID_OK);
	}

	wxTextCtrl *pattern_ctrl_ = nullptr;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, Mode mode, UnicodeString &pattern_out, const UnicodeString &initial)
{
	MaskInputDialog dlg(parent, mode, initial);
	if (dlg.ShowModal() != wxID_OK) return false;
	pattern_out = dlg.Pattern();
	return true;
}

}  // namespace mask_dialog
