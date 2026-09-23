/**
 * @file gui/join_text_dialog.cpp
 * @brief gui/join_text_dialog.h の実装
 */
#include "gui/join_text_dialog.h"

#include <algorithm>

#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/filedlg.h>
#include <wx/listbox.h>
#include <wx/statline.h>
#include <wx/textctrl.h>

#include "gui/new_file.h"

namespace join_text_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<std::size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

class JoinTextDialog final : public wxDialog {
public:
	JoinTextDialog(wxWindow *parent, const Options &options)
		: wxDialog(parent, wxID_ANY, to_wx(_T("テキストファイルの結合")),
		           wxDefaultPosition, wxSize(620, 520), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		wxArrayString sources;
		for (const UnicodeString &path : options.sources) sources.Add(to_wx(path));
		source_ctrl_ = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(560, 220), sources,
		                            wxLB_SINGLE);
		top->Add(source_ctrl_, wxSizerFlags(1).Expand().Border(wxALL, 8));

		wxBoxSizer *order = new wxBoxSizer(wxHORIZONTAL);
		up_ctrl_ = new wxButton(this, ID_UP, to_wx(_T("上へ")));
		down_ctrl_ = new wxButton(this, ID_DOWN, to_wx(_T("下へ")));
		delete_ctrl_ = new wxButton(this, ID_DELETE, to_wx(_T("削除")));
		order->Add(up_ctrl_, wxSizerFlags().Border(wxRIGHT, 6));
		order->Add(down_ctrl_, wxSizerFlags().Border(wxRIGHT, 6));
		order->Add(delete_ctrl_, wxSizerFlags());
		top->Add(order, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxFlexGridSizer *grid = new wxFlexGridSizer(2, 3, 8);
		grid->AddGrowableCol(1, 1);
		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("出力ファイル名"))), wxSizerFlags().CentreVertical());
		output_ctrl_ = new wxTextCtrl(this, wxID_ANY, to_wx(options.output_name));
		grid->Add(output_ctrl_, wxSizerFlags(1).Expand());

		wxArrayString encodings;
		for (int i = 0; i < join_text::EncodingCount(); ++i)
			encodings.Add(to_wx(join_text::EncodingName(i)));
		code_ctrl_ = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                            wxDefaultSize, encodings, wxCB_DROPDOWN | wxCB_READONLY);
		int code_index = 0;
		for (int i = 0; i < join_text::EncodingCount(); ++i) {
			if (SameText(options.output_code, join_text::EncodingName(i))) { code_index = i; break; }
		}
		code_ctrl_->SetSelection(code_index);
		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("出力コード"))), wxSizerFlags().CentreVertical());
		grid->Add(code_ctrl_, wxSizerFlags(1).Expand());

		wxArrayString breaks;
		breaks.Add(to_wx(_T("CR/LF")));
		breaks.Add(to_wx(_T("LF")));
		breaks.Add(to_wx(_T("CR")));
		line_ctrl_ = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                            wxDefaultSize, breaks, wxCB_DROPDOWN | wxCB_READONLY);
		line_ctrl_->SetSelection(options.line_break == _T("\n")? 1 : options.line_break == _T("\r")? 2 : 0);
		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("改行"))), wxSizerFlags().CentreVertical());
		grid->Add(line_ctrl_, wxSizerFlags(1).Expand());

		bom_ctrl_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("BOM を付ける")));
		bom_ctrl_->SetValue(options.with_bom);
		grid->Add(new wxStaticText(this, wxID_ANY, wxEmptyString));
		grid->Add(bom_ctrl_);

		wxBoxSizer *template_box = new wxBoxSizer(wxHORIZONTAL);
		template_ctrl_ = new wxTextCtrl(this, wxID_ANY, to_wx(options.template_path));
		wxButton *browse = new wxButton(this, ID_BROWSE_TEMPLATE, to_wx(_T("参照...")));
		wxButton *edit = new wxButton(this, ID_EDIT_TEMPLATE, to_wx(_T("編集")));
		edit->Enable(false);
		edit->SetToolTip(to_wx(_T("未移植 (未実装扱い): 外部テキストエディタでの編集")));
		template_box->Add(template_ctrl_, wxSizerFlags(1).Expand().Border(wxRIGHT, 6));
		template_box->Add(browse, wxSizerFlags().Border(wxRIGHT, 6));
		template_box->Add(edit);
		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("テンプレート"))), wxSizerFlags().CentreVertical());
		grid->Add(template_box, wxSizerFlags(1).Expand());
		top->Add(grid, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		wxBoxSizer *buttons = new wxBoxSizer(wxHORIZONTAL);
		buttons->AddStretchSpacer();
		buttons->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("キャンセル"))), wxSizerFlags().Border(wxLEFT, 6));
		ok_ctrl_ = new wxButton(this, wxID_OK, to_wx(_T("開始")));
		buttons->Add(ok_ctrl_);
		top->Add(buttons, wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		Bind(wxEVT_BUTTON, &JoinTextDialog::OnUp, this, ID_UP);
		Bind(wxEVT_BUTTON, &JoinTextDialog::OnDown, this, ID_DOWN);
		Bind(wxEVT_BUTTON, &JoinTextDialog::OnDelete, this, ID_DELETE);
		Bind(wxEVT_BUTTON, &JoinTextDialog::OnBrowseTemplate, this, ID_BROWSE_TEMPLATE);
		Bind(wxEVT_BUTTON, &JoinTextDialog::OnOk, this, wxID_OK);
		Bind(wxEVT_COMBOBOX, &JoinTextDialog::OnCodeChanged, this, code_ctrl_->GetId());
		Bind(wxEVT_TEXT, &JoinTextDialog::OnChanged, this, output_ctrl_->GetId());
		Bind(wxEVT_CHECKBOX, &JoinTextDialog::OnChanged, this, bom_ctrl_->GetId());
		Bind(wxEVT_LISTBOX, &JoinTextDialog::OnChanged, this, source_ctrl_->GetId());
		Bind(wxEVT_KEY_DOWN, &JoinTextDialog::OnSourceKeyDown, this, source_ctrl_->GetId());

		UpdateEnabled();
		output_ctrl_->SetFocus();
		output_ctrl_->SelectAll();
	}

	const Options &Result() const { return options_; }

private:
	enum {
		ID_UP = wxID_HIGHEST + 1,
		ID_DOWN,
		ID_DELETE,
		ID_BROWSE_TEMPLATE,
		ID_EDIT_TEMPLATE
	};

	Options OptionsFromControls() const
	{
		Options options;
		for (unsigned int i = 0; i < source_ctrl_->GetCount(); ++i)
			options.sources.push_back(to_us(source_ctrl_->GetString(i)));
		options.output_name = to_us(output_ctrl_->GetValue());
		options.output_code = join_text::EncodingName(code_ctrl_->GetSelection());
		options.line_break = join_text::LineBreakFor(line_ctrl_->GetSelection());
		options.template_path = to_us(template_ctrl_->GetValue());
		options.with_bom = bom_ctrl_->GetValue();
		return options;
	}

	void UpdateEnabled()
	{
		const Options options = OptionsFromControls();
		ok_ctrl_->Enable(join_text::CanSubmit(options.sources.size(), options.output_name));
		bom_ctrl_->Enable(join_text::BomAvailable(options.output_code));
		const int selection = source_ctrl_->GetSelection();
		up_ctrl_->Enable(join_text::CanMoveSource(options.sources.size(), selection, -1));
		down_ctrl_->Enable(join_text::CanMoveSource(options.sources.size(), selection, 1));
		delete_ctrl_->Enable(join_text::CanDeleteSource(options.sources.size(), selection));
	}

	void ReplaceSources(const std::vector<UnicodeString> &sources, int selection)
	{
		source_ctrl_->Freeze();
		source_ctrl_->Clear();
		for (const UnicodeString &source : sources) source_ctrl_->Append(to_wx(source));
		if (!sources.empty()) source_ctrl_->SetSelection(selection);
		source_ctrl_->Thaw();
		UpdateEnabled();
	}

	void OnUp(wxCommandEvent &) { Move(-1); }
	void OnDown(wxCommandEvent &) { Move(1); }

	void Move(int delta)
	{
		const int index = source_ctrl_->GetSelection();
		std::vector<UnicodeString> sources;
		for (unsigned int i = 0; i < source_ctrl_->GetCount(); ++i)
			sources.push_back(to_us(source_ctrl_->GetString(i)));
		const int moved = join_text::MoveSource(sources, index, delta);
		if (moved >= 0) ReplaceSources(sources, moved);
	}

	void DeleteSelected()
	{
		const int index = source_ctrl_->GetSelection();
		std::vector<UnicodeString> sources;
		for (unsigned int i = 0; i < source_ctrl_->GetCount(); ++i)
			sources.push_back(to_us(source_ctrl_->GetString(i)));
		join_text::RemoveSource(sources, index);
		const int next = sources.empty()? -1 : std::min(index, static_cast<int>(sources.size()) - 1);
		ReplaceSources(sources, next);
	}

	void OnDelete(wxCommandEvent &) { DeleteSelected(); }

	void OnSourceKeyDown(wxKeyEvent &event)
	{
		if (event.CmdDown() && event.ShiftDown() && event.GetKeyCode() == WXK_UP) {
			Move(-1);
			return;
		}
		if (event.CmdDown() && event.ShiftDown() && event.GetKeyCode() == WXK_DOWN) {
			Move(1);
			return;
		}
		if (event.GetKeyCode() == WXK_DELETE) {
			DeleteSelected();
			return;
		}
		event.Skip();
	}

	void OnBrowseTemplate(wxCommandEvent &)
	{
		UnicodeString dir = new_file::TemplateDirectory(to_us(template_ctrl_->GetValue()));
		wxFileDialog dlg(this, to_wx(_T("出力テンプレートの選択")), to_wx(dir),
		                 wxEmptyString, to_wx(_T("All files (*.*)|*.*|Text files (*.txt)|*.txt")),
		                 wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() == wxID_OK) template_ctrl_->ChangeValue(dlg.GetPath());
	}

	void OnCodeChanged(wxCommandEvent &event) { UpdateEnabled(); event.Skip(); }
	void OnChanged(wxCommandEvent &event) { UpdateEnabled(); event.Skip(); }

	void OnOk(wxCommandEvent &)
	{
		const Options options = OptionsFromControls();
		if (!options.template_path.IsEmpty()) {
			wxMessageBox(to_wx(_T("未移植 (未実装扱い): テンプレートによる結合\n")
			                      _T("テンプレートを空にして開始してください。")),
			             to_wx(_T("テンプレートによる結合")), wxOK | wxICON_WARNING, this);
			return;
		}
		if (!join_text::CanSubmit(options.sources.size(), options.output_name)) return;
		options_ = options;
		EndModal(wxID_OK);
	}

	wxListBox *source_ctrl_ = nullptr;
	wxButton *up_ctrl_ = nullptr;
	wxButton *down_ctrl_ = nullptr;
	wxButton *delete_ctrl_ = nullptr;
	wxTextCtrl *output_ctrl_ = nullptr;
	wxComboBox *code_ctrl_ = nullptr;
	wxComboBox *line_ctrl_ = nullptr;
	wxCheckBox *bom_ctrl_ = nullptr;
	wxTextCtrl *template_ctrl_ = nullptr;
	wxButton *ok_ctrl_ = nullptr;
	Options options_;
};

}  // namespace

bool Run(wxWindow *parent, Options &options)
{
	JoinTextDialog dialog(parent, options);
	if (dialog.ShowModal() != wxID_OK) return false;
	options = dialog.Result();
	return true;
}

}  // namespace join_text_dialog
