/**
 * @file gui/dupl_dialog.cpp
 * @brief gui/dupl_dialog.h の実装
 */
#include "gui/dupl_dialog.h"

#include <wx/checkbox.h>
#include <wx/radiobox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace dupl_dialog {

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
 * @brief 重複ファイル検索の条件入力ダイアログ
 * @details `src/DuplDlg.cpp` (`TFindDuplDlg`) の移植可能な部分
 *          (判定方法・サブディレクトリ) とマスクだけを wx で再構成したもの
 */
class DuplInputDialog : public wxDialog {
public:
	DuplInputDialog(wxWindow *parent)
		: wxDialog(parent, wxID_ANY, to_wx(_T("重複ファイルの検索")), wxDefaultPosition, wxDefaultSize,
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		wxArrayString methods;
		methods.Add(to_wx(_T("内容で比べる (サイズで絞ってからハッシュ)")));
		methods.Add(to_wx(_T("名前とサイズで比べる (速い)")));
		method_radio_ = new wxRadioBox(this, wxID_ANY, to_wx(_T("判定方法")),
		                               wxDefaultPosition, wxDefaultSize, methods, 1, wxRA_SPECIFY_COLS);
		method_radio_->SetSelection(0);
		top->Add(method_radio_, wxSizerFlags().Expand().Border(wxALL, 8));

		wxFlexGridSizer *grid = new wxFlexGridSizer(2, 4, 8);
		grid->AddGrowableCol(1);
		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("マスク (; 区切り)"))),
		          wxSizerFlags().CentreVertical());
		mask_ctrl_ = new wxTextCtrl(this, wxID_ANY, _T("*"), wxDefaultPosition, wxSize(280, -1));
		grid->Add(mask_ctrl_, wxSizerFlags(1).Expand());
		top->Add(grid, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		recursive_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("サブディレクトリを含む")));
		recursive_chk_->SetValue(true);
		top->Add(recursive_chk_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		mask_ctrl_->SetFocus();
		mask_ctrl_->SelectAll();
	}

	find_files::DuplicateOptions Options() const
	{
		find_files::DuplicateOptions opt;
		opt.how = (method_radio_->GetSelection() == 1) ? find_files::DuplicateBy::NameSize
		                                               : find_files::DuplicateBy::Content;
		opt.mask = to_us(mask_ctrl_->GetValue());
		opt.recursive = recursive_chk_->GetValue();
		return opt;
	}

private:
	wxRadioBox *method_radio_ = nullptr;
	wxTextCtrl *mask_ctrl_ = nullptr;
	wxCheckBox *recursive_chk_ = nullptr;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, find_files::DuplicateOptions &opt_out)
{
	DuplInputDialog dlg(parent);
	if (dlg.ShowModal() != wxID_OK) return false;
	opt_out = dlg.Options();
	return true;
}

}  // namespace dupl_dialog
