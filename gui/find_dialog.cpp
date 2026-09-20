/**
 * @file gui/find_dialog.cpp
 * @brief gui/find_dialog.h の実装
 */
#include "gui/find_dialog.h"

#include <wx/checkbox.h>
#include <wx/radiobox.h>
#include <wx/choice.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "usr_str.h"

namespace find_dialog {

namespace {

/// wxString への変換 (gui/grep_dialog.cpp と同じ変換ヘルパー)
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
 * @brief ファイル名検索の条件入力ダイアログ
 * @details `src/FindDlg.cpp` (`TFindFileDlg`) の基本条件部
 *          (マスク・検索語・日付・サイズ・属性) だけを wx で再構成したもの。
 *          拡張条件部 (Exif・動画・画像・テキスト内容等) は未移植のため
 *          扱わない (ヘッダの説明を参照)
 */
class FindInputDialog : public wxDialog {
public:
	FindInputDialog(wxWindow *parent, find_files::Target initial_target,
	                const UnicodeString &initial_mask)
		: wxDialog(parent, wxID_ANY, to_wx(_T("ファイル名検索")), wxDefaultPosition, wxDefaultSize,
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		wxFlexGridSizer *grid = new wxFlexGridSizer(2, 4, 8);
		grid->AddGrowableCol(1);

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("マスク (; 区切り)"))),
		          wxSizerFlags().CentreVertical());
		mask_ctrl_ = new wxTextCtrl(this, wxID_ANY,
		                            to_wx(initial_mask.IsEmpty() ? _T("*") : initial_mask),
		                            wxDefaultPosition, wxSize(300, -1));
		grid->Add(mask_ctrl_, wxSizerFlags(1).Expand());

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("検索語"))),
		          wxSizerFlags().CentreVertical());
		keyword_ctrl_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
		                               wxDefaultPosition, wxSize(300, -1));
		keyword_ctrl_->SetHint(to_wx(_T("空白区切り。\"...\" で囲むと空白を含む1語")));
		grid->Add(keyword_ctrl_, wxSizerFlags(1).Expand());

		top->Add(grid, wxSizerFlags().Expand().Border(wxALL, 8));

		regex_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("正規表現として扱う")));
		case_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("大小文字を区別する")));
		and_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("空白区切りを AND で結ぶ (既定は OR)")));
		top->Add(regex_chk_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		top->Add(case_chk_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		top->Add(and_chk_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxArrayString targets;
		targets.Add(to_wx(_T("ファイル名")));
		targets.Add(to_wx(_T("ディレクトリ名")));
		targets.Add(to_wx(_T("両方")));
		target_radio_ = new wxRadioBox(this, wxID_ANY, to_wx(_T("対象")),
		                               wxDefaultPosition, wxDefaultSize, targets, 1, wxRA_SPECIFY_COLS);
		target_radio_->SetSelection(initial_target == find_files::Target::Directories ? 1
		                            : initial_target == find_files::Target::Both ? 2 : 0);
		top->Add(target_radio_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		recursive_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("サブディレクトリを含む")));
		recursive_chk_->SetValue(true);
		top->Add(recursive_chk_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		//サイズ条件 (VCL の SizeRadioGroup: 0=指定なし、1=以下、2=以上 + 単位)
		wxBoxSizer *size_row = new wxBoxSizer(wxHORIZONTAL);
		wxArrayString size_modes;
		size_modes.Add(to_wx(_T("指定なし")));
		size_modes.Add(to_wx(_T("以下")));
		size_modes.Add(to_wx(_T("以上")));
		size_mode_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, size_modes);
		size_mode_->SetSelection(0);
		size_row->Add(size_mode_, wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		size_value_ = new wxTextCtrl(this, wxID_ANY, _T("0"), wxDefaultPosition, wxSize(100, -1));
		size_row->Add(size_value_, wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		wxArrayString units;
		units.Add(_T("B"));
		units.Add(_T("KB"));
		units.Add(_T("MB"));
		units.Add(_T("GB"));
		size_unit_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, units);
		size_unit_->SetSelection(0);
		size_row->Add(size_unit_, wxSizerFlags().CentreVertical());
		top->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("サイズ"))),
		         wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(size_row, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		//日付条件 (VCL の DateRadioGroup: 0=指定なし、1=同じ日、2=以前、3=以後)
		wxBoxSizer *date_row = new wxBoxSizer(wxHORIZONTAL);
		wxArrayString date_modes;
		date_modes.Add(to_wx(_T("指定なし")));
		date_modes.Add(to_wx(_T("同じ日")));
		date_modes.Add(to_wx(_T("以前")));
		date_modes.Add(to_wx(_T("以後")));
		date_mode_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, date_modes);
		date_mode_->SetSelection(0);
		date_row->Add(date_mode_, wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		date_value_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
		                             wxDefaultPosition, wxSize(120, -1));
		date_value_->SetHint(_T("YYYY/MM/DD"));
		date_row->Add(date_value_, wxSizerFlags().CentreVertical());
		top->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("更新日"))),
		         wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(date_row, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		//属性条件 (VCL の AttrRadioGroup: 0=指定なし、1=含む、2=含まない + R/H/S/A/C)
		wxBoxSizer *attr_row = new wxBoxSizer(wxHORIZONTAL);
		wxArrayString attr_modes;
		attr_modes.Add(to_wx(_T("指定なし")));
		attr_modes.Add(to_wx(_T("含む")));
		attr_modes.Add(to_wx(_T("含まない")));
		attr_mode_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, attr_modes);
		attr_mode_->SetSelection(0);
		attr_row->Add(attr_mode_, wxSizerFlags().CentreVertical().Border(wxRIGHT, 8));
		attr_r_ = new wxCheckBox(this, wxID_ANY, _T("R"));
		attr_h_ = new wxCheckBox(this, wxID_ANY, _T("H"));
		attr_s_ = new wxCheckBox(this, wxID_ANY, _T("S"));
		attr_a_ = new wxCheckBox(this, wxID_ANY, _T("A"));
		attr_c_ = new wxCheckBox(this, wxID_ANY, _T("C"));
		attr_row->Add(attr_r_, wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		attr_row->Add(attr_h_, wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		attr_row->Add(attr_s_, wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		attr_row->Add(attr_a_, wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		attr_row->Add(attr_c_, wxSizerFlags().CentreVertical());
		top->Add(new wxStaticText(this, wxID_ANY,
		                          to_wx(_T("属性 (R:読取専用 H:隠し S:システム A:アーカイブ C:圧縮)"))),
		         wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		top->Add(attr_row, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		mask_ctrl_->SetFocus();
		mask_ctrl_->SelectAll();

		Bind(wxEVT_BUTTON, &FindInputDialog::OnOk, this, wxID_OK);
	}

	find_files::Query Options() const
	{
		find_files::Query q;
		q.mask = to_us(mask_ctrl_->GetValue());
		if (q.mask.IsEmpty()) q.mask = _T("*");
		q.keyword = to_us(keyword_ctrl_->GetValue());
		q.use_regex = regex_chk_->GetValue();
		q.case_sensitive = case_chk_->GetValue();
		q.match_all = and_chk_->GetValue();
		switch (target_radio_->GetSelection()) {
		case 1:  q.target = find_files::Target::Directories; break;
		case 2:  q.target = find_files::Target::Both; break;
		default: q.target = find_files::Target::Files; break;
		}
		q.recursive = recursive_chk_->GetValue();

		const long size_num = ParseSizeValue(size_value_->GetValue());
		q.size_value = size_num * SizeUnitMult(size_unit_->GetSelection());
		switch (size_mode_->GetSelection()) {
		case 1:  q.size_mode = find_files::SizeMode::AtMost; break;
		case 2:  q.size_mode = find_files::SizeMode::AtLeast; break;
		default: q.size_mode = find_files::SizeMode::None; break;
		}

		q.date_value = ParseDateValue(to_us(date_value_->GetValue()));
		switch (date_mode_->GetSelection()) {
		case 1:  q.date_mode = find_files::DateMode::Same; break;
		case 2:  q.date_mode = find_files::DateMode::Before; break;
		case 3:  q.date_mode = find_files::DateMode::After; break;
		default: q.date_mode = find_files::DateMode::None; break;
		}

		q.attr_bits = 0;
		if (attr_r_->GetValue()) q.attr_bits |= faReadOnly;
		if (attr_h_->GetValue()) q.attr_bits |= faHidden;
		if (attr_s_->GetValue()) q.attr_bits |= faSysFile;
		if (attr_a_->GetValue()) q.attr_bits |= faArchive;
		if (attr_c_->GetValue()) q.attr_bits |= faCompressed;
		switch (attr_mode_->GetSelection()) {
		case 1:  q.attr_mode = find_files::AttrMode::HasAny; break;
		case 2:  q.attr_mode = find_files::AttrMode::HasNone; break;
		default: q.attr_mode = find_files::AttrMode::None; break;
		}

		return q;
	}

private:
	void OnOk(wxCommandEvent & /*event*/)
	{
		//正規表現の事前検証 (VCL の TFindFileDlg::FindOkActionUpdate と同じ)。
		//不正なら閉じずにやり直させる
		if (regex_chk_->GetValue() && !to_us(keyword_ctrl_->GetValue()).IsEmpty()
			&& !chk_RegExPtn(to_us(keyword_ctrl_->GetValue()))) {
			wxMessageBox(to_wx(_T("正規表現が正しくありません")),
			             to_wx(_T("ファイル名検索")), wxOK | wxICON_WARNING, this);
			return;
		}
		//サイズの検証
		if (size_mode_->GetSelection() != 0 && ParseSizeValue(size_value_->GetValue()) < 0) {
			wxMessageBox(to_wx(_T("サイズは0以上の数値で入力してください")),
			             to_wx(_T("ファイル名検索")), wxOK | wxICON_WARNING, this);
			return;
		}
		//日付の検証
		if (date_mode_->GetSelection() != 0
			&& ParseDateValue(to_us(date_value_->GetValue())) == 0.0) {
			wxMessageBox(to_wx(_T("日付は YYYY/MM/DD で入力してください")),
			             to_wx(_T("ファイル名検索")), wxOK | wxICON_WARNING, this);
			return;
		}
		EndModal(wxID_OK);
	}

	/// サイズ欄の解釈。数値でなければ -1
	static long ParseSizeValue(const wxString &s)
	{
		long v = 0;
		if (!s.ToLong(&v) || v < 0) return -1;
		return v;
	}

	/// 単位の倍率
	static long long SizeUnitMult(int sel)
	{
		switch (sel) {
		case 1:  return 1024;
		case 2:  return 1048576;
		case 3:  return 1073741824;
		default: return 1;
		}
	}

	/// 日付欄の解釈。解釈できなければ 0.0 (VCL の str_to_DateTime を使う)
	static TDateTime ParseDateValue(const UnicodeString &s)
	{
		if (s.IsEmpty()) return 0.0;
		try {
			return str_to_DateTime(s);
		}
		catch (...) {
			return 0.0;
		}
	}

	wxTextCtrl *mask_ctrl_ = nullptr;
	wxTextCtrl *keyword_ctrl_ = nullptr;
	wxCheckBox *regex_chk_ = nullptr;
	wxCheckBox *case_chk_ = nullptr;
	wxCheckBox *and_chk_ = nullptr;
	wxRadioBox *target_radio_ = nullptr;
	wxCheckBox *recursive_chk_ = nullptr;
	wxChoice *size_mode_ = nullptr;
	wxTextCtrl *size_value_ = nullptr;
	wxChoice *size_unit_ = nullptr;
	wxChoice *date_mode_ = nullptr;
	wxTextCtrl *date_value_ = nullptr;
	wxChoice *attr_mode_ = nullptr;
	wxCheckBox *attr_r_ = nullptr;
	wxCheckBox *attr_h_ = nullptr;
	wxCheckBox *attr_s_ = nullptr;
	wxCheckBox *attr_a_ = nullptr;
	wxCheckBox *attr_c_ = nullptr;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, find_files::Target initial_target,
         const UnicodeString &initial_mask, find_files::Query &query_out)
{
	FindInputDialog dlg(parent, initial_target, initial_mask);
	if (dlg.ShowModal() != wxID_OK) return false;
	query_out = dlg.Options();
	return true;
}

}  // namespace find_dialog
