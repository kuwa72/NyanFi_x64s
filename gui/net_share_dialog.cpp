/**
 * @file gui/net_share_dialog.cpp
 * @brief gui/net_share_dialog.h の実装
 */
#include "gui/net_share_dialog.h"

#include <wx/clipbrd.h>
#include <wx/listbox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace net_share_dialog {

namespace {

inline wxString to_wx(const UnicodeString &value)
{
	return wxString(value.c_str(), static_cast<size_t>(value.Length()));
}

inline UnicodeString to_us(const wxString &value)
{
	return UnicodeString(value.wc_str());
}

/// VCL ShareDlg.dfm のコンピュータ名・絞り込み・共有一覧・操作ボタン相当。
class NetShareDialog final : public wxDialog {
public:
	NetShareDialog(wxWindow *parent, const Context &context)
		: wxDialog(parent, wxID_ANY, to_wx(_T("共有フォルダ一覧")),
		           wxDefaultPosition, wxSize(620, 430),
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
		  context_(context)
	{
		const UnicodeString caption = net_share::FormatComputerCaption(context.computer);
		if (!caption.IsEmpty()) SetTitle(to_wx(caption + _T(" - 共有フォルダ一覧")));

		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
		wxFlexGridSizer *grid = new wxFlexGridSizer(2, 4, 8);
		grid->AddGrowableCol(1);

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("コンピュータ"))),
		          wxSizerFlags().CentreVertical());
		computer_ctrl_ = new wxTextCtrl(this, wxID_ANY, to_wx(context.computer),
		                                 wxDefaultPosition, wxSize(360, -1));
		grid->Add(computer_ctrl_, wxSizerFlags(1).Expand());

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("絞り込み"))),
		          wxSizerFlags().CentreVertical());
		filter_ctrl_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
		                               wxDefaultPosition, wxSize(360, -1));
		grid->Add(filter_ctrl_, wxSizerFlags(1).Expand());
		top->Add(grid, wxSizerFlags().Expand().Border(wxALL, 8));

		status_ctrl_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		status_ctrl_->Wrap(580);
		top->Add(status_ctrl_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));

		list_ctrl_ = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(580, 260));
		top->Add(list_ctrl_, wxSizerFlags(1).Expand().Border(wxALL, 8));

		wxBoxSizer *copy_box = new wxBoxSizer(wxHORIZONTAL);
		copy_selected_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("選択UNCをコピー")));
		copy_all_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("すべてのUNCをコピー")));
		copy_box->Add(copy_selected_btn_);
		copy_box->Add(copy_all_btn_, wxSizerFlags().Border(wxLEFT, 8));
		copy_box->AddStretchSpacer();
		copy_box->Add(new wxButton(this, wxID_REFRESH, to_wx(_T("更新"))));
		open_btn_ = new wxButton(this, wxID_OK, to_wx(_T("開く")));
		copy_box->Add(open_btn_, wxSizerFlags().Border(wxLEFT, 8));
		copy_box->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("閉じる"))),
		              wxSizerFlags().Border(wxLEFT, 8));
		top->Add(copy_box, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		SetSizerAndFit(top);
		SetMinSize(wxSize(560, 360));
		CentreOnParent();

		filter_ctrl_->Bind(wxEVT_TEXT, &NetShareDialog::OnFilterChanged, this);
		list_ctrl_->Bind(wxEVT_LISTBOX, &NetShareDialog::OnSelectionChanged, this);
		list_ctrl_->Bind(wxEVT_LISTBOX_DCLICK, &NetShareDialog::OnOpen, this);
		copy_selected_btn_->Bind(wxEVT_BUTTON, &NetShareDialog::OnCopySelected, this);
		copy_all_btn_->Bind(wxEVT_BUTTON, &NetShareDialog::OnCopyAll, this);
		Bind(wxEVT_BUTTON, &NetShareDialog::OnOpen, this, wxID_OK);
		Bind(wxEVT_BUTTON, &NetShareDialog::OnRefresh, this, wxID_REFRESH);

		UpdateList();
		filter_ctrl_->SetFocus();
	}

	UnicodeString SelectedPath() const { return selected_path_; }

private:
	void SetStatus(const UnicodeString &text)
	{
		status_ctrl_->SetLabel(to_wx(text));
		status_ctrl_->Wrap(580);
		Layout();
	}

	void UpdateList()
	{
		visible_.clear();
		list_ctrl_->Clear();
		copy_selected_btn_->Enable(false);
		copy_all_btn_->Enable(false);
		open_btn_->Enable(false);

		const net_share::ValidationResult input =
			net_share::NormalizeComputer(to_us(computer_ctrl_->GetValue()));
		if (!input.ok) {
			SetStatus(_T("入力エラー: ") + input.error);
			return;
		}

		const net_share::ValidationResult initial =
			net_share::NormalizeComputer(context_.computer);
		if (!initial.ok || !SameText(initial.value, input.value)) {
			SetStatus(_T("未移植 (未実装扱い): リモート共有の列挙・接続"
			            " (NetShareEnum/WNetAddConnection3)"));
			return;
		}

		visible_ = net_share::SortFilterShares(context_.shares, to_us(filter_ctrl_->GetValue()));
		for (const net_share::ShareItem &item : visible_) {
			const UnicodeString row = net_share::FormatShareRow(input.value, item);
			if (!row.IsEmpty()) list_ctrl_->Append(to_wx(row));
		}

		const bool has_items = !visible_.empty();
		open_btn_->Enable(has_items);
		copy_all_btn_->Enable(has_items);
		if (has_items) {
			list_ctrl_->SetSelection(0);
			copy_selected_btn_->Enable(true);
		}
		if (!context_.warning.IsEmpty()) {
			SetStatus(context_.warning);
		}
		else if (has_items) {
			UnicodeString text;
			text.sprintf(_T("共有 %d 件"), static_cast<int>(visible_.size()));
			SetStatus(text);
		}
		else {
			SetStatus(_T("表示できる共有がありません"));
		}
	}

	void OnFilterChanged(wxCommandEvent &event)
	{
		event.Skip();
		UpdateList();
	}

	void OnRefresh(wxCommandEvent &event)
	{
		event.Skip();
		UpdateList();
	}

	void OnSelectionChanged(wxCommandEvent &event)
	{
		event.Skip();
		copy_selected_btn_->Enable(list_ctrl_->GetSelection() != wxNOT_FOUND);
	}

	void OnOpen(wxCommandEvent &event)
	{
		event.Skip();
		const int index = list_ctrl_->GetSelection();
		if (index < 0 || index >= static_cast<int>(visible_.size())) {
			wxMessageBox(to_wx(_T("共有を選んでください")), to_wx(_T("共有フォルダ一覧")),
			             wxOK | wxICON_WARNING, this);
			return;
		}

		const net_share::ValidationResult computer =
			net_share::NormalizeComputer(to_us(computer_ctrl_->GetValue()));
		if (!computer.ok) {
			wxMessageBox(to_wx(computer.error), to_wx(_T("共有フォルダ一覧")),
			             wxOK | wxICON_WARNING, this);
			return;
		}
		selected_path_ = net_share::MakeUncSharePath(computer.value, visible_[static_cast<std::size_t>(index)].name);
		const net_share::ValidationResult path = net_share::NormalizeUncPath(selected_path_);
		if (!path.ok) {
			wxMessageBox(to_wx(path.error), to_wx(_T("共有フォルダ一覧")),
			             wxOK | wxICON_WARNING, this);
			return;
		}
		selected_path_ = path.value;
		EndModal(wxID_OK);
	}

	void CopyText(const UnicodeString &text)
	{
		if (text.IsEmpty() || !wxTheClipboard->Open()) return;
		wxTextDataObject data;
		data.SetText(to_wx(text));
		wxTheClipboard->SetData(&data);
		wxTheClipboard->Close();
	}

	void OnCopySelected(wxCommandEvent &event)
	{
		event.Skip();
		const int index = list_ctrl_->GetSelection();
		if (index < 0 || index >= static_cast<int>(visible_.size())) return;
		const net_share::ValidationResult computer =
			net_share::NormalizeComputer(to_us(computer_ctrl_->GetValue()));
		if (!computer.ok) return;
		CopyText(net_share::MakeUncSharePath(computer.value,
		                                     visible_[static_cast<std::size_t>(index)].name));
	}

	void OnCopyAll(wxCommandEvent &event)
	{
		event.Skip();
		const net_share::ValidationResult computer =
			net_share::NormalizeComputer(to_us(computer_ctrl_->GetValue()));
		if (!computer.ok) return;
		UnicodeString text;
		for (const net_share::ShareItem &item : visible_) {
			if (!text.IsEmpty()) text += _T("\r\n");
			text += net_share::MakeUncSharePath(computer.value, item.name);
		}
		CopyText(text);
	}

	Context context_;
	std::vector<net_share::ShareItem> visible_;
	wxTextCtrl *computer_ctrl_ = nullptr;
	wxTextCtrl *filter_ctrl_ = nullptr;
	wxStaticText *status_ctrl_ = nullptr;
	wxListBox *list_ctrl_ = nullptr;
	wxButton *copy_selected_btn_ = nullptr;
	wxButton *copy_all_btn_ = nullptr;
	wxButton *open_btn_ = nullptr;
	UnicodeString selected_path_;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, const Context &context, UnicodeString &selected_path_out)
{
	NetShareDialog dialog(parent, context);
	if (dialog.ShowModal() != wxID_OK) return false;
	const net_share::ValidationResult path = net_share::NormalizeUncPath(dialog.SelectedPath());
	if (!path.ok) return false;
	selected_path_out = path.value;
	return true;
}

}  // namespace net_share_dialog
