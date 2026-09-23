/**
 * @file gui/find_txt_dialog.cpp
 * @brief gui/find_txt_dialog.h の実装
 */
#include "gui/find_txt_dialog.h"

#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/radiobox.h>
#include <wx/statline.h>
#include <wx/stattext.h>

namespace find_txt_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

const int kCodePages[] = {932, 1252, 1200, 1201, 20127, 20932, 65000, 65001};

}  // namespace

//---------------------------------------------------------------------------
/**
 * @brief テキスト内検索ダイアログ
 * @details VCL の FindOptChangedClick/MigemoCheckBoxClick/RegExCheckBoxClick
 *          (src/FindTxtDlg.cpp:116-155) の排他・有効状態を gui/find_txt.h
 *          と同じ規則で再現する。検索実処理は TextViewer 側で行う。
 */
class FindTextDialog : public wxDialog {
public:
	FindTextDialog(wxWindow *parent, bool binary, const find_txt::Options &input)
		: wxDialog(parent, wxID_ANY, to_wx(binary ? _T("バイト列検索") : _T("文字列検索")),
		           wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, binary_(binary)
	{
		const find_txt::Options initial = find_txt::Normalize(input, binary);
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		wxBoxSizer *keyword_row = new wxBoxSizer(wxHORIZONTAL);
		keyword_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("検索語"))),
		                 wxSizerFlags().CentreVertical());
		keyword_ = new wxComboBox(this, wxID_ANY, to_wx(initial.keyword), wxDefaultPosition,
		                           wxSize(350, -1), wxTE_PROCESS_ENTER);
		keyword_row->Add(keyword_, wxSizerFlags(1).Border(wxLEFT, 6));
		wxButton *find_button = new wxButton(this, wxID_OK, to_wx(_T("次を検索")));
		keyword_row->Add(find_button, wxSizerFlags().Border(wxLEFT, 6));
		top->Add(keyword_row, wxSizerFlags().Expand().Border(wxALL, 8));

		wxBoxSizer *options = new wxBoxSizer(wxHORIZONTAL);
		case_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("大文字小文字を区別")));
		case_->SetValue(initial.case_sensitive);
		word_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("単語")));
		word_->SetValue(initial.whole_word);
		regex_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("正規表現")));
		regex_->SetValue(initial.regex);
		migemo_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("Migemo")));
		migemo_->SetValue(initial.migemo);
		options->Add(case_, wxSizerFlags().Border(wxRIGHT, 10));
		options->Add(word_, wxSizerFlags().Border(wxRIGHT, 10));
		options->Add(regex_, wxSizerFlags().Border(wxRIGHT, 10));
		options->Add(migemo_);
		top->Add(options, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxArrayString dirs;
		dirs.Add(to_wx(_T("上へ")));
		dirs.Add(to_wx(_T("下へ")));
		wxRadioBox *direction = new wxRadioBox(this, wxID_ANY, to_wx(_T("検索方向")),
		                                       wxDefaultPosition, wxDefaultSize, dirs, 1,
		                                       wxRA_SPECIFY_ROWS);
		direction->SetSelection(find_txt::DirectionIndex(initial.direction));
		direction_ = direction;
		top->Add(direction_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxBoxSizer *bottom = new wxBoxSizer(wxHORIZONTAL);
		bytes_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("バイト列")));
		bytes_->SetValue(initial.bytes);
		bottom->Add(bytes_, wxSizerFlags().Border(wxRIGHT, 12));
		wxArrayString pages;
		for (int cp : kCodePages) {
			if (cp == 932) pages.Add(to_wx(_T("Shift_JIS")));
			else if (cp == 1252) pages.Add(to_wx(_T("ISO-8859-1")));
			else if (cp == 1200) pages.Add(to_wx(_T("UTF-16")));
			else if (cp == 1201) pages.Add(to_wx(_T("UTF-16BE")));
			else if (cp == 20127) pages.Add(to_wx(_T("US-ASCII")));
			else if (cp == 20932) pages.Add(to_wx(_T("EUC-JP")));
			else if (cp == 65000) pages.Add(to_wx(_T("UTF-7")));
			else pages.Add(to_wx(_T("UTF-8")));
		}
		code_page_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, pages);
		for (int i = 0; i < static_cast<int>(sizeof(kCodePages) / sizeof(kCodePages[0])); ++i) {
			if (kCodePages[i] == initial.code_page) code_page_->SetSelection(i);
		}
		bottom->Add(code_page_, wxSizerFlags().CentreVertical());
		highlight_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("検索結果を強調")));
		highlight_->SetValue(initial.highlight);
		bottom->Add(highlight_, wxSizerFlags().Border(wxLEFT, 16));
		close_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("検索後に閉じる")));
		close_->SetValue(initial.close_after);
		bottom->Add(close_, wxSizerFlags().Border(wxLEFT, 12));
		top->Add(bottom, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxStaticText *note = new wxStaticText(
			this, wxID_ANY,
			to_wx(_T("※ wx版は文字列検索・正規表現・大小文字/単語判定を実装。\n"
			         _T("   Migemo、バイト列の実検索、強調描画は未実装扱い。"))));
		top->Add(note, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));
		SetSizerAndFit(top);
		CentreOnParent();

		for (wxWindow *w : {static_cast<wxWindow *>(case_), static_cast<wxWindow *>(word_),
		                     static_cast<wxWindow *>(regex_), static_cast<wxWindow *>(migemo_),
		                     static_cast<wxWindow *>(bytes_), static_cast<wxWindow *>(highlight_),
		                     static_cast<wxWindow *>(close_)}) {
			w->Bind(wxEVT_CHECKBOX, &FindTextDialog::OnOptionChanged, this);
		}
		code_page_->Bind(wxEVT_CHOICE, &FindTextDialog::OnOptionChanged, this);
		keyword_->Bind(wxEVT_TEXT, &FindTextDialog::OnOptionChanged, this);
		Bind(wxEVT_BUTTON, &FindTextDialog::OnOk, this, wxID_OK);
		UpdateEnabled();
	}

	find_txt::Options Values() const
	{
		find_txt::Options o;
		o.keyword = to_us(keyword_->GetValue());
		o.case_sensitive = case_->GetValue();
		o.whole_word = word_->GetValue();
		o.regex = regex_->GetValue();
		o.migemo = migemo_->GetValue();
		o.bytes = bytes_->GetValue();
		o.highlight = highlight_->GetValue();
		o.close_after = close_->GetValue();
		o.direction = find_txt::DirectionFromIndex(direction_->GetSelection());
		const int page = code_page_->GetSelection();
		o.code_page = (page >= 0 && page < static_cast<int>(sizeof(kCodePages) / sizeof(kCodePages[0])))
		              ? kCodePages[page] : 932;
		return find_txt::Normalize(o, binary_);
	}

private:
	void OnOptionChanged(wxCommandEvent &event)
	{
		// VCL と同じく、bytes > Migemo > RegEx の排他を先に解決する。
		if (bytes_->GetValue()) {
			word_->SetValue(false);
			regex_->SetValue(false);
			migemo_->SetValue(false);
		}
		else if (migemo_->GetValue()) {
			regex_->SetValue(false);
		}
		else if (regex_->GetValue()) {
			migemo_->SetValue(false);
		}
		UpdateEnabled();
		event.Skip();
	}

	void UpdateEnabled()
	{
		const find_txt::Availability a = find_txt::ResolveAvailability(binary_);
		word_->Enable(a.word && !bytes_->GetValue());
		regex_->Enable(a.regex && !bytes_->GetValue() && !migemo_->GetValue());
		migemo_->Enable(a.migemo && !bytes_->GetValue());
		bytes_->Enable(a.bytes);
		code_page_->Enable(a.code_page && bytes_->GetValue());
		highlight_->Enable(a.highlight && !bytes_->GetValue());
	}

	void OnOk(wxCommandEvent &)
	{
		const find_txt::Options o = Values();
		UnicodeString error;
		if (!find_txt::Validate(o, error)) {
			wxMessageBox(to_wx(error), to_wx(_T("文字列検索")), wxOK | wxICON_WARNING, this);
			return;
		}
		EndModal(wxID_OK);
	}

	bool binary_ = false;
	wxComboBox *keyword_ = nullptr;
	wxCheckBox *case_ = nullptr;
	wxCheckBox *word_ = nullptr;
	wxCheckBox *regex_ = nullptr;
	wxCheckBox *migemo_ = nullptr;
	wxCheckBox *bytes_ = nullptr;
	wxCheckBox *highlight_ = nullptr;
	wxCheckBox *close_ = nullptr;
	wxRadioBox *direction_ = nullptr;
	wxChoice *code_page_ = nullptr;
};

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, bool binary, find_txt::Options &options)
{
	FindTextDialog dlg(parent, binary, options);
	if (dlg.ShowModal() != wxID_OK) return false;
	options = dlg.Values();
	return true;
}

}  // namespace find_txt_dialog
