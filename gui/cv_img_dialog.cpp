/**
 * @file gui/cv_img_dialog.cpp
 * @brief gui/cv_img_dialog.h の実装
 */
#include "gui/cv_img_dialog.h"

#include <algorithm>

#include <wx/checkbox.h>
#include <wx/colordlg.h>
#include <wx/colour.h>
#include <wx/radiobox.h>
#include <wx/slider.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace cv_img_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

unsigned int colour_ref(const wxColour &colour)
{
	return static_cast<unsigned int>(RGB(colour.Red(), colour.Green(), colour.Blue()));
}

wxColour from_colour_ref(unsigned int value)
{
	return wxColour(static_cast<unsigned char>(value & 0xff),
	                static_cast<unsigned char>((value >> 8) & 0xff),
	                static_cast<unsigned char>((value >> 16) & 0xff));
}

int selected_or_zero(wxChoice *choice)
{
	const int selected = choice->GetSelection();
	return selected < 0 ? 0 : selected;
}

}  // namespace

//---------------------------------------------------------------------------
/**
 * @brief 画像変換設定ダイアログ
 * @details VCL の CvFmtRadioGroupClick/ScaleModeComboBoxChange/ClipNameComboBoxChange
 *          (src/CvImgDlg.cpp:136-212) の表示判定は gui/cv_img.h へ委譲。
 *          wx 側では同じ順序で各 format/scale の入力欄を隠す。
 */
class CvImageDialog : public wxDialog {
public:
	CvImageDialog(wxWindow *parent, const cv_img::Options &input, bool from_clipboard,
	              const UnicodeString &title_info)
		: wxDialog(parent, wxID_ANY,
		           to_wx(from_clipboard ? _T("クリップボード画像の変換/保存")
		                                : _T("画像ファイルの変換") + title_info),
		           wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, from_clipboard_(from_clipboard)
	{
		const cv_img::Options initial = cv_img::Normalize(input);
		margin_color_ = initial.margin_color;
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		wxArrayString formats;
		formats.Add(to_wx(_T("BMP")));
		formats.Add(to_wx(_T("JPG")));
		formats.Add(to_wx(_T("PNG")));
		formats.Add(to_wx(_T("GIF")));
		formats.Add(to_wx(_T("TIF")));
		formats.Add(to_wx(_T("HDP")));
		format_ = new wxRadioBox(this, wxID_ANY, to_wx(_T("変換形式")), wxDefaultPosition,
		                         wxDefaultSize, formats, 2, wxRA_SPECIFY_COLS);
		format_->SetSelection(cv_img::FormatIndex(initial.format));
		top->Add(format_, wxSizerFlags().Expand().Border(wxALL, 8));

		wxBoxSizer *sub = new wxBoxSizer(wxHORIZONTAL);
		quality_ = new wxSlider(this, wxID_ANY, initial.quality, 0, 100, wxDefaultPosition,
		                        wxSize(180, -1), wxSL_HORIZONTAL | wxSL_LABELS);
		quality_label_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		sub->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("JPEG品質"))), wxSizerFlags().CentreVertical());
		sub->Add(quality_, wxSizerFlags(1).CentreVertical().Border(wxLEFT, 6));
		sub->Add(quality_label_, wxSizerFlags().CentreVertical().Border(wxLEFT, 4));
		top->Add(sub, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));

		wxFlexGridSizer *format_grid = new wxFlexGridSizer(2, 4, 8);
		format_grid->AddGrowableCol(1);
		wxArrayString ycc_items;
		ycc_items.Add(to_wx(_T("既定")));
		ycc_items.Add(to_wx(_T("4:2:0")));
		ycc_items.Add(to_wx(_T("4:2:2")));
		ycc_items.Add(to_wx(_T("4:4:4")));
		ycr_cb_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, ycc_items);
		ycr_cb_->SetSelection(initial.ycrcb);
		format_grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("YCrCb"))), wxSizerFlags().CentreVertical());
		format_grid->Add(ycr_cb_, wxSizerFlags().Expand());

		wxArrayString cmp_items;
		cmp_items.Add(to_wx(_T("自動選択")));
		cmp_items.Add(to_wx(_T("圧縮なし")));
		cmp_items.Add(to_wx(_T("CCITT3")));
		cmp_items.Add(to_wx(_T("CCITT4")));
		cmp_items.Add(to_wx(_T("LZW")));
		cmp_items.Add(to_wx(_T("RLE")));
		cmp_items.Add(to_wx(_T("ZIP")));
		cmp_items.Add(to_wx(_T("LZWH差分")));
		compression_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, cmp_items);
		compression_->SetSelection(initial.compression);
		format_grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("TIFF圧縮"))), wxSizerFlags().CentreVertical());
		format_grid->Add(compression_, wxSizerFlags().Expand());
		top->Add(format_grid, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, 8));

		grayscale_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("グレースケール")));
		grayscale_->SetValue(initial.grayscale);
		top->Add(grayscale_, wxSizerFlags().Border(wxALL, 8));

		wxBoxSizer *scale_box = new wxBoxSizer(wxVERTICAL);
		wxArrayString scale_items;
		scale_items.Add(to_wx(_T("縮小・拡大を行わない")));
		scale_items.Add(to_wx(_T("倍率をパーセントで指定")));
		scale_items.Add(to_wx(_T("縦横の長い方のサイズを指定")));
		scale_items.Add(to_wx(_T("横サイズを指定")));
		scale_items.Add(to_wx(_T("縦サイズを指定")));
		scale_items.Add(to_wx(_T("指定サイズ内に収める")));
		scale_items.Add(to_wx(_T("指定サイズにストレッチ")));
		scale_items.Add(to_wx(_T("指定サイズに余白付きで収める")));
		scale_items.Add(to_wx(_T("指定サイズに合わせて切り出し")));
		scale_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, scale_items);
		scale_->SetSelection(static_cast<int>(initial.scale_mode));
		scale_box->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("縮小・拡大方法"))),
		               wxSizerFlags().Border(wxBOTTOM, 4));
		scale_box->Add(scale_, wxSizerFlags().Expand().Border(wxBOTTOM, 6));

		wxFlexGridSizer *scale_grid = new wxFlexGridSizer(2, 4, 8);
		scale_grid->AddGrowableCol(1);
		scale1_label_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		scale1_ = new wxTextCtrl(this, wxID_ANY, to_wx(initial.scale_param1), wxDefaultPosition, wxSize(90, -1));
		scale2_label_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		scale2_ = new wxTextCtrl(this, wxID_ANY, to_wx(initial.scale_param2), wxDefaultPosition, wxSize(90, -1));
		scale_grid->Add(scale1_label_, wxSizerFlags().CentreVertical());
		scale_grid->Add(scale1_, wxSizerFlags().CentreVertical());
		scale_grid->Add(scale2_label_, wxSizerFlags().CentreVertical());
		scale_grid->Add(scale2_, wxSizerFlags().CentreVertical());
		scale_box->Add(scale_grid, wxSizerFlags().Expand().Border(wxBOTTOM, 6));
		scale_opt_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxArrayString());
		scale_opt_->Append(to_wx(_T("既定")));
		scale_opt_->Append(to_wx(_T("ニアレストネイバー")));
		scale_opt_->Append(to_wx(_T("バイリニア")));
		scale_opt_->Append(to_wx(_T("バイキュービック")));
		scale_opt_->SetSelection(initial.interpolation);
		scale_box->Add(scale_opt_, wxSizerFlags().Expand());
		top->Add(scale_box, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxBoxSizer *margin = new wxBoxSizer(wxHORIZONTAL);
		margin->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("余白色"))), wxSizerFlags().CentreVertical());
		margin_panel_ = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(70, 24));
		margin_panel_->SetBackgroundColour(from_colour_ref(initial.margin_color));
		margin_button_ = new wxButton(this, wxID_ANY, _T("..."));
		margin_button_->Bind(wxEVT_BUTTON, &CvImageDialog::OnMargin, this);
		margin->Add(margin_panel_, wxSizerFlags().Border(wxLEFT, 6));
		margin->Add(margin_button_, wxSizerFlags().Border(wxLEFT, 4));
		top->Add(margin, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxBoxSizer *name_box = new wxBoxSizer(wxVERTICAL);
		wxArrayString name_items;
		name_items.Add(to_wx(_T("ファイル名の先頭に挿入")));
		name_items.Add(to_wx(_T("ファイル名主部の末尾に追加")));
		name_mode_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, name_items);
		name_mode_->SetSelection(static_cast<int>(initial.name_mode));
		name_edit_ = new wxTextCtrl(this, wxID_ANY, to_wx(initial.name_text));
		wxBoxSizer *name_row = new wxBoxSizer(wxHORIZONTAL);
		name_row->Add(name_mode_, wxSizerFlags(1).Expand());
		name_row->Add(name_edit_, wxSizerFlags().Border(wxLEFT, 6));
		name_box->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("ファイル名の変更"))),
		              wxSizerFlags().Border(wxBOTTOM, 4));
		name_box->Add(name_row, wxSizerFlags().Expand());
		top->Add(name_box, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		keep_time_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("ファイルの時刻を保持")));
		keep_time_->SetValue(initial.keep_time);
		no_preview_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("プレビューを使用しない")));
		no_preview_->SetValue(initial.not_use_preview);
		top->Add(keep_time_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(no_preview_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		if (from_clipboard_) {
			wxBoxSizer *clip_box = new wxBoxSizer(wxVERTICAL);
			clip_box->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("クリップボードのファイル名"))),
			              wxSizerFlags().Border(wxBOTTOM, 4));
			clip_name_ = new wxTextCtrl(this, wxID_ANY, to_wx(initial.clipboard_name));
			clip_box->Add(clip_name_, wxSizerFlags().Expand());
			clip_overwrite_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("同名を手動で変更")));
			clip_overwrite_->SetValue(initial.clipboard_overwrite);
			clip_box->Add(clip_overwrite_, wxSizerFlags().Border(wxTOP, 6));
			top->Add(clip_box, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		}
		else {
			clip_name_ = nullptr;
			clip_overwrite_ = nullptr;
		}

		wxStaticText *note = new wxStaticText(
			this, wxID_ANY,
			to_wx(_T("※ wx版で実処理に渡るのは形式とJPEG品質です。\n"
			         _T("   拡大縮小、グレースケール、TIFF圧縮、名前変更、クリップボード保存は未実装扱い。"))));
		top->Add(note, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		Bind(wxEVT_RADIOBOX, &CvImageDialog::OnValueChanged, this);
		quality_->Bind(wxEVT_SLIDER, &CvImageDialog::OnValueChanged, this);
		for (wxWindow *w : {static_cast<wxWindow *>(grayscale_),
		                     static_cast<wxWindow *>(keep_time_),
		                     static_cast<wxWindow *>(no_preview_)}) {
			w->Bind(wxEVT_CHECKBOX, &CvImageDialog::OnValueChanged, this);
		}
		ycr_cb_->Bind(wxEVT_CHOICE, &CvImageDialog::OnValueChanged, this);
		compression_->Bind(wxEVT_CHOICE, &CvImageDialog::OnValueChanged, this);
		scale_->Bind(wxEVT_CHOICE, &CvImageDialog::OnValueChanged, this);
		scale_opt_->Bind(wxEVT_CHOICE, &CvImageDialog::OnValueChanged, this);
		name_mode_->Bind(wxEVT_CHOICE, &CvImageDialog::OnValueChanged, this);
		scale1_->Bind(wxEVT_TEXT, &CvImageDialog::OnValueChanged, this);
		scale2_->Bind(wxEVT_TEXT, &CvImageDialog::OnValueChanged, this);
		if (clip_name_ != nullptr) clip_name_->Bind(wxEVT_TEXT, &CvImageDialog::OnValueChanged, this);
		UpdateEnabled();
	}

	cv_img::Options Values() const
	{
		cv_img::Options o;
		o.format = cv_img::FormatFromIndex(format_->GetSelection());
		o.quality = quality_->GetValue();
		o.ycrcb = selected_or_zero(ycr_cb_);
		o.compression = selected_or_zero(compression_);
		o.grayscale = grayscale_->GetValue();
		o.scale_mode = static_cast<cv_img::ScaleMode>(std::max(0, scale_->GetSelection()));
		o.scale_param1 = to_us(scale1_->GetValue()).ToIntDef(100);
		o.scale_param2 = to_us(scale2_->GetValue()).ToIntDef(100);
		o.interpolation = selected_or_zero(scale_opt_);
		o.margin_color = margin_color_;
		o.name_mode = static_cast<cv_img::NameMode>(std::max(0, name_mode_->GetSelection()));
		o.name_text = to_us(name_edit_->GetValue());
		o.keep_time = keep_time_->GetValue();
		o.not_use_preview = no_preview_->GetValue();
		o.from_clipboard = from_clipboard_;
		if (clip_name_ != nullptr) {
			o.clipboard_name = to_us(clip_name_->GetValue());
			o.clipboard_overwrite = clip_overwrite_->GetValue();
		}
		return cv_img::Normalize(o);
	}

private:
	void OnValueChanged(wxCommandEvent &event)
	{
		UpdateEnabled();
		event.Skip();
	}

	void UpdateEnabled()
	{
		const cv_img::Options o = Values();
		const cv_img::FormatVisibility v = cv_img::ResolveVisibility(o);
		quality_->Enable(v.quality);
		quality_label_->Enable(v.quality);
		ycr_cb_->Enable(v.ycrcb && !o.grayscale);
		compression_->Enable(v.compression);
		const cv_img::ScaleState s = cv_img::ResolveScaleState(o.scale_mode);
		scale_opt_->Enable(s.option);
		scale1_->Enable(s.param1);
		scale2_->Enable(s.param2);
		scale1_label_->SetLabel(to_wx(s.label1));
		scale2_label_->SetLabel(to_wx(s.label2));
		const bool margin_visible = o.scale_mode == cv_img::ScaleMode::Fit
		                            || o.scale_mode == cv_img::ScaleMode::Stretch
		                            || o.scale_mode == cv_img::ScaleMode::FitPad
		                            || o.scale_mode == cv_img::ScaleMode::Crop;
		margin_panel_->Show(margin_visible);
		margin_button_->Enable(margin_visible);
		quality_label_->SetLabel(to_wx(UnicodeString().sprintf(_T("品質 %3d"), quality_->GetValue())));
	}

	void OnMargin(wxCommandEvent &)
	{
		wxColourData data;
		data.SetColour(from_colour_ref(margin_color_));
		wxColourDialog dlg(this, &data);
		if (dlg.ShowModal() != wxID_OK) return;
		margin_color_ = colour_ref(dlg.GetColourData().GetColour());
		margin_panel_->SetBackgroundColour(from_colour_ref(margin_color_));
		Refresh();
	}

	bool from_clipboard_ = false;
	unsigned int margin_color_ = 0;
	wxRadioBox *format_ = nullptr;
	wxSlider *quality_ = nullptr;
	wxStaticText *quality_label_ = nullptr;
	wxChoice *ycr_cb_ = nullptr;
	wxChoice *compression_ = nullptr;
	wxCheckBox *grayscale_ = nullptr;
	wxChoice *scale_ = nullptr;
	wxTextCtrl *scale1_ = nullptr;
	wxTextCtrl *scale2_ = nullptr;
	wxStaticText *scale1_label_ = nullptr;
	wxStaticText *scale2_label_ = nullptr;
	wxChoice *scale_opt_ = nullptr;
	wxPanel *margin_panel_ = nullptr;
	wxButton *margin_button_ = nullptr;
	wxChoice *name_mode_ = nullptr;
	wxTextCtrl *name_edit_ = nullptr;
	wxCheckBox *keep_time_ = nullptr;
	wxCheckBox *no_preview_ = nullptr;
	wxTextCtrl *clip_name_ = nullptr;
	wxCheckBox *clip_overwrite_ = nullptr;
};

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, cv_img::Options &options, bool from_clipboard,
         const UnicodeString &title_info)
{
	CvImageDialog dlg(parent, options, from_clipboard, title_info);
	if (dlg.ShowModal() != wxID_OK) return false;
	options = dlg.Values();
	return true;
}

}  // namespace cv_img_dialog
