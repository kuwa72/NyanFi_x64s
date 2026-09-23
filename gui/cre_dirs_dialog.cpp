/**
 * @file gui/cre_dirs_dialog.cpp
 * @brief gui/cre_dirs_dialog.h の実装
 */
#include "gui/cre_dirs_dialog.h"

#include <wx/checkbox.h>
#include <wx/radiobox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "usr_file_ex.h"

namespace cre_dirs_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

std::vector<UnicodeString> ReadLines(const wxString &value)
{
	UnicodeString all = to_us(value);
	TStringList list;
	list.Text = all;
	std::vector<UnicodeString> out;
	out.reserve(static_cast<std::size_t>(list.Count));
	for (int i = 0; i < list.Count; ++i) out.push_back(list.Strings[i]);
	return out;
}

void WriteLines(wxTextCtrl *ctrl, const std::vector<UnicodeString> &lines)
{
	TStringList list;
	for (const UnicodeString &line : lines) list.Add(line);
	ctrl->SetValue(to_wx(list.Text));
}

}  // namespace

//---------------------------------------------------------------------------
/**
 * @brief 階層的ディレクトリ作成ダイアログ
 * @details VCL の ListMemo/AddSer/AddStr/AddDate/CreateAction の入力部分を
 *          再構成した。空行の追加、1段階の Undo、リスト編集、作成前の検証を
 *          移植し、実作成は MainFrame が file_ops2::CreateDirs を呼ぶ。
 */
class CreateDirsDialog : public wxDialog {
public:
	CreateDirsDialog(wxWindow *parent, const cre_dirs::DialogState &input)
		: wxDialog(parent, wxID_ANY, to_wx(_T("ディレクトリ一括作成")), wxDefaultPosition,
		           wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, state_(input)
	{
		if (state_.date_format.IsEmpty()) state_.date_format = _T("yyyy/mm/dd");
		if (state_.date_text.IsEmpty()) {
			state_.date_text = FormatDateTime(_T("yyyy'/'mm'/'dd"), Now());
		}
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		list_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(470, 190),
		                       wxTE_MULTILINE | wxTE_DONTWRAP);
		WriteLines(list_, state_.entries);
		top->Add(list_, wxSizerFlags(1).Expand().Border(wxALL, 8));

		wxBoxSizer *serial = new wxBoxSizer(wxHORIZONTAL);
		serial->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("連番"))), wxSizerFlags().CentreVertical());
		serial_start_ = new wxTextCtrl(this, wxID_ANY, to_wx(state_.serial_start), wxDefaultPosition, wxSize(70, -1));
		serial_inc_ = new wxTextCtrl(this, wxID_ANY, to_wx(state_.serial_increment), wxDefaultPosition, wxSize(70, -1));
		serial->Add(serial_start_, wxSizerFlags().Border(wxLEFT, 4));
		serial->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("増分"))), wxSizerFlags().CentreVertical().Border(wxLEFT, 8));
		serial->Add(serial_inc_, wxSizerFlags().Border(wxLEFT, 4));
		wxRadioBox *serial_pos = MakePositionBox(state_.serial_before);
		serial_pos_ = serial_pos;
		serial->Add(serial_pos_, wxSizerFlags().Border(wxLEFT, 8));
		wxButton *serial_add = new wxButton(this, wxID_ANY, to_wx(_T("付加")));
		serial_add->Bind(wxEVT_BUTTON, &CreateDirsDialog::OnAddSerial, this);
		serial->Add(serial_add, wxSizerFlags().Border(wxLEFT, 8));
		top->Add(serial, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxBoxSizer *text = new wxBoxSizer(wxHORIZONTAL);
		text->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("文字列"))), wxSizerFlags().CentreVertical());
		text_value_ = new wxTextCtrl(this, wxID_ANY, to_wx(state_.text), wxDefaultPosition, wxSize(150, -1));
		text->Add(text_value_, wxSizerFlags(1).Border(wxLEFT, 4));
		wxRadioBox *text_pos = MakePositionBox(state_.text_before);
		text_pos_ = text_pos;
		text->Add(text_pos_, wxSizerFlags().Border(wxLEFT, 8));
		wxButton *text_add = new wxButton(this, wxID_ANY, to_wx(_T("付加")));
		text_add->Bind(wxEVT_BUTTON, &CreateDirsDialog::OnAddText, this);
		text->Add(text_add, wxSizerFlags().Border(wxLEFT, 8));
		top->Add(text, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxBoxSizer *date = new wxBoxSizer(wxHORIZONTAL);
		date->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("日付"))), wxSizerFlags().CentreVertical());
		date_format_ = new wxTextCtrl(this, wxID_ANY, to_wx(state_.date_format), wxDefaultPosition, wxSize(110, -1));
		date_value_ = new wxTextCtrl(this, wxID_ANY, to_wx(state_.date_text), wxDefaultPosition, wxSize(100, -1));
		date->Add(date_format_, wxSizerFlags().Border(wxLEFT, 4));
		date->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("開始日"))), wxSizerFlags().CentreVertical().Border(wxLEFT, 8));
		date->Add(date_value_, wxSizerFlags().Border(wxLEFT, 4));
		wxRadioBox *date_pos = MakePositionBox(state_.date_before);
		date_pos_ = date_pos;
		date->Add(date_pos_, wxSizerFlags().Border(wxLEFT, 8));
		wxButton *date_add = new wxButton(this, wxID_ANY, to_wx(_T("付加")));
		date_add->Bind(wxEVT_BUTTON, &CreateDirsDialog::OnAddDate, this);
		date->Add(date_add, wxSizerFlags().Border(wxLEFT, 8));
		top->Add(date, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxBoxSizer *actions = new wxBoxSizer(wxHORIZONTAL);
		wxButton *empty = new wxButton(this, wxID_ANY, to_wx(_T("空項目")));
		empty->Bind(wxEVT_BUTTON, &CreateDirsDialog::OnAddEmpty, this);
		wxButton *undo = new wxButton(this, wxID_ANY, to_wx(_T("元に戻す")));
		undo->Bind(wxEVT_BUTTON, &CreateDirsDialog::OnUndo, this);
		wxButton *clear = new wxButton(this, wxID_ANY, to_wx(_T("クリア")));
		clear->Bind(wxEVT_BUTTON, &CreateDirsDialog::OnClear, this);
		actions->Add(empty, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(undo, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(clear);
		top->Add(actions, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxStaticText *note = new wxStaticText(
			this, wxID_ANY,
			to_wx(_T("※ VCLのリスト保存/読み込み・禁止文字変換・サブディレクトリ取得・進捗表示は未実装扱い。")));
		top->Add(note, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));
		SetSizerAndFit(top);
		CentreOnParent();
		list_->SetFocus();
		Bind(wxEVT_BUTTON, &CreateDirsDialog::OnOk, this, wxID_OK);
	}

	cre_dirs::DialogState Values() const
	{
		cre_dirs::DialogState out = state_;
		out.entries = ReadLines(list_->GetValue());
		out.serial_start = to_us(serial_start_->GetValue()).ToIntDef(1);
		out.serial_increment = to_us(serial_inc_->GetValue()).ToIntDef(1);
		out.serial_before = serial_pos_->GetSelection() == 0;
		out.text = to_us(text_value_->GetValue());
		out.text_before = text_pos_->GetSelection() == 0;
		out.date_format = to_us(date_format_->GetValue());
		out.date_text = to_us(date_value_->GetValue());
		out.date_before = date_pos_->GetSelection() == 0;
		return out;
	}

private:
	wxRadioBox *MakePositionBox(bool before)
	{
		wxArrayString items;
		items.Add(to_wx(_T("前")));
		items.Add(to_wx(_T("後")));
		wxRadioBox *box = new wxRadioBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                                 wxDefaultSize, items, 1, wxRA_SPECIFY_ROWS);
		box->SetSelection(before ? 0 : 1);
		return box;
	}

	void Remember()
	{
		undo_ = ReadLines(list_->GetValue());
		has_undo_ = true;
	}

	void OnAddSerial(wxCommandEvent &)
	{
		const cre_dirs::DialogState current = Values();
		const UnicodeString start_text = to_us(serial_start_->GetValue()).Trim();
		const int increment = to_us(serial_inc_->GetValue()).ToIntDef(0);
		if (!cre_dirs::CanAddSerial(start_text, increment)) {
			wxMessageBox(to_wx(_T("開始値と正の増分を入力してください")), to_wx(_T("連番")),
			             wxOK | wxICON_WARNING, this);
			return;
		}
		Remember();
		const std::vector<UnicodeString> lines = cre_dirs::AddSerial(
			current.entries, start_text.ToIntDef(1), increment, current.serial_before,
			start_text.Length());
		WriteLines(list_, lines);
	}

	void OnAddText(wxCommandEvent &)
	{
		const cre_dirs::DialogState current = Values();
		if (current.text.IsEmpty()) return;
		Remember();
		WriteLines(list_, cre_dirs::AddText(current.entries, current.text, current.text_before));
	}

	void OnAddDate(wxCommandEvent &)
	{
		const cre_dirs::DialogState current = Values();
		UnicodeString error;
		TDateTime date;
		try {
			date = str_to_DateTime(current.date_text);
		}
		catch (...) {
			wxMessageBox(to_wx(_T("日付が不正です (YYYY/MM/DD)")), to_wx(_T("日付")),
			             wxOK | wxICON_WARNING, this);
			return;
		}
		std::vector<UnicodeString> lines;
		if (!cre_dirs::AddDate(current.entries, date, current.date_format, current.date_before,
		                       lines, error)) {
			wxMessageBox(to_wx(error), to_wx(_T("日付")), wxOK | wxICON_WARNING, this);
			return;
		}
		Remember();
		WriteLines(list_, lines);
	}

	void OnAddEmpty(wxCommandEvent &)
	{
		Remember();
		std::vector<UnicodeString> lines = ReadLines(list_->GetValue());
		lines.push_back(EmptyStr);
		WriteLines(list_, lines);
	}

	void OnUndo(wxCommandEvent &)
	{
		if (!has_undo_) return;
		WriteLines(list_, undo_);
		has_undo_ = false;
	}

	void OnClear(wxCommandEvent &)
	{
		Remember();
		WriteLines(list_, {});
	}

	void OnOk(wxCommandEvent &)
	{
		const cre_dirs::DialogState current = Values();
		const cre_dirs::Validation v = cre_dirs::ValidateEntries(current.entries);
		if (!v.valid) {
			wxMessageBox(to_wx(_T("作成する項目を入力してください")), to_wx(_T("ディレクトリ一括作成")),
			             wxOK | wxICON_WARNING, this);
			return;
		}
		state_ = current;
		EndModal(wxID_OK);
	}

	cre_dirs::DialogState state_;
	wxTextCtrl *list_ = nullptr;
	wxTextCtrl *serial_start_ = nullptr;
	wxTextCtrl *serial_inc_ = nullptr;
	wxRadioBox *serial_pos_ = nullptr;
	wxTextCtrl *text_value_ = nullptr;
	wxRadioBox *text_pos_ = nullptr;
	wxTextCtrl *date_format_ = nullptr;
	wxTextCtrl *date_value_ = nullptr;
	wxRadioBox *date_pos_ = nullptr;
	std::vector<UnicodeString> undo_;
	bool has_undo_ = false;
};

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, cre_dirs::DialogState &state)
{
	CreateDirsDialog dlg(parent, state);
	if (dlg.ShowModal() != wxID_OK) return false;
	state = dlg.Values();
	return true;
}

}  // namespace cre_dirs_dialog
