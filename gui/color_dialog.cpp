/**
 * @file gui/color_dialog.cpp
 * @brief gui/color_dialog.h の実装
 */
#include "gui/color_dialog.h"

#include <wx/colordlg.h>

namespace color_dialog {

namespace {

/// wxString への変換 (gui/sync_dialog.cpp と同じ変換ヘルパー)
inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

/// wxString → UnicodeString (MSW では両方 UTF-16)
inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

/// TColor 整数値 (0x00BBGGRR) → wxColour。無効値は呼ばないこと
wxColour to_wx_colour(int color)
{
	return wxColour(static_cast<unsigned char>(color & 0xFF),
	                static_cast<unsigned char>((color >> 8) & 0xFF),
	                static_cast<unsigned char>((color >> 16) & 0xFF));
}

/// wxColour → TColor 整数値 (0x00BBGGRR)
int from_wx_colour(const wxColour &col)
{
	return (static_cast<int>(col.Blue()) << 16) | (static_cast<int>(col.Green()) << 8) |
	       static_cast<int>(col.Red());
}

/// 一覧の1行表示 (VCL のオーナー描画の代わりに「表示名 (キー) #RRGGBB/無効」)
wxString row_of(const color_settings::ColorEntry &e)
{
	const color_settings::ColorItem *item = color_settings::FindItem(e.key);
	UnicodeString row = (item != nullptr) ? item->caption : e.key;
	row += UnicodeString(_T(" (")) + e.key + _T(")");
	if (e.color == color_settings::DisabledColor()) {
		row += UnicodeString(_T(" 無効"));
	}
	else {
		const wxColour col = to_wx_colour(e.color);
		row.cat_sprintf(_T(" #%02X%02X%02X"), col.Red(), col.Green(), col.Blue());
	}
	return to_wx(row);
}

/**
 * @brief 配色の編集ダイアログ
 * @details `src/ColDlg.cpp` (`TColorDlg`) のうち、一覧の表示
 *          (`ColorListBox`)・色の参照 (`RefColBtnClick` のカラー選択)・
 *          配色の無効化 (`DisableColActionExecute`/`DisableColActionUpdate`)
 *          だけを wx で再構成したもの。確定 (`OkActionExecute` の
 *          `ObjViewer->SetColor`)・全体への反映 (`OptApplyBtnClick` の
 *          `SetOptColor`) はビューア側の機構が無いため行わず、編集結果の
 *          保持だけ行う (未実装扱い)
 */
class ColorInputDialog : public wxDialog {
public:
	ColorInputDialog(wxWindow *parent, std::vector<color_settings::ColorEntry> &entries)
		: wxDialog(parent, wxID_ANY, to_wx(_T("テキストビューアの配色")),
		           wxDefaultPosition, wxDefaultSize,
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, entries_(entries)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		list_ = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(420, 320));
		top->Add(list_, wxSizerFlags(1).Expand().Border(wxALL, 8));
		list_->Bind(wxEVT_LISTBOX, &ColorInputDialog::OnSelect, this);
		list_->Bind(wxEVT_LISTBOX_DCLICK, &ColorInputDialog::OnRefer, this);

		wxBoxSizer *btn_row = new wxBoxSizer(wxHORIZONTAL);
		refer_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("参照(&R)...")));
		btn_row->Add(refer_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		disable_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("無効化(&D)")));
		btn_row->Add(disable_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		top->Add(btn_row, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		refer_btn_->Bind(wxEVT_BUTTON, &ColorInputDialog::OnRefer, this);
		disable_btn_->Bind(wxEVT_BUTTON, &ColorInputDialog::OnDisable, this);

		wxBoxSizer *ok_row = new wxBoxSizer(wxHORIZONTAL);
		ok_row->Add(new wxButton(this, wxID_OK, to_wx(_T("OK"))),
		            wxSizerFlags().Border(wxRIGHT, 4));
		ok_row->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("キャンセル"))));
		top->Add(ok_row, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		RefreshList();
		UpdateButtons();
	}

private:
	void RefreshList()
	{
		const int sel = list_->GetSelection();
		list_->Clear();
		for (const color_settings::ColorEntry &e : entries_) list_->Append(row_of(e));
		if (sel >= 0 && sel < static_cast<int>(entries_.size())) list_->SetSelection(sel);
	}

	void UpdateButtons()
	{
		const int sel = list_->GetSelection();
		const bool has_sel = sel != wxNOT_FOUND;
		refer_btn_->Enable(has_sel);
		// DisableColActionUpdate: 6項目だけ有効
		disable_btn_->Enable(has_sel &&
		                     color_settings::CanDisable(entries_[static_cast<std::size_t>(sel)].key));
	}

	void OnSelect(wxCommandEvent &) { UpdateButtons(); }

	void OnRefer(wxCommandEvent &)
	{
		const int sel = list_->GetSelection();
		if (sel == wxNOT_FOUND) return;
		color_settings::ColorEntry &e = entries_[static_cast<std::size_t>(sel)];

		// RefColBtnClick: 現在値を初期色にしてカラー選択する。
		// 無効値のときは VCL の既定 (clBlack) から選ばせる
		wxColourData data;
		data.SetColour(e.color == color_settings::DisabledColor()
		                   ? wxColour(0, 0, 0)
		                   : to_wx_colour(e.color));
		wxColourDialog dlg(this, &data);
		if (dlg.ShowModal() != wxID_OK) return;
		e.color = from_wx_colour(dlg.GetColourData().GetColour());
		RefreshList();
		UpdateButtons();
	}

	void OnDisable(wxCommandEvent &)
	{
		const int sel = list_->GetSelection();
		if (sel == wxNOT_FOUND) return;
		// DisableColActionExecute: Values[key] = IntToStr(col_None)
		if (!color_settings::DisableEntry(entries_,
		                                   entries_[static_cast<std::size_t>(sel)].key))
			return;
		RefreshList();
		UpdateButtons();
	}

	std::vector<color_settings::ColorEntry> &entries_;
	wxListBox *list_ = nullptr;
	wxButton *refer_btn_ = nullptr;
	wxButton *disable_btn_ = nullptr;
};

}  // namespace

bool Run(wxWindow *parent, std::vector<color_settings::ColorEntry> &entries)
{
	std::vector<color_settings::ColorEntry> work =
		color_settings::EnsureEntries(entries, color_settings::DisabledColor());
	ColorInputDialog dlg(parent, work);
	if (dlg.ShowModal() != wxID_OK) return false;
	entries = std::move(work);
	return true;
}

}  // namespace color_dialog
