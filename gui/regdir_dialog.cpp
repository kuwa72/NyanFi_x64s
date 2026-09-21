/**
 * @file gui/regdir_dialog.cpp
 * @brief gui/regdir_dialog.h の実装
 */
#include "gui/regdir_dialog.h"

#include <wx/checkbox.h>
#include <wx/listbox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>

#include "usr_file_ex.h"  // dir_exists

namespace regdir_dialog {

namespace {

/// wxString への変換 (gui/dupl_dialog.cpp と同じ変換ヘルパー)
inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

/// wxString → UnicodeString (MSW では両方 UTF-16)
inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

/// 一覧の1行表示 (VCL のオーナー描画の代わりに「登録名  パス」)。
/// セパレータ行は罫線表示にする
wxString row_of(const regdir::RegDirItem &item)
{
	if (regdir::IsSeparator(item)) return _T("----------");
	UnicodeString row = item.title;
	if (!item.key.IsEmpty()) row += UnicodeString(_T(" [")) + item.key + _T("]");
	row += UnicodeString(_T("  ")) + item.path;
	return to_wx(row);
}

/**
 * @brief 登録ディレクトリの一覧・選択ダイアログ
 * @details `src/DirDlg.cpp` (`TRegDirDlg`) の通常モードのうち、一覧の表示・
 *          フィルタ (`FilterEdit` + `AndOrAction`)・キーでのジャンプ
 *          (`RegDirListBoxKeyPress` の f_cnt 分岐)・追加 (`IsAddMode` 相当)・
 *          削除・使用後の先頭移動 (`move_top_RegDirItem`) だけを wx で
 *          再構成したもの
 */
class RegDirInputDialog : public wxDialog {
public:
	RegDirInputDialog(wxWindow *parent, std::vector<regdir::RegDirItem> &items,
	                  const UnicodeString &current_dir)
		: wxDialog(parent, wxID_ANY, to_wx(_T("登録ディレクトリ")), wxDefaultPosition,
		           wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, items_(items)
		, current_dir_(current_dir)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		wxBoxSizer *filter_row = new wxBoxSizer(wxHORIZONTAL);
		filter_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("フィルタ"))),
		                wxSizerFlags().CentreVertical().Border(wxRIGHT, 4));
		filter_ctrl_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
		                              wxDefaultPosition, wxSize(260, -1));
		filter_row->Add(filter_ctrl_, wxSizerFlags(1).Expand().Border(wxRIGHT, 8));
		and_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("AND")));
		filter_row->Add(and_chk_, wxSizerFlags().CentreVertical());
		top->Add(filter_row, wxSizerFlags().Expand().Border(wxALL, 8));
		filter_ctrl_->Bind(wxEVT_TEXT, &RegDirInputDialog::OnFilter, this);
		and_chk_->Bind(wxEVT_CHECKBOX, &RegDirInputDialog::OnFilter, this);

		list_ = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(420, 220));
		top->Add(list_, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		list_->Bind(wxEVT_LISTBOX_DCLICK, &RegDirInputDialog::OnEnter, this);
		list_->Bind(wxEVT_CHAR, &RegDirInputDialog::OnChar, this);

		wxBoxSizer *btn_row = new wxBoxSizer(wxHORIZONTAL);
		open_btn_ = new wxButton(this, wxID_OK, to_wx(_T("開く")));
		btn_row->Add(open_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		add_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("追加...")));
		btn_row->Add(add_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		del_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("削除")));
		btn_row->Add(del_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		btn_row->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("キャンセル"))));
		top->Add(btn_row, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		add_btn_->Bind(wxEVT_BUTTON, &RegDirInputDialog::OnAdd, this);
		del_btn_->Bind(wxEVT_BUTTON, &RegDirInputDialog::OnDelete, this);
		open_btn_->Bind(wxEVT_BUTTON, &RegDirInputDialog::OnEnter, this);

		SetSizerAndFit(top);
		CentreOnParent();

		RefreshList();
		// 初期カーソルは現在のディレクトリ (VCL の IndexOfDir(CurPathName) 相当)
		for (std::size_t i = 0; i < shown_.size(); i++) {
			const regdir::RegDirItem &it = items_[shown_[i]];
			if (!regdir::IsSeparator(it) && SameText(it.path, current_dir_)) {
				list_->SetSelection(static_cast<int>(i));
				break;
			}
		}
		if (list_->GetSelection() == wxNOT_FOUND && !shown_.empty())
			list_->SetSelection(0);
		filter_ctrl_->SetFocus();
	}

	/// 選ばれた項目の items_ 内の添字 (-1 なら未選択)。OK 時に先頭移動済み
	int Selected() const { return selected_; }

private:
	void RefreshList()
	{
		shown_.clear();
		list_->Clear();
		const UnicodeString filter = to_us(filter_ctrl_->GetValue());
		const bool and_mode = and_chk_->GetValue();
		for (std::size_t i = 0; i < items_.size(); i++) {
			// セパレータはフィルタ対象外だが表示は残す (VCL の一覧と同じ)
			if (!regdir::IsSeparator(items_[i]) &&
			    !regdir::MatchesFilter(items_[i], filter, and_mode))
				continue;
			shown_.push_back(i);
			list_->Append(row_of(items_[i]));
		}
		if (!shown_.empty() && list_->GetSelection() == wxNOT_FOUND) list_->SetSelection(0);
	}

	void OnFilter(wxCommandEvent &) { RefreshList(); }

	void AcceptSelection()
	{
		const int sel = list_->GetSelection();
		if (sel == wxNOT_FOUND) return;
		const int idx = static_cast<int>(shown_[static_cast<std::size_t>(sel)]);
		// MoveTop で vector が入れ替わるため、値は先に複写する
		const UnicodeString sel_path = regdir::SelectablePath(items_[static_cast<std::size_t>(idx)]);
		const UnicodeString sel_title = items_[static_cast<std::size_t>(idx)].title;
		if (sel_path.IsEmpty()) {
			wxMessageBox(to_wx(_T("セパレータは選べません")), to_wx(_T("登録ディレクトリ")),
			             wxOK | wxICON_INFORMATION, this);
			return;
		}
		if (!dir_exists(sel_path)) {
			wxMessageBox(to_wx(_T("開けません: ") + sel_path), to_wx(_T("登録ディレクトリ")),
			             wxOK | wxICON_WARNING, this);
			return;
		}
		// VCL の RegDirDlgMoveTop (使用項目をグループ先頭へ) と同じ
		regdir::MoveTop(items_, idx);
		// 移動後の添字を探し直す (同一タイトル・パスの最初の一致)
		selected_ = 0;
		for (std::size_t i = 0; i < items_.size(); i++) {
			if (SameText(items_[i].path, sel_path) &&
			    SameText(items_[i].title, sel_title)) {
				selected_ = static_cast<int>(i);
				break;
			}
		}
		EndModal(wxID_OK);
	}

	void OnEnter(wxCommandEvent &) { AcceptSelection(); }

	void OnChar(wxKeyEvent &event)
	{
		// VCL の RegDirListBoxKeyPress: 1文字キーでジャンプ。
		// 一致1件なら確定、複数なら最後の一致へカーソル移動だけ
		const int code = event.GetKeyCode();
		if (code >= 32 && code < 0x10000 && event.GetModifiers() == wxMOD_NONE) {
			wchar_t ch = static_cast<wchar_t>(code);
			const std::vector<int> hit =
				regdir::KeyMatches(items_, UnicodeString(&ch, 1));
			if (hit.size() == 1) {
				// フィルタ表示上の位置を探してカーソルを合わせてから確定する
				for (std::size_t i = 0; i < shown_.size(); i++) {
					if (shown_[i] == static_cast<std::size_t>(hit[0])) {
						list_->SetSelection(static_cast<int>(i));
						AcceptSelection();
						return;
					}
				}
			}
			else if (!hit.empty()) {
				for (std::size_t i = 0; i < shown_.size(); i++) {
					if (shown_[i] == static_cast<std::size_t>(hit.back())) {
						list_->SetSelection(static_cast<int>(i));
						list_->SetFirstItem(static_cast<int>(i));
						return;
					}
				}
			}
			else {
				::wxBell();
				return;
			}
		}
		event.Skip();
	}

	void OnAdd(wxCommandEvent &)
	{
		// VCL の追加モード (IsAddMode) の簡易版: 現在のディレクトリを登録する。
		// 登録名の既定は末尾要素名、キーは空 (後で ChangeRegDir には使えないが
		// 一覧からの選択には使える)
		const UnicodeString leaf =
			ExtractFileName(ExcludeTrailingPathDelimiter(current_dir_));
		const wxString title = wxGetTextFromUser(
			to_wx(_T("登録名を入力してください")), to_wx(_T("登録ディレクトリの追加")),
			to_wx(leaf.IsEmpty() ? current_dir_ : leaf), this);
		if (title.IsEmpty()) return;
		const wxString key = wxGetTextFromUser(
			to_wx(_T("アクセスキー (1文字。空でも可)")), to_wx(_T("登録ディレクトリの追加")),
			wxEmptyString, this);
		const UnicodeString key_in = to_us(key);
		regdir::RegDirItem item;
		item.key = key_in.IsEmpty() ? EmptyStr : key_in.SubString(1, 1);
		item.title = to_us(title);
		item.path = current_dir_;
		items_.push_back(item);
		RefreshList();
		list_->SetSelection(static_cast<int>(shown_.size()) - 1);
		list_->SetFirstItem(static_cast<int>(shown_.size()) - 1);
	}

	void OnDelete(wxCommandEvent &)
	{
		const int sel = list_->GetSelection();
		if (sel == wxNOT_FOUND) return;
		const std::size_t idx = shown_[static_cast<std::size_t>(sel)];
		items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(idx));
		RefreshList();
	}

	std::vector<regdir::RegDirItem> &items_;
	const UnicodeString current_dir_;
	wxTextCtrl *filter_ctrl_ = nullptr;
	wxCheckBox *and_chk_ = nullptr;
	wxListBox *list_ = nullptr;
	wxButton *open_btn_ = nullptr;
	wxButton *add_btn_ = nullptr;
	wxButton *del_btn_ = nullptr;
	std::vector<std::size_t> shown_;  //!< 表示行→items_ の添字
	int selected_ = -1;
};

}  // namespace

bool Run(wxWindow *parent, std::vector<regdir::RegDirItem> &items,
         const UnicodeString &current_dir, int &selected_out)
{
	RegDirInputDialog dlg(parent, items, current_dir);
	if (dlg.ShowModal() != wxID_OK) return false;
	selected_out = dlg.Selected();
	return selected_out >= 0 && selected_out < static_cast<int>(items.size());
}

}  // namespace regdir_dialog
