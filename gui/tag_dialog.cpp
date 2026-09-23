/**
 * @file gui/tag_dialog.cpp
 * @brief gui/tag_dialog.h の実装
 */
#include "gui/tag_dialog.h"

#include <functional>
#include <map>
#include <vector>

#include <wx/clipbrd.h>
#include <wx/colordlg.h>
#include <wx/filedlg.h>
#include <wx/file.h>
#include <wx/textdlg.h>

#include "usr_file_ex.h"
#include "usr_tag.h"

namespace tag_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

bool write_utf8(const wxString &path, const UnicodeString &text)
{
	wxFile file;
	if (!file.Create(path, true)) return false;
	const wxString value = to_wx(text);
	const wxCharBuffer utf8 = value.utf8_str();
	return file.Write(utf8.data(), utf8.length());
}

class TagManagerDialog final : public wxDialog {
public:
	TagManagerDialog(wxWindow *parent, TagManager &manager, const tag::Options &options,
	                 const std::vector<UnicodeString> &folder_icons)
		: wxDialog(parent, wxID_ANY, to_wx(tag::Title(options.mode, options.and_match)),
		           wxDefaultPosition, wxDefaultSize,
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
		  manager_(manager),
		  options_(options),
		  folder_icons_(folder_icons)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		input_label_ = new wxStaticText(this, wxID_ANY, to_wx(_T("タグ (; 区切り)")));
		top->Add(input_label_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, 8));
		input_ = new wxTextCtrl(this, wxID_ANY, to_wx(options_.tags),
		                        wxDefaultPosition, wxSize(360, -1));
		top->Add(input_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		list_ = new wxCheckListBox(this, wxID_ANY, wxDefaultPosition, wxSize(430, 240));
		top->Add(list_, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT, 8));

		wxBoxSizer *opts = new wxBoxSizer(wxHORIZONTAL);
		and_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("AND検索")));
		res_link_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("リソースリンクも照合")));
		hide_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("入力欄を隠す")));
		mask_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("選択項目だけ残す")));
		count_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("タグの使用数を表示")));
		opts->Add(and_chk_, wxSizerFlags().Border(wxRIGHT, 10));
		opts->Add(res_link_chk_, wxSizerFlags().Border(wxRIGHT, 10));
		opts->Add(hide_chk_, wxSizerFlags().Border(wxRIGHT, 10));
		opts->Add(mask_chk_, wxSizerFlags().Border(wxRIGHT, 10));
		opts->Add(count_chk_);
		top->Add(opts, wxSizerFlags().Border(wxALL, 8));

		wxBoxSizer *tools = new wxBoxSizer(wxHORIZONTAL);
		rename_btn_ = AddTool(this, tools, _T("タグ名変更(&R)"), [this] { RenameSelected(); });
		delete_btn_ = AddTool(this, tools, _T("タグ削除(&D)"), [this] { DeleteSelected(); });
		color_btn_ = AddTool(this, tools, _T("タグ色(&O)"), [this] { SetSelectedColor(); });
		default_color_btn_ = AddTool(this, tools, _T("既定色"), [this] { SetDefaultColor(); });
		trim_btn_ = AddTool(this, tools, _T("存在しない項目を整理"), [this] { TrimData(); });
		save_nbt_btn_ = AddTool(this, tools, _T("検索コマンドを保存"),
		                        [this] { SaveSearchCommand(false); });
		save_nbt_opp_btn_ = AddTool(this, tools, _T("反対側へ検索"),
		                            [this] { SaveSearchCommand(true); });
		top->Add(tools, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));

		status_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		top->Add(status_, wxSizerFlags().Expand().Border(wxALL, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();
		ConfigureMode();

		RefreshTags();
		SyncInputToChecks();

		input_->Bind(wxEVT_TEXT, &TagManagerDialog::OnInputChanged, this);
		list_->Bind(wxEVT_CHECKLISTBOX, &TagManagerDialog::OnCheckChanged, this);
		list_->Bind(wxEVT_LISTBOX_DCLICK, &TagManagerDialog::OnAccept, this);
		hide_chk_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &) { UpdateInputVisibility(); });
		and_chk_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &event) {
			options_.and_match = and_chk_->GetValue();
			SetTitle(to_wx(tag::Title(options_.mode, options_.and_match)));
			event.Skip();
		});
		res_link_chk_->Bind(wxEVT_CHECKBOX,
		                    [this](wxCommandEvent &) { options_.resolve_links = res_link_chk_->GetValue(); });
		mask_chk_->Bind(wxEVT_CHECKBOX,
		                [this](wxCommandEvent &) { options_.select_mask = mask_chk_->GetValue(); });
		count_chk_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &) {
			options_.show_count = count_chk_->GetValue();
			RefreshTags();
		});
		Bind(wxEVT_BUTTON, &TagManagerDialog::OnAccept, this, wxID_OK);
	}

	tag::Options GetOptions() const { return options_; }

private:
	static wxButton *AddTool(wxWindow *parent, wxSizer *sizer, const UnicodeString &label,
	                         std::function<void()> fn)
	{
		wxButton *button = new wxButton(parent, wxID_ANY, to_wx(label));
		button->Bind(wxEVT_BUTTON, [fn](wxCommandEvent &) { fn(); });
		sizer->Add(button, wxSizerFlags().Border(wxRIGHT, 6));
		return button;
	}

	void ConfigureMode()
	{
		const bool folder = options_.mode == tag::Mode::FolderIcon;
		const bool find = options_.mode == tag::Mode::Find;
		const bool select = options_.mode == tag::Mode::Select;
		const bool edit = options_.mode == tag::Mode::Add || options_.mode == tag::Mode::Set;

		and_chk_->Enable(find || select);
		and_chk_->SetValue(options_.and_match && (find || select));
		res_link_chk_->Enable(find || folder);
		res_link_chk_->SetValue(options_.resolve_links && (find || folder));
		mask_chk_->Enable(select);
		mask_chk_->SetValue(options_.select_mask && select);
		hide_chk_->Enable(!folder);
		hide_chk_->SetValue(options_.hide_input && !folder);
		count_chk_->Enable(!folder);
		count_chk_->SetValue(options_.show_count && !folder);

		input_label_->Show(!folder);
		input_->Show(!folder && !options_.hide_input);
		rename_btn_->Enable(!folder);
		delete_btn_->Enable(!folder);
		color_btn_->Enable(!folder);
		default_color_btn_->Enable(!folder);
		trim_btn_->Enable(!folder);
		save_nbt_btn_->Enable(find);
		save_nbt_opp_btn_->Enable(find);
		(void)edit;
	}

	void UpdateInputVisibility()
	{
		const bool show = !hide_chk_->GetValue();
		input_label_->Show(show);
		input_->Show(show);
		Layout();
		if (show) input_->SetFocus();
	}

	void RefreshTags()
	{
		const std::vector<UnicodeString> old_selected = SelectedTags();
		syncing_ = true;
		list_->Clear();

		if (options_.mode == tag::Mode::FolderIcon) {
			tag_names_ = folder_icons_;
		}
		else if (manager_.TagNameList != nullptr) {
			for (int i = 0; i < manager_.TagNameList->Count; ++i) {
				tag_names_.push_back(manager_.TagNameList->Strings[i]);
			}
		}

		std::map<std::wstring, int> counts;
		if (options_.show_count && manager_.TagDataList != nullptr) {
			for (int i = 0; i < manager_.TagDataList->Count; ++i) {
				UnicodeString rest = get_post_tab(manager_.TagDataList->Strings[i]);
				while (!rest.IsEmpty()) {
					const UnicodeString name = Trim(split_tkn(rest, _T(";")));
					if (!name.IsEmpty()) ++counts[std::wstring(name.c_str(), name.Length())];
				}
			}
		}

		for (std::size_t i = 0; i < tag_names_.size(); ++i) {
			wxString label = to_wx(tag_names_[i]);
			if (options_.show_count) {
				label += wxString::Format(_T("  (%d)"),
				                          counts[std::wstring(tag_names_[i].c_str(), tag_names_[i].Length())]);
			}
			const bool checked = ContainsTag(old_selected, tag_names_[i]);
			const unsigned int index = list_->Append(label);
			list_->Check(index, checked);
		}
		syncing_ = false;
	}

	std::vector<UnicodeString> SelectedTags() const
	{
		std::vector<UnicodeString> out;
		for (unsigned int i = 0; i < list_->GetCount(); ++i) {
			if (list_->IsChecked(i)) out.push_back(tag_names_[i]);
		}
		return out;
	}

	static bool ContainsTag(const std::vector<UnicodeString> &tags, const UnicodeString &tag)
	{
		for (const UnicodeString &item : tags) {
			if (SameText(item, tag)) return true;
		}
		return false;
	}

	void SyncInputToChecks()
	{
		if (options_.mode == tag::Mode::FolderIcon) return;
		syncing_ = true;
		const std::vector<UnicodeString> selected = tag::SplitTags(to_us(input_->GetValue()));
		for (unsigned int i = 0; i < list_->GetCount(); ++i) {
			list_->Check(i, ContainsTag(selected, tag_names_[i]));
		}
		syncing_ = false;
	}

	void SyncChecksToInput()
	{
		if (options_.mode == tag::Mode::FolderIcon) return;
		const bool trailing = EndsStr(_T(";"), to_us(input_->GetValue()));
		std::vector<UnicodeString> selected = SelectedTags();
		for (const UnicodeString &tag : tag::SplitTags(to_us(input_->GetValue()))) {
			if (!ContainsTag(selected, tag) && !ContainsTag(tag_names_, tag)) selected.push_back(tag);
		}
		syncing_ = true;
		input_->ChangeValue(to_wx(tag::JoinTags(selected, trailing)));
		input_->SetInsertionPointEnd();
		syncing_ = false;
	}

	void OnInputChanged(wxCommandEvent &event)
	{
		if (!syncing_) SyncInputToChecks();
		event.Skip();
	}

	void OnCheckChanged(wxCommandEvent &event)
	{
		if (!syncing_) SyncChecksToInput();
		event.Skip();
	}

	void OnAccept(wxCommandEvent &event)
	{
		if (options_.mode != tag::Mode::FolderIcon) SyncChecksToInput();
		options_.tags = options_.mode == tag::Mode::FolderIcon
			? tag::JoinTags(SelectedTags())
			: to_us(input_->GetValue());
		options_.and_match = and_chk_->GetValue();
		options_.resolve_links = res_link_chk_->GetValue();
		options_.hide_input = hide_chk_->GetValue();
		options_.select_mask = mask_chk_->GetValue();
		options_.show_count = count_chk_->GetValue();
		if (options_.mode == tag::Mode::FolderIcon && options_.tags.IsEmpty()
		    && list_->GetSelection() != wxNOT_FOUND) {
			options_.tags = tag_names_[list_->GetSelection()];
		}
		event.Skip();
	}

	void RenameSelected()
	{
		const int index = list_->GetSelection();
		if (index == wxNOT_FOUND) return;
		const UnicodeString old_name = tag_names_[index];
		wxTextEntryDialog dlg(this, to_wx(_T("タグ名")), to_wx(_T("タグ名の変更")),
		                      to_wx(old_name));
		if (dlg.ShowModal() != wxID_OK) return;
		const UnicodeString new_name = Trim(to_us(dlg.GetValue()));
		if (new_name.IsEmpty() || SameText(old_name, new_name)) return;

		const int duplicate = manager_.TagNameList->IndexOf(new_name);
		if (duplicate != -1 && duplicate != index
		    && wxMessageBox(to_wx(_T("既存の同名タグに統合しますか?")), to_wx(_T("タグ名の変更")),
		                    wxYES_NO | wxICON_QUESTION, this) != wxYES) {
			return;
		}
		const int changed = manager_.RenTag(old_name, new_name);
		RefreshTags();
		SyncInputToChecks();
		status_->SetLabel(to_wx(UnicodeString().sprintf(_T("%d 項目を更新しました"), changed)));
	}

	void DeleteSelected()
	{
		const int index = list_->GetSelection();
		if (index == wxNOT_FOUND) return;
		const UnicodeString name = tag_names_[index];
		UnicodeString msg = _T("[") + name + _T("] をタグデータから削除しますか?");
		if (wxMessageBox(to_wx(msg), to_wx(_T("タグの削除")), wxYES_NO | wxICON_QUESTION, this) != wxYES) {
			return;
		}
		const int changed = manager_.DelTagData(name);
		RefreshTags();
		SyncInputToChecks();
		status_->SetLabel(to_wx(UnicodeString().sprintf(_T("%d 項目を削除しました"), changed)));
	}

	wxColour CurrentColour(const UnicodeString &tag) const
	{
		const unsigned int color = manager_.GetColor(tag, 0xFFFFFFFFu);
		return wxColour((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
	}

	void SetSelectedColor()
	{
		const int index = list_->GetSelection();
		if (index == wxNOT_FOUND) return;
		const UnicodeString tag = tag_names_[index];
		wxColourData data;
		data.SetColour(CurrentColour(tag));
		wxColourDialog dlg(this, &data);
		if (dlg.ShowModal() != wxID_OK) return;
		const wxColour color = dlg.GetColourData().GetColour();
		manager_.SetColor(tag, (static_cast<unsigned int>(color.Red()) << 16)
		                     | (static_cast<unsigned int>(color.Green()) << 8)
		                     | static_cast<unsigned int>(color.Blue()));
		status_->SetLabel(to_wx(_T("タグ色を設定しました")));
	}

	void SetDefaultColor()
	{
		const int index = list_->GetSelection();
		if (index == wxNOT_FOUND) return;
		manager_.SetColor(tag_names_[index], Graphics::clNone);
		status_->SetLabel(to_wx(_T("タグ色を既定色にしました")));
	}

	void TrimData()
	{
		if (wxMessageBox(to_wx(_T("存在しない項目のタグデータを削除しますか?")),
		                 to_wx(_T("タグデータの整理")), wxYES_NO | wxICON_QUESTION, this) != wxYES) {
			return;
		}
		wxBusyCursor busy;
		const int changed = manager_.TrimData();
		status_->SetLabel(to_wx(UnicodeString().sprintf(_T("%d 個のデータを削除しました"), changed)));
		RefreshTags();
	}

	void SaveSearchCommand(bool opposite)
	{
		if (options_.mode != tag::Mode::Find) return;
		SyncChecksToInput();
		wxFileDialog dlg(this, to_wx(_T("コマンドファイルとして保存")), wxEmptyString,
		                 _T("*.nbt"), to_wx(_T("NyanFi コマンド (*.nbt)|*.nbt|すべて (*.*)|*.*")),
		                 wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
		if (dlg.ShowModal() != wxID_OK) return;
		const UnicodeString command = tag::BuildSearchCommand(
			to_us(input_->GetValue()), and_chk_->GetValue(), opposite);
		if (!write_utf8(dlg.GetPath(), command)) {
			wxMessageBox(to_wx(_T("保存できませんでした")), to_wx(_T("エラー")), wxOK | wxICON_ERROR, this);
		}
	}

	TagManager &manager_;
	tag::Options options_;
	std::vector<UnicodeString> folder_icons_;
	std::vector<UnicodeString> tag_names_;
	wxTextCtrl *input_ = nullptr;
	wxStaticText *input_label_ = nullptr;
	wxCheckListBox *list_ = nullptr;
	wxCheckBox *and_chk_ = nullptr;
	wxCheckBox *res_link_chk_ = nullptr;
	wxCheckBox *hide_chk_ = nullptr;
	wxCheckBox *mask_chk_ = nullptr;
	wxCheckBox *count_chk_ = nullptr;
	wxButton *rename_btn_ = nullptr;
	wxButton *delete_btn_ = nullptr;
	wxButton *color_btn_ = nullptr;
	wxButton *default_color_btn_ = nullptr;
	wxButton *trim_btn_ = nullptr;
	wxButton *save_nbt_btn_ = nullptr;
	wxButton *save_nbt_opp_btn_ = nullptr;
	wxStaticText *status_ = nullptr;
	bool syncing_ = false;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, TagManager &manager, tag::Options &options,
         const std::vector<UnicodeString> &folder_icons)
{
	TagManagerDialog dlg(parent, manager, options, folder_icons);
	if (dlg.ShowModal() != wxID_OK) return false;
	options = dlg.GetOptions();
	return true;
}

}  // namespace tag_dialog
