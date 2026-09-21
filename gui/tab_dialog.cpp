/**
 * @file gui/tab_dialog.cpp
 * @brief gui/tab_dialog.h の実装
 */
#include "gui/tab_dialog.h"

#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/radiobox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace tab_dialog {

namespace {

/// wxString への変換 (gui/dupl_dialog.cpp と同じ変換ヘルパー)
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
 * @brief タブの設定入力ダイアログ
 * @details `src/TabDlg.cpp` (`TTabSetDlg`) を wx で再構成したもの。
 *          参照ボタンは `UserModule->SelectDirEx` / `PrepareOpenDlg` +
 *          `OpenDlgToEdit` の置き換え (wxDirDialog / wxFileDialog)。
 *          アイコンのプレビューは未移植のため無い (ヘッダの説明を参照)
 */
class TabInputDialog : public wxDialog {
public:
	TabInputDialog(wxWindow *parent, const tab_settings::TabSettings &initial,
	               const UnicodeString &current0, const UnicodeString &current1,
	               const UnicodeString &group_title)
		: wxDialog(parent, wxID_ANY,
		           to_wx(group_title.IsEmpty() ? UnicodeString(_T("タブの設定"))
		                                       : UnicodeString(_T("タブの設定 - ")) + group_title),
		           wxDefaultPosition, wxDefaultSize,
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, current0_(current0)
		, current1_(current1)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		wxFlexGridSizer *grid = new wxFlexGridSizer(3, 4, 8);
		grid->AddGrowableCol(1);

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("キャプション"))),
		          wxSizerFlags().CentreVertical());
		caption_ctrl_ = new wxTextCtrl(this, wxID_ANY, to_wx(initial.caption),
		                               wxDefaultPosition, wxSize(280, -1));
		grid->Add(caption_ctrl_, wxSizerFlags(1).Expand());
		grid->Add(new wxStaticText(this, wxID_ANY, wxEmptyString));

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("アイコン"))),
		          wxSizerFlags().CentreVertical());
		icon_ctrl_ = new wxTextCtrl(this, wxID_ANY, to_wx(initial.icon),
		                            wxDefaultPosition, wxSize(280, -1));
		grid->Add(icon_ctrl_, wxSizerFlags(1).Expand());
		icon_ref_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("参照...")));
		grid->Add(icon_ref_btn_);
		icon_ref_btn_->Bind(wxEVT_BUTTON, &TabInputDialog::OnRefIcon, this);

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("左のホーム"))),
		          wxSizerFlags().CentreVertical());
		home0_ctrl_ = new wxTextCtrl(this, wxID_ANY, to_wx(initial.home0),
		                             wxDefaultPosition, wxSize(280, -1));
		grid->Add(home0_ctrl_, wxSizerFlags(1).Expand());
		home0_ref_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("参照...")));
		grid->Add(home0_ref_btn_);
		home0_ref_btn_->Bind(wxEVT_BUTTON, &TabInputDialog::OnRefHome0, this);

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("右のホーム"))),
		          wxSizerFlags().CentreVertical());
		home1_ctrl_ = new wxTextCtrl(this, wxID_ANY, to_wx(initial.home1),
		                             wxDefaultPosition, wxSize(280, -1));
		grid->Add(home1_ctrl_, wxSizerFlags(1).Expand());
		home1_ref_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("参照...")));
		grid->Add(home1_ref_btn_);
		home1_ref_btn_->Bind(wxEVT_BUTTON, &TabInputDialog::OnRefHome1, this);

		top->Add(grid, wxSizerFlags().Expand().Border(wxALL, 8));

		// VCL の SetCurDirBtn (現在のディレクトリを両ホームに入れる)
		set_cur_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("現在のディレクトリを設定")));
		top->Add(set_cur_btn_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		set_cur_btn_->Bind(wxEVT_BUTTON, &TabInputDialog::OnSetCurDir, this);

		// VCL の Work*RadioBtn (0=使わない/1=現在のワークリスト/2=指定)
		wxArrayString work_modes;
		work_modes.Add(to_wx(_T("使わない")));
		work_modes.Add(to_wx(_T("現在のワークリスト")));
		work_modes.Add(to_wx(_T("指定のワークリスト")));
		work_radio_ = new wxRadioBox(this, wxID_ANY, to_wx(_T("ワークリスト")),
		                             wxDefaultPosition, wxDefaultSize, work_modes, 1,
		                             wxRA_SPECIFY_COLS);
		work_radio_->SetSelection(initial.work_mode >= 0 && initial.work_mode <= 2
		                              ? initial.work_mode : 0);
		top->Add(work_radio_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		work_radio_->Bind(wxEVT_RADIOBOX, &TabInputDialog::OnWorkMode, this);

		wxBoxSizer *work_row = new wxBoxSizer(wxHORIZONTAL);
		work_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("ワークリスト"))),
		              wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		work_ctrl_ = new wxTextCtrl(this, wxID_ANY, to_wx(initial.work_list),
		                            wxDefaultPosition, wxSize(220, -1));
		work_row->Add(work_ctrl_, wxSizerFlags(1).Expand().Border(wxRIGHT, 4));
		work_ref_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("参照...")));
		work_row->Add(work_ref_btn_);
		work_ref_btn_->Bind(wxEVT_BUTTON, &TabInputDialog::OnRefWork, this);
		top->Add(work_row, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		UpdateWorkEnabled();

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		caption_ctrl_->SetFocus();
		caption_ctrl_->SelectAll();
	}

	tab_settings::TabSettings Settings() const
	{
		tab_settings::TabSettings s;
		s.caption = to_us(caption_ctrl_->GetValue());
		s.icon = to_us(icon_ctrl_->GetValue());
		s.home0 = to_us(home0_ctrl_->GetValue());
		s.home1 = to_us(home1_ctrl_->GetValue());
		s.work_mode = work_radio_->GetSelection();
		// VCL の OkButtonClick: mode!=2 なら [7] に空を書く
		s.work_list = (s.work_mode == tab_settings::kWorkNamed)
			? to_us(work_ctrl_->GetValue()) : EmptyStr;
		return s;
	}

private:
	// VCL の WorkRadioBtnClick: 指定のときだけ WorkListEdit を触れる
	void UpdateWorkEnabled()
	{
		const bool named = (work_radio_->GetSelection() == tab_settings::kWorkNamed);
		work_ctrl_->Enable(named);
		work_ref_btn_->Enable(named);
	}

	void OnWorkMode(wxCommandEvent &) { UpdateWorkEnabled(); }

	void OnSetCurDir(wxCommandEvent &)
	{
		home0_ctrl_->SetValue(to_wx(current0_));
		home1_ctrl_->SetValue(to_wx(current1_));
	}

	void OnRefIcon(wxCommandEvent &)
	{
		// VCL の PrepareOpenDlg("タブのアイコン", F_FILTER_ICO) 相当
		wxFileDialog dlg(this, to_wx(_T("タブのアイコン")),
		                 wxEmptyString, wxEmptyString,
		                 to_wx(_T("アイコン (*.ico;*.exe)|*.ico;*.exe|すべてのファイル (*.*)|*.*")),
		                 wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() == wxID_OK) icon_ctrl_->SetValue(dlg.GetPath());
	}

	void PickDir(const wxString &title, wxTextCtrl *edit)
	{
		// VCL の SelectDirEx 相当
		wxDirDialog dlg(this, title, edit->GetValue(), wxDD_DEFAULT_STYLE);
		if (dlg.ShowModal() == wxID_OK) edit->SetValue(dlg.GetPath());
	}

	void OnRefHome0(wxCommandEvent &) { PickDir(to_wx(_T("左のホーム")), home0_ctrl_); }
	void OnRefHome1(wxCommandEvent &) { PickDir(to_wx(_T("右のホーム")), home1_ctrl_); }

	void OnRefWork(wxCommandEvent &)
	{
		// VCL の PrepareOpenDlg("ワークリストの指定", F_FILTER_NWL, "*.nwl") 相当
		wxFileDialog dlg(this, to_wx(_T("ワークリストの指定")),
		                 wxEmptyString, wxEmptyString,
		                 to_wx(_T("ワークリスト (*.nwl)|*.nwl|すべてのファイル (*.*)|*.*")),
		                 wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() == wxID_OK) work_ctrl_->SetValue(dlg.GetPath());
	}

	const UnicodeString current0_;
	const UnicodeString current1_;
	wxTextCtrl *caption_ctrl_ = nullptr;
	wxTextCtrl *icon_ctrl_ = nullptr;
	wxTextCtrl *home0_ctrl_ = nullptr;
	wxTextCtrl *home1_ctrl_ = nullptr;
	wxTextCtrl *work_ctrl_ = nullptr;
	wxButton *icon_ref_btn_ = nullptr;
	wxButton *home0_ref_btn_ = nullptr;
	wxButton *home1_ref_btn_ = nullptr;
	wxButton *set_cur_btn_ = nullptr;
	wxButton *work_ref_btn_ = nullptr;
	wxRadioBox *work_radio_ = nullptr;
};

}  // namespace

bool Run(wxWindow *parent, tab_settings::TabSettings &settings,
         const UnicodeString &current0, const UnicodeString &current1,
         const UnicodeString &group_title)
{
	TabInputDialog dlg(parent, settings, current0, current1, group_title);
	if (dlg.ShowModal() != wxID_OK) return false;
	settings = dlg.Settings();
	return true;
}

}  // namespace tab_dialog
