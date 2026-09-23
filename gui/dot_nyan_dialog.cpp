/**
 * @file gui/dot_nyan_dialog.cpp
 * @brief gui/dot_nyan_dialog.h の実装
 */
#include "gui/dot_nyan_dialog.h"

#include <wx/checkbox.h>
#include <wx/radiobox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "usr_file_ex.h"

namespace dot_nyan_dialog {
namespace {

inline wxString to_wx(const UnicodeString &value)
{
	return wxString(value.c_str(), static_cast<size_t>(value.Length()));
}

inline UnicodeString to_us(const wxString &value)
{
	return UnicodeString(value.wc_str());
}

wxArrayString Choices(const wchar_t *const *items, int count)
{
	wxArrayString result;
	for (int i = 0; i < count; ++i) result.Add(to_wx(UnicodeString(items[i])));
	return result;
}

class DotNyanInputDialog final : public wxDialog {
public:
	DotNyanInputDialog(wxWindow *parent, const UnicodeString &config_path,
	                   const dot_nyan::Options &options)
		: wxDialog(parent, wxID_ANY,
		           to_wx(_T(".nyanfi の設定 - ") + ExtractFileName(config_path)),
		           wxDefaultPosition, wxSize(760, 620),
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
		  initial_options_(options)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		const wchar_t *sort_items[] = {_T("指定なし"), _T("名前"), _T("拡張子"), _T("更新日"),
		                               _T("サイズ"), _T("属性"), _T("その他")};
		sort_ = new wxRadioBox(this, wxID_ANY, to_wx(_T("ソート方法")), wxDefaultPosition,
		                        wxDefaultSize, Choices(sort_items, 7), 2, wxRA_SPECIFY_COLS);
		sort_->SetSelection(Clamp(options.sort_mode, 0, 6));
		top->Add(sort_, wxSizerFlags().Expand().Border(wxALL, 8));

		wxBoxSizer *order_row = new wxBoxSizer(wxHORIZONTAL);
		no_order_ = new wxCheckBox(this, wxID_ANY, _T("順序指定なし"));
		no_order_->SetValue(options.no_order);
		natural_ = new wxCheckBox(this, wxID_ANY, _T("自然順"));
		dsc_name_ = new wxCheckBox(this, wxID_ANY, _T("降順"));
		small_ = new wxCheckBox(this, wxID_ANY, _T("小さい順"));
		old_ = new wxCheckBox(this, wxID_ANY, _T("古い順"));
		dsc_attr_ = new wxCheckBox(this, wxID_ANY, _T("属性降順"));
		natural_->SetValue(options.natural_order);
		dsc_name_->SetValue(options.dsc_name_order);
		small_->SetValue(options.small_order);
		old_->SetValue(options.old_order);
		dsc_attr_->SetValue(options.dsc_attr_order);
		order_row->Add(no_order_, wxSizerFlags().Border(wxRIGHT, 12));
		order_row->Add(natural_, wxSizerFlags().Border(wxRIGHT, 8));
		order_row->Add(dsc_name_, wxSizerFlags().Border(wxRIGHT, 8));
		order_row->Add(small_, wxSizerFlags().Border(wxRIGHT, 8));
		order_row->Add(old_, wxSizerFlags().Border(wxRIGHT, 8));
		order_row->Add(dsc_attr_);
		top->Add(order_row, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		const wchar_t *attr_items[] = {_T("指定なし"), _T("表示"), _T("非表示")};
		show_hidden_ = Radio(this, _T("隠しファイル"), attr_items, 3, options.show_hidden);
		show_system_ = Radio(this, _T("システムファイル"), attr_items, 3, options.show_system);
		show_size_ = Radio(this, _T("サイズ表示"), attr_items, 3, options.show_byte_size);
		sync_ = Radio(this, _T("左右同期"), attr_items, 3, options.sync_lr);
		const wchar_t *icon_items[] = {_T("指定なし"), _T("アイコン"), _T("ディレクトリのみ"),
		                               _T("その他")};
		show_icon_ = Radio(this, _T("アイコン"), icon_items, 4, options.show_icon);

		wxFlexGridSizer *attrs = new wxFlexGridSizer(2, 10, 6);
		attrs->AddGrowableCol(1);
		AddRadioRow(attrs, _T("属性表示"), show_hidden_);
		AddRadioRow(attrs, _T("同期設定"), sync_);
		AddRadioRow(attrs, _T("サイズ表示"), show_size_);
		AddRadioRow(attrs, _T("アイコン表示"), show_icon_);
		attrs->Add(new wxStaticText(this, wxID_ANY, _T("システム表示")), wxSizerFlags().CentreVertical());
		attrs->Add(show_system_);
		top->Add(attrs, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxFlexGridSizer *fields = new wxFlexGridSizer(2, 4, 6);
		fields->AddGrowableCol(1);
		AddField(this, fields, _T("パスマスク"), options.path_mask, path_mask_);
		AddField(this, fields, _T("GREPマスク"), options.grep_mask, grep_mask_);
		AddField(this, fields, _T("リスト幅"), options.list_width, list_width_);
		AddField(this, fields, _T("音声ファイル"), options.play_sound, sound_);
		AddField(this, fields, _T("背景画像"), options.bg_image, bg_image_);
		AddField(this, fields, _T("説明"), options.description, description_);
		AddField(this, fields, _T("コマンドファイル"), options.exe_commands, commands_);
		top->Add(fields, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		handled_ = new wxCheckBox(this, wxID_ANY, _T("ステップ実行時にコマンドを停止"));
		handled_->SetValue(options.handled);
		hidden_ = new wxCheckBox(this, wxID_ANY, _T("隠し属性"));
		hidden_->SetValue(options.hidden);
		wxBoxSizer *checks = new wxBoxSizer(wxHORIZONTAL);
		checks->Add(handled_, wxSizerFlags().Border(wxRIGHT, 12));
		checks->Add(hidden_);
		top->Add(checks, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		top->Add(new wxStaticText(this, wxID_ANY,
		                          _T("※ 配色・spuit・音声/画像/コマンドファイルの参照と継承・削除は未移植 (未実装扱い)")),
		         wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();
		SetEscapeId(wxID_CANCEL);
		no_order_->Bind(wxEVT_CHECKBOX, &DotNyanInputDialog::OnNoOrder, this);
		const bool order_enabled = !no_order_->GetValue();
		natural_->Enable(order_enabled);
		dsc_name_->Enable(order_enabled);
		small_->Enable(order_enabled);
		old_->Enable(order_enabled);
		dsc_attr_->Enable(order_enabled);
	}

	dot_nyan::Options ResultOptions() const
	{
		dot_nyan::Options result = initial_options_;
		result.sort_mode = sort_->GetSelection();
		result.no_order = no_order_->GetValue();
		result.natural_order = natural_->GetValue();
		result.dsc_name_order = dsc_name_->GetValue();
		result.small_order = small_->GetValue();
		result.old_order = old_->GetValue();
		result.dsc_attr_order = dsc_attr_->GetValue();
		result.show_hidden = show_hidden_->GetSelection();
		result.show_system = show_system_->GetSelection();
		result.show_byte_size = show_size_->GetSelection();
		result.show_icon = show_icon_->GetSelection();
		result.sync_lr = sync_->GetSelection();
		result.path_mask = to_us(path_mask_->GetValue()).Trim();
		result.grep_mask = to_us(grep_mask_->GetValue()).Trim();
		result.list_width = to_us(list_width_->GetValue()).Trim();
		result.play_sound = to_us(sound_->GetValue()).Trim();
		result.bg_image = to_us(bg_image_->GetValue()).Trim();
		result.description = to_us(description_->GetValue()).Trim();
		result.exe_commands = to_us(commands_->GetValue()).Trim();
		result.handled = handled_->GetValue();
		result.hidden = hidden_->GetValue();
		return result;
	}

private:
	static int Clamp(int value, int low, int high)
	{
		return value < low ? low : value > high ? high : value;
	}

	static wxRadioBox *Radio(wxWindow *parent, const wchar_t *label,
	                         const wchar_t *const *items, int count, int selection)
	{
		wxRadioBox *ctrl = new wxRadioBox(parent, wxID_ANY, to_wx(UnicodeString(label)),
		                                   wxDefaultPosition, wxDefaultSize, Choices(items, count), 1,
		                                   wxRA_SPECIFY_COLS);
		ctrl->SetSelection(Clamp(selection, 0, count - 1));
		return ctrl;
	}

	static void AddRadioRow(wxFlexGridSizer *grid, const wchar_t *label, wxRadioBox *radio)
	{
		grid->Add(new wxStaticText(radio->GetParent(), wxID_ANY, to_wx(UnicodeString(label))),
		          wxSizerFlags().CentreVertical());
		grid->Add(radio, wxSizerFlags(1).Expand());
	}

	template <typename T>
	static void AddField(wxWindow *parent, wxFlexGridSizer *grid, const wchar_t *label,
	                     const UnicodeString &value, T *&control)
	{
		grid->Add(new wxStaticText(parent, wxID_ANY, to_wx(UnicodeString(label))),
		          wxSizerFlags().CentreVertical());
		control = new T(parent, wxID_ANY, to_wx(value));
		grid->Add(control, wxSizerFlags(1).Expand());
	}

	void OnNoOrder(wxCommandEvent &event)
	{
		const bool enabled = !no_order_->GetValue();
		natural_->Enable(enabled);
		dsc_name_->Enable(enabled);
		small_->Enable(enabled);
		old_->Enable(enabled);
		dsc_attr_->Enable(enabled);
		event.Skip();
	}

	dot_nyan::Options initial_options_;
	wxRadioBox *sort_ = nullptr;
	wxCheckBox *no_order_ = nullptr;
	wxCheckBox *natural_ = nullptr;
	wxCheckBox *dsc_name_ = nullptr;
	wxCheckBox *small_ = nullptr;
	wxCheckBox *old_ = nullptr;
	wxCheckBox *dsc_attr_ = nullptr;
	wxRadioBox *show_hidden_ = nullptr;
	wxRadioBox *show_system_ = nullptr;
	wxRadioBox *show_size_ = nullptr;
	wxRadioBox *show_icon_ = nullptr;
	wxRadioBox *sync_ = nullptr;
	wxTextCtrl *path_mask_ = nullptr;
	wxTextCtrl *grep_mask_ = nullptr;
	wxTextCtrl *list_width_ = nullptr;
	wxTextCtrl *sound_ = nullptr;
	wxTextCtrl *bg_image_ = nullptr;
	wxTextCtrl *description_ = nullptr;
	wxTextCtrl *commands_ = nullptr;
	wxCheckBox *handled_ = nullptr;
	wxCheckBox *hidden_ = nullptr;
};

}  // namespace

bool Run(wxWindow *parent, const UnicodeString &config_path, dot_nyan::Options &options)
{
	DotNyanInputDialog dialog(parent, config_path, options);
	if (dialog.ShowModal() != wxID_OK) return false;
	options = dialog.ResultOptions();
	return true;
}

}  // namespace dot_nyan_dialog
