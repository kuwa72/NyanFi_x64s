/**
 * @file gui/sync_dialog.cpp
 * @brief gui/sync_dialog.h の実装
 */
#include "gui/sync_dialog.h"

#include <wx/checklst.h>
#include <wx/listbox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textdlg.h>

namespace sync_dialog {

namespace {

/// wxString への変換 (gui/regdir_dialog.cpp と同じ変換ヘルパー)
inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

/// wxString → UnicodeString (MSW では両方 UTF-16)
inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

/// 登録一覧の1行表示 (VCL のオーナー描画の代わりに「名前 [OD] dir数」)
wxString row_of(const sync_dirs::SyncEntry &e)
{
	UnicodeString row = e.title;
	UnicodeString opt;
	if (e.overwrite) opt += "O";
	if (e.sync_delete) opt += "D";
	if (!opt.IsEmpty()) row += UnicodeString(_T(" [")) + opt + _T("]");
	row.cat_sprintf(_T(" (%u)"), static_cast<unsigned>(e.dirs.size()));
	if (!e.dirs.empty()) row += UnicodeString(_T("  ")) + e.dirs[0];
	return to_wx(row);
}

/**
 * @brief 同期コピー設定の編集ダイアログ
 * @details `src/SyncDlg.cpp` (`TRegSyncDlg`) のうち、登録の追加・変更・削除
 *          (`AddRegAction`/`ChgRegAction`/`DelRegAction`) とディレクトリの
 *          追加・削除・クリア (`AddDirAction`/`DelDirAction`/`ClrDirAction`)、
 *          有効チェック (`RegListBoxClickCheck`)、確定時の正規化
 *          (`OkButtonClick`) だけを wx で再構成したもの
 */
class SyncInputDialog : public wxDialog {
public:
	SyncInputDialog(wxWindow *parent, std::vector<sync_dirs::SyncEntry> &entries,
	                const UnicodeString &current_dir)
		: wxDialog(parent, wxID_ANY, to_wx(_T("同期コピーの設定")), wxDefaultPosition,
		           wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, entries_(entries)
		, current_dir_(current_dir)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		reg_list_ = new wxCheckListBox(this, wxID_ANY, wxDefaultPosition,
		                               wxSize(480, 160));
		top->Add(reg_list_, wxSizerFlags(1).Expand().Border(wxALL, 8));
		reg_list_->Bind(wxEVT_CHECKLISTBOX, &SyncInputDialog::OnRegCheck, this);
		reg_list_->Bind(wxEVT_LISTBOX, &SyncInputDialog::OnRegSelect, this);

		wxBoxSizer *name_row = new wxBoxSizer(wxHORIZONTAL);
		name_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("名前"))),
		              wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		name_edit_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
		                            wxDefaultPosition, wxSize(160, -1));
		name_row->Add(name_edit_, wxSizerFlags().Border(wxRIGHT, 8));
		owr_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("上書き")));
		name_row->Add(owr_chk_, wxSizerFlags().CentreVertical().Border(wxRIGHT, 8));
		del_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("同期削除")));
		name_row->Add(del_chk_, wxSizerFlags().CentreVertical());
		top->Add(name_row, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxBoxSizer *reg_btn_row = new wxBoxSizer(wxHORIZONTAL);
		add_reg_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("登録")));
		reg_btn_row->Add(add_reg_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		chg_reg_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("変更")));
		reg_btn_row->Add(chg_reg_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		del_reg_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("削除")));
		reg_btn_row->Add(del_reg_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		top->Add(reg_btn_row, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		add_reg_btn_->Bind(wxEVT_BUTTON, &SyncInputDialog::OnAddReg, this);
		chg_reg_btn_->Bind(wxEVT_BUTTON, &SyncInputDialog::OnChgReg, this);
		del_reg_btn_->Bind(wxEVT_BUTTON, &SyncInputDialog::OnDelReg, this);

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		dir_list_ = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(480, 110));
		top->Add(dir_list_, wxSizerFlags(1).Expand().Border(wxALL, 8));

		wxBoxSizer *dir_btn_row = new wxBoxSizer(wxHORIZONTAL);
		add_dir_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("追加...")));
		dir_btn_row->Add(add_dir_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		del_dir_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("削除")));
		dir_btn_row->Add(del_dir_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		clr_dir_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("クリア")));
		dir_btn_row->Add(clr_dir_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		top->Add(dir_btn_row, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		add_dir_btn_->Bind(wxEVT_BUTTON, &SyncInputDialog::OnAddDir, this);
		del_dir_btn_->Bind(wxEVT_BUTTON, &SyncInputDialog::OnDelDir, this);
		clr_dir_btn_->Bind(wxEVT_BUTTON, &SyncInputDialog::OnClrDir, this);

		wxBoxSizer *ok_row = new wxBoxSizer(wxHORIZONTAL);
		ok_row->Add(new wxButton(this, wxID_OK, to_wx(_T("OK"))),
		            wxSizerFlags().Border(wxRIGHT, 4));
		ok_row->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("キャンセル"))));
		top->Add(ok_row, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		RefreshRegList();
		UpdateButtons();
	}

private:
	/// 編集中の下部 (名前・オプション・ディレクトリ) から項目を作る
	sync_dirs::SyncEntry DraftEntry(int checked_idx) const
	{
		sync_dirs::SyncEntry e;
		e.title = to_us(name_edit_->GetValue());
		if (e.title.IsEmpty())
			e.title = sync_dirs::DefaultTitle(static_cast<int>(entries_.size()));
		e.enabled = checked_idx >= 0 ? entries_[static_cast<std::size_t>(checked_idx)].enabled
		                             : false;  // 新規は無効 (MakeRegItem の idx==-1 分岐)
		e.overwrite = owr_chk_->GetValue();
		e.sync_delete = del_chk_->GetValue();
		for (unsigned i = 0; i < dir_list_->GetCount(); i++)
			e.dirs.push_back(to_us(dir_list_->GetString(i)));
		return e;
	}

	void RefreshRegList()
	{
		reg_list_->Clear();
		for (const sync_dirs::SyncEntry &e : entries_) reg_list_->Append(row_of(e));
		for (unsigned i = 0; i < entries_.size(); i++)
			reg_list_->Check(i, entries_[i].enabled);
	}

	void RefreshDirList()
	{
		dir_list_->Clear();
		const int sel = reg_list_->GetSelection();
		if (sel == wxNOT_FOUND) return;
		const sync_dirs::SyncEntry &e = entries_[static_cast<std::size_t>(sel)];
		name_edit_->SetValue(to_wx(e.title));
		owr_chk_->SetValue(e.overwrite);
		del_chk_->SetValue(e.sync_delete);
		for (const UnicodeString &d : e.dirs) dir_list_->Append(to_wx(d));
	}

	void UpdateButtons()
	{
		const bool has_reg = reg_list_->GetSelection() != wxNOT_FOUND;
		chg_reg_btn_->Enable(has_reg);
		del_reg_btn_->Enable(has_reg);
		// AddRegActionUpdate: DirListBox->Count>=2
		add_reg_btn_->Enable(sync_dirs::CanAdd(static_cast<int>(dir_list_->GetCount())));
		del_dir_btn_->Enable(dir_list_->GetSelection() != wxNOT_FOUND);
	}

	void OnRegSelect(wxCommandEvent &) { RefreshDirList(); UpdateButtons(); }

	void OnRegCheck(wxCommandEvent &event)
	{
		// RegListBoxClickCheck: チェックを CSV の [1] に反映する
		const int idx = event.GetInt();
		if (idx >= 0 && idx < static_cast<int>(entries_.size()))
			entries_[static_cast<std::size_t>(idx)].enabled = reg_list_->IsChecked(idx);
		RefreshRegList();
		reg_list_->SetSelection(idx);
		UpdateButtons();
	}

	void OnAddReg(wxCommandEvent &)
	{
		entries_.push_back(DraftEntry(-1));
		RefreshRegList();
		reg_list_->SetSelection(static_cast<int>(entries_.size()) - 1);
		RefreshDirList();
		UpdateButtons();
	}

	void OnChgReg(wxCommandEvent &)
	{
		const int sel = reg_list_->GetSelection();
		if (sel == wxNOT_FOUND) return;
		entries_[static_cast<std::size_t>(sel)] = DraftEntry(sel);
		RefreshRegList();
		reg_list_->SetSelection(sel);
		UpdateButtons();
	}

	void OnDelReg(wxCommandEvent &)
	{
		const int sel = reg_list_->GetSelection();
		if (sel == wxNOT_FOUND) return;
		entries_.erase(entries_.begin() + sel);
		RefreshRegList();
		name_edit_->Clear();
		dir_list_->Clear();
		UpdateButtons();
	}

	void OnAddDir(wxCommandEvent &)
	{
		// VCL は SelectDirEx のフォルダ参照。wx 移植では直接入力
		// (未実装扱いではなく簡易化。フォルダ参照 UI 自体は未移植)
		const wxString path = wxGetTextFromUser(
			to_wx(_T("同期するディレクトリ")), to_wx(_T("同期コピー先の追加")),
			to_wx(current_dir_), this);
		if (path.IsEmpty()) return;
		const UnicodeString dir = to_us(path);
		if (dir_list_->FindString(to_wx(dir)) != wxNOT_FOUND) {
			wxMessageBox(to_wx(_T("登録済みです")), to_wx(_T("同期コピーの設定")),
			             wxOK | wxICON_INFORMATION, this);
			return;
		}
		dir_list_->Append(to_wx(dir));
		UpdateButtons();
	}

	void OnDelDir(wxCommandEvent &)
	{
		const int sel = dir_list_->GetSelection();
		if (sel == wxNOT_FOUND) return;
		dir_list_->Delete(sel);
		UpdateButtons();
	}

	void OnClrDir(wxCommandEvent &)
	{
		// ClrDirActionExecute: 選択解除・名前消去・ディレクトリ消去
		reg_list_->SetSelection(wxNOT_FOUND);
		name_edit_->Clear();
		dir_list_->Clear();
		UpdateButtons();
	}

	std::vector<sync_dirs::SyncEntry> &entries_;
	const UnicodeString current_dir_;
	wxCheckListBox *reg_list_ = nullptr;
	wxTextCtrl *name_edit_ = nullptr;
	wxCheckBox *owr_chk_ = nullptr;
	wxCheckBox *del_chk_ = nullptr;
	wxListBox *dir_list_ = nullptr;
	wxButton *add_reg_btn_ = nullptr;
	wxButton *chg_reg_btn_ = nullptr;
	wxButton *del_reg_btn_ = nullptr;
	wxButton *add_dir_btn_ = nullptr;
	wxButton *del_dir_btn_ = nullptr;
	wxButton *clr_dir_btn_ = nullptr;
};

}  // namespace

bool Run(wxWindow *parent, std::vector<sync_dirs::SyncEntry> &entries,
         const UnicodeString &current_dir)
{
	SyncInputDialog dlg(parent, entries, current_dir);
	if (dlg.ShowModal() != wxID_OK) return false;
	// OkButtonClick: 不正行を落として正規化する
	std::vector<sync_dirs::SyncEntry> fixed;
	for (const sync_dirs::SyncEntry &e : entries) {
		if (!sync_dirs::IsValid(e)) continue;
		const UnicodeString norm = sync_dirs::NormalizeRecord(
			sync_dirs::FormatRecord(e, 0));
		if (!norm.IsEmpty()) fixed.push_back(sync_dirs::ParseRecord(norm));
	}
	entries = std::move(fixed);
	return true;
}

}  // namespace sync_dialog
