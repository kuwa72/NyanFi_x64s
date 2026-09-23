/**
 * @file gui/cmd_list_dialog.cpp
 * @brief gui/cmd_list_dialog.h の実装
 */
#include "gui/cmd_list_dialog.h"

#include <algorithm>
#include <memory>

#include <wx/checkbox.h>
#include <wx/listctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "usr_file_ex.h"
#include "usr_file_inf.h"
#include "usr_str.h"
#include "gui/text_viewer_core.h"

namespace cmd_list_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<std::size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

class CommandFileListDialog final : public wxDialog {
public:
	CommandFileListDialog(wxWindow *parent, const std::vector<cmd_list::Entry> &entries,
	                      const cmd_list::Options &options)
		: wxDialog(parent, wxID_ANY,
		           to_wx(options.select_only? _T("コマンドファイルの選択") : _T("コマンドファイル一覧")),
		           wxDefaultPosition, wxDefaultSize,
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, entries_(entries)
		, options_(options)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
		wxBoxSizer *filter_row = new wxBoxSizer(wxHORIZONTAL);
		filter_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("フィルタ"))),
		                wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		filter_edit_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(260, -1));
		filter_row->Add(filter_edit_, wxSizerFlags(1).Expand().Border(wxRIGHT, 8));
		migemo_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("Migemo")));
		preview_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("プレビュー")));
		preview_chk_->SetValue(options_.preview);
		filter_row->Add(migemo_chk_, wxSizerFlags().Border(wxRIGHT, 8));
		filter_row->Add(preview_chk_);
		top->Add(filter_row, wxSizerFlags().Expand().Border(wxALL, 8));
		filter_edit_->Bind(wxEVT_TEXT, &CommandFileListDialog::OnFilter, this);
		migemo_chk_->Bind(wxEVT_CHECKBOX, &CommandFileListDialog::OnFilter, this);
		preview_chk_->Bind(wxEVT_CHECKBOX, &CommandFileListDialog::OnPreviewToggle, this);

		list_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(700, 230),
		                        wxLC_REPORT | wxLC_SINGLE_SEL);
		list_->InsertColumn(0, to_wx(_T("ファイル名")), wxLIST_FORMAT_LEFT, 180);
		list_->InsertColumn(1, to_wx(_T("説明")), wxLIST_FORMAT_LEFT, 180);
		list_->InsertColumn(2, to_wx(_T("サイズ")), wxLIST_FORMAT_RIGHT, 90);
		list_->InsertColumn(3, to_wx(_T("更新日時")), wxLIST_FORMAT_LEFT, 145);
		list_->InsertColumn(4, to_wx(_T("ディレクトリ")), wxLIST_FORMAT_LEFT, 180);
		top->Add(list_, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT, 8));
		list_->Bind(wxEVT_LIST_ITEM_SELECTED, &CommandFileListDialog::OnSelect, this);
		list_->Bind(wxEVT_LIST_ITEM_ACTIVATED, &CommandFileListDialog::OnActivate, this);

		preview_view_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                                wxSize(700, 130), wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
		top->Add(preview_view_, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT, 8));
		status_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		top->Add(status_, wxSizerFlags().Expand().Border(wxALL, 8));

		wxBoxSizer *buttons = new wxBoxSizer(wxHORIZONTAL);
		edit_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("編集")));
		buttons->Add(edit_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		buttons->AddStretchSpacer();
		if (options_.select_only) {
			buttons->Add(new wxButton(this, wxID_OK, to_wx(_T("選択"))), wxSizerFlags().Border(wxRIGHT, 4));
		}
		else {
			buttons->Add(new wxButton(this, wxID_OK, to_wx(_T("実行"))), wxSizerFlags().Border(wxRIGHT, 4));
		}
		buttons->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("キャンセル"))));
		top->Add(buttons, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		edit_btn_->Bind(wxEVT_BUTTON, &CommandFileListDialog::OnEdit, this);
		Bind(wxEVT_BUTTON, &CommandFileListDialog::OnOk, this, wxID_OK);

		SetSizerAndFit(top);
		CentreOnParent();
		RefreshList();
		const int initial = cmd_list::FindSelected(entries_, options_.initial_file);
		if (initial >= 0) SelectIndex(initial);
		if (options_.to_filter) {
			filter_edit_->SetFocus();
			filter_edit_->SelectAll();
		}
		else {
			list_->SetFocus();
		}
	}

	cmd_list::Result ResultValue() const { return result_; }

private:
	void RefreshList()
	{
		cmd_list::FilterOptions filter;
		filter.fuzzy = migemo_chk_->GetValue();
		shown_ = cmd_list::FilterEntries(entries_, to_us(filter_edit_->GetValue()), filter);
		const UnicodeString keep = SelectedPath();
		list_->DeleteAllItems();
		for (std::size_t i = 0; i < shown_.size(); ++i) {
			const cmd_list::Entry &entry = shown_[i];
			const UnicodeString stamp = DateTimeToStr(get_file_age(entry.path));
			const long row = list_->InsertItem(static_cast<long>(i), to_wx(entry.name));
			list_->SetItem(row, 1, to_wx(entry.description));
			list_->SetItem(row, 2, to_wx(get_size_str_G(entry.size, 8, 1)));
			list_->SetItem(row, 3, to_wx(stamp));
			list_->SetItem(row, 4, to_wx(ExtractFilePath(ExcludeTrailingPathDelimiter(entry.path))));
		}
		status_->SetLabel(to_wx(UnicodeString().sprintf(_T("%u / %u 件"), static_cast<unsigned>(shown_.size()),
		                                                   static_cast<unsigned>(entries_.size()))));
		const int selected = cmd_list::FindSelected(shown_, keep);
		if (selected >= 0) SelectIndex(selected);
		else if (!shown_.empty()) SelectIndex(0);
		UpdateButtons();
	}

	UnicodeString SelectedPath() const
	{
		const int row = list_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (row < 0 || row >= static_cast<int>(shown_.size())) return EmptyStr;
		return shown_[static_cast<std::size_t>(row)].path;
	}

	void SelectIndex(int index)
	{
		if (index < 0 || index >= static_cast<int>(shown_.size())) return;
		list_->SetItemState(static_cast<long>(index), wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
		                    wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
		UpdateButtons();
		RefreshPreview();
	}

	void UpdateButtons()
	{
		const bool has = list_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) >= 0;
		edit_btn_->Enable(has && !options_.select_only);
		preview_view_->Show(preview_chk_->GetValue());
		Layout();
	}

	void OnFilter(wxCommandEvent &event)
	{
		RefreshList();
		if (cmd_list::ShouldAutoExecute(options_.select_only, options_.confirm_execute,
		                                filter_edit_->HasFocus(), to_us(filter_edit_->GetValue()).IsEmpty(),
		                                static_cast<int>(shown_.size()))) {
			// VCL は ProcessMessages/Sleep 後に自動実行するが、ここでは
			// 入力確定時点で同じ操作を確定する。
			Finish(cmd_list::Action::Execute);
			return;
		}
		event.Skip();
	}
	void OnPreviewToggle(wxCommandEvent &) { UpdateButtons(); RefreshPreview(); }
	void OnSelect(wxCommandEvent &) { UpdateButtons(); RefreshPreview(); }
	void OnActivate(wxCommandEvent &) { Finish(options_.select_only? cmd_list::Action::None : cmd_list::Action::Execute); }
	void OnEdit(wxCommandEvent &) { Finish(cmd_list::Action::Edit); }
	void OnOk(wxCommandEvent &event) { Finish(options_.select_only? cmd_list::Action::None : cmd_list::Action::Execute); event.Skip(); }

	void RefreshPreview()
	{
		if (!preview_chk_->GetValue()) return;
		const UnicodeString path = SelectedPath();
		if (path.IsEmpty()) {
			preview_view_->Clear();
			return;
		}
		try {
			const text_viewer_core::LoadResult loaded = text_viewer_core::LoadForView(path);
			if (!loaded.ok || loaded.is_binary) {
				preview_view_->SetValue(to_wx(_T("プレビューを読み込めません")));
				return;
			}
			UnicodeString text;
			for (const UnicodeString &line : loaded.lines) {
				if (!text.IsEmpty()) text += _T("\r\n");
				text += line;
			}
			preview_view_->SetValue(to_wx(text));
		}
		catch (...) {
			preview_view_->SetValue(to_wx(_T("プレビューを読み込めません")));
		}
	}

	void Finish(cmd_list::Action action)
	{
		const UnicodeString path = SelectedPath();
		if (path.IsEmpty()) {
			wxMessageBox(to_wx(_T("コマンドファイルを選んでください")), to_wx(_T("コマンドファイル一覧")),
			             wxOK | wxICON_INFORMATION, this);
			return;
		}
		result_.path = path;
		result_.action = action;
		result_.preview = preview_chk_->GetValue();
		result_.confirm_execute = options_.confirm_execute;
		result_.fuzzy = migemo_chk_->GetValue();
		EndModal(wxID_OK);
	}

	std::vector<cmd_list::Entry> entries_;
	std::vector<cmd_list::Entry> shown_;
	cmd_list::Options options_;
	cmd_list::Result result_;
	wxTextCtrl *filter_edit_ = nullptr;
	wxCheckBox *migemo_chk_ = nullptr;
	wxCheckBox *preview_chk_ = nullptr;
	wxListCtrl *list_ = nullptr;
	wxTextCtrl *preview_view_ = nullptr;
	wxStaticText *status_ = nullptr;
	wxButton *edit_btn_ = nullptr;
};

}  // namespace

//---------------------------------------------------------------------------
std::vector<cmd_list::Entry> Enumerate(const UnicodeString &exe_path)
{
	std::vector<cmd_list::Entry> result;
	std::unique_ptr<TStringList> files(new TStringList());
	get_files(exe_path, _T("*.nbt"), files.get(), true);
	for (int i = 0; i < files->Count; ++i) {
		const UnicodeString path = files->Strings[i];
		if (!cmd_list::IsCommandFile(path)) continue;
		cmd_list::Entry entry;
		entry.path = path;
		entry.name = ExtractFileName(path);
		entry.size = get_file_size(path);
		try {
			entry.description = Trim(get_top_line(path));
			if (StartsStr(_T(";"), entry.description)) entry.description.Delete(1, 1);
		}
		catch (...) {
			entry.description = EmptyStr;
		}
		result.push_back(entry);
	}
	std::sort(result.begin(), result.end(), [](const cmd_list::Entry &a, const cmd_list::Entry &b) {
		return cmd_list::CompareNatural(a, b) < 0;
	});
	return result;
}

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, const std::vector<cmd_list::Entry> &entries,
         const cmd_list::Options &options, cmd_list::Result &out)
{
	CommandFileListDialog dlg(parent, entries, options);
	if (dlg.ShowModal() != wxID_OK) return false;
	out = dlg.ResultValue();
	return true;
}

}  // namespace cmd_list_dialog
