/**
 * @file gui/comp_dialog.cpp
 * @brief gui/comp_dialog.h の実装
 */
#include "gui/comp_dialog.h"

#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/radiobox.h>
#include <wx/statline.h>

#include "usr_file_inf.h"  // HASH_ID_STR (ハッシュ算法)
#include "usr_str.h"       // get_word_i_idx

namespace comp_dialog {

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

/// 条件選択欄のラベル (src/CompDlg.dfm の Items.Strings と同じ並び)
const wchar_t *const kTimeItems[] = {_T("無視"), _T("不一致"), _T("一致"), _T("新しい"), _T("古い")};
const wchar_t *const kSizeItems[] = {_T("無視"), _T("不一致"), _T("一致"), _T("大きい"), _T("小さい")};
const wchar_t *const kHashItems[] = {_T("無視"), _T("不一致"), _T("一致")};
const wchar_t *const kIdItems[] = {_T("無視"), _T("不一致"), _T("一致")};

wxArrayString MakeChoices(const wchar_t *const *items, int count)
{
	wxArrayString a;
	for (int i = 0; i < count; ++i) a.Add(to_wx(UnicodeString(items[i])));
	return a;
}

/**
 * @brief 同名ファイルの比較条件入力ダイアログ
 * @details `src/CompDlg.cpp` (`TFileCompDlg`) の移植可能な範囲を wx で
 *          再構成したもの。有効・無効 (`TFileCompDlg::OkActionUpdate`) と
 *          ハッシュ/同一性の排他 (`TFileCompDlg::OptRadioGroupClick`) は
 *          `gui/compare.h` の純関数で決める
 */
class CompInputDialog : public wxDialog {
public:
	CompInputDialog(wxWindow *parent, const compare::CompOptions &opt, const Context &ctx)
		: wxDialog(parent, wxID_ANY,
		           to_wx(ctx.case_sensitive
		                 ? _T("同名ファイルの比較 - 大文字・小文字を区別")
		                 : _T("同名ファイルの比較")),
		           wxDefaultPosition, wxDefaultSize,
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
		  ctx_(ctx)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		//-- タイムスタンプ (VCL TimeRadioGroup) ----------------------------
		time_radio_ = new wxRadioBox(this, wxID_ANY, to_wx(_T("タイムスタンプ(&T)")),
		                             wxDefaultPosition, wxDefaultSize,
		                             MakeChoices(kTimeItems, 5), 2, wxRA_SPECIFY_ROWS);
		time_radio_->SetSelection(static_cast<int>(opt.time_mode));
		top->Add(time_radio_, wxSizerFlags().Expand().Border(wxALL, 8));

		//-- サイズ (VCL SizeRadioGroup) ------------------------------------
		size_radio_ = new wxRadioBox(this, wxID_ANY, to_wx(_T("サイズ(&S)")),
		                             wxDefaultPosition, wxDefaultSize,
		                             MakeChoices(kSizeItems, 5), 2, wxRA_SPECIFY_ROWS);
		size_radio_->SetSelection(static_cast<int>(opt.size_mode));
		top->Add(size_radio_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		//-- ハッシュ + 算法 (VCL HashPanel) --------------------------------
		wxBoxSizer *hash_box = new wxBoxSizer(wxHORIZONTAL);
		hash_radio_ = new wxRadioBox(this, wxID_ANY, to_wx(_T("ハッシュ(&H)")),
		                             wxDefaultPosition, wxDefaultSize,
		                             MakeChoices(kHashItems, 3), 3, wxRA_SPECIFY_COLS);
		hash_radio_->SetSelection(static_cast<int>(opt.hash_mode));
		hash_box->Add(hash_radio_, wxSizerFlags().Expand());

		// 算法は HASH_ID_STR ("MD5|SHA1|...") を順に並べる
		alg_ctrl_ = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                           wxSize(120, -1), 0, nullptr, wxCB_DROPDOWN | wxCB_READONLY);
		for (int i = 0; ; ++i) {
			const UnicodeString id = get_word_i_idx(HASH_ID_STR, i);
			if (id.IsEmpty()) break;
			alg_ctrl_->Append(to_wx(id));
		}
		alg_ctrl_->SetSelection(opt.alg_index >= 0 && opt.alg_index < static_cast<int>(alg_ctrl_->GetCount())
		                        ? opt.alg_index : 0);
		hash_box->Add(alg_ctrl_, wxSizerFlags().CentreVertical().Border(wxLEFT, 8));
		top->Add(hash_box, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		//-- 同一性 (VCL IdPanel) -------------------------------------------
		id_radio_ = new wxRadioBox(this, wxID_ANY, to_wx(_T("同一性/ファイル識別番号ID(&I)")),
		                           wxDefaultPosition, wxDefaultSize,
		                           MakeChoices(kIdItems, 3), 3, wxRA_SPECIFY_COLS);
		id_radio_->SetSelection(static_cast<int>(opt.id_mode));
		top->Add(id_radio_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		//-- チェック類 (VCL の 5 つの TCheckBox) ---------------------------
		cmp_dir_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("ディレクトリも比較(&D)")));
		cmp_dir_chk_->SetValue(opt.cmp_dir);
		cmp_arc_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("ディレクトリとアーカイブも比較(&P)")));
		cmp_arc_chk_->SetValue(opt.cmp_arc);
		wxBoxSizer *dir_box = new wxBoxSizer(wxHORIZONTAL);
		dir_box->Add(cmp_dir_chk_, wxSizerFlags().Border(wxRIGHT, 12));
		dir_box->Add(cmp_arc_chk_);
		top->Add(dir_box, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		sel_opp_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("決定側も選択(&O)")));
		sel_opp_chk_->SetValue(opt.sel_opp);
		reverse_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("選択を反転(&R)")));
		reverse_chk_->SetValue(opt.sel_rev);
		sel_mask_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("選択項目だけ残す(&M)")));
		sel_mask_chk_->SetValue(opt.sel_msk);
		wxBoxSizer *sel_box = new wxBoxSizer(wxHORIZONTAL);
		sel_box->Add(sel_opp_chk_, wxSizerFlags().Border(wxRIGHT, 12));
		sel_box->Add(reverse_chk_, wxSizerFlags().Border(wxRIGHT, 12));
		sel_box->Add(sel_mask_chk_);
		top->Add(sel_box, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		// VCL は Update で常に有効/無効を計算し直す
		hash_radio_->Bind(wxEVT_RADIOBOX, &CompInputDialog::OnHash, this);
		id_radio_->Bind(wxEVT_RADIOBOX, &CompInputDialog::OnId, this);
		size_radio_->Bind(wxEVT_RADIOBOX, &CompInputDialog::OnOptChanged, this);
		cmp_dir_chk_->Bind(wxEVT_CHECKBOX, &CompInputDialog::OnOptChanged, this);
		UpdateEnabled();
	}

	compare::CompOptions Options() const
	{
		compare::CompOptions o;
		o.time_mode = static_cast<compare::CompTimeMode>(time_radio_->GetSelection());
		o.size_mode = static_cast<compare::CompSizeMode>(size_radio_->GetSelection());
		o.hash_mode = static_cast<compare::CompHashMode>(hash_radio_->GetSelection());
		o.id_mode = static_cast<compare::CompIdMode>(id_radio_->GetSelection());
		o.alg_index = alg_ctrl_->GetSelection();
		o.cmp_dir = cmp_dir_chk_->GetValue();
		o.cmp_arc = cmp_arc_chk_->GetValue();
		o.sel_opp = sel_opp_chk_->GetValue();
		o.sel_rev = reverse_chk_->GetValue();
		o.sel_msk = sel_mask_chk_->GetValue();
		return o;
	}

private:
	// VCL (OptRadioGroupClick): ハッシュ/同一性を選んだら相手を「無視」にする
	void OnHash(wxCommandEvent &event)
	{
		compare::CompOptions o = compare::ApplyExclusiveOpt(Options(), /*hash_clicked=*/true);
		id_radio_->SetSelection(static_cast<int>(o.id_mode));
		event.Skip();
		UpdateEnabled();
	}

	void OnId(wxCommandEvent &event)
	{
		compare::CompOptions o = compare::ApplyExclusiveOpt(Options(), /*hash_clicked=*/false);
		hash_radio_->SetSelection(static_cast<int>(o.hash_mode));
		event.Skip();
		UpdateEnabled();
	}

	void OnOptChanged(wxCommandEvent &event)
	{
		event.Skip();
		UpdateEnabled();
	}

	// VCL (TFileCompDlg::OkActionUpdate): 条件から有効・無効を決める
	void UpdateEnabled()
	{
		const compare::CompEnable en =
			compare::ResolveCompEnabled(Options(), ctx_.all_dir_has_size, ctx_.ftp_either,
			                            ctx_.arc_either, ctx_.sel_mask_available);
		size_radio_->Enable(en.size);
		hash_radio_->Enable(en.hash);
		alg_ctrl_->Enable(en.alg);
		id_radio_->Enable(en.id);
		cmp_arc_chk_->Enable(en.cmp_arc);
		sel_mask_chk_->Enable(en.sel_mask);
	}

	wxRadioBox *time_radio_ = nullptr;
	wxRadioBox *size_radio_ = nullptr;
	wxRadioBox *hash_radio_ = nullptr;
	wxRadioBox *id_radio_ = nullptr;
	wxComboBox *alg_ctrl_ = nullptr;
	wxCheckBox *cmp_dir_chk_ = nullptr;
	wxCheckBox *cmp_arc_chk_ = nullptr;
	wxCheckBox *sel_opp_chk_ = nullptr;
	wxCheckBox *reverse_chk_ = nullptr;
	wxCheckBox *sel_mask_chk_ = nullptr;
	Context ctx_;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, compare::CompOptions &opt_inout, const Context &ctx)
{
	// CS は MainFrm 側の指定なので、入力欄には戻さない
	const bool case_sensitive = opt_inout.case_sensitive;
	CompInputDialog dlg(parent, opt_inout, ctx);
	if (dlg.ShowModal() != wxID_OK) return false;
	opt_inout = dlg.Options();
	opt_inout.case_sensitive = case_sensitive;
	return true;
}

}  // namespace comp_dialog
