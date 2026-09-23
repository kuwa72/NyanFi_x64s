/**
 * @file gui/distribution_dialog.cpp
 * @brief gui/distribution_dialog.h の実装
 */
#include "gui/distribution_dialog.h"

#include <memory>

#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/combobox.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/listbox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "usr_file_ex.h"
#include "usr_str.h"
#include "gui/text_viewer_core.h"

namespace distribution_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<std::size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

UnicodeString RuleRow(const distribution::Rule &rule)
{
	UnicodeString row = rule.title;
	if (row.IsEmpty()) row = _T("(無題)");
	row += _T("  [");
	row += rule.mask;
	row += _T("] → ");
	row += rule.destination.IsEmpty()? _T("(反対パス)") : rule.destination;
	return row;
}

UnicodeString PreviewRow(const distribution::PreviewItem &item)
{
	UnicodeString source = item.source;
	UnicodeString dest = item.destination;
	if (item.skipped) dest = _T("[SKIP] ") + dest;
	return source + _T("  →  ") + dest;
}

class DistributionInputDialog final : public wxDialog {
public:
	DistributionInputDialog(wxWindow *parent, const std::vector<distribution::Input> &items,
	                        const std::vector<distribution::Rule> &rules,
	                        const distribution::Options &options)
		: wxDialog(parent, wxID_ANY, to_wx(_T("振り分け")), wxDefaultPosition,
		           wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, items_(items)
		, rules_(rules)
		, options_(options)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		rules_view_ = new wxCheckListBox(this, wxID_ANY, wxDefaultPosition, wxSize(640, 150));
		top->Add(rules_view_, wxSizerFlags(1).Expand().Border(wxALL, 8));
		rules_view_->Bind(wxEVT_LISTBOX, &DistributionInputDialog::OnRuleSelect, this);
		rules_view_->Bind(wxEVT_CHECKLISTBOX, &DistributionInputDialog::OnRuleCheck, this);

		wxFlexGridSizer *edit = new wxFlexGridSizer(3, 2, 6);
		edit->AddGrowableCol(1);
		edit->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("タイトル"))));
		title_edit_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(420, -1));
		edit->Add(title_edit_, wxSizerFlags(1).Expand());
		edit->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("マスク/パターン"))));
		mask_edit_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(420, -1));
		edit->Add(mask_edit_, wxSizerFlags(1).Expand());
		edit->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("振り分け先"))));
		dest_edit_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(420, -1));
		edit->Add(dest_edit_, wxSizerFlags(1).Expand());
		top->Add(edit, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));

		wxBoxSizer *edit_buttons = new wxBoxSizer(wxHORIZONTAL);
		add_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("追加")));
		chg_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("変更")));
		del_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("削除")));
		up_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("上へ")));
		down_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("下へ")));
		all_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("全選択/解除")));
		edit_buttons->Add(add_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		edit_buttons->Add(chg_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		edit_buttons->Add(del_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		edit_buttons->Add(up_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		edit_buttons->Add(down_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		edit_buttons->Add(all_btn_);
		top->Add(edit_buttons, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		add_btn_->Bind(wxEVT_BUTTON, &DistributionInputDialog::OnAdd, this);
		chg_btn_->Bind(wxEVT_BUTTON, &DistributionInputDialog::OnChange, this);
		del_btn_->Bind(wxEVT_BUTTON, &DistributionInputDialog::OnDelete, this);
		up_btn_->Bind(wxEVT_BUTTON, &DistributionInputDialog::OnUp, this);
		down_btn_->Bind(wxEVT_BUTTON, &DistributionInputDialog::OnDown, this);
		all_btn_->Bind(wxEVT_BUTTON, &DistributionInputDialog::OnToggleAll, this);

		wxBoxSizer *ref_buttons = new wxBoxSizer(wxHORIZONTAL);
		ref_list_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("リストファイル...")));
		ref_dir_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("振り分け先...")));
		find_edit_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(180, -1));
		ref_buttons->Add(ref_list_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		ref_buttons->Add(ref_dir_btn_, wxSizerFlags().Border(wxRIGHT, 8));
		ref_buttons->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("検索"))),
		                 wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		ref_buttons->Add(find_edit_);
		top->Add(ref_buttons, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		ref_list_btn_->Bind(wxEVT_BUTTON, &DistributionInputDialog::OnRefList, this);
		ref_dir_btn_->Bind(wxEVT_BUTTON, &DistributionInputDialog::OnRefDir, this);
		find_edit_->Bind(wxEVT_TEXT, &DistributionInputDialog::OnFind, this);

		wxBoxSizer *options_sizer = new wxBoxSizer(wxHORIZONTAL);
		create_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("振り分け先を自動作成")));
		group_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("同一タイトルを同じ状態にする")));
		preview_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("リストファイルプレビュー (未実装)")));
		create_chk_->SetValue(options_.create_directories);
		group_chk_->SetValue(false);
		preview_chk_->SetValue(false);
		preview_chk_->Enable(false);
		mode_box_ = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                            wxDefaultSize, 0, nullptr, wxCB_READONLY);
		mode_box_->Append(to_wx(_T("強制上書き")));
		mode_box_->Append(to_wx(_T("最新なら上書き")));
		mode_box_->Append(to_wx(_T("スキップ")));
		mode_box_->Append(to_wx(_T("自動的に名前を変更")));
		mode_box_->SetSelection(static_cast<int>(options_.copy_mode));
		options_sizer->Add(create_chk_, wxSizerFlags().Border(wxRIGHT, 8));
		options_sizer->Add(group_chk_, wxSizerFlags().Border(wxRIGHT, 8));
		options_sizer->Add(preview_chk_, wxSizerFlags().Border(wxRIGHT, 8));
		options_sizer->Add(mode_box_, wxSizerFlags().Border(wxRIGHT, 4));
		top->Add(options_sizer, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		create_chk_->Bind(wxEVT_CHECKBOX, &DistributionInputDialog::OnOptionChanged, this);
		group_chk_->Bind(wxEVT_CHECKBOX, &DistributionInputDialog::OnGroupToggle, this);
		preview_chk_->Bind(wxEVT_CHECKBOX, &DistributionInputDialog::OnOptionChanged, this);
		mode_box_->Bind(wxEVT_COMBOBOX, &DistributionInputDialog::OnOptionChanged, this);

		preview_view_ = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(640, 150));
		top->Add(preview_view_, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT, 8));
		status_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		top->Add(status_, wxSizerFlags().Expand().Border(wxALL, 8));

		wxBoxSizer *buttons = new wxBoxSizer(wxHORIZONTAL);
		copy_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("コピー")));
		move_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("移動")));
		buttons->Add(copy_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		buttons->Add(move_btn_, wxSizerFlags().Border(wxRIGHT, 8));
		buttons->AddStretchSpacer();
		buttons->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("キャンセル"))));
		top->Add(buttons, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		copy_btn_->Bind(wxEVT_BUTTON, &DistributionInputDialog::OnCopy, this);
		move_btn_->Bind(wxEVT_BUTTON, &DistributionInputDialog::OnMove, this);

		SetSizerAndFit(top);
		CentreOnParent();
		RefreshRules();
		RefreshPreview();
		LoadSelected();
	}

	Result ResultValue() const
	{
		Result result;
		result.rules = rules_;
		result.options = options_;
		result.preview = preview_;
		return result;
	}

private:
	void RefreshRules()
	{
		const int old = rules_view_->GetSelection();
		rules_view_->Freeze();
		rules_view_->Clear();
		for (const distribution::Rule &rule : rules_) rules_view_->Append(to_wx(RuleRow(rule)));
		for (std::size_t i = 0; i < rules_.size(); ++i) rules_view_->Check(static_cast<unsigned>(i), rules_[i].enabled);
		if (old >= 0 && old < static_cast<int>(rules_.size())) rules_view_->SetSelection(old);
		rules_view_->Thaw();
	}

	void LoadSelected()
	{
		const int sel = rules_view_->GetSelection();
		if (sel < 0 || sel >= static_cast<int>(rules_.size())) {
			title_edit_->Clear();
			mask_edit_->Clear();
			dest_edit_->Clear();
			return;
		}
		const distribution::Rule &rule = rules_[static_cast<std::size_t>(sel)];
		title_edit_->SetValue(to_wx(rule.title));
		mask_edit_->SetValue(to_wx(rule.mask));
		dest_edit_->SetValue(to_wx(rule.destination));
	}

	distribution::Rule Draft(int selected) const
	{
		distribution::Rule rule;
		rule.title = to_us(title_edit_->GetValue());
		rule.mask = to_us(mask_edit_->GetValue());
		rule.destination = to_us(dest_edit_->GetValue());
		rule.enabled = selected >= 0 ? rules_[static_cast<std::size_t>(selected)].enabled : true;
		return rule;
	}

	void RefreshPreview()
	{
		std::vector<distribution::Rule> expanded = rules_;
		for (const distribution::Rule &rule : rules_) {
			if (!StartsStr(_T("@"), rule.mask)) continue;
			const UnicodeString list_path = to_absolute_name(rule.mask.SubString(2));
			const text_viewer_core::LoadResult loaded = text_viewer_core::LoadForView(list_path);
			if (!loaded.ok || loaded.is_binary) continue;
			for (const UnicodeString &line : loaded.lines) {
				const TStringDynArray cols = split_strings_tab(line);
				if (cols.Length != 2 || cols[0].IsEmpty() || cols[1].IsEmpty()) continue;
				distribution::Rule item = rule;
				item.mask = cols[0];
				item.destination = cols[1];
				expanded.push_back(item);
			}
		}
		preview_ = distribution::BuildPreview(items_, expanded, options_,
			[](const UnicodeString &path) { return dir_exists(path); });
		preview_view_->Clear();
		for (const distribution::PreviewItem &row : preview_.items) preview_view_->Append(to_wx(PreviewRow(row)));
		UnicodeString msg;
		msg.sprintf(_T("登録:%u/%u  マッチ:%u  Dirs:%u  Files:%u  Skip:%u"),
		            static_cast<unsigned>(CountChecked()), static_cast<unsigned>(rules_.size()),
		            static_cast<unsigned>(preview_.matched), static_cast<unsigned>(preview_.directories),
		            static_cast<unsigned>(preview_.files), static_cast<unsigned>(preview_.skipped));
		status_->SetLabel(to_wx(msg));
		copy_btn_->Enable(preview_.matched > preview_.skipped);
		move_btn_->Enable(copy_btn_->IsEnabled());
	}

	int CountChecked() const
	{
		int count = 0;
		for (const distribution::Rule &rule : rules_) if (rule.enabled) ++count;
		return count;
	}

	void OnRuleSelect(wxCommandEvent &) { LoadSelected(); }
	void OnRuleCheck(wxCommandEvent &event)
	{
		const int idx = event.GetInt();
		if (idx >= 0 && idx < static_cast<int>(rules_.size())) rules_[static_cast<std::size_t>(idx)].enabled = rules_view_->IsChecked(idx);
		RefreshRules();
		RefreshPreview();
	}

	void OnAdd(wxCommandEvent &)
	{
		const distribution::Rule rule = Draft(-1);
		UnicodeString error;
		if (!distribution::CanAddRule(true, rule.title, rule.mask, rule.destination,
		                              static_cast<int>(rules_.size()), error)) {
			wxMessageBox(to_wx(error.IsEmpty()? _T("登録項目を入力してください") : error),
			             to_wx(_T("振り分け")), wxOK | wxICON_WARNING, this);
			return;
		}
		if (distribution::HasDuplicateRule(rules_, rule.mask, rule.destination)) {
			wxMessageBox(to_wx(_T("同じ登録があります")), to_wx(_T("振り分け")), wxOK | wxICON_INFORMATION, this);
			return;
		}
		rules_.push_back(rule);
		RefreshRules();
		rules_view_->SetSelection(static_cast<int>(rules_.size()) - 1);
		RefreshPreview();
	}

	void OnChange(wxCommandEvent &)
	{
		const int sel = rules_view_->GetSelection();
		if (sel < 0) return;
		const distribution::Rule rule = Draft(sel);
		UnicodeString error;
		if (!distribution::IsValidMask(rule.mask, error)) {
			wxMessageBox(to_wx(error), to_wx(_T("振り分け")), wxOK | wxICON_WARNING, this);
			return;
		}
		rules_[static_cast<std::size_t>(sel)] = rule;
		RefreshRules();
		rules_view_->SetSelection(sel);
		RefreshPreview();
	}

	void OnDelete(wxCommandEvent &)
	{
		const int sel = rules_view_->GetSelection();
		if (sel < 0) return;
		rules_.erase(rules_.begin() + sel);
		RefreshRules();
		LoadSelected();
		RefreshPreview();
	}

	void OnUp(wxCommandEvent &) { MoveSelected(-1); }
	void OnDown(wxCommandEvent &) { MoveSelected(1); }

	void MoveSelected(int delta)
	{
		const int sel = rules_view_->GetSelection();
		const int dst = sel + delta;
		if (sel < 0 || dst < 0 || dst >= static_cast<int>(rules_.size())) return;
		std::swap(rules_[static_cast<std::size_t>(sel)], rules_[static_cast<std::size_t>(dst)]);
		RefreshRules();
		rules_view_->SetSelection(dst);
		RefreshPreview();
	}

	void OnToggleAll(wxCommandEvent &)
	{
		const bool check = CountChecked() == 0;
		for (distribution::Rule &rule : rules_) rule.enabled = check;
		RefreshRules();
		RefreshPreview();
	}

	void OnGroupToggle(wxCommandEvent &event)
	{
		const int sel = rules_view_->GetSelection();
		if (sel >= 0) {
			const std::vector<bool> grouped = distribution::GroupChecked(rules_, sel, group_chk_->GetValue());
			for (std::size_t i = 0; i < rules_.size(); ++i) {
				if (!rules_[i].title.IsEmpty() &&
				    SameText(rules_[i].title, rules_[static_cast<std::size_t>(sel)].title))
					rules_[i].enabled = grouped[i];
			}
		}
		RefreshRules();
		RefreshPreview();
		event.Skip();
	}

	void OnRefList(wxCommandEvent &)
	{
		wxFileDialog dlg(this, to_wx(_T("リストファイルの指定")), wxEmptyString, wxEmptyString,
		                 to_wx(_T("テキスト (*.txt)|*.txt|すべて (*.*)|*.*")), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() != wxID_OK) return;
		mask_edit_->SetValue(to_wx(_T("@") + to_us(dlg.GetPath())));
		dest_edit_->Clear();
		RefreshPreview();
	}

	void OnRefDir(wxCommandEvent &)
	{
		wxDirDialog dlg(this, to_wx(_T("振り分け先")), to_wx(options_.opposite_path));
		if (dlg.ShowModal() != wxID_OK) return;
		dest_edit_->SetValue(dlg.GetPath());
		RefreshPreview();
	}

	void OnFind(wxCommandEvent &)
	{
		const UnicodeString word = to_us(find_edit_->GetValue());
		if (word.IsEmpty()) return;
		for (std::size_t i = 0; i < rules_.size(); ++i) {
			const distribution::Rule &rule = rules_[i];
			if (ContainsText(rule.title, word) || ContainsText(rule.mask, word) ||
			    ContainsText(rule.destination, word)) {
				rules_view_->SetSelection(static_cast<int>(i));
				LoadSelected();
				return;
			}
		}
	}

	void OnOptionChanged(wxCommandEvent &)
	{
		options_.create_directories = create_chk_->GetValue();
		const int mode = mode_box_->GetSelection();
		if (mode >= 0) options_.copy_mode = static_cast<distribution::CopyMode>(mode);
		RefreshPreview();
	}

	void OnCopy(wxCommandEvent &) { Finish(false); }
	void OnMove(wxCommandEvent &) { Finish(true); }
	void Finish(bool move)
	{
		if (preview_.matched <= preview_.skipped) {
			wxMessageBox(to_wx(_T("振り分け対象がありません")), to_wx(_T("振り分け")), wxOK | wxICON_INFORMATION, this);
			return;
		}
		options_.move = move;
		EndModal(wxID_OK);
	}

	std::vector<distribution::Input> items_;
	std::vector<distribution::Rule> rules_;
	distribution::Options options_;
	distribution::Preview preview_;
	wxCheckListBox *rules_view_ = nullptr;
	wxTextCtrl *title_edit_ = nullptr;
	wxTextCtrl *mask_edit_ = nullptr;
	wxTextCtrl *dest_edit_ = nullptr;
	wxButton *add_btn_ = nullptr;
	wxButton *chg_btn_ = nullptr;
	wxButton *del_btn_ = nullptr;
	wxButton *up_btn_ = nullptr;
	wxButton *down_btn_ = nullptr;
	wxButton *all_btn_ = nullptr;
	wxButton *ref_list_btn_ = nullptr;
	wxButton *ref_dir_btn_ = nullptr;
	wxTextCtrl *find_edit_ = nullptr;
	wxCheckBox *create_chk_ = nullptr;
	wxCheckBox *group_chk_ = nullptr;
	wxCheckBox *preview_chk_ = nullptr;
	wxComboBox *mode_box_ = nullptr;
	wxListBox *preview_view_ = nullptr;
	wxStaticText *status_ = nullptr;
	wxButton *copy_btn_ = nullptr;
	wxButton *move_btn_ = nullptr;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, const std::vector<distribution::Input> &items,
         const std::vector<distribution::Rule> &initial_rules,
         const distribution::Options &initial_options, Result &out)
{
	DistributionInputDialog dlg(parent, items, initial_rules, initial_options);
	if (dlg.ShowModal() != wxID_OK) return false;
	out = dlg.ResultValue();
	return true;
}

}  // namespace distribution_dialog
