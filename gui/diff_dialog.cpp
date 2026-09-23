/**
 * @file gui/diff_dialog.cpp
 * @brief gui/diff_dialog.h の実装
 */
#include "gui/diff_dialog.h"

#include <wx/combobox.h>
#include <wx/checkbox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace diff_dialog {

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
 * @brief ディレクトリ比較の条件入力ダイアログ
 * @details `src/DiffDlg.cpp` (`TDiffDirDlg`) の入力部分だけを wx で
 *          再構成したもの。除外ディレクトリ欄はサブディレクトリ対象の
 *          ときだけ有効になる (`TDiffDirDlg::StartActionUpdate` と同じ)
 */
class DiffInputDialog : public wxDialog {
public:
	DiffInputDialog(wxWindow *parent, const UnicodeString &src_dir, const UnicodeString &dst_dir,
	                const compare::DiffDirOptions &opt)
		: wxDialog(parent, wxID_ANY,
		           to_wx(opt.case_sensitive
		                 ? _T("ディレクトリの比較 - 大文字・小文字を区別")
		                 : _T("ディレクトリの比較")),
		           wxDefaultPosition, wxDefaultSize,
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		wxFlexGridSizer *grid = new wxFlexGridSizer(2, 4, 8);
		grid->AddGrowableCol(1);

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("比較元"))),
		          wxSizerFlags().CentreVertical());
		src_ctrl_ = new wxTextCtrl(this, wxID_ANY, to_wx(src_dir),
		                           wxDefaultPosition, wxSize(360, -1), wxTE_READONLY);
		grid->Add(src_ctrl_, wxSizerFlags(1).Expand());

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("比較先"))),
		          wxSizerFlags().CentreVertical());
		dst_ctrl_ = new wxTextCtrl(this, wxID_ANY, to_wx(dst_dir),
		                           wxDefaultPosition, wxSize(360, -1), wxTE_READONLY);
		grid->Add(dst_ctrl_, wxSizerFlags(1).Expand());

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("対象マスク"))),
		          wxSizerFlags().CentreVertical());
		inc_ctrl_ = new wxComboBox(this, wxID_ANY, to_wx(opt.inc_mask),
		                           wxDefaultPosition, wxDefaultSize, 0, nullptr, wxTE_PROCESS_ENTER);
		inc_ctrl_->SetToolTip(to_wx(_T("; で区切って複数指定可能")));
		grid->Add(inc_ctrl_, wxSizerFlags(1).Expand());

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("除外マスク"))),
		          wxSizerFlags().CentreVertical());
		exc_ctrl_ = new wxComboBox(this, wxID_ANY, to_wx(opt.exc_mask),
		                           wxDefaultPosition, wxDefaultSize, 0, nullptr, wxTE_PROCESS_ENTER);
		exc_ctrl_->SetToolTip(to_wx(_T("; で区切って複数指定可能")));
		grid->Add(exc_ctrl_, wxSizerFlags(1).Expand());

		top->Add(grid, wxSizerFlags().Expand().Border(wxALL, 8));

		sub_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("サブディレクトリも対象にする(&S)")));
		sub_chk_->SetValue(opt.sub_dir);
		top->Add(sub_chk_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxFlexGridSizer *exc_grid = new wxFlexGridSizer(2, 4, 8);
		exc_grid->AddGrowableCol(1);
		exc_grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("除外マスク"))),
		              wxSizerFlags().CentreVertical());
		exc_dir_ctrl_ = new wxComboBox(this, wxID_ANY, to_wx(opt.exc_dir),
		                               wxDefaultPosition, wxDefaultSize, 0, nullptr,
		                               wxTE_PROCESS_ENTER);
		exc_dir_ctrl_->SetToolTip(to_wx(_T("; で区切って複数指定可能")));
		exc_grid->Add(exc_dir_ctrl_, wxSizerFlags(1).Expand());
		top->Add(exc_grid, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		UpdateExcDir();
		sub_chk_->Bind(wxEVT_CHECKBOX, &DiffInputDialog::OnSubDir, this);

		inc_ctrl_->SetFocus();
		inc_ctrl_->SelectAll();
	}

	compare::DiffDirOptions Options(bool case_sensitive) const
	{
		compare::DiffDirOptions opt;
		// VCL (TDiffDirDlg::FormClose): 空の対象マスクは *.* にする
		opt.inc_mask = compare::NormalizeDiffIncMask(to_us(inc_ctrl_->GetValue()));
		opt.exc_mask = to_us(exc_ctrl_->GetValue());
		opt.sub_dir = sub_chk_->GetValue();
		// VCL は無効でも値を残す。こちらもそのまま返す
		opt.exc_dir = to_us(exc_dir_ctrl_->GetValue());
		opt.case_sensitive = case_sensitive;
		return opt;
	}

private:
	// VCL (TDiffDirDlg::StartActionUpdate): 除外欄は SubDir のときだけ有効
	void OnSubDir(wxCommandEvent & /*event*/) { UpdateExcDir(); }

	void UpdateExcDir()
	{
		exc_dir_ctrl_->Enable(compare::IsDiffExcDirEnabled(sub_chk_->GetValue()));
	}

	wxTextCtrl *src_ctrl_ = nullptr;
	wxTextCtrl *dst_ctrl_ = nullptr;
	wxComboBox *inc_ctrl_ = nullptr;
	wxComboBox *exc_ctrl_ = nullptr;
	wxCheckBox *sub_chk_ = nullptr;
	wxComboBox *exc_dir_ctrl_ = nullptr;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, const UnicodeString &src_dir, const UnicodeString &dst_dir,
         compare::DiffDirOptions &opt_inout)
{
	DiffInputDialog dlg(parent, src_dir, dst_dir, opt_inout);
	if (dlg.ShowModal() != wxID_OK) return false;
	opt_inout = dlg.Options(opt_inout.case_sensitive);
	return true;
}

}  // namespace diff_dialog
