/**
 * @file gui/input_ex_dialog.cpp
 * @brief gui/input_ex_dialog.h の実装
 */
#include "gui/inp_ex_dialog.h"

#include <algorithm>

#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/radiobox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace input_ex_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

class InputExDialog : public wxDialog {
public:
	InputExDialog(wxWindow *parent, inp_ex::Values &values)
		: wxDialog(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, values_(values)
	{
		BuildUi();
		SetSizerAndFit(top_);
		CentreOnParent();
		if (value_edit_ != nullptr) value_edit_->SetFocus();
		else if (value_combo_ != nullptr) value_combo_->SetFocus();
	}

private:
	void BuildUi()
	{
		const UnicodeString default_title = inp_ex::Title(values_.mode, values_.path_name);
		SetTitle(to_wx(values_.title.IsEmpty() ? default_title : values_.title));
		top_ = new wxBoxSizer(wxVERTICAL);

		wxBoxSizer *input_row = new wxBoxSizer(wxHORIZONTAL);
		const UnicodeString prompt = values_.prompt.IsEmpty()
			? inp_ex::Prompt(values_.mode) : values_.prompt;
		input_row->Add(new wxStaticText(this, wxID_ANY, to_wx(prompt)),
		               wxSizerFlags().CentreVertical().Border(wxRIGHT, 8));
		if (inp_ex::UsesCombo(values_.mode)) {
			value_combo_ = new wxComboBox(this, wxID_ANY, to_wx(values_.value),
			                              wxDefaultPosition,
			                              values_.custom_width > 0 ? wxSize(values_.custom_width, -1) : wxSize(360, -1),
			                              0, nullptr, wxTE_PROCESS_ENTER);
			input_row->Add(value_combo_, wxSizerFlags(1).Expand());
			value_combo_->Bind(wxEVT_TEXT, &InputExDialog::OnValueChanged, this);
		}
		else {
			value_edit_ = new wxTextCtrl(this, wxID_ANY, to_wx(values_.value),
			                             wxDefaultPosition,
			                             values_.custom_width > 0 ? wxSize(values_.custom_width, -1) : wxSize(360, -1),
			                             values_.numeric_only ? wxTE_PROCESS_ENTER : 0);
			input_row->Add(value_edit_, wxSizerFlags(1).Expand());
			value_edit_->Bind(wxEVT_TEXT, &InputExDialog::OnValueChanged, this);
		}
		top_->Add(input_row, wxSizerFlags().Expand().Border(wxALL, 8));

		switch (values_.mode) {
		case inp_ex::Mode::CreateDir:
			BuildCreateDirOptions();
			break;
		case inp_ex::Mode::NewTextFile:
		case inp_ex::Mode::ClipPaste:
			BuildNewTextOptions();
			break;
		case inp_ex::Mode::CreateTestFile:
			BuildTestFileOptions();
			break;
		case inp_ex::Mode::JumpAddress:
		case inp_ex::Mode::SetTopAddress:
			BuildAddressOptions();
			break;
		default:
			break;
		}

		const UnicodeString hint = values_.hint.IsEmpty() ? inp_ex::Hint(values_.mode) : values_.hint;
		if (!hint.IsEmpty()) {
			value_combo_ != nullptr ? value_combo_->SetHint(to_wx(hint))
			                         : value_edit_->SetHint(to_wx(hint));
		}

		top_->Add(new wxStaticText(this, wxID_ANY,
		                           to_wx(_T("※ VCLの特殊編集・IME・ヘルプ・履歴互換保存は未実装扱い。"))),
		          wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		top_->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top_->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));
		Bind(wxEVT_BUTTON, &InputExDialog::OnOk, this, wxID_OK);
	}

	void BuildCreateDirOptions()
	{
		wxBoxSizer *box = new wxBoxSizer(wxVERTICAL);
		change_dir_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("作成後にカレントへ変更")));
		change_dir_->SetValue(values_.change_dir);
		convert_chars_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("文字変換を適用")));
		convert_chars_->SetValue(values_.convert_chars);
		select_default_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("デフォルトを常に初期選択")));
		select_default_->SetValue(values_.select_default);
		box->Add(change_dir_, wxSizerFlags().Border(wxBOTTOM, 4));
		box->Add(convert_chars_, wxSizerFlags().Border(wxBOTTOM, 4));
		box->Add(select_default_);
		top_->Add(box, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		path_info_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		name_info_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		top_->Add(path_info_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top_->Add(name_info_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		UpdateLengthLabels();
	}

	void BuildNewTextOptions()
	{
		wxBoxSizer *box = new wxBoxSizer(wxVERTICAL);
		wxArrayString pages;
		for (const UnicodeString &name : inp_ex::CodePageNames()) pages.Add(to_wx(name));
		page_ctrl_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, pages);
		page_ctrl_->SetSelection(std::max(0, std::min(values_.code_page,
		                                            static_cast<int>(page_ctrl_->GetCount()) - 1)));
		box->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("文字コード"))),
		         wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		box->Add(page_ctrl_, wxSizerFlags(1).Expand());
		clip_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("クリップボードから内容を使う")));
		clip_chk_->SetValue(values_.use_clipboard);
		clip_chk_->Enable(values_.mode != inp_ex::Mode::ClipPaste);
		box->Add(clip_chk_, wxSizerFlags().Border(wxTOP, 4));
		edit_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("エディタで開く")));
		edit_chk_->SetValue(values_.edit_new_text);
		box->Add(edit_chk_);
		top_->Add(box, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
	}

	void BuildTestFileOptions()
	{
		wxFlexGridSizer *grid = new wxFlexGridSizer(2, 6, 4);
		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("サイズ"))), wxSizerFlags().CentreVertical());
		size_ctrl_ = new wxTextCtrl(this, wxID_ANY, to_wx(values_.test_size), wxDefaultPosition, wxSize(120, -1));
		grid->Add(size_ctrl_, wxSizerFlags(1).Expand());
		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("個数"))), wxSizerFlags().CentreVertical());
		count_ctrl_ = new wxTextCtrl(this, wxID_ANY, to_wx(values_.test_count), wxDefaultPosition, wxSize(80, -1));
		grid->Add(count_ctrl_, wxSizerFlags(1).Expand());
		top_->Add(grid, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
	}

	void BuildAddressOptions()
	{
		wxArrayString notations;
		notations.Add(to_wx(_T("16進")));
		notations.Add(to_wx(_T("10進")));
		notation_ctrl_ = new wxRadioBox(this, wxID_ANY, to_wx(_T("表記")),
		                                wxDefaultPosition, wxDefaultSize, notations, 1, wxRA_SPECIFY_ROWS);
		notation_ctrl_->SetSelection(values_.hexadecimal ? 0 : 1);
		top_->Add(notation_ctrl_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
	}

	void UpdateLengthLabels()
	{
		if (path_info_ == nullptr || name_info_ == nullptr) return;
		const inp_ex::LengthStatus status = inp_ex::MeasureCreateDir(
			values_.path_name, CurrentValue());
		UnicodeString path_text;
		path_text.sprintf(_T("フルパス名の文字数 = %d"), status.path_length);
		UnicodeString name_text;
		name_text.sprintf(_T("名前の文字数 = %d"), status.name_length);
		path_info_->SetLabel(to_wx(path_text));
		name_info_->SetLabel(to_wx(name_text));
	}

	UnicodeString CurrentValue() const
	{
		if (value_combo_ != nullptr) return to_us(value_combo_->GetValue());
		if (value_edit_ != nullptr) return to_us(value_edit_->GetValue());
		return values_.value;
	}

	void OnValueChanged(wxCommandEvent &)
	{
		UpdateLengthLabels();
	}

	void OnOk(wxCommandEvent &)
	{
		inp_ex::Values current = values_;
		current.value = CurrentValue();
		if (page_ctrl_ != nullptr) current.code_page = page_ctrl_->GetSelection();
		if (clip_chk_ != nullptr) current.use_clipboard = clip_chk_->GetValue();
		if (edit_chk_ != nullptr) current.edit_new_text = edit_chk_->GetValue();
		if (change_dir_ != nullptr) current.change_dir = change_dir_->GetValue();
		if (convert_chars_ != nullptr) current.convert_chars = convert_chars_->GetValue();
		if (select_default_ != nullptr) current.select_default = select_default_->GetValue();
		if (size_ctrl_ != nullptr) current.test_size = to_us(size_ctrl_->GetValue()).Trim();
		if (count_ctrl_ != nullptr) current.test_count = to_us(count_ctrl_->GetValue()).ToIntDef(0);
		if (notation_ctrl_ != nullptr) current.hexadecimal = notation_ctrl_->GetSelection() == 0;

		UnicodeString error;
		if (!inp_ex::Validate(current, error)) {
			wxMessageBox(to_wx(error), to_wx(_T("入力")), wxOK | wxICON_WARNING, this);
			return;
		}
		inp_ex::NormalizeOnClose(current);
		values_ = current;
		EndModal(wxID_OK);
	}

	inp_ex::Values &values_;
	wxBoxSizer *top_ = nullptr;
	wxTextCtrl *value_edit_ = nullptr;
	wxComboBox *value_combo_ = nullptr;
	wxChoice *page_ctrl_ = nullptr;
	wxCheckBox *clip_chk_ = nullptr;
	wxCheckBox *edit_chk_ = nullptr;
	wxCheckBox *change_dir_ = nullptr;
	wxCheckBox *convert_chars_ = nullptr;
	wxCheckBox *select_default_ = nullptr;
	wxRadioBox *notation_ctrl_ = nullptr;
	wxTextCtrl *size_ctrl_ = nullptr;
	wxTextCtrl *count_ctrl_ = nullptr;
	wxStaticText *path_info_ = nullptr;
	wxStaticText *name_info_ = nullptr;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, inp_ex::Values &values)
{
	InputExDialog dlg(parent, values);
	return dlg.ShowModal() == wxID_OK;
}

}  // namespace input_ex_dialog
