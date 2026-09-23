/**
 * @file gui/print_image_dialog.cpp
 * @brief gui/print_image_dialog.h の実装
 */
#include "gui/print_image_dialog.h"

#include <wx/checkbox.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/radiobox.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace print_image_dialog {

namespace {

inline wxString to_wx(const UnicodeString &value)
{
	return wxString(value.c_str(), static_cast<size_t>(value.Length()));
}

inline UnicodeString to_us(const wxString &value)
{
	return UnicodeString(value.wc_str());
}

const wchar_t *const kOrientationItems[] = {_T("縦"), _T("横")};
const wchar_t *const kFitItems[] = {_T("用紙に合わせる"), _T("用紙サイズで切り抜き"),
                                    _T("中央"), _T("左上")};
const wchar_t *const kRangeItems[] = {_T("1枚"), _T("全枚"), _T("選択範囲")};
const wchar_t *const kTextPositionItems[] = {_T("上"), _T("下")};
const wchar_t *const kTextAlignmentItems[] = {_T("左"), _T("中央"), _T("右")};

wxArrayString MakeChoices(const wchar_t *const *items, int count)
{
	wxArrayString choices;
	for (int i = 0; i < count; ++i) choices.Add(to_wx(items[i]));
	return choices;
}

/// PrnImgDlg.dfm の基本/文字タブを wx で再構成した設定ダイアログ。
class PrintImageDialog final : public wxDialog {
public:
	PrintImageDialog(wxWindow *parent, const Context &context, const print_image::PrintOptions &options)
		: wxDialog(parent, wxID_ANY, to_wx(_T("画像の印刷")),
		           wxDefaultPosition, wxSize(760, 520),
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
		  context_(context)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		wxBoxSizer *image_row = new wxBoxSizer(wxHORIZONTAL);
		image_row->Add(new wxStaticText(this, wxID_ANY,
		                                to_wx(_T("画像: ") + ExtractFileName(context.image_path))),
		               wxSizerFlags().CentreVertical());
		image_row->AddStretchSpacer();
		UnicodeString page_text;
		page_text.sprintf(_T("%d / %d 枚"), context.current_page, context.page_count);
		image_row->Add(new wxStaticText(this, wxID_ANY, to_wx(page_text)));
		top->Add(image_row, wxSizerFlags().Expand().Border(wxALL, 8));

		wxBoxSizer *columns = new wxBoxSizer(wxHORIZONTAL);
		wxBoxSizer *left = new wxBoxSizer(wxVERTICAL);

		wxFlexGridSizer *printer = new wxFlexGridSizer(2, 4, 8);
		printer->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("部数"))), wxSizerFlags().CentreVertical());
		copies_ctrl_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                              wxSize(100, -1), wxSP_ARROW_KEYS, 1, 32767, options.copies);
		printer->Add(copies_ctrl_, wxSizerFlags().CentreVertical());
		orientation_radio_ = new wxRadioBox(this, wxID_ANY, to_wx(_T("方向")),
		                                    wxDefaultPosition, wxDefaultSize,
		                                    MakeChoices(kOrientationItems, 2), 2, wxRA_SPECIFY_COLS);
		orientation_radio_->SetSelection(options.orientation == print_image::Orientation::Landscape ? 1 : 0);
		printer->Add(orientation_radio_, wxSizerFlags(1).Expand());
		left->Add(printer, wxSizerFlags().Expand().Border(wxBOTTOM, 8));

		wxButton *printer_setup = new wxButton(this, wxID_ANY, to_wx(_T("プリンタの設定...")));
		left->Add(printer_setup, wxSizerFlags().Expand().Border(wxBOTTOM, 8));

		wxNotebook *book = new wxNotebook(this, wxID_ANY);
		wxPanel *basic = new wxPanel(book);
		wxBoxSizer *basic_box = new wxBoxSizer(wxVERTICAL);
		range_radio_ = new wxRadioBox(basic, wxID_ANY, to_wx(_T("印刷範囲")),
		                               wxDefaultPosition, wxDefaultSize,
		                               MakeChoices(kRangeItems, 3), 1, wxRA_SPECIFY_COLS);
		range_radio_->SetSelection(static_cast<int>(options.print_range));
		basic_box->Add(range_radio_, wxSizerFlags().Expand().Border(wxALL, 6));
		fit_radio_ = new wxRadioBox(basic, wxID_ANY, to_wx(_T("サイズ・位置")),
		                             wxDefaultPosition, wxDefaultSize,
		                             MakeChoices(kFitItems, 4), 2, wxRA_SPECIFY_COLS);
		fit_radio_->SetSelection(static_cast<int>(options.fit));
		basic_box->Add(fit_radio_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 6));

		wxFlexGridSizer *geometry = new wxFlexGridSizer(3, 4, 6);
		geometry->Add(new wxStaticText(basic, wxID_ANY, to_wx(_T("倍率"))), wxSizerFlags().CentreVertical());
		scale_ctrl_ = new wxSpinCtrl(basic, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                             wxSize(90, -1), wxSP_ARROW_KEYS, 1, 100, options.scale_percent);
		geometry->Add(scale_ctrl_, wxSizerFlags().CentreVertical());
		geometry->Add(new wxStaticText(basic, wxID_ANY, to_wx("%")), wxSizerFlags().CentreVertical());
		geometry->Add(new wxStaticText(basic, wxID_ANY, to_wx(_T("オフセット"))), wxSizerFlags().CentreVertical());
		offset_x_ctrl_ = new wxSpinCtrl(basic, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                                wxSize(90, -1), wxSP_ARROW_KEYS, 0, 99, options.offset_x_percent);
		offset_y_ctrl_ = new wxSpinCtrl(basic, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                                wxSize(90, -1), wxSP_ARROW_KEYS, 0, 99, options.offset_y_percent);
		geometry->Add(offset_x_ctrl_);
		geometry->Add(offset_y_ctrl_);
		basic_box->Add(geometry, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 6));
		gray_chk_ = new wxCheckBox(basic, wxID_ANY, to_wx(_T("グレースケール")));
		gray_chk_->SetValue(options.grayscale);
		basic_box->Add(gray_chk_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 6));
		basic->SetSizer(basic_box);
		book->AddPage(basic, to_wx(_T("基本")));

		wxPanel *text_page = new wxPanel(book);
		wxBoxSizer *text_box = new wxBoxSizer(wxVERTICAL);
		text_chk_ = new wxCheckBox(text_page, wxID_ANY, to_wx(_T("文字を印刷")));
		text_chk_->SetValue(options.print_text);
		text_box->Add(text_chk_, wxSizerFlags().Border(wxALL, 6));
		wxFlexGridSizer *text_grid = new wxFlexGridSizer(3, 4, 6);
		text_grid->Add(new wxStaticText(text_page, wxID_ANY, to_wx(_T("書式"))), wxSizerFlags().CentreVertical());
		text_format_ctrl_ = new wxTextCtrl(text_page, wxID_ANY, to_wx(options.text_format));
		text_grid->Add(text_format_ctrl_, wxSizerFlags(1).Expand());
		font_btn_ = new wxButton(text_page, wxID_ANY, to_wx(_T("フォント...")));
		text_grid->Add(font_btn_, wxSizerFlags().CentreVertical());
		text_grid->Add(new wxStaticText(text_page, wxID_ANY, to_wx(_T("余白"))), wxSizerFlags().CentreVertical());
		text_margin_ctrl_ = new wxSpinCtrl(text_page, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                                   wxSize(90, -1), wxSP_ARROW_KEYS, 0, 99,
		                                   options.text_margin_percent);
		text_grid->Add(text_margin_ctrl_);
		text_grid->Add(new wxStaticText(text_page, wxID_ANY, to_wx("%")), wxSizerFlags().CentreVertical());
		text_box->Add(text_grid, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 6));
		wxBoxSizer *text_radios = new wxBoxSizer(wxHORIZONTAL);
		text_position_radio_ = new wxRadioBox(text_page, wxID_ANY, to_wx(_T("位置")),
		                                      wxDefaultPosition, wxDefaultSize,
		                                      MakeChoices(kTextPositionItems, 2), 2, wxRA_SPECIFY_COLS);
		text_position_radio_->SetSelection(static_cast<int>(options.text_position));
		text_alignment_radio_ = new wxRadioBox(text_page, wxID_ANY, to_wx(_T("揃え")),
		                                       wxDefaultPosition, wxDefaultSize,
		                                       MakeChoices(kTextAlignmentItems, 3), 3, wxRA_SPECIFY_COLS);
		text_alignment_radio_->SetSelection(static_cast<int>(options.text_alignment));
		text_radios->Add(text_position_radio_, 1, wxEXPAND);
		text_radios->Add(text_alignment_radio_, 1, wxEXPAND | wxLEFT, 8);
		text_box->Add(text_radios, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 6));
		text_page->SetSizer(text_box);
		book->AddPage(text_page, to_wx(_T("文字")));
		left->Add(book, wxSizerFlags(1).Expand());
		columns->Add(left, 3, wxEXPAND);

		wxPanel *preview = new wxPanel(this, wxID_ANY);
		preview->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_APPWORKSPACE));
		wxBoxSizer *preview_box = new wxBoxSizer(wxVERTICAL);
		preview_box->Add(new wxStaticText(preview, wxID_ANY,
		                                   to_wx(_T("画像プレビュー\n\n未移植 (未実装扱い):\n"
		                                           L"プリンタ描画・実印刷"))),
		                 wxSizerFlags().Centre().Expand());
		preview->SetSizer(preview_box);
		columns->Add(preview, 2, wxEXPAND | wxLEFT, 8);
		top->Add(columns, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT, 8));

		summary_ctrl_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		summary_ctrl_->Wrap(720);
		top->Add(summary_ctrl_, wxSizerFlags().Expand().Border(wxALL, 8));
		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));

		wxBoxSizer *buttons = new wxBoxSizer(wxHORIZONTAL);
		buttons->Add(new wxButton(this, wxID_PRINT, to_wx(_T("印刷"))));
		buttons->AddStretchSpacer();
		buttons->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("閉じる"))));
		top->Add(buttons, wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		SetMinSize(wxSize(700, 460));
		CentreOnParent();
		SetEscapeId(wxID_CANCEL);
		SetAffirmativeId(wxID_PRINT);

		printer_setup->Bind(wxEVT_BUTTON, &PrintImageDialog::OnUnimplemented, this);
		font_btn_->Bind(wxEVT_BUTTON, &PrintImageDialog::OnUnimplemented, this);
		orientation_radio_->Bind(wxEVT_RADIOBOX, &PrintImageDialog::OnSettingsChanged, this);
		fit_radio_->Bind(wxEVT_RADIOBOX, &PrintImageDialog::OnSettingsChanged, this);
		range_radio_->Bind(wxEVT_RADIOBOX, &PrintImageDialog::OnSettingsChanged, this);
		gray_chk_->Bind(wxEVT_CHECKBOX, &PrintImageDialog::OnSettingsChanged, this);
		text_chk_->Bind(wxEVT_CHECKBOX, &PrintImageDialog::OnSettingsChanged, this);
		for (wxSpinCtrl *ctrl : {copies_ctrl_, scale_ctrl_, offset_x_ctrl_, offset_y_ctrl_,
		                          text_margin_ctrl_}) {
			ctrl->Bind(wxEVT_SPINCTRL, &PrintImageDialog::OnSettingsChanged, this);
		}
		Bind(wxEVT_BUTTON, &PrintImageDialog::OnPrint, this, wxID_PRINT);
		UpdateEnabled();
	}

	print_image::PrintOptions Options() const { return options_; }

private:
	void OnSettingsChanged(wxCommandEvent &event)
	{
		event.Skip();
		UpdateEnabled();
	}

	void OnUnimplemented(wxCommandEvent &event)
	{
		event.Skip();
		wxMessageBox(to_wx(_T("未移植 (未実装扱い): プリンタ設定・フォント選択・実印刷")),
		             to_wx(_T("画像の印刷")), wxOK | wxICON_INFORMATION, this);
	}

	print_image::PrintOptions ReadOptions() const
	{
		print_image::PrintOptions options;
		options.orientation = orientation_radio_->GetSelection() == 1
			? print_image::Orientation::Landscape : print_image::Orientation::Portrait;
		options.copies = copies_ctrl_->GetValue();
		options.scale_percent = scale_ctrl_->GetValue();
		options.fit = static_cast<print_image::ImageFit>(fit_radio_->GetSelection());
		options.print_range = static_cast<print_image::PrintRange>(range_radio_->GetSelection());
		options.offset_x_percent = offset_x_ctrl_->GetValue();
		options.offset_y_percent = offset_y_ctrl_->GetValue();
		options.grayscale = gray_chk_->GetValue();
		options.print_text = text_chk_->GetValue();
		options.text_format = to_us(text_format_ctrl_->GetValue());
		options.text_position = static_cast<print_image::TextPosition>(text_position_radio_->GetSelection());
		options.text_alignment = static_cast<print_image::TextAlignment>(text_alignment_radio_->GetSelection());
		options.text_margin_percent = text_margin_ctrl_->GetValue();
		return options;
	}

	void UpdateEnabled()
	{
		const print_image::PrintOptions options = ReadOptions();
		const bool scale_enabled = options.fit == print_image::ImageFit::Center ||
		                           options.fit == print_image::ImageFit::TopLeft;
		scale_ctrl_->Enable(scale_enabled);
		offset_x_ctrl_->Enable(options.fit == print_image::ImageFit::TopLeft);
		offset_y_ctrl_->Enable(options.fit == print_image::ImageFit::TopLeft);
		range_radio_->Enable(true);
		text_format_ctrl_->Enable(options.print_text);
		text_margin_ctrl_->Enable(options.print_text);
		text_position_radio_->Enable(options.print_text);
		text_alignment_radio_->Enable(options.print_text);

		const print_image::ResolvedSettings resolved = print_image::ResolvePrintSettings(
			options, context_.page_count, context_.current_page, context_.selected_pages);
		summary_ctrl_->SetLabel(to_wx(resolved.valid
			? print_image::FormatPrintSettings(resolved)
			: _T("入力エラー: ") + resolved.error));
		Layout();
	}

	void OnPrint(wxCommandEvent &event)
	{
		event.Skip();
		const print_image::ResolvedSettings resolved = print_image::ResolvePrintSettings(
			ReadOptions(), context_.page_count, context_.current_page, context_.selected_pages);
		if (!resolved.valid) {
			wxMessageBox(to_wx(resolved.error), to_wx(_T("画像の印刷")),
			             wxOK | wxICON_WARNING, this);
			return;
		}
		options_ = ReadOptions();
		EndModal(wxID_OK);
	}

	Context context_;
	wxRadioBox *orientation_radio_ = nullptr;
	wxSpinCtrl *copies_ctrl_ = nullptr;
	wxRadioBox *range_radio_ = nullptr;
	wxRadioBox *fit_radio_ = nullptr;
	wxSpinCtrl *scale_ctrl_ = nullptr;
	wxSpinCtrl *offset_x_ctrl_ = nullptr;
	wxSpinCtrl *offset_y_ctrl_ = nullptr;
	wxCheckBox *gray_chk_ = nullptr;
	wxCheckBox *text_chk_ = nullptr;
	wxTextCtrl *text_format_ctrl_ = nullptr;
	wxButton *font_btn_ = nullptr;
	wxSpinCtrl *text_margin_ctrl_ = nullptr;
	wxRadioBox *text_position_radio_ = nullptr;
	wxRadioBox *text_alignment_radio_ = nullptr;
	wxStaticText *summary_ctrl_ = nullptr;
	print_image::PrintOptions options_;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, const Context &context, print_image::PrintOptions &options_out,
         bool &print_requested_out)
{
	print_image::PrintOptions initial;
	PrintImageDialog dialog(parent, context, initial);
	print_requested_out = false;
	if (dialog.ShowModal() != wxID_OK) return false;
	options_out = dialog.Options();
	print_requested_out = true;
	return true;
}

}  // namespace print_image_dialog
