/**
 * @file gui/btn_dialog.cpp
 * @brief gui/btn_dialog.h の実装
 */
#include "gui/btn_dialog.h"

#include <algorithm>

#include <wx/combobox.h>
#include <wx/filedlg.h>
#include <wx/listbox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace btn_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

class ToolButtonDialog : public wxDialog {
public:
	ToolButtonDialog(wxWindow *parent, btn::Mode mode,
	                 const std::vector<UnicodeString> &commands,
	                 std::vector<btn::Item> &items, int initial_index)
		: wxDialog(parent, wxID_ANY, to_wx(_T("ツールバーの設定")), wxDefaultPosition,
		           wxSize(700, 540), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, mode_(mode)
		, work_(items)  // 作業コピー。キャンセル時は caller に反映しない
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
		list_ = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(660, 300));
		top->Add(list_, wxSizerFlags(1).Expand().Border(wxALL, 8));
		list_->Bind(wxEVT_LISTBOX, &ToolButtonDialog::OnSelect, this);
		list_->Bind(wxEVT_LISTBOX_DCLICK, &ToolButtonDialog::OnChange, this);

		wxFlexGridSizer *grid = new wxFlexGridSizer(3, 6, 4);
		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("表示名"))), wxSizerFlags().CentreVertical());
		caption_ctrl_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(150, -1));
		grid->Add(caption_ctrl_, wxSizerFlags(1).Expand());
		grid->Add(new wxStaticText(this, wxID_ANY, wxEmptyString), wxSizerFlags().CentreVertical());

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("コマンド"))), wxSizerFlags().CentreVertical());
		command_ctrl_ = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(390, -1));
		for (const UnicodeString &command : commands) command_ctrl_->Append(to_wx(command));
		grid->Add(command_ctrl_, wxSizerFlags(1).Expand());
		ref_command_btn_ = new wxButton(this, wxID_ANY, _T("..."));
		grid->Add(ref_command_btn_, wxSizerFlags().CentreVertical());
		ref_command_btn_->Bind(wxEVT_BUTTON, &ToolButtonDialog::OnReferenceCommand, this);

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("アイコン"))), wxSizerFlags().CentreVertical());
		icon_ctrl_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(390, -1));
		grid->Add(icon_ctrl_, wxSizerFlags(1).Expand());
		ref_icon_btn_ = new wxButton(this, wxID_ANY, _T("..."));
		grid->Add(ref_icon_btn_, wxSizerFlags().CentreVertical());
		ref_icon_btn_->Bind(wxEVT_BUTTON, &ToolButtonDialog::OnReferenceIcon, this);
		top->Add(grid, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxBoxSizer *actions = new wxBoxSizer(wxHORIZONTAL);
		add_btn_ = MakeButton(_T("追加"), &ToolButtonDialog::OnAdd);
		insert_btn_ = MakeButton(_T("挿入"), &ToolButtonDialog::OnInsert);
		change_btn_ = MakeButton(_T("変更"), &ToolButtonDialog::OnChange);
		delete_btn_ = MakeButton(_T("削除"), &ToolButtonDialog::OnDelete);
		up_btn_ = MakeButton(_T("上へ"), &ToolButtonDialog::OnUp);
		down_btn_ = MakeButton(_T("下へ"), &ToolButtonDialog::OnDown);
		edit_file_btn_ = MakeButton(_T("ファイル編集"), &ToolButtonDialog::OnEditFile);
		actions->Add(add_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(insert_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(change_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(delete_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(up_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(down_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(edit_file_btn_);
		top->Add(actions, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		top->Add(new wxStaticText(this, wxID_ANY,
		                          to_wx(_T("※ VCLのドラッグ&ドロップ・アイコン描画・"
		                                   _T("コマンドファイル参照・ExtTool/Menu別名解決は未実装扱い。")))),
		         wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));
		SetSizerAndFit(top);
		Bind(wxEVT_BUTTON, &ToolButtonDialog::OnOk, this, wxID_OK);

		RefreshList();
		if (initial_index >= 0 && initial_index < static_cast<int>(work_.size())) {
			list_->SetSelection(initial_index);
			SyncSelection();
		}
	}

	const std::vector<btn::Item> &WorkItems() const { return work_; }

private:
	template <typename Handler>
	wxButton *MakeButton(const wxString &caption, Handler handler)
	{
		wxButton *button = new wxButton(this, wxID_ANY, caption);
		button->Bind(wxEVT_BUTTON, handler, this);
		return button;
	}

	btn::Item CurrentItem() const
	{
		btn::Item item;
		item.caption = to_us(caption_ctrl_->GetValue());
		item.command = to_us(command_ctrl_->GetValue());
		item.icon = to_us(icon_ctrl_->GetValue());
		return item;
	}

	void RefreshList()
	{
		const int old = list_->GetSelection();
		list_->Clear();
		for (const btn::Item &item : work_) {
			UnicodeString row;
			if (btn::IsSeparator(item)) row = _T("--------------------");
			else row = item.caption + _T("\t") + item.command;
			list_->Append(to_wx(row));
		}
		if (old >= 0 && old < list_->GetCount()) list_->SetSelection(old);
		UpdateButtons();
	}

	void OnSelect(wxCommandEvent &)
	{
		SyncSelection();
	}

	void SyncSelection()
	{
		const int index = list_->GetSelection();
		if (index < 0 || index >= static_cast<int>(work_.size())) return;
		const btn::Item &item = work_[static_cast<std::size_t>(index)];
		caption_ctrl_->ChangeValue(to_wx(item.caption));
		command_ctrl_->ChangeValue(to_wx(item.command));
		icon_ctrl_->ChangeValue(to_wx(item.icon));
		UpdateButtons();
	}

	void UpdateButtons()
	{
		const int index = list_->GetSelection();
		const btn::Item current = CurrentItem();
		add_btn_->Enable(btn::CanAdd(current));
		insert_btn_->Enable(btn::CanAdd(current));
		change_btn_->Enable(btn::CanChange(work_, index, current));
		delete_btn_->Enable(index >= 0);
		up_btn_->Enable(index > 0);
		down_btn_->Enable(index >= 0 && index + 1 < static_cast<int>(work_.size()));
		edit_file_btn_->Enable(!to_us(command_ctrl_->GetValue()).IsEmpty());
	}

	void OnAdd(wxCommandEvent &)
	{
		const btn::Item item = CurrentItem();
		if (!btn::CanAdd(item)) return;
		work_.push_back(item);
		RefreshList();
		list_->SetSelection(static_cast<int>(work_.size()) - 1);
		SyncSelection();
	}

	void OnInsert(wxCommandEvent &)
	{
		const btn::Item item = CurrentItem();
		if (!btn::CanAdd(item)) return;
		int index = list_->GetSelection();
		if (index < 0) index = static_cast<int>(work_.size());
		work_.insert(work_.begin() + index, item);
		RefreshList();
		list_->SetSelection(index);
		SyncSelection();
	}

	void OnChange(wxCommandEvent &)
	{
		const int index = list_->GetSelection();
		const btn::Item item = CurrentItem();
		if (!btn::CanChange(work_, index, item)) return;
		work_[static_cast<std::size_t>(index)] = item;
		RefreshList();
		list_->SetSelection(index);
		SyncSelection();
	}

	void OnDelete(wxCommandEvent &)
	{
		const int index = list_->GetSelection();
		if (index < 0 || index >= static_cast<int>(work_.size())) return;
		work_.erase(work_.begin() + index);
		RefreshList();
		if (list_->GetCount() > 0) list_->SetSelection(std::min(index, static_cast<int>(list_->GetCount()) - 1));
		SyncSelection();
	}

	void OnUp(wxCommandEvent &)
	{
		const int index = list_->GetSelection();
		if (btn::Move(work_, index, -1)) {
			RefreshList();
			list_->SetSelection(index - 1);
			SyncSelection();
		}
	}

	void OnDown(wxCommandEvent &)
	{
		const int index = list_->GetSelection();
		if (btn::Move(work_, index, 1)) {
			RefreshList();
			list_->SetSelection(index + 1);
			SyncSelection();
		}
	}

	void OnReferenceCommand(wxCommandEvent &)
	{
		wxMessageBox(to_wx(_T("VCLのコマンド参照ダイアログは未移植です。\n")
		                      _T("一覧から実在コマンドを選んでください。")),
		             to_wx(_T("コマンド参照")), wxOK | wxICON_INFORMATION, this);
	}

	void OnReferenceIcon(wxCommandEvent &)
	{
		wxFileDialog dlg(this, to_wx(_T("アイコンの指定")), wxEmptyString, wxEmptyString,
		                 _T("アイコン (*.ico;*.exe;*.dll;*.lnk)|*.ICO;*.EXE;*.DLL;*.LNK|"
		                    _T("すべて (*.*)|*.*")),
		                 wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() == wxID_OK) icon_ctrl_->ChangeValue(dlg.GetPath());
	}

	void OnEditFile(wxCommandEvent &)
	{
		wxMessageBox(to_wx(_T("VCLの外部エディタ起動は未移植です。コマンドファイルのパスだけ保存します。")),
		             to_wx(_T("ファイル編集")), wxOK | wxICON_INFORMATION, this);
	}

	void OnOk(wxCommandEvent &)
	{
		EndModal(wxID_OK);
	}

	btn::Mode mode_;
	std::vector<btn::Item> work_;
	wxListBox *list_ = nullptr;
	wxTextCtrl *caption_ctrl_ = nullptr;
	wxComboBox *command_ctrl_ = nullptr;
	wxTextCtrl *icon_ctrl_ = nullptr;
	wxButton *add_btn_ = nullptr;
	wxButton *insert_btn_ = nullptr;
	wxButton *change_btn_ = nullptr;
	wxButton *delete_btn_ = nullptr;
	wxButton *up_btn_ = nullptr;
	wxButton *down_btn_ = nullptr;
	wxButton *edit_file_btn_ = nullptr;
	wxButton *ref_command_btn_ = nullptr;
	wxButton *ref_icon_btn_ = nullptr;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, btn::Mode mode,
         const std::vector<UnicodeString> &commands,
         std::vector<btn::Item> &items, int initial_index)
{
	ToolButtonDialog dlg(parent, mode, commands, items, initial_index);
	if (dlg.ShowModal() != wxID_OK) return false;
	items = dlg.WorkItems();
	return true;
}

}  // namespace btn_dialog
