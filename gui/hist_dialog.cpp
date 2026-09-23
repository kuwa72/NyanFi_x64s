/**
 * @file gui/hist_dialog.cpp
 * @brief gui/hist_dialog.h の実装
 */
#include "gui/hist_dialog.h"

#include <wx/clipbrd.h>
#include <wx/dirdlg.h>
#include <wx/listbox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "gui/file_info_panel.h"
#include "gui/file_item.h"
#include "usr_file_ex.h"
#include "usr_str.h"

namespace hist_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

std::vector<UnicodeString> collect_directories(const UnicodeString &root)
{
	std::vector<UnicodeString> out;
	if (root.IsEmpty() || !dir_exists(root)) return out;

	const UnicodeString base = IncludeTrailingPathDelimiter(root);
	out.push_back(base);
	TSearchRec rec;
	if (FindFirst(base + _T("*"), faDirectory, rec) != 0) return out;
	do {
		if (SameText(rec.Name, _T(".")) || SameText(rec.Name, _T(".."))) continue;
		if ((rec.Attr & faDirectory) == 0) continue;
		const UnicodeString child = base + rec.Name;
		const std::vector<UnicodeString> descendants = collect_directories(child);
		out.insert(out.end(), descendants.begin(), descendants.end());
	} while (FindNext(rec) == 0);
	FindClose(rec);
	return out;
}

class HistoryDialog : public wxDialog {
public:
	HistoryDialog(wxWindow *parent, hist::State &state, hist::Result &result)
		: wxDialog(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(520, 430),
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, state_(state)
		, result_(result)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		if (hist::UsesFilter(state_.mode)) {
			wxBoxSizer *filter_row = new wxBoxSizer(wxHORIZONTAL);
			filter_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("検索"))),
			                wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
			filter_ctrl_ = new wxTextCtrl(this, wxID_ANY, to_wx(state_.filter),
			                              wxDefaultPosition, wxSize(300, -1));
			filter_row->Add(filter_ctrl_, wxSizerFlags(1).Expand().Border(wxRIGHT, 8));
			migemo_ctrl_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("Migemo")));
			migemo_ctrl_->SetValue(state_.migemo);
			filter_row->Add(migemo_ctrl_, wxSizerFlags().CentreVertical());
			top->Add(filter_row, wxSizerFlags().Expand().Border(wxALL, 8));
			filter_ctrl_->Bind(wxEVT_TEXT, &HistoryDialog::OnFilterChanged, this);
			migemo_ctrl_->Bind(wxEVT_CHECKBOX, &HistoryDialog::OnFilterChanged, this);
		}

		list_ = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(490, 300));
		top->Add(list_, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		list_->Bind(wxEVT_LISTBOX_DCLICK, &HistoryDialog::OnOk, this);

		wxBoxSizer *actions = new wxBoxSizer(wxHORIZONTAL);
		if (state_.mode == hist::Mode::Search) {
			add_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("ディレクトリを追加...")));
			actions->Add(add_btn_, wxSizerFlags().Border(wxRIGHT, 4));
			add_btn_->Bind(wxEVT_BUTTON, &HistoryDialog::OnAddDirectories, this);
		}
		clear_all_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("すべて削除")));
		actions->Add(clear_all_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		clear_all_btn_->Bind(wxEVT_BUTTON, &HistoryDialog::OnClearAll, this);

		delete_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("選択項目を削除")));
		actions->Add(delete_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		delete_btn_->Bind(wxEVT_BUTTON, &HistoryDialog::OnDelete, this);

		clear_filter_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("絞り込み中を削除")));
		actions->Add(clear_filter_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		clear_filter_btn_->Bind(wxEVT_BUTTON, &HistoryDialog::OnClearFiltered, this);

		copy_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("コピー")));
		actions->Add(copy_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		copy_btn_->Bind(wxEVT_BUTTON, &HistoryDialog::OnCopy, this);

		property_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("プロパティ")));
		actions->Add(property_btn_);
		property_btn_->Bind(wxEVT_BUTTON, &HistoryDialog::OnProperty, this);
		top->Add(actions, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		top->Add(new wxStaticText(this, wxID_ANY,
		                          to_wx(_T("※ VCLのRecentシェル列挙・Migemo辞書・登録名変換・")
		                                   _T("ワークリスト実読込・位置保存は未実装扱い。"))),
		         wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));
		SetSizerAndFit(top);
		Bind(wxEVT_BUTTON, &HistoryDialog::OnOk, this, wxID_OK);

		RefreshList();
		SetTitle(to_wx(hist::ModeTitle(state_.mode,
		                               static_cast<int>(view_.visible.size()),
		                               static_cast<int>(state_.entries.size()))));
		if (filter_ctrl_ != nullptr) filter_ctrl_->SetFocus();
		else if (list_->GetCount() > 0) list_->SetSelection(0);
		list_->SetFocus();
	}

private:
	void RefreshList()
	{
		const int old = list_->GetSelection();
		view_ = hist::BuildView(state_, contains_upper(ToFilter()));
		list_->Clear();
		for (std::size_t i = 0; i < view_.visible.size(); ++i) {
			const hist::Entry &entry = state_.entries[view_.visible[i]];
			const int display_index = static_cast<int>(i);
			const UnicodeString text = hist::UsesFilter(state_.mode)
				? hist::DisplayPath(entry)
				: hist::DisplayWithAccelerator(entry, display_index);
			list_->Append(to_wx(text));
		}
		if (old >= 0 && old < list_->GetCount()) list_->SetSelection(old);
		else if (list_->GetCount() > 0) list_->SetSelection(0);
		UpdateButtons();
		SetTitle(to_wx(hist::ModeTitle(state_.mode,
		                               static_cast<int>(view_.visible.size()),
		                               static_cast<int>(state_.entries.size()))));
	}

	UnicodeString ToFilter() const
	{
		return filter_ctrl_ == nullptr ? EmptyStr : to_us(filter_ctrl_->GetValue()).Trim();
	}

	int SelectedOriginal() const
	{
		const int selected = list_->GetSelection();
		if (selected < 0 || selected >= static_cast<int>(view_.visible.size())) return -1;
		return static_cast<int>(view_.visible[static_cast<std::size_t>(selected)]);
	}

	void UpdateButtons()
	{
		const int selected = SelectedOriginal();
		const int count = static_cast<int>(view_.visible.size());
		clear_all_btn_->Enable(hist::CanClearAll(state_.mode, static_cast<int>(state_.entries.size())));
		delete_btn_->Enable(selected >= 0 && state_.mode != hist::Mode::Stack);
		clear_filter_btn_->Enable(hist::CanClearFiltered(
			state_.mode, count, static_cast<int>(state_.entries.size())));
		copy_btn_->Enable(hist::CanCopy(count));
		property_btn_->Enable(hist::CanProperty(selected));
		if (add_btn_ != nullptr) add_btn_->Enable(!state_.entries.empty() || state_.mode == hist::Mode::Search);
	}

	void OnFilterChanged(wxCommandEvent &)
	{
		state_.filter = ToFilter();
		state_.migemo = migemo_ctrl_ != nullptr && migemo_ctrl_->GetValue();
		RefreshList();
	}

	void OnAddDirectories(wxCommandEvent &)
	{
		wxDirDialog dlg(this, to_wx(_T("全体履歴に追加するディレクトリを選択")), wxEmptyString);
		if (dlg.ShowModal() != wxID_OK) return;
		const std::vector<UnicodeString> dirs = collect_directories(to_us(dlg.GetPath()));
		hist::MergeDirectories(state_.entries, dirs);
		RefreshList();
	}

	void OnClearAll(wxCommandEvent &)
	{
		if (wxMessageBox(to_wx(_T("表示中の履歴をすべて削除しますか?")),
		                 to_wx(_T("履歴の削除")), wxYES_NO | wxICON_QUESTION, this) != wxYES) return;
		state_.entries.clear();
		result_.clear_all = true;
		RefreshList();
	}

	void OnDelete(wxCommandEvent &)
	{
		const int index = SelectedOriginal();
		if (index < 0) return;
		if (wxMessageBox(to_wx(_T("選択項目を履歴から削除しますか?")),
		                 to_wx(_T("履歴の削除")), wxYES_NO | wxICON_QUESTION, this) != wxYES) return;
		if (index < static_cast<int>(state_.entries.size())) {
			result_.deleted_path = hist::DisplayPath(state_.entries[static_cast<std::size_t>(index)]);
			state_.entries.erase(state_.entries.begin() + index);
		}
		result_.delete_selected = true;
		RefreshList();
	}

	void OnClearFiltered(wxCommandEvent &)
	{
		if (wxMessageBox(to_wx(_T("絞り込み表示中の履歴を削除しますか?")),
		                 to_wx(_T("履歴の削除")), wxYES_NO | wxICON_QUESTION, this) != wxYES) return;
		hist::RemoveVisible(state_.entries, view_.visible);
		result_.clear_filtered = true;
		state_.filter = EmptyStr;
		if (filter_ctrl_ != nullptr) filter_ctrl_->ChangeValue(to_wx(state_.filter));
		RefreshList();
	}

	void OnCopy(wxCommandEvent &)
	{
		const std::vector<UnicodeString> lines = hist::CopyLines(state_, view_);
		if (lines.empty() || !wxTheClipboard->Open()) return;
		UnicodeString text;
		for (std::size_t i = 0; i < lines.size(); ++i) {
			if (i > 0) text += _T("\r\n");
			text += lines[i];
		}
		wxTheClipboard->SetData(new wxTextDataObject(to_wx(text)));
		wxTheClipboard->Close();
		result_.copy = true;
	}

	void OnProperty(wxCommandEvent &)
	{
		const int index = SelectedOriginal();
		if (index < 0) return;
		const hist::Entry &entry = state_.entries[static_cast<std::size_t>(index)];
		const UnicodeString path = hist::DisplayPath(entry);
		FileItem item;
		item.name = ExtractFileName(ExcludeTrailingPathDelimiter(path));
		item.full_path = path;
		item.is_dir = dir_exists(path);
		ShowFileInfoDialog(this, path, item);
		result_.property = true;
	}

	void OnOk(wxCommandEvent &event)
	{
		const int index = SelectedOriginal();
		if (index >= 0) {
			result_.selected = index;
			result_.accepted = true;
		}
		EndModal(wxID_OK);
		if (event.GetEventObject() != this) return;
	}

	hist::State &state_;
	hist::Result &result_;
	hist::View view_;
	wxTextCtrl *filter_ctrl_ = nullptr;
	wxCheckBox *migemo_ctrl_ = nullptr;
	wxListBox *list_ = nullptr;
	wxButton *add_btn_ = nullptr;
	wxButton *clear_all_btn_ = nullptr;
	wxButton *delete_btn_ = nullptr;
	wxButton *clear_filter_btn_ = nullptr;
	wxButton *copy_btn_ = nullptr;
	wxButton *property_btn_ = nullptr;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, hist::State &state, hist::Result &result)
{
	// 同じ MainFrame インスタンスで再度開いたときに前回の操作結果を
	// 誤って再利用しない。削除/クリアはダイアログ内で即時に state へ反映する。
	result = hist::Result{};
	HistoryDialog dlg(parent, state, result);
	dlg.ShowModal();
	return result.accepted;
}

}  // namespace hist_dialog
