/**
 * @file gui/key_dialog.cpp
 * @brief gui/key_dialog.h の実装
 */
#include "gui/key_dialog.h"

#include <algorithm>

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/clipbrd.h>
#include <wx/filedlg.h>
#include <wx/listctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "usr_file_ex.h"
#include "usr_str.h"

namespace key_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

class KeyListDialog : public wxDialog {
public:
	KeyListDialog(wxWindow *parent, key::StateStore &state,
	              const std::vector<key::Entry> &rows, Result &result)
		: wxDialog(parent, wxID_ANY, to_wx(_T("キー割り当て一覧")), wxDefaultPosition,
		           wxSize(650, 500), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, state_(state)
		, rows_(rows)
		, result_(result)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		wxBoxSizer *tab_row = new wxBoxSizer(wxHORIZONTAL);
		tab_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("対象"))),
		             wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		tab_ctrl_ = new wxChoice(this, wxID_ANY);
		for (int i = 0; i < 5; ++i) tab_ctrl_->Append(to_wx(key::TabLabel(static_cast<key::Tab>(i))));
		tab_ctrl_->SetSelection(static_cast<int>(state_.tab));
		tab_row->Add(tab_ctrl_, wxSizerFlags().CentreVertical().Border(wxRIGHT, 12));
		tab_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("並べ替え"))),
		             wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		sort_ctrl_ = new wxChoice(this, wxID_ANY);
		sort_ctrl_->Append(to_wx(_T("キー")));
		sort_ctrl_->Append(to_wx(_T("コマンド")));
		sort_ctrl_->Append(to_wx(_T("説明")));
		sort_ctrl_->SetSelection(static_cast<int>(state_.sort_mode));
		tab_row->Add(sort_ctrl_, wxSizerFlags().CentreVertical());
		top->Add(tab_row, wxSizerFlags().Expand().Border(wxALL, 8));
		tab_ctrl_->Bind(wxEVT_CHOICE, &KeyListDialog::OnTabChanged, this);
		sort_ctrl_->Bind(wxEVT_CHOICE, &KeyListDialog::OnSortChanged, this);

		wxBoxSizer *filter_row = new wxBoxSizer(wxHORIZONTAL);
		filter_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("フィルタ"))),
		                wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		filter_ctrl_ = new wxTextCtrl(this, wxID_ANY, to_wx(state_.filter), wxDefaultPosition, wxSize(260, -1));
		filter_row->Add(filter_ctrl_, wxSizerFlags(1).Expand().Border(wxRIGHT, 8));
		show_all_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("未登録コマンドも表示")));
		show_all_chk_->SetValue(state_.show_all);
		filter_row->Add(show_all_chk_, wxSizerFlags().CentreVertical().Border(wxRIGHT, 8));
		show_all_chk_->Bind(wxEVT_CHECKBOX, &KeyListDialog::OnShowAllChanged, this);

		migemo_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("Migemo")));
		migemo_chk_->SetValue(state_.migemo);
		filter_row->Add(migemo_chk_, wxSizerFlags().CentreVertical().Border(wxRIGHT, 8));
		confirm_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("確定即実行")));
		confirm_chk_->SetValue(state_.confirm_execute);
		filter_row->Add(confirm_chk_, wxSizerFlags().CentreVertical());
		top->Add(filter_row, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		filter_ctrl_->Bind(wxEVT_TEXT, &KeyListDialog::OnFilterChanged, this);
		migemo_chk_->Bind(wxEVT_CHECKBOX, &KeyListDialog::OnMigemoChanged, this);
		confirm_chk_->Bind(wxEVT_CHECKBOX, &KeyListDialog::OnConfirmChanged, this);

		list_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		                       wxLC_REPORT | wxLC_SINGLE_SEL);
		list_->AppendColumn(to_wx(_T("キー")), wxLIST_FORMAT_LEFT, 150);
		list_->AppendColumn(to_wx(_T("コマンド")), wxLIST_FORMAT_LEFT, 220);
		list_->AppendColumn(to_wx(_T("説明")), wxLIST_FORMAT_LEFT, 240);
		top->Add(list_, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		list_->Bind(wxEVT_LIST_ITEM_ACTIVATED, &KeyListDialog::OnActivated, this);
		list_->Bind(wxEVT_LIST_ITEM_SELECTED, &KeyListDialog::OnSelect, this);

		wxBoxSizer *actions = new wxBoxSizer(wxHORIZONTAL);
		wxButton *copy_btn = new wxButton(this, wxID_ANY, to_wx(_T("一覧をコピー")));
		copy_btn->Bind(wxEVT_BUTTON, &KeyListDialog::OnCopy, this);
		wxButton *save_btn = new wxButton(this, wxID_ANY, to_wx(_T("名前を付けて保存")));
		save_btn->Bind(wxEVT_BUTTON, &KeyListDialog::OnSave, this);
		wxButton *help_btn = new wxButton(this, wxID_ANY, to_wx(_T("コマンドのヘルプ")));
		help_btn->Bind(wxEVT_BUTTON, &KeyListDialog::OnHelp, this);
		actions->Add(copy_btn, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(save_btn, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(help_btn);
		top->Add(actions, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		top->Add(new wxStaticText(this, wxID_ANY,
		                           to_wx(_T("※ VCLのS/V/I/Lのini割当・2ストローク/SELECT+・")
		                                    _T("Migemo辞書・ヘルプ/設定画面・グリッド位置保存は未実装扱い。"))),
		          wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));
		SetSizerAndFit(top);
		Bind(wxEVT_BUTTON, &KeyListDialog::OnOk, this, wxID_OK);
		RefreshList();
		filter_ctrl_->SetFocus();
	}

private:
	void RefreshList()
	{
		shown_.clear();
		for (const key::Entry &entry : rows_) {
			if (entry.tab != state_.tab) continue;
			// rows_ には割り当て行と未登録コマンド行を同じ形式で統一行している。
			// 空キーは未登録行の印なので、設定がオフなら表示しない。
			if (!state_.show_all && entry.key.IsEmpty()) continue;
			shown_.push_back(entry);
		}
		shown_ = key::FilterAndSort(shown_, state_.filter, state_.sort_mode);
		list_->DeleteAllItems();
		for (std::size_t i = 0; i < shown_.size(); ++i) {
			const long row = list_->InsertItem(static_cast<long>(i), to_wx(key::FormatKeyForDisplay(shown_[i].key)));
			list_->SetItem(row, 1, to_wx(shown_[i].command));
			list_->SetItem(row, 2, to_wx(shown_[i].description));
		}
		if (!shown_.empty()) {
			list_->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
			list_->SetItemState(0, wxLIST_STATE_FOCUSED, wxLIST_STATE_FOCUSED);
		}
	}

	void OnTabChanged(wxCommandEvent &)
	{
		state_.tab = static_cast<key::Tab>(std::max(0, tab_ctrl_->GetSelection()));
		RefreshList();
	}

	void OnSortChanged(wxCommandEvent &)
	{
		state_.sort_mode = static_cast<key::SortMode>(std::max(0, sort_ctrl_->GetSelection()));
		RefreshList();
	}

	void OnFilterChanged(wxCommandEvent &)
	{
		state_.filter = to_us(filter_ctrl_->GetValue());
		state_.migemo = migemo_chk_->GetValue();
		RefreshList();
		if (state_.confirm_execute && shown_.size() == 1 && !shown_[0].command.IsEmpty()) {
			result_.command = shown_[0].command;
			result_.accepted = true;
			EndModal(wxID_OK);
		}
	}

	void OnMigemoChanged(wxCommandEvent &)
	{
		state_.migemo = migemo_chk_->GetValue();
		RefreshList();
	}

	void OnShowAllChanged(wxCommandEvent &)
	{
		state_.show_all = show_all_chk_->GetValue();
		RefreshList();
	}

	void OnConfirmChanged(wxCommandEvent &)
	{
		state_.confirm_execute = confirm_chk_->GetValue();
	}

	void OnSelect(wxListEvent &)
	{
		// 選択行は OnOk で直接読む。wx 3.3 のイベント型をここで固定する。
	}

	void AcceptSelection()
	{
		const long selected = list_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (selected >= 0 && static_cast<std::size_t>(selected) < shown_.size()) {
			result_.command = shown_[static_cast<std::size_t>(selected)].command;
			result_.accepted = true;
		}
		EndModal(wxID_OK);
	}

	void OnOk(wxCommandEvent &)
	{
		AcceptSelection();
	}

	void OnActivated(wxListEvent &)
	{
		AcceptSelection();
	}

	void OnCopy(wxCommandEvent &)
	{
		if (!wxTheClipboard->Open()) return;
		wxTheClipboard->SetData(new wxTextDataObject(to_wx(key::FormatList(shown_))));
		wxTheClipboard->Close();
	}

	void OnSave(wxCommandEvent &)
	{
		wxFileDialog dlg(this, to_wx(_T("キー割り当て一覧を保存")), wxEmptyString, _T("keylist.txt"),
		                 _T("テキスト (*.txt)|*.txt|すべて (*.*)|*.*"),
		                 wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
		if (dlg.ShowModal() != wxID_OK) return;
		TStringList lines;
		lines.Text = key::FormatList(shown_);
		const UnicodeString path = to_us(dlg.GetPath());
		lines.SaveToFile(path);
		if (!file_exists(path)) {
			wxMessageBox(to_wx(_T("保存できませんでした")), to_wx(_T("キー割り当て一覧")),
			             wxOK | wxICON_ERROR, this);
		}
	}

	void OnHelp(wxCommandEvent &)
	{
		wxMessageBox(to_wx(_T("VCLのヘルプブラウザ連携は未移植です。選択コマンドはExecuteへ渡します。")),
		             to_wx(_T("コマンドのヘルプ")), wxOK | wxICON_INFORMATION, this);
	}

	key::StateStore &state_;
	std::vector<key::Entry> rows_;
	key_dialog::Result &result_;
	std::vector<key::Entry> shown_;
	wxChoice *tab_ctrl_ = nullptr;
	wxChoice *sort_ctrl_ = nullptr;
	wxTextCtrl *filter_ctrl_ = nullptr;
	wxCheckBox *show_all_chk_ = nullptr;
	wxCheckBox *migemo_chk_ = nullptr;
	wxCheckBox *confirm_chk_ = nullptr;
	wxListCtrl *list_ = nullptr;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, key::StateStore &state,
         const std::vector<key::Entry> &rows, Result &result)
{
	KeyListDialog dlg(parent, state, rows, result);
	dlg.ShowModal();
	return result.accepted;
}

}  // namespace key_dialog
