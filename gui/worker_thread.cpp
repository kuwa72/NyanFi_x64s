/**
 * @file gui/worker_thread.cpp
 * @brief gui/worker_thread.h の実装
 *
 * @details VCL の task/thumb/icon は gui/workers.h の純関数を介して
 *          wxThread から呼ぶ。実測は task_thread.cpp 169-213/1863-1963行、
 *          thumb_thread.cpp 73-92/234-297行、icon_thread.cpp 40-96行。
 *          イベントの payload は件数と中断ビットだけにし、画像/RGB/アイコンの
 *          vector は Wait() 後の Result() で受け取る。
 */
#include "gui/worker_thread.h"

#include <chrono>
#include <utility>

#include <objbase.h>

wxDEFINE_EVENT(wxEVT_WORKER_PROGRESS, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_WORKER_DONE, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_GREP_PROGRESS, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_GREP_DONE, wxThreadEvent);

namespace worker_thread {
namespace {

int NowMs()
{
	// VCL の GetTickCount と同じ単調増加の ms 用途。時刻値が大きくなっても
	// int の ShouldNotify 契約へ安全に折り返す。
	const auto now = std::chrono::steady_clock::now().time_since_epoch();
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
	return static_cast<int>(ms & 0x7fffffffLL);
}

/// usr_wic.cpp の注記どおり、worker ごとに COM を初期化する。
class ComScope {
public:
	ComScope()
	{
		const HRESULT rc = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		uninitialize_ = SUCCEEDED(rc);
	}

	~ComScope()
	{
		if (uninitialize_) ::CoUninitialize();
	}

	ComScope(const ComScope &) = delete;
	ComScope &operator=(const ComScope &) = delete;

private:
	bool uninitialize_ = false;
};

}  // namespace

//---------------------------------------------------------------------------
// BatchedWorkerThread
//---------------------------------------------------------------------------
BatchedWorkerThread::BatchedWorkerThread(wxEvtHandler *owner, int total,
                                       workers::BatchItemCallback item_cb,
                                       workers::CancelFlag *cancel,
                                       int progress_interval)
	: CancelableWorkerThread()
	, owner_(owner)
	, total_(total)
	, item_cb_(std::move(item_cb))
	, cancel_(cancel)
	, progress_interval_(progress_interval)
{
}

void BatchedWorkerThread::RequestCancel()
{
	if (cancel_) cancel_->RequestCancel();
}

void BatchedWorkerThread::QueueProgress(int processed, int total) const
{
	if (!owner_) return;
	wxThreadEvent *event = new wxThreadEvent(wxEVT_WORKER_PROGRESS);
	event->SetInt(processed);
	event->SetExtraLong(total);
	owner_->QueueEvent(event);
}

void BatchedWorkerThread::QueueDone() const
{
	if (!owner_) return;
	wxThreadEvent *event = new wxThreadEvent(wxEVT_WORKER_DONE);
	event->SetInt(result_.cancelled ? 1 : 0);
	owner_->QueueEvent(event);
}

wxThread::ExitCode BatchedWorkerThread::Entry()
{
	workers::BatchCancelCallback cancel_cb = [this]() {
		return cancel_ && cancel_->IsCancelled();
	};
	workers::BatchProgressCallback progress_cb = [this](int processed, int total) {
		if (workers::ShouldReportProgress(processed, total, progress_interval_)) {
			QueueProgress(processed, total);
		}
	};

	try {
		result_ = workers::ProcessBatched(total_, cancel_cb, progress_cb, item_cb_);
	}
	catch (...) {
		// wxThread の例外を GUI まで戻さない。処理は不完全として中断扱いにする。
		result_.cancelled = true;
	}
	QueueDone();
	return static_cast<ExitCode>(0);
}

//---------------------------------------------------------------------------
// ThumbWorkerThread
//---------------------------------------------------------------------------
ThumbWorkerThread::ThumbWorkerThread(wxEvtHandler *owner,
                                     std::vector<UnicodeString> paths,
                                     int start_index, int max_candidates,
                                     int max_size, workers::CancelFlag *cancel)
	: CancelableWorkerThread()
	, owner_(owner)
	, paths_(std::move(paths))
	, start_index_(start_index)
	, max_candidates_(max_candidates)
	, max_size_(max_size)
	, cancel_(cancel)
{
}

void ThumbWorkerThread::RequestCancel()
{
	if (cancel_) cancel_->RequestCancel();
}

void ThumbWorkerThread::QueueProgress(int processed, int total) const
{
	if (!owner_) return;
	wxThreadEvent *event = new wxThreadEvent(wxEVT_WORKER_PROGRESS);
	event->SetInt(processed);
	event->SetExtraLong(total);
	owner_->QueueEvent(event);
}

void ThumbWorkerThread::QueueDone() const
{
	if (!owner_) return;
	wxThreadEvent *event = new wxThreadEvent(wxEVT_WORKER_DONE);
	event->SetInt(result_.cancelled ? 1 : 0);
	owner_->QueueEvent(event);
}

wxThread::ExitCode ThumbWorkerThread::Entry()
{
	// WIC ラッパーはスレッド毎の COM 初期化を要求する。
	ComScope com_scope;
	(void)com_scope;

	// src/thumb_thread.cpp Execute 240-268行の tag/n/p 交互走査を
	// gui/workers.h の純粋関数へ移し、上限だけをここで合成する。
	const std::vector<int> order = workers::ThumbCandidates(
		static_cast<int>(paths_.size()), start_index_, max_candidates_);

	workers::BatchCancelCallback cancel_cb = [this]() {
		return cancel_ && cancel_->IsCancelled();
	};
	workers::BatchProgressCallback progress_cb = [this](int processed, int total) {
		QueueProgress(processed, total);
	};
	workers::BatchItemCallback item_cb = [this, &order](int ordinal) {
		if (ordinal < 0 || ordinal >= static_cast<int>(order.size())) return;
		const int source_index = order[static_cast<std::size_t>(ordinal)];
		if (source_index < 0 || source_index >= static_cast<int>(paths_.size())) return;

		ThumbCandidate candidate;
		candidate.source_index = source_index;
		candidate.path = paths_[static_cast<std::size_t>(source_index)];
		try {
			// src/thumb_thread.cpp MakeThumbnail の WIC 読み込みを、
			// 移植済み image_load::LoadForView に接続する。
			candidate.image = image_load::LoadForView(candidate.path);
			if (candidate.image.ok) {
				candidate.fit = workers::FitThumb(
					static_cast<int>(candidate.image.width),
					static_cast<int>(candidate.image.height), max_size_);
			}
		}
		catch (...) {
			candidate.image = image_load::LoadResult();
		}
		result_.candidates.push_back(std::move(candidate));
	};

	try {
		const workers::BatchResult batch =
			workers::ProcessBatched(static_cast<int>(order.size()), cancel_cb,
			                        progress_cb, item_cb);
		result_.processed = batch.processed;
		result_.cancelled = batch.cancelled;
	}
	catch (...) {
		result_.cancelled = true;
	}
	QueueDone();
	return static_cast<ExitCode>(0);
}

//---------------------------------------------------------------------------
// IconWorkerThread
//---------------------------------------------------------------------------
IconWorkerThread::IconWorkerThread(wxEvtHandler *owner,
                                   std::vector<UnicodeString> paths,
                                   std::vector<char> has_icon, int cache_limit,
                                   IconExtractCallback extract,
                                   workers::CancelFlag *cancel,
                                   IconEvictCallback evict, int cache_count)
	: CancelableWorkerThread()
	, owner_(owner)
	, paths_(std::move(paths))
	, has_icon_(std::move(has_icon))
	, cache_limit_(cache_limit)
	, extract_(std::move(extract))
	, cancel_(cancel)
	, evict_(std::move(evict))
	, cache_count_(cache_count)
{
}

void IconWorkerThread::RequestCancel()
{
	if (cancel_) cancel_->RequestCancel();
}

void IconWorkerThread::QueueProgress(int processed, int total) const
{
	if (!owner_) return;
	wxThreadEvent *event = new wxThreadEvent(wxEVT_WORKER_PROGRESS);
	event->SetInt(processed);
	event->SetExtraLong(total);
	owner_->QueueEvent(event);
}

void IconWorkerThread::QueueDone() const
{
	if (!owner_) return;
	wxThreadEvent *event = new wxThreadEvent(wxEVT_WORKER_DONE);
	event->SetInt(result_.cancelled ? 1 : 0);
	owner_->QueueEvent(event);
}

wxThread::ExitCode IconWorkerThread::Entry()
{
	const int current_count = cache_count_ >= 0
		? cache_count_ : static_cast<int>(paths_.size());
	const int start_ms = NowMs();
	int last_notify_ms = start_ms;
	int pending_notifications = 0;

	const workers::IconBatchPlan plan = workers::PlanIconBatch(
		current_count, cache_limit_, has_icon_, start_ms, last_notify_ms);
	result_.evicted = plan.evict_count;
	if (result_.evicted > 0 && evict_) {
		try {
			evict_(result_.evicted);
		}
		catch (...) {
			// 破棄 callback の例外で worker 全体を落とさない。
		}
	}

	workers::BatchCancelCallback cancel_cb = [this]() {
		return cancel_ && cancel_->IsCancelled();
	};
	workers::BatchProgressCallback progress_cb = [this, &last_notify_ms,
	                                             &pending_notifications](int processed,
	                                                                          int total) {
		const int now = NowMs();
		if (workers::ShouldNotify(now, last_notify_ms, pending_notifications)) {
			++result_.notifications;
			last_notify_ms = now;
			pending_notifications = 0;
		}
		QueueProgress(processed, total);
	};
	workers::BatchItemCallback item_cb = [this, &plan, &pending_notifications](int ordinal) {
		if (ordinal < 0 || ordinal >= static_cast<int>(plan.pending_indices.size())) return;
		const int index = plan.pending_indices[static_cast<std::size_t>(ordinal)];
		if (index < 0 || index >= static_cast<int>(paths_.size()) || !extract_) return;
		try {
			if (extract_(index, paths_[static_cast<std::size_t>(index)])) {
				result_.loaded_indices.push_back(index);
				++pending_notifications;
			}
		}
		catch (...) {
			// 1件の OS 依存失敗をワーカー全体の失敗にしない。
		}
	};

	try {
		const workers::BatchResult batch = workers::ProcessBatched(
			static_cast<int>(plan.pending_indices.size()), cancel_cb, progress_cb, item_cb);
		result_.processed = batch.processed;
		result_.cancelled = batch.cancelled;
	}
	catch (...) {
		result_.cancelled = true;
	}

	// VCL Execute 91-92行の残件通知。通知時刻を待らず、取得済みの残りを
	// 最後の進捗イベントとして伝える。
	if (pending_notifications > 0) {
		++result_.notifications;
		QueueProgress(result_.processed,
		              static_cast<int>(plan.pending_indices.size()));
	}
	QueueDone();
	return static_cast<ExitCode>(0);
}

//---------------------------------------------------------------------------
// GrepWorkerThread (既存)
//---------------------------------------------------------------------------
GrepWorkerThread::GrepWorkerThread(wxEvtHandler *owner, const UnicodeString &dir,
                                   const grep_core::GrepOptions &opt,
                                   const grep_core::GrepLimits &limits,
                                   workers::CancelFlag *cancel)
	: CancelableWorkerThread()
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
	grep_core::GrepCancelCallback cancel_cb = [this]() {
		return cancel_ && cancel_->IsCancelled();
	};
	grep_core::GrepProgressCallback progress_cb = [this](int files, int found) {
		if (!workers::ShouldReportGrepProgress(files)) return;
		if (!owner_) return;
		wxThreadEvent *ev = new wxThreadEvent(wxEVT_GREP_PROGRESS);
		ev->SetInt(files);
		ev->SetExtraLong(found);
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
