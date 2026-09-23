/**
 * @file gui/task_man_dialog.cpp
 * @brief gui/task_man_dialog.h の実装
 */
#include "gui/task_man_dialog.h"

#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>

namespace task_man_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

}  // namespace

namespace {

//---------------------------------------------------------------------------
/**
 * @brief タスク一覧と状態操作ボタンの wx 再構成
 * @details `src/TaskDlg.cpp:99-194` の行生成を state のスナップショットから
 *          再構成し、`src/TaskDlg.cpp:292-404` の Action を状態フラグだけ
 *          に反映する。TTaskThread/TaskReserveList は実体が無いため、
 *          進捗・速度・タイマー更新と予約開始は未実装 (未実装扱い)。
 */
class TaskManagerDialog final : public wxDialog {
public:
	TaskManagerDialog(wxWindow *parent, task_man::State &state)
		: wxDialog(parent, wxID_ANY, to_wx(_T("タスクマネージャ")), wxDefaultPosition,
		           wxSize(760, 390), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, state_(state)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		list_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(740, 235),
		                        wxLC_REPORT | wxLC_SINGLE_SEL);
		list_->InsertColumn(0, to_wx(_T("#")), wxLIST_FORMAT_CENTER, 35);
		list_->InsertColumn(1, to_wx(_T("タスク")), wxLIST_FORMAT_LEFT, 150);
		list_->InsertColumn(2, to_wx(_T("詳細")), wxLIST_FORMAT_LEFT, 245);
		list_->InsertColumn(3, to_wx(_T("残")), wxLIST_FORMAT_RIGHT, 55);
		list_->InsertColumn(4, to_wx(_T("状態")), wxLIST_FORMAT_LEFT, 105);
		list_->InsertColumn(5, to_wx(_T("経過時間")), wxLIST_FORMAT_LEFT, 120);
		top->Add(list_, wxSizerFlags(1).Expand().Border(wxALL, 8));
		list_->Bind(wxEVT_LIST_ITEM_SELECTED, &TaskManagerDialog::OnSelect, this);
		list_->Bind(wxEVT_LIST_ITEM_ACTIVATED, &TaskManagerDialog::OnSelect, this);

		status_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		top->Add(status_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxBoxSizer *actions = new wxBoxSizer(wxHORIZONTAL);
		cancel_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("中止")));
		cancel_all_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("すべて中止")));
		pause_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("一旦停止")));
		restart_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("再開")));
		suspend_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("保留")));
		start_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("開始")));
		ext_start_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("割り込み実行")));
		actions->Add(cancel_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(cancel_all_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(pause_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(restart_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(suspend_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(start_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(ext_start_btn_);
		top->Add(actions, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxStaticText *note = new wxStaticText(
			this, wxID_ANY,
			to_wx(_T("※ TTaskThread のコピー/移動、進捗タイマー、TaskReserveList、"
			         _T("予約開始は未移植 (未実装扱い) です。状態操作は MainFrame の")
			         _T(" pause/cancel 状態だけを更新します。"))));
		note->Wrap(720);
		top->Add(note, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxBoxSizer *bottom = new wxBoxSizer(wxHORIZONTAL);
		bottom->AddStretchSpacer();
		bottom->Add(new wxButton(this, wxID_OK, to_wx(_T("閉じる"))), wxSizerFlags().Border(wxRIGHT, 4));
		bottom->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("キャンセル"))));
		top->Add(bottom, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		cancel_btn_->Bind(wxEVT_BUTTON, &TaskManagerDialog::OnCancel, this);
		cancel_all_btn_->Bind(wxEVT_BUTTON, &TaskManagerDialog::OnCancelAll, this);
		pause_btn_->Bind(wxEVT_BUTTON, &TaskManagerDialog::OnPause, this);
		restart_btn_->Bind(wxEVT_BUTTON, &TaskManagerDialog::OnRestart, this);
		suspend_btn_->Bind(wxEVT_BUTTON, &TaskManagerDialog::OnSuspend, this);
		start_btn_->Bind(wxEVT_BUTTON, &TaskManagerDialog::OnStartUnimplemented, this);
		ext_start_btn_->Bind(wxEVT_BUTTON, &TaskManagerDialog::OnStartUnimplemented, this);
		Bind(wxEVT_BUTTON, &TaskManagerDialog::OnOk, this, wxID_OK);

		RefreshList();
		if (!rows_.empty()) SelectIndex(0);
		UpdateButtons();
	}

private:
	int SelectedIndex() const
	{
		return list_ ? list_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) : -1;
	}

	void SelectIndex(int index)
	{
		if (index < 0 || index >= static_cast<int>(rows_.size())) return;
		list_->SetItemState(index, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
		                    wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
		UpdateButtons();
	}

	void RefreshList()
	{
		const int old = SelectedIndex();
		rows_ = task_man::BuildRows(state_);
		list_->DeleteAllItems();
		for (std::size_t i = 0; i < rows_.size(); ++i) {
			const task_man::TaskRow &row = rows_[i];
			const long line = list_->InsertItem(static_cast<long>(i), to_wx(IntToStr(row.number)));
			list_->SetItem(line, 1, to_wx(row.command));
			list_->SetItem(line, 2, to_wx(row.detail));
			list_->SetItem(line, 3, to_wx(row.reserved ? _T("-") : _T("未移植")));
			list_->SetItem(line, 4, to_wx(task_man::StatusText(row)));
			// 実スレッドの開始時刻/進捗はないので、黙って 0 秒にしない。
			list_->SetItem(line, 5, to_wx(row.reserved ? _T("-") : _T("未移植")));
		}
		status_->SetLabel(to_wx(task_man::Summary(state_) + _T("    ") +
		                        f_misc_ops::PauseAllCaption(task_man::PausedCount(state_))));
		if (old >= 0 && old < static_cast<int>(rows_.size())) SelectIndex(old);
		else if (!rows_.empty()) SelectIndex(0);
	}

	void UpdateButtons()
	{
		const int index = SelectedIndex();
		// 実タスクの空きスロットは MainFrame に未移植なので常に false。
		const task_man::Actions actions = task_man::ResolveActions(
			state_, index, state_.reserved_count, /*has_empty_slot=*/false,
			/*has_force_empty_slot=*/false);
		cancel_btn_->Enable(actions.cancel);
		cancel_all_btn_->Enable(actions.cancel_all);
		pause_btn_->Enable(actions.pause);
		restart_btn_->Enable(actions.restart);
		suspend_btn_->Enable(actions.suspend);
		start_btn_->Enable(actions.start);
		ext_start_btn_->Enable(actions.ext_start);
		suspend_btn_->SetLabel(to_wx(f_misc_ops::SuspendCaption(state_.suspended)));
	}

	void OnSelect(wxCommandEvent &) { UpdateButtons(); }

	void OnCancel(wxCommandEvent &)
	{
		const int index = SelectedIndex();
		if (index >= task_man::BusyCount(state_)) {
			ShowUnimplemented(_T("予約タスクの削除/実スレッドの中断"));
			return;
		}
		if (task_man::RequestCancel(state_, index)) {
			RefreshList();
		}
	}

	void OnCancelAll(wxCommandEvent &)
	{
		if (wxMessageBox(to_wx(_T("すべてのタスクを中止しますか?\n"
		                          _T("(実スレッドの中断は未移植)"))) ,
		                 to_wx(_T("タスクマネージャ")), wxYES_NO | wxICON_QUESTION, this) != wxYES)
			return;
		task_man::RequestCancelAll(state_);
		RefreshList();
	}

	void OnPause(wxCommandEvent &)
	{
		if (task_man::PauseSelected(state_, SelectedIndex())) RefreshList();
	}

	void OnRestart(wxCommandEvent &)
	{
		if (task_man::RestartSelected(state_, SelectedIndex())) RefreshList();
	}

	void OnSuspend(wxCommandEvent &)
	{
		task_man::ToggleSuspend(state_, EmptyStr);
		RefreshList();
		if (!state_.suspended) {
			wxMessageBox(to_wx(_T("予約の解除は状態だけ更新しました。\n"
			                      _T("StartReserve の実処理は未移植 (未実装扱い) です。"))),
			             to_wx(_T("タスクマネージャ")), wxOK | wxICON_INFORMATION, this);
		}
	}

	void OnStartUnimplemented(wxCommandEvent &event)
	{
		wxObject *source = event.GetEventObject();
		ShowUnimplemented(source == start_btn_ ? _T("予約タスクの開始")
		                                       : _T("予約タスクの割り込み実行"));
	}

	void ShowUnimplemented(const UnicodeString &feature)
	{
		wxMessageBox(to_wx(feature + _T("は未移植 (未実装扱い) です")),
		             to_wx(_T("タスクマネージャ")), wxOK | wxICON_WARNING, this);
	}

	void OnOk(wxCommandEvent &event)
	{
		EndModal(wxID_OK);
		event.Skip();
	}

	task_man::State &state_;
	std::vector<task_man::TaskRow> rows_;
	wxListCtrl *list_ = nullptr;
	wxStaticText *status_ = nullptr;
	wxButton *cancel_btn_ = nullptr;
	wxButton *cancel_all_btn_ = nullptr;
	wxButton *pause_btn_ = nullptr;
	wxButton *restart_btn_ = nullptr;
	wxButton *suspend_btn_ = nullptr;
	wxButton *start_btn_ = nullptr;
	wxButton *ext_start_btn_ = nullptr;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, task_man::State &state)
{
	TaskManagerDialog dlg(parent, state);
	return dlg.ShowModal() == wxID_OK;
}

}  // namespace task_man_dialog
