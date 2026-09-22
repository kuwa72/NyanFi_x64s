/**
 * @file gui/grep_dialog.cpp
 * @brief gui/grep_dialog.h の実装
 */
#include "gui/grep_dialog.h"

#include <memory>
#include <vector>

#include <wx/checkbox.h>
#include <wx/listctrl.h>
#include <wx/progdlg.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/utils.h>

#include "usr_str.h"
#include "gui/worker_thread.h"
#include "gui/workers.h"

namespace grep_dialog {

namespace {

/// wxString への変換 (UnicodeString は UTF-16、wxString も MSW では UTF-16。
/// gui/main_frame.cpp 等と同じ変換ヘルパー)
inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

/// wxString → UnicodeString (MSW では両方 UTF-16)
inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

//---------------------------------------------------------------------------
/**
 * @brief 検索条件の入力ダイアログ
 * @details 検索対象ディレクトリは固定 (要件1: 現在のペインのディレクトリ)
 * のため、入力できるのは検索文字列・ファイル名マスク・サブディレクトリを
 * 含むか・正規表現として扱うか・大小文字を区別するかの5点のみ
 */
class GrepInputDialog : public wxDialog {
public:
	GrepInputDialog(wxWindow *parent, const UnicodeString &dir, const UnicodeString &initial_mask)
		: wxDialog(parent, wxID_ANY, to_wx(_T("文字列検索 (GREP)")), wxDefaultPosition, wxDefaultSize,
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		top->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("検索対象: ") + dir)),
		         wxSizerFlags().Border(wxALL, 8));

		wxFlexGridSizer *grid = new wxFlexGridSizer(2, 4, 8);
		grid->AddGrowableCol(1);

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("検索文字列"))),
		          wxSizerFlags().CentreVertical());
		keyword_ctrl_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(280, -1));
		grid->Add(keyword_ctrl_, wxSizerFlags(1).Expand());

		grid->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("ファイル名マスク"))),
		          wxSizerFlags().CentreVertical());
		mask_ctrl_ = new wxTextCtrl(this, wxID_ANY, to_wx(initial_mask.IsEmpty() ? _T("*") : initial_mask));
		grid->Add(mask_ctrl_, wxSizerFlags(1).Expand());

		top->Add(grid, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));

		recursive_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("サブディレクトリを含む")));
		regex_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("正規表現として扱う")));
		case_chk_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("大小文字を区別する")));
		top->Add(recursive_chk_, wxSizerFlags().Border(wxALL, 8));
		top->Add(regex_chk_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
		top->Add(case_chk_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		keyword_ctrl_->SetFocus();

		// OK は検証してから閉じる (キーワード必須・正規表現の妥当性)。
		// wxID_OK にバインドして EndModal を自前で呼ぶことで、
		// 検証に失敗したときはダイアログを閉じずにやり直せるようにする
		Bind(wxEVT_BUTTON, &GrepInputDialog::OnOk, this, wxID_OK);
	}

	grep_core::GrepOptions Options() const
	{
		grep_core::GrepOptions opt;
		opt.keyword = to_us(keyword_ctrl_->GetValue());
		opt.mask = to_us(mask_ctrl_->GetValue());
		opt.recursive = recursive_chk_->GetValue();
		opt.use_regex = regex_chk_->GetValue();
		opt.case_sensitive = case_chk_->GetValue();
		return opt;
	}

private:
	void OnOk(wxCommandEvent &event)
	{
		const grep_core::GrepOptions opt = Options();

		if (opt.keyword.Trim().IsEmpty()) {
			wxMessageBox(to_wx(_T("検索文字列を入力してください")), to_wx(_T("文字列検索 (GREP)")),
			             wxOK | wxICON_WARNING, this);
			return;
		}

		// 正規表現の事前検証。grep_core::SearchDirectory も同じ検証をするが、
		// ここで確認すればダイアログを閉じずにやり直させられる
		if (opt.use_regex) {
			try {
				TRegExOptions re_opt;
				if (!opt.case_sensitive) re_opt << roIgnoreCase;
				TRegEx test(opt.keyword, re_opt);
			}
			catch (...) {
				wxMessageBox(to_wx(_T("正規表現が不正です")), to_wx(_T("文字列検索 (GREP)")),
				             wxOK | wxICON_ERROR, this);
				return;
			}
		}

		EndModal(wxID_OK);
	}

	wxTextCtrl *keyword_ctrl_ = nullptr;
	wxTextCtrl *mask_ctrl_ = nullptr;
	wxCheckBox *recursive_chk_ = nullptr;
	wxCheckBox *regex_chk_ = nullptr;
	wxCheckBox *case_chk_ = nullptr;
};

//---------------------------------------------------------------------------
/**
 * @brief 検索結果一覧ダイアログ
 * @details 「ファイル名:行番号:行の内容」(要件5) を3列の wxListCtrl で表示する。
 * ダブルクリックまたは Enter (wxListCtrl は既定でどちらも
 * wxEVT_LIST_ITEM_ACTIVATED を発生させる) で選択して閉じる
 */
class GrepResultsDialog : public wxDialog {
public:
	GrepResultsDialog(wxWindow *parent, const UnicodeString &dir, const grep_core::GrepResult &result)
		: wxDialog(parent, wxID_ANY, to_wx(_T("検索結果")), wxDefaultPosition, wxSize(760, 480),
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, dir_(dir)
		, matches_(result.matches)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		top->Add(new wxStaticText(this, wxID_ANY, to_wx(Summary(result))),
		         wxSizerFlags().Expand().Border(wxALL, 8));

		list_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		                        wxLC_REPORT | wxLC_SINGLE_SEL);
		list_->AppendColumn(to_wx(_T("ファイル")), wxLIST_FORMAT_LEFT, 320);
		list_->AppendColumn(to_wx(_T("行")), wxLIST_FORMAT_RIGHT, 60);
		list_->AppendColumn(to_wx(_T("内容")), wxLIST_FORMAT_LEFT, 340);

		for (std::size_t i = 0; i < matches_.size(); ++i) {
			const grep_core::GrepMatch &m = matches_[i];
			const UnicodeString rel = RelativePath(dir_, m.file);

			const long row = list_->InsertItem(static_cast<long>(i), to_wx(rel));
			wxString line_no;
			line_no.Printf("%d", m.line);
			list_->SetItem(row, 1, line_no);
			list_->SetItem(row, 2, to_wx(m.text));
		}

		top->Add(list_, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT, 8));

		wxBoxSizer *buttons = new wxBoxSizer(wxHORIZONTAL);
		buttons->Add(new wxStaticText(this, wxID_ANY,
		                               to_wx(_T("ダブルクリックまたは Enter でビューアを開きます"))),
		             wxSizerFlags(1).CentreVertical());
		buttons->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("閉じる"))));
		top->Add(buttons, wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		if (list_->GetItemCount() > 0) {
			list_->SetItemState(0, wxLIST_STATE_FOCUSED | wxLIST_STATE_SELECTED,
			                     wxLIST_STATE_FOCUSED | wxLIST_STATE_SELECTED);
			list_->SetFocus();
		}

		list_->Bind(wxEVT_LIST_ITEM_ACTIVATED, &GrepResultsDialog::OnActivate, this);
	}

	/// 選ばれたマッチを取得する。ダブルクリック/Enter で選ばれていなければ false
	bool GetSelected(grep_core::GrepMatch &out) const
	{
		if (picked_index_ < 0 || picked_index_ >= static_cast<int>(matches_.size())) return false;
		out = matches_[static_cast<std::size_t>(picked_index_)];
		return true;
	}

private:
	void OnActivate(wxListEvent &event)
	{
		picked_index_ = static_cast<int>(event.GetIndex());
		EndModal(wxID_OK);
	}

	/// dir を先頭に持つ場合は相対パスにする (一覧を読みやすくするため)
	static UnicodeString RelativePath(const UnicodeString &dir, const UnicodeString &full_path)
	{
		if (StartsText(dir, full_path)) return full_path.SubString(dir.Length() + 1);
		return full_path;
	}

	/// 一覧上部の要約表示 (要件6・7: バイナリスキップ・上限による打ち切りを
	/// 黙って隠さず明示する)
	static UnicodeString Summary(const grep_core::GrepResult &result)
	{
		UnicodeString s;
		s.sprintf(_T("%d 件一致 (%d ファイルを検索"), static_cast<int>(result.matches.size()),
		          result.files_scanned);
		if (result.files_skipped_binary > 0) {
			s.cat_sprintf(_T("、バイナリ %d 件はスキップ"), result.files_skipped_binary);
		}
		if (result.files_truncated > 0) {
			s.cat_sprintf(_T("、%d 件は大きいため先頭のみ検索"), result.files_truncated);
		}
		s += _T(")");
		if (result.stopped_by_file_limit) {
			s += _T("\n※ ファイル数の上限に達したため、途中で打ち切りました");
		}
		if (result.stopped_by_match_limit) {
			s += _T("\n※ 一致件数の上限に達したため、途中で打ち切りました");
		}
		if (result.cancelled) {
			s += _T("\n※ 中断されました (途中経過を表示しています)");
		}
		return s;
	}

	UnicodeString dir_;
	std::vector<grep_core::GrepMatch> matches_;
	wxListCtrl *list_ = nullptr;
	int picked_index_ = -1;
};

}  // namespace

//---------------------------------------------------------------------------
/**
 * @brief GrepWorkerThread のイベント受け口 (Issue #41 batch3)
 * @details worker_thread::GrepWorkerThread が QueueEvent する
 *   wxEVT_GREP_PROGRESS (Int=走査ファイル数、ExtraLong=一致件数) と
 *   wxEVT_GREP_DONE (Int=中断なら1) を workers::GrepAsyncState に畳む。
 *   イベントの受け渡し (Bind/QueueEvent) だけがここの仕事で、畳み込みの
 *   契約自体は tests/core/test_gui_workers.cpp で担保する
 */
class GrepSearchSession : public wxEvtHandler {
public:
	GrepSearchSession()
	{
		Bind(wxEVT_GREP_PROGRESS, &GrepSearchSession::OnProgress, this);
		Bind(wxEVT_GREP_DONE, &GrepSearchSession::OnDone, this);
	}

	const workers::GrepAsyncState &State() const { return state_; }
	bool IsDone() const { return state_.done; }

private:
	void OnProgress(wxThreadEvent &event)
	{
		state_.OnProgress(event.GetInt(), static_cast<int>(event.GetExtraLong()));
	}

	void OnDone(wxThreadEvent &event) { state_.OnDone(event.GetInt() != 0); }

	workers::GrepAsyncState state_;
};

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, const UnicodeString &dir, const UnicodeString &initial_mask,
         grep_core::GrepMatch &selected, std::vector<UnicodeString> &matched_files_out)
{
	matched_files_out.clear();

	GrepInputDialog input(parent, dir, initial_mask);
	if (input.ShowModal() != wxID_OK) return false;

	const grep_core::GrepOptions opt = input.Options();

	// 非同期検索 (Issue #41 batch3)。GrepWorkerThread で走査し、GUI スレッドは
	// 進捗表示の更新と中断の受付だけを行う。総ファイル数は事前に数えないため
	// (それ自体に時間がかかり、走査を二度行うことになる)、確定的な進捗率では
	// なく Pulse (不定進捗) で「動いている」ことと途中経過の件数だけを示す
	workers::CancelFlag cancel_flag;
	GrepSearchSession session;
	worker_thread::GrepWorkerThread *thread =
		new worker_thread::GrepWorkerThread(&session, dir, opt, grep_core::GrepLimits(), &cancel_flag);

	grep_core::GrepResult result;
	if (thread->Run() != wxTHREAD_NO_ERROR) {
		// スレッド起動に失敗したときだけ同期実行に fallback する (稀な経路)。
		// 結果の扱いは非同期の場合と同じ
		delete thread;
		grep_core::GrepCancelCallback no_cancel = []() { return false; };
		result = grep_core::SearchDirectory(dir, opt, grep_core::GrepLimits(), no_cancel, nullptr);
	}
	else {
		wxProgressDialog progress(to_wx(_T("検索中")), to_wx(_T("検索しています...")), 100, parent,
		                           wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_AUTO_HIDE | wxPD_ELAPSED_TIME);
		while (!session.IsDone()) {
			UnicodeString msg;
			msg.sprintf(_T("%d ファイルを検索 (%d 件一致)"), session.State().files_scanned,
			            session.State().matches_found);
			// Pulse は内部でイベントを配送するため、ワーカーからの
			// wxEVT_GREP_PROGRESS/DONE はこの待ちの間に届く
			if (!progress.Pulse(to_wx(msg))) cancel_flag.RequestCancel();
			if (progress.WasCancelled()) cancel_flag.RequestCancel();
			wxMilliSleep(20);
		}
		thread->Wait();
		result = thread->Result();
		delete thread;
	}

	if (!result.error.IsEmpty()) {
		wxMessageBox(to_wx(result.error), to_wx(_T("文字列検索 (GREP)")), wxOK | wxICON_ERROR, parent);
		return false;
	}

	if (result.matches.empty()) {
		UnicodeString msg = _T("一致する行が見つかりませんでした");
		if (result.cancelled) msg = _T("中断しました (一致する行はありませんでした)");
		wxMessageBox(to_wx(msg), to_wx(_T("文字列検索 (GREP)")), wxOK | wxICON_INFORMATION, parent);
		return false;
	}

	// 一致したファイルを重複を除いて集める (結果リストに出すため)。
	// 選ばれずに閉じられてもここは埋めておく
	for (const grep_core::GrepMatch &m : result.matches) {
		if (matched_files_out.empty() || !SameText(matched_files_out.back(), m.file)) {
			bool dup = false;
			for (const UnicodeString &f : matched_files_out) {
				if (SameText(f, m.file)) { dup = true; break; }
			}
			if (!dup) matched_files_out.push_back(m.file);
		}
	}

	GrepResultsDialog results(parent, dir, result);
	if (results.ShowModal() != wxID_OK) return false;

	return results.GetSelected(selected);
}

}  // namespace grep_dialog
