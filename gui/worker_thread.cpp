/**
 * @file gui/worker_thread.cpp
 * @brief gui/worker_thread.h の実装
 */
#include "gui/worker_thread.h"

wxDEFINE_EVENT(wxEVT_GREP_PROGRESS, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_GREP_DONE, wxThreadEvent);

namespace worker_thread {

GrepWorkerThread::GrepWorkerThread(wxEvtHandler *owner, const UnicodeString &dir,
                                   const grep_core::GrepOptions &opt,
                                   const grep_core::GrepLimits &limits,
                                   workers::CancelFlag *cancel)
	: wxThread(wxTHREAD_JOINABLE)
	, owner_(owner)
	, dir_(dir)
	, opt_(opt)
	, limits_(limits)
	, cancel_(cancel)
{
}

void GrepWorkerThread::RequestCancel()
{
	if (cancel_) cancel_->RequestCancel();
}

wxThread::ExitCode GrepWorkerThread::Entry()
{
	// ProcessBatched の契約との対応: SearchDirectory は1ファイル処理するたびに
	// 「cancel_cb を見て、処理して、progress_cb を呼ぶ」。ProcessBatched の
	// 「1件の前に中断を見て、処理後に進捗を呼ぶ」と同じ順序 (1ファイル=1単位)。
	grep_core::GrepCancelCallback cancel_cb = [this]() {
		return cancel_ && cancel_->IsCancelled();
	};
	grep_core::GrepProgressCallback progress_cb = [this](int files, int found) {
		// repaint/QueueEvent 自体が重いため workers 共有の間引き規則で絞る
		// (gui/grep_dialog.cpp と同じ 20件ごと)
		if (!workers::ShouldReportGrepProgress(files)) return;
		if (!owner_) return;
		wxThreadEvent *ev = new wxThreadEvent(wxEVT_GREP_PROGRESS);
		ev->SetInt(files);
		ev->SetExtraLong(found);
		// CallAfter 相当: イベントは GUI スレッドで処理される
		owner_->QueueEvent(ev);
	};

	result_ = grep_core::SearchDirectory(dir_, opt_, limits_, cancel_cb, progress_cb);

	if (owner_) {
		wxThreadEvent *done = new wxThreadEvent(wxEVT_GREP_DONE);
		done->SetInt(result_.cancelled ? 1 : 0);
		owner_->QueueEvent(done);
	}
	return static_cast<ExitCode>(0);
}

}  // namespace worker_thread
