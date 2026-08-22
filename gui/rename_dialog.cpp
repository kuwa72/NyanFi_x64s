/**
 * @file gui/rename_dialog.cpp
 * @brief gui/rename_dialog.h の実装
 */
#include "gui/rename_dialog.h"

#include <algorithm>
#include <functional>
#include <memory>

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/radiobut.h>
#include <wx/simplebook.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace rename_dialog {

namespace {

/// wxString への変換 (gui/main_frame.cpp 等と同じ変換ヘルパー。MSW では
/// UnicodeString も wxString も UTF-16)
inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

/// RowStatus を一覧表示用の短い文字列にする
UnicodeString StatusLabel(rename_core::RowStatus status)
{
	switch (status) {
	case rename_core::RowStatus::Unchanged: return _T("変更なし");
	case rename_core::RowStatus::Ok:        return _T("OK");
	case rename_core::RowStatus::Invalid:   return _T("不正な名前");
	case rename_core::RowStatus::Conflict:  return _T("衝突");
	}
	return EmptyStr;
}

//---------------------------------------------------------------------------
/// 「正規表現置換」モードの設定パネル
class RegexPanel : public wxPanel {
public:
	explicit RegexPanel(wxWindow *parent) : wxPanel(parent)
	{
		wxFlexGridSizer *grid = new wxFlexGridSizer(2, 4, 8);
		grid->AddGrowableCol(1);

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("検索文字列"))), wxSizerFlags().CentreVertical());
		pattern_ = new wxTextCtrl(this, wxID_ANY);
		grid->Add(pattern_, wxSizerFlags(1).Expand());

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("置換文字列"))), wxSizerFlags().CentreVertical());
		replacement_ = new wxTextCtrl(this, wxID_ANY);
		grid->Add(replacement_, wxSizerFlags(1).Expand());

		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
		top->Add(grid, wxSizerFlags().Expand().Border(wxALL, 4));

		use_regex_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("正規表現として扱う (OFF ならそのままの文字列として一致)")));
		use_regex_->SetValue(true);
		case_sensitive_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("大小文字を区別する")));
		case_sensitive_->SetValue(true);
		only_base_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("拡張子を除いた部分だけに適用する")));

		top->Add(use_regex_, wxSizerFlags().Border(wxALL, 4));
		top->Add(case_sensitive_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 4));
		top->Add(only_base_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 4));

		SetSizerAndFit(top);
	}

	rename_core::RegexOptions Options() const
	{
		rename_core::RegexOptions opt;
		opt.pattern = to_us(pattern_->GetValue());
		opt.replacement = to_us(replacement_->GetValue());
		opt.use_regex = use_regex_->GetValue();
		opt.case_sensitive = case_sensitive_->GetValue();
		opt.only_base = only_base_->GetValue();
		return opt;
	}

	/// 変更を検知するイベントすべてに handler を bind する (プレビュー更新用)
	void BindChange(const std::function<void()> &handler)
	{
		pattern_->Bind(wxEVT_TEXT, [handler](wxCommandEvent &) { handler(); });
		replacement_->Bind(wxEVT_TEXT, [handler](wxCommandEvent &) { handler(); });
		use_regex_->Bind(wxEVT_CHECKBOX, [handler](wxCommandEvent &) { handler(); });
		case_sensitive_->Bind(wxEVT_CHECKBOX, [handler](wxCommandEvent &) { handler(); });
		only_base_->Bind(wxEVT_CHECKBOX, [handler](wxCommandEvent &) { handler(); });
	}

private:
	wxTextCtrl *pattern_ = nullptr;
	wxTextCtrl *replacement_ = nullptr;
	wxCheckBox *use_regex_ = nullptr;
	wxCheckBox *case_sensitive_ = nullptr;
	wxCheckBox *only_base_ = nullptr;
};

//---------------------------------------------------------------------------
/// 「連番付与」モードの設定パネル
class SerialPanel : public wxPanel {
public:
	explicit SerialPanel(wxWindow *parent) : wxPanel(parent)
	{
		wxFlexGridSizer *grid = new wxFlexGridSizer(2, 4, 8);
		grid->AddGrowableCol(1);

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("前に付ける文字列"))), wxSizerFlags().CentreVertical());
		prefix_ = new wxTextCtrl(this, wxID_ANY);
		grid->Add(prefix_, wxSizerFlags(1).Expand());

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("後に付ける文字列"))), wxSizerFlags().CentreVertical());
		suffix_ = new wxTextCtrl(this, wxID_ANY);
		grid->Add(suffix_, wxSizerFlags(1).Expand());

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("開始番号"))), wxSizerFlags().CentreVertical());
		start_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
		                         wxSP_ARROW_KEYS, 0, 999999, 1);
		grid->Add(start_, wxSizerFlags().Expand());

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("増分"))), wxSizerFlags().CentreVertical());
		step_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
		                        wxSP_ARROW_KEYS, 1, 9999, 1);
		grid->Add(step_, wxSizerFlags().Expand());

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("桁数 (0埋め。0で連番なし)"))),
		          wxSizerFlags().CentreVertical());
		width_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
		                         wxSP_ARROW_KEYS, 0, 10, 2);
		grid->Add(width_, wxSizerFlags().Expand());

		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
		top->Add(grid, wxSizerFlags().Expand().Border(wxALL, 4));

		wxBoxSizer *ext_row = new wxBoxSizer(wxHORIZONTAL);
		change_ext_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("拡張子を変更する")));
		ext_row->Add(change_ext_, wxSizerFlags().CentreVertical().Border(wxRIGHT, 8));
		new_ext_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(100, -1));
		new_ext_->Enable(false);
		ext_row->Add(new_ext_, wxSizerFlags().CentreVertical());
		top->Add(ext_row, wxSizerFlags().Border(wxALL, 4));

		change_ext_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &) { new_ext_->Enable(change_ext_->GetValue()); });

		SetSizerAndFit(top);
	}

	rename_core::SerialOptions Options() const
	{
		rename_core::SerialOptions opt;
		opt.prefix = to_us(prefix_->GetValue());
		opt.suffix = to_us(suffix_->GetValue());
		opt.start = start_->GetValue();
		opt.step = step_->GetValue();
		opt.width = width_->GetValue();
		opt.change_ext = change_ext_->GetValue();
		opt.new_ext = to_us(new_ext_->GetValue());
		return opt;
	}

	void BindChange(const std::function<void()> &handler)
	{
		prefix_->Bind(wxEVT_TEXT, [handler](wxCommandEvent &) { handler(); });
		suffix_->Bind(wxEVT_TEXT, [handler](wxCommandEvent &) { handler(); });
		new_ext_->Bind(wxEVT_TEXT, [handler](wxCommandEvent &) { handler(); });
		start_->Bind(wxEVT_SPINCTRL, [handler](wxSpinEvent &) { handler(); });
		step_->Bind(wxEVT_SPINCTRL, [handler](wxSpinEvent &) { handler(); });
		width_->Bind(wxEVT_SPINCTRL, [handler](wxSpinEvent &) { handler(); });
		start_->Bind(wxEVT_TEXT, [handler](wxCommandEvent &) { handler(); });
		step_->Bind(wxEVT_TEXT, [handler](wxCommandEvent &) { handler(); });
		width_->Bind(wxEVT_TEXT, [handler](wxCommandEvent &) { handler(); });
		change_ext_->Bind(wxEVT_CHECKBOX, [handler](wxCommandEvent &) { handler(); });
	}

private:
	wxTextCtrl *prefix_ = nullptr;
	wxTextCtrl *suffix_ = nullptr;
	wxSpinCtrl *start_ = nullptr;
	wxSpinCtrl *step_ = nullptr;
	wxSpinCtrl *width_ = nullptr;
	wxCheckBox *change_ext_ = nullptr;
	wxTextCtrl *new_ext_ = nullptr;
};

//---------------------------------------------------------------------------
/// 「大文字/小文字変換」モードの設定パネル
class CasePanel : public wxPanel {
public:
	explicit CasePanel(wxWindow *parent) : wxPanel(parent)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		upper_ = new wxRadioButton(this, wxID_ANY, to_wx(_T("大文字に変換 (ABC)")), wxDefaultPosition,
		                            wxDefaultSize, wxRB_GROUP);
		lower_ = new wxRadioButton(this, wxID_ANY, to_wx(_T("小文字に変換 (abc)")));
		upper_->SetValue(true);
		top->Add(upper_, wxSizerFlags().Border(wxALL, 4));
		top->Add(lower_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 4));

		only_base_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("拡張子を除いた部分だけ変換する (拡張子は変化しない)")));
		top->Add(only_base_, wxSizerFlags().Border(wxALL, 4));

		SetSizerAndFit(top);
	}

	rename_core::CaseOptions Options() const
	{
		rename_core::CaseOptions opt;
		opt.mode = upper_->GetValue() ? rename_core::CaseMode::Upper : rename_core::CaseMode::Lower;
		opt.only_base = only_base_->GetValue();
		return opt;
	}

	void BindChange(const std::function<void()> &handler)
	{
		upper_->Bind(wxEVT_RADIOBUTTON, [handler](wxCommandEvent &) { handler(); });
		lower_->Bind(wxEVT_RADIOBUTTON, [handler](wxCommandEvent &) { handler(); });
		only_base_->Bind(wxEVT_CHECKBOX, [handler](wxCommandEvent &) { handler(); });
	}

private:
	wxRadioButton *upper_ = nullptr;
	wxRadioButton *lower_ = nullptr;
	wxCheckBox *only_base_ = nullptr;
};

//---------------------------------------------------------------------------
/**
 * @brief 一括リネームのダイアログ本体
 * @details モード選択 (wxChoice) + モードごとの設定パネル (wxSimplebook で
 * 切り替え) + プレビュー一覧 (wxListCtrl) + 実行/キャンセルボタン
 */
class RenameDialog : public wxDialog {
public:
	RenameDialog(wxWindow *parent, const UnicodeString &dir,
	             const std::vector<rename_core::RenameTarget> &targets)
		: wxDialog(parent, wxID_ANY, to_wx(_T("一括リネーム")), wxDefaultPosition, wxSize(720, 560),
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, dir_(dir)
		, targets_(targets)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		UnicodeString header;
		header.sprintf(_T("対象: %d 件 (%s)"), static_cast<int>(targets_.size()), dir_.c_str());
		top->Add(new wxStaticText(this, wxID_ANY, to_wx(header)), wxSizerFlags().Border(wxALL, 8));

		wxBoxSizer *mode_row = new wxBoxSizer(wxHORIZONTAL);
		mode_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("方式"))), wxSizerFlags().CentreVertical().Border(wxRIGHT, 8));
		mode_choice_ = new wxChoice(this, wxID_ANY);
		mode_choice_->Append(to_wx(_T("正規表現置換")));
		mode_choice_->Append(to_wx(_T("連番の付与")));
		mode_choice_->Append(to_wx(_T("大文字/小文字の変換")));
		mode_choice_->SetSelection(0);
		mode_row->Add(mode_choice_, wxSizerFlags().CentreVertical());
		top->Add(mode_row, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		book_ = new wxSimplebook(this, wxID_ANY);
		regex_panel_ = new RegexPanel(book_);
		serial_panel_ = new SerialPanel(book_);
		case_panel_ = new CasePanel(book_);
		book_->AddPage(regex_panel_, wxEmptyString);
		book_->AddPage(serial_panel_, wxEmptyString);
		book_->AddPage(case_panel_, wxEmptyString);
		top->Add(book_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));

		list_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
		list_->AppendColumn(to_wx(_T("元の名前")), wxLIST_FORMAT_LEFT, 240);
		list_->AppendColumn(to_wx(_T("新しい名前")), wxLIST_FORMAT_LEFT, 240);
		list_->AppendColumn(to_wx(_T("状態")), wxLIST_FORMAT_LEFT, 100);
		top->Add(list_, wxSizerFlags(1).Expand().Border(wxALL, 8));

		summary_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		top->Add(summary_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxALL, 8));

		wxBoxSizer *buttons = new wxBoxSizer(wxHORIZONTAL);
		buttons->AddStretchSpacer();
		exec_btn_ = new wxButton(this, wxID_OK, to_wx(_T("実行")));
		buttons->Add(exec_btn_, wxSizerFlags().Border(wxRIGHT, 8));
		buttons->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("閉じる"))));
		top->Add(buttons, wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		auto refresh = [this]() { UpdatePreview(); };
		regex_panel_->BindChange(refresh);
		serial_panel_->BindChange(refresh);
		case_panel_->BindChange(refresh);
		mode_choice_->Bind(wxEVT_CHOICE, &RenameDialog::OnModeChanged, this);
		Bind(wxEVT_BUTTON, &RenameDialog::OnExecute, this, wxID_OK);

		UpdatePreview();
	}

	/// 実行の結果、1件以上成功したか (呼び出し側の Reload 要否)
	bool AnyRenamed() const { return any_renamed_; }
	const std::vector<rename_core::AppliedRename> &Applied() const { return applied_; }

private:
	void OnModeChanged(wxCommandEvent &event)
	{
		book_->SetSelection(static_cast<size_t>(mode_choice_->GetSelection()));
		UpdatePreview();
	}

	/// 現在選択中のモードでプレビューを計算し直し、一覧・要約・実行ボタンを更新する
	void UpdatePreview()
	{
		switch (mode_choice_->GetSelection()) {
		case 0: current_plan_ = rename_core::BuildRegexPlan(dir_, targets_, regex_panel_->Options()); break;
		case 1: current_plan_ = rename_core::BuildSerialPlan(dir_, targets_, serial_panel_->Options()); break;
		case 2: current_plan_ = rename_core::BuildCasePlan(dir_, targets_, case_panel_->Options()); break;
		default: return;
		}

		list_->DeleteAllItems();

		if (current_plan_.pattern_error) {
			summary_->SetLabel(to_wx(current_plan_.error));
			exec_btn_->Enable(false);
			return;
		}

		int ok = 0, unchanged = 0, invalid = 0, conflict = 0;
		for (std::size_t i = 0; i < current_plan_.rows.size(); ++i) {
			const rename_core::PreviewRow &row = current_plan_.rows[i];
			const long item = list_->InsertItem(static_cast<long>(i), to_wx(row.old_name));
			list_->SetItem(item, 1, to_wx(row.new_name));
			list_->SetItem(item, 2, to_wx(StatusLabel(row.status)));

			// 行の色で状態を分かりやすくする (テストで検証できるのは status
			// 文字列そのものなので、色付けは補助的な表示に留める)
			switch (row.status) {
			case rename_core::RowStatus::Ok: ok++; break;
			case rename_core::RowStatus::Unchanged:
				unchanged++;
				list_->SetItemTextColour(item, *wxLIGHT_GREY);
				break;
			case rename_core::RowStatus::Invalid:
				invalid++;
				list_->SetItemTextColour(item, *wxRED);
				break;
			case rename_core::RowStatus::Conflict:
				conflict++;
				list_->SetItemTextColour(item, *wxRED);
				break;
			}
		}

		UnicodeString summary;
		summary.sprintf(_T("変更 %d 件 / 変更なし %d 件 / 衝突 %d 件 / 不正な名前 %d 件"),
		                 ok, unchanged, conflict, invalid);
		summary_->SetLabel(to_wx(summary));
		exec_btn_->Enable(ok > 0);
	}

	void OnExecute(wxCommandEvent &event)
	{
		int ok_count = 0;
		for (const rename_core::PreviewRow &row : current_plan_.rows) {
			if (row.status == rename_core::RowStatus::Ok) ok_count++;
		}
		if (ok_count == 0) return;  // 実行ボタンは ok_count>0 でのみ有効なはずだが念のため

		UnicodeString confirm;
		confirm.sprintf(_T("%d 件の名前を変更します。よろしいですか?"), ok_count);
		if (wxMessageBox(to_wx(confirm), to_wx(_T("一括リネーム")), wxYES_NO | wxICON_QUESTION, this) != wxYES) {
			return;
		}

		const rename_core::RenameExecResult result = rename_core::ExecutePlan(dir_, current_plan_);
		any_renamed_ = (result.success_count > 0);
		applied_ = result.applied;

		UnicodeString msg;
		msg.sprintf(_T("成功 %d 件 / スキップ %d 件"), result.success_count, result.skipped_count);
		if (!result.failures.empty()) {
			msg.cat_sprintf(_T(" / 失敗 %d 件\n\n"), static_cast<int>(result.failures.size()));
			const std::size_t show = std::min<std::size_t>(result.failures.size(), 8);
			for (std::size_t i = 0; i < show; ++i) msg += _T("・") + result.failures[i] + _T("\n");
			if (result.failures.size() > show) {
				msg.cat_sprintf(_T("...ほか %d 件\n"), static_cast<int>(result.failures.size() - show));
			}
		}
		wxMessageBox(to_wx(msg), to_wx(_T("一括リネームの結果")), wxOK | wxICON_INFORMATION, this);

		EndModal(wxID_OK);
	}

	UnicodeString dir_;
	std::vector<rename_core::RenameTarget> targets_;
	rename_core::RenamePlan current_plan_;
	bool any_renamed_ = false;
	std::vector<rename_core::AppliedRename> applied_;  //!< 実際に変わったもの (取り消し用)

	wxChoice *mode_choice_ = nullptr;
	wxSimplebook *book_ = nullptr;
	RegexPanel *regex_panel_ = nullptr;
	SerialPanel *serial_panel_ = nullptr;
	CasePanel *case_panel_ = nullptr;
	wxListCtrl *list_ = nullptr;
	wxStaticText *summary_ = nullptr;
	wxButton *exec_btn_ = nullptr;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, const UnicodeString &dir,
         const std::vector<rename_core::RenameTarget> &targets,
         std::vector<rename_core::AppliedRename> &applied_out)
{
	applied_out.clear();
	if (targets.empty()) return false;

	RenameDialog dlg(parent, dir, targets);
	dlg.ShowModal();
	applied_out = dlg.Applied();
	return dlg.AnyRenamed();
}

}  // namespace rename_dialog
