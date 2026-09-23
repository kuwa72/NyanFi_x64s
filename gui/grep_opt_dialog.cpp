/**
 * @file gui/grep_opt_dialog.cpp
 * @brief gui/grep_opt_dialog.h の実装
 */
#include "gui/grep_opt_dialog.h"

#include <wx/checkbox.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/notebook.h>
#include <wx/radiobox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace grep_opt_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

/// 1行の labelled edit を作る (VCL の TLabeledEdit 相当)
wxTextCtrl *AddLabeled(wxWindow *parent, wxFlexGridSizer *grid, const wxString &label,
                       const UnicodeString &value, wxSize size = wxSize(300, -1))
{
	grid->Add(new wxStaticText(parent, wxID_ANY, label), wxSizerFlags().CentreVertical());
	wxTextCtrl *ctrl = new wxTextCtrl(parent, wxID_ANY, to_wx(value), wxDefaultPosition, size);
	grid->Add(ctrl, wxSizerFlags(1).Expand());
	return ctrl;
}

}  // namespace

//---------------------------------------------------------------------------
/**
 * @brief GREP 拡張設定の wx ダイアログ
 * @details VCL の DFM は 3 タブ (OutModeSheet/ReplaceSheet/OutFormSheet) だが、
 *          wx では同じ内容を Notebook の3ページへ移す。値の説明/サンプル更新は
 *          gui/grep_opt.cpp の純関数、参照ボタンは wx のファイル選択だけ担当。
 */
class GrepOptionsDialog : public wxDialog {
public:
	GrepOptionsDialog(wxWindow *parent, const grep_opt::Options &initial, bool replace_mode)
		: wxDialog(parent, wxID_ANY, to_wx(_T("拡張設定")), wxDefaultPosition, wxDefaultSize,
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, replace_mode_(replace_mode)
	{
		const grep_opt::Options initial_opt = grep_opt::Normalize(initial);
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
		wxNotebook *book = new wxNotebook(this, wxID_ANY);

		//-- 出力方法 (src/GrepOptDlg.dfm:33-198) ----------------------------
		wxPanel *out_page = new wxPanel(book);
		wxBoxSizer *out = new wxBoxSizer(wxVERTICAL);
		wxArrayString out_choices;
		out_choices.Add(to_wx(_T("出力なし")));
		out_choices.Add(to_wx(_T("ファイル")));
		out_choices.Add(to_wx(_T("クリップボード")));
		output_mode_ = new wxRadioBox(out_page, wxID_ANY, to_wx(_T("出力方式")),
		                              wxDefaultPosition, wxDefaultSize, out_choices, 1,
		                              wxRA_SPECIFY_ROWS);
		output_mode_->SetSelection(grep_opt::OutputModeIndex(initial_opt.output_mode));
		out->Add(output_mode_, wxSizerFlags().Expand().Border(wxALL, 8));

		wxFlexGridSizer *out_grid = new wxFlexGridSizer(2, 4, 8);
		out_grid->AddGrowableCol(1);
		output_file_ = AddLabeled(out_page, out_grid, to_wx(_T("ファイル名")), initial_opt.output_file, wxSize(360, -1));
		out->Add(out_grid, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		wxButton *out_browse = new wxButton(out_page, wxID_ANY, _T("..."));
		out_browse->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { BrowseOutput(); });
		// ファイル欄の右にボタンを置くため、grid の2列目へ入れる
		out_grid->Add(out_browse, wxSizerFlags().CentreVertical());

		append_output_ = new wxCheckBox(out_page, wxID_ANY, to_wx(_T("既存ファイルに追加(&A)")));
		append_output_->SetValue(initial_opt.append_output);
		out->Add(append_output_, wxSizerFlags().Border(wxALL, 8));

		wxBoxSizer *app_box = new wxBoxSizer(wxVERTICAL);
		app_enabled_ = new wxCheckBox(out_page, wxID_ANY, to_wx(_T("起動アプリケーション")));
		app_enabled_->SetValue(initial_opt.app_enabled);
		app_box->Add(app_enabled_, wxSizerFlags().Border(wxBOTTOM, 6));
		wxFlexGridSizer *app_grid = new wxFlexGridSizer(2, 4, 8);
		app_grid->AddGrowableCol(1);
		app_name_ = AddLabeled(out_page, app_grid, to_wx(_T("プログラム")), initial_opt.app_name, wxSize(270, -1));
		app_param_ = AddLabeled(out_page, app_grid, to_wx(_T("引数")), initial_opt.app_param, wxSize(270, -1));
		app_dir_ = AddLabeled(out_page, app_grid, to_wx(_T("作業ディレクトリ")), initial_opt.app_dir, wxSize(270, -1));
		app_box->Add(app_grid, wxSizerFlags().Expand());
		wxBoxSizer *app_buttons = new wxBoxSizer(wxHORIZONTAL);
		wxButton *app_browse = new wxButton(out_page, wxID_ANY, _T("..."));
		wxButton *dir_browse = new wxButton(out_page, wxID_ANY, _T("..."));
		app_browse->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { BrowseApp(); });
		dir_browse->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { BrowseDir(app_dir_); });
		app_buttons->Add(app_browse, wxSizerFlags().Border(wxRIGHT, 4));
		app_buttons->Add(dir_browse);
		app_box->Add(app_buttons, wxSizerFlags().Border(wxTOP, 4));
		out->Add(app_box, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		out_page->SetSizer(out);
		book->AddPage(out_page, to_wx(replace_mode ? _T("置換・出力") : _T("出力方法")), true);

		//-- 置換/ログ (src/GrepOptDlg.dfm:199-326) -------------------------
		wxPanel *replace_page = new wxPanel(book);
		wxBoxSizer *replace = new wxBoxSizer(wxVERTICAL);
		wxFlexGridSizer *backup_grid = new wxFlexGridSizer(2, 4, 8);
		backup_grid->AddGrowableCol(1);
		backup_ext_ = AddLabeled(replace_page, backup_grid, to_wx(_T("バックアップ")),
		                          initial_opt.backup_extension, wxSize(240, -1));
		backup_dir_ = AddLabeled(replace_page, backup_grid, to_wx(_T("保存先")),
		                          initial_opt.backup_dir, wxSize(240, -1));
		wxButton *backup_browse = new wxButton(replace_page, wxID_ANY, _T("..."));
		backup_browse->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { BrowseDir(backup_dir_); });
		backup_grid->Add(backup_browse, wxSizerFlags().CentreVertical());
		replace->Add(backup_grid, wxSizerFlags().Expand().Border(wxALL, 8));
		backup_replace_ = new wxCheckBox(replace_page, wxID_ANY, to_wx(_T("置換前にバックアップを作る")));
		backup_replace_->SetValue(initial_opt.backup_replace);
		replace->Add(backup_replace_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		save_log_ = new wxCheckBox(replace_page, wxID_ANY, to_wx(_T("置換ログをファイルに保存")));
		save_log_->SetValue(initial_opt.save_log);
		replace->Add(save_log_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		wxFlexGridSizer *log_grid = new wxFlexGridSizer(2, 4, 8);
		log_grid->AddGrowableCol(1);
		log_file_ = AddLabeled(replace_page, log_grid, to_wx(_T("ログファイル")), initial_opt.log_file, wxSize(260, -1));
		wxButton *log_browse = new wxButton(replace_page, wxID_ANY, _T("..."));
		log_browse->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { BrowseLog(); });
		log_grid->Add(log_browse, wxSizerFlags().CentreVertical());
		replace->Add(log_grid, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		append_log_ = new wxCheckBox(replace_page, wxID_ANY, to_wx(_T("既存ログに追加")));
		append_log_->SetValue(initial_opt.append_log);
		open_log_ = new wxCheckBox(replace_page, wxID_ANY, to_wx(_T("保存後にログを開く")));
		open_log_->SetValue(initial_opt.open_log);
		replace->Add(append_log_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		replace->Add(open_log_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		replace_page->SetSizer(replace);
		book->AddPage(replace_page, to_wx(_T("置換・ログ")));

		//-- 出力形式 (src/GrepOptDlg.dfm:327-441) --------------------------
		wxPanel *format_page = new wxPanel(book);
		wxBoxSizer *format = new wxBoxSizer(wxVERTICAL);
		wxFlexGridSizer *format_grid = new wxFlexGridSizer(2, 4, 8);
		format_grid->AddGrowableCol(1);
		file_format_ = AddLabeled(format_page, format_grid, to_wx(_T("ファイル書式")),
		                          initial_opt.file_format, wxSize(360, -1));
		insert_before_ = AddLabeled(format_page, format_grid, to_wx(_T("検索語の")),
		                            initial_opt.insert_before, wxSize(170, -1));
		insert_after_ = AddLabeled(format_page, format_grid, to_wx(_T("検索語の後")),
		                           initial_opt.insert_after, wxSize(170, -1));
		replacement_ = AddLabeled(format_page, format_grid, to_wx(_T("改行の置換")),
		                          initial_opt.replacement, wxSize(170, -1));
		format->Add(format_grid, wxSizerFlags().Expand().Border(wxALL, 8));
		trim_left_ = new wxCheckBox(format_page, wxID_ANY, to_wx(_T("行頭のタブや空白を削除")));
		trim_left_->SetValue(initial_opt.trim_left);
		replace_tab_ = new wxCheckBox(format_page, wxID_ANY, to_wx(_T("タブを空白1文字に置換")));
		replace_tab_->SetValue(initial_opt.replace_tab);
		replace_cr_ = new wxCheckBox(format_page, wxID_ANY, to_wx(_T("改行を指定文字列を置換")));
		replace_cr_->SetValue(initial_opt.replace_cr);
		format->Add(trim_left_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		format->Add(replace_tab_, wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		format->Add(replace_cr_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		format->Add(new wxStaticText(format_page, wxID_ANY,
		                              to_wx(_T("$F=ファイル名  $B=ベース名  $L=行番号  \\t=タブ  \\n=改行"))),
		            wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		sample_ = new wxTextCtrl(format_page, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(430, 120),
		                         wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
		format->Add(sample_, wxSizerFlags().Expand().Border(wxALL, 8));
		format_page->SetSizer(format);
		book->AddPage(format_page, to_wx(_T("出力形式")));

		top->Add(book, wxSizerFlags(1).Expand().Border(wxALL, 6));
		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));
		SetSizerAndFit(top);
		CentreOnParent();

		// 値のANYイベントで状態とサンプルを同期する
		for (wxWindow *w : {static_cast<wxWindow *>(append_output_),
		                     static_cast<wxWindow *>(app_enabled_),
		                     static_cast<wxWindow *>(backup_replace_),
		                     static_cast<wxWindow *>(save_log_),
		                     static_cast<wxWindow *>(append_log_),
		                     static_cast<wxWindow *>(open_log_),
		                     static_cast<wxWindow *>(trim_left_),
		                     static_cast<wxWindow *>(replace_tab_),
		                     static_cast<wxWindow *>(replace_cr_)}) {
			w->Bind(wxEVT_CHECKBOX, &GrepOptionsDialog::OnValueChanged, this);
		}
		output_mode_->Bind(wxEVT_RADIOBOX, &GrepOptionsDialog::OnValueChanged, this);
		for (wxTextCtrl *edit : {output_file_, app_name_, app_param_, app_dir_, backup_ext_,
		                         backup_dir_, log_file_, file_format_, insert_before_,
		                         insert_after_, replacement_}) {
			edit->Bind(wxEVT_TEXT, &GrepOptionsDialog::OnValueChanged, this);
		}
		UpdateEnabled();
	}

	grep_opt::Options Values() const
	{
		grep_opt::Options o;
		o.output_mode = grep_opt::OutputModeFromIndex(output_mode_->GetSelection());
		o.edit_mode = replace_mode_ ? grep_opt::EditMode::Replace : grep_opt::EditMode::Search;
		o.output_file = to_us(output_file_->GetValue());
		o.append_output = append_output_->GetValue();
		o.app_enabled = app_enabled_->GetValue();
		o.app_name = to_us(app_name_->GetValue());
		o.app_param = to_us(app_param_->GetValue());
		o.app_dir = to_us(app_dir_->GetValue());
		o.file_format = to_us(file_format_->GetValue());
		o.insert_before = to_us(insert_before_->GetValue());
		o.insert_after = to_us(insert_after_->GetValue());
		o.trim_left = trim_left_->GetValue();
		o.replace_tab = replace_tab_->GetValue();
		o.replace_cr = replace_cr_->GetValue();
		o.replacement = to_us(replacement_->GetValue());
		o.backup_replace = backup_replace_->GetValue();
		o.backup_extension = to_us(backup_ext_->GetValue());
		o.backup_dir = to_us(backup_dir_->GetValue());
		o.save_log = save_log_->GetValue();
		o.log_file = to_us(log_file_->GetValue());
		o.append_log = append_log_->GetValue();
		o.open_log = open_log_->GetValue();
		return grep_opt::Normalize(o);
	}

private:
	void OnValueChanged(wxCommandEvent &event)
	{
		UpdateEnabled();
		event.Skip();
	}

	void UpdateEnabled()
	{
		const grep_opt::Options o = Values();
		const grep_opt::EnabledState e = grep_opt::ResolveEnabled(o);
		output_file_->Enable(e.output_file);
		app_name_->Enable(e.app_name);
		app_param_->Enable(e.app);
		app_dir_->Enable(e.app_dir);
		backup_ext_->Enable(e.backup);
		backup_dir_->Enable(e.backup);
		log_file_->Enable(e.log);
		insert_before_->Enable(e.insert_words);
		insert_after_->Enable(e.insert_words);
		sample_->SetValue(to_wx(grep_opt::BuildSample(o)));
	}

	void BrowseOutput()
	{
		wxFileDialog dlg(this, to_wx(_T("出力ファイルの指定")), wxEmptyString,
		                  output_file_->GetValue(),
		                  to_wx(_T("テキスト (*.txt)|*.txt|すべてのファイル (*.*)|*.*")),
		                  wxFD_SAVE, wxDefaultPosition, wxDefaultSize);
		if (dlg.ShowModal() == wxID_OK) output_file_->SetValue(dlg.GetPath());
	}

	void BrowseApp()
	{
		wxFileDialog dlg(this, to_wx(_T("起動アプリケーションの指定")), wxEmptyString,
		                  app_name_->GetValue(),
		                  to_wx(_T("実行ファイル (*.exe)|*.exe|すべてのファイル (*.*)|*.*")),
		                  wxFD_OPEN | wxFD_FILE_MUST_EXIST, wxDefaultPosition, wxDefaultSize);
		if (dlg.ShowModal() == wxID_OK) app_name_->SetValue(dlg.GetPath());
	}

	void BrowseDir(wxTextCtrl *target)
	{
		wxDirDialog dlg(this, to_wx(_T("ディレクトリの選択")), target->GetValue());
		if (dlg.ShowModal() == wxID_OK) target->SetValue(dlg.GetPath());
	}

	void BrowseLog()
	{
		wxFileDialog dlg(this, to_wx(_T("置換ログファイルの指定")), wxEmptyString,
		                  log_file_->GetValue(), wxEmptyString, wxFD_SAVE,
		                  wxDefaultPosition, wxDefaultSize);
		if (dlg.ShowModal() == wxID_OK) log_file_->SetValue(dlg.GetPath());
	}

	bool replace_mode_ = false;
	wxRadioBox *output_mode_ = nullptr;
	wxTextCtrl *output_file_ = nullptr;
	wxCheckBox *append_output_ = nullptr;
	wxCheckBox *app_enabled_ = nullptr;
	wxTextCtrl *app_name_ = nullptr;
	wxTextCtrl *app_param_ = nullptr;
	wxTextCtrl *app_dir_ = nullptr;
	wxCheckBox *backup_replace_ = nullptr;
	wxTextCtrl *backup_ext_ = nullptr;
	wxTextCtrl *backup_dir_ = nullptr;
	wxCheckBox *save_log_ = nullptr;
	wxTextCtrl *log_file_ = nullptr;
	wxCheckBox *append_log_ = nullptr;
	wxCheckBox *open_log_ = nullptr;
	wxTextCtrl *file_format_ = nullptr;
	wxTextCtrl *insert_before_ = nullptr;
	wxTextCtrl *insert_after_ = nullptr;
	wxCheckBox *trim_left_ = nullptr;
	wxCheckBox *replace_tab_ = nullptr;
	wxCheckBox *replace_cr_ = nullptr;
	wxTextCtrl *replacement_ = nullptr;
	wxTextCtrl *sample_ = nullptr;
};

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, grep_opt::Options &options, bool replace_mode)
{
	GrepOptionsDialog dlg(parent, options, replace_mode);
	if (dlg.ShowModal() != wxID_OK) return false;
	options = dlg.Values();
	return true;
}

}  // namespace grep_opt_dialog
