/**
 * @file gui/sort_mode_dialog.cpp
 * @brief gui/sort_mode_dialog.h の実装
 */
#include "gui/sort_mode_dialog.h"

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/radiobox.h>
#include <wx/statline.h>
#include <wx/textctrl.h>

namespace sort_mode_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

wxArrayString ModeChoices()
{
	wxArrayString choices;
	choices.Add(to_wx(_T("名前(&F)")));
	choices.Add(to_wx(_T("拡張子(&E)")));
	choices.Add(to_wx(_T("更新日時(&D)")));
	choices.Add(to_wx(_T("サイズ(&S)")));
	choices.Add(to_wx(_T("属性(&A)")));
	choices.Add(to_wx(_T("なし")));
	return choices;
}

wxArrayString DirectoryChoices()
{
	wxArrayString choices;
	choices.Add(to_wx(_T("ファイルと同じ")));
	choices.Add(to_wx(_T("名前")));
	choices.Add(to_wx(_T("更新日時")));
	choices.Add(to_wx(_T("サイズ")));
	choices.Add(to_wx(_T("属性")));
	choices.Add(to_wx(_T("ディレクトリを区別しない")));
	choices.Add(to_wx(_T("アイコン(未実装)")));
	return choices;
}

wxArrayString SecondaryChoices()
{
	wxArrayString choices;
	choices.Add(to_wx(_T("名前")));
	choices.Add(to_wx(_T("拡張子")));
	choices.Add(to_wx(_T("更新日時")));
	choices.Add(to_wx(_T("サイズ")));
	choices.Add(to_wx(_T("属性")));
	choices.Add(to_wx(_T("なし")));
	return choices;
}

class SortInputDialog : public wxDialog {
public:
	SortInputDialog(wxWindow *parent, const sort_mode::Options &initial,
	                bool show_dir_options)
		: wxDialog(parent, wxID_ANY, to_wx(_T("ソート")), wxDefaultPosition,
		           wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, initial_(initial)
		, show_dir_options_(show_dir_options)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		mode_ = new wxRadioBox(this, wxID_ANY, to_wx(_T("ソート方法")),
		                       wxDefaultPosition, wxDefaultSize, ModeChoices(), 1,
		                       wxRA_SPECIFY_ROWS);
		mode_->SetSelection(sort_mode::ToIndex(initial.mode));
		top->Add(mode_, wxSizerFlags().Expand().Border(wxALL, 8));
		mode_->Bind(wxEVT_RADIOBOX, &SortInputDialog::OnModeChanged, this);

		dir_mode_ = new wxRadioBox(this, wxID_ANY, to_wx(_T("ディレクトリのソート方法")),
		                            wxDefaultPosition, wxDefaultSize, DirectoryChoices(), 1,
		                            wxRA_SPECIFY_ROWS);
		dir_mode_->SetSelection(sort_mode::ToIndex(initial.dir_mode));
		dir_mode_->Show(show_dir_options_);
		top->Add(dir_mode_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		natural_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("自然順")));
		desc_name_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("名前・拡張子，降順")));
		old_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("更新日時，降順")));
		small_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("サイズ，小さい順")));
		attr_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("属性，降順")));
		natural_->SetValue(initial.natural);
		desc_name_->SetValue(initial.descending_name);
		old_->SetValue(initial.descending_old);
		small_->SetValue(initial.descending_small);
		attr_->SetValue(initial.descending_attribute);
		top->Add(natural_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(desc_name_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(old_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(small_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(attr_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxBoxSizer *secondary = new wxBoxSizer(wxHORIZONTAL);
		secondary->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("第2ソート方法"))),
		               wxSizerFlags().CentreVertical().Border(wxRIGHT, 8));
		secondary_mode_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		                                SecondaryChoices());
		secondary_mode_->SetSelection(sort_mode::ToIndex(initial.secondary[0]));
		secondary->Add(secondary_mode_, wxSizerFlags().Expand());
		top->Add(secondary, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		ext_ = new wxTextCtrl(this, wxID_ANY, to_wx(initial.extension_list),
		                       wxDefaultPosition, wxSize(260, -1), wxTE_PROCESS_ENTER);
		wxBoxSizer *ext_row = new wxBoxSizer(wxHORIZONTAL);
		ext_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("優先する拡張子"))),
		             wxSizerFlags().CentreVertical().Border(wxRIGHT, 8));
		ext_row->Add(ext_, wxSizerFlags(1).Expand());
		top->Add(ext_row, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		both_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("左右同じ設定にする")));
		logical_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("名前と拡張子に論理順を使う")));
		acc_dt_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("更新日時(T/D)を切り替える")));
		same_close_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("同じキーで閉じたら確定")));
		extended_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("拡張設定を表示")));
		both_->SetValue(initial.both);
		logical_->SetValue(initial.logical);
		acc_dt_->SetValue(initial.acc_date_time);
		same_close_->SetValue(initial.same_close);
		extended_->SetValue(initial.show_extended);
		top->Add(both_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(logical_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(acc_dt_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(same_close_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(extended_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));
		SetSizerAndFit(top);
		CentreOnParent();
	}

	sort_mode::Options Options() const
	{
		sort_mode::Options out = initial_;
		out.mode = sort_mode::FromIndex(mode_->GetSelection());
		if (show_dir_options_) out.dir_mode = sort_mode::DirectoryFromIndex(dir_mode_->GetSelection());
		out.natural = natural_->GetValue();
		out.descending_name = desc_name_->GetValue();
		out.descending_old = old_->GetValue();
		out.descending_small = small_->GetValue();
		out.descending_attribute = attr_->GetValue();
		out.both = both_->GetValue();
		out.logical = logical_->GetValue();
		out.acc_date_time = acc_dt_->GetValue();
		out.same_close = same_close_->GetValue();
		out.show_extended = extended_->GetValue();
		out.extension_list = to_us(ext_->GetValue());
		const int sub = secondary_mode_->GetSelection();
		if (sub >= 0) out.secondary[0] = sort_mode::FromIndex(sub);
		return sort_mode::Normalize(out);
	}

private:
	void OnModeChanged(wxCommandEvent &event)
	{
		const sort_mode::Mode mode = sort_mode::FromIndex(mode_->GetSelection());
		secondary_mode_->Enable(mode != sort_mode::Mode::None);
		event.Skip();
	}

	sort_mode::Options initial_;
	bool show_dir_options_ = true;
	wxRadioBox *mode_ = nullptr;
	wxRadioBox *dir_mode_ = nullptr;
	wxCheckBox *natural_ = nullptr;
	wxCheckBox *desc_name_ = nullptr;
	wxCheckBox *old_ = nullptr;
	wxCheckBox *small_ = nullptr;
	wxCheckBox *attr_ = nullptr;
	wxChoice *secondary_mode_ = nullptr;
	wxTextCtrl *ext_ = nullptr;
	wxCheckBox *both_ = nullptr;
	wxCheckBox *logical_ = nullptr;
	wxCheckBox *acc_dt_ = nullptr;
	wxCheckBox *same_close_ = nullptr;
	wxCheckBox *extended_ = nullptr;
};

}  // namespace

bool Run(wxWindow *parent, sort_mode::Options &options, bool show_dir_options)
{
	SortInputDialog dlg(parent, options, show_dir_options);
	if (dlg.ShowModal() != wxID_OK) return false;
	options = dlg.Options();
	return true;
}

}  // namespace sort_mode_dialog
