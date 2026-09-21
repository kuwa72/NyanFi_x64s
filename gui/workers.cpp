/**
 * @file gui/workers.cpp
 * @brief gui/workers.h の実装 (wx 非依存)
 */
#include "gui/workers.h"

namespace workers {

StepAction DecideStep(bool cancelled, bool paused)
{
	// src/task_thread.cpp ProgressCore 171-178行: 一旦停止中は WaitIfPause 後に
	// cancel を見直すが、ここでは判断だけを返す。中断が最優先なのは VCL と同じ
	if (cancelled) return StepAction::Cancel;
	if (paused) return StepAction::Wait;
	return StepAction::Continue;
}

double ProgressRatio(long long total, long long transferred)
{
	// ProgressCore 181行: (TotalFileSize>0)? rate : -1
	if (total <= 0) return -1.0;
	return 1.0 * static_cast<double>(transferred) / static_cast<double>(total);
}

int TransferSpeed(long long sampled_bytes, int elapsed_ms)
{
	// ProgressCore 189-193行: 1000ms 超ごとに SmplSize/smpl_cnt
	if (elapsed_ms <= 0) return 0;
	return static_cast<int>(sampled_bytes / elapsed_ms);
}

int RemainingMs(long long total, long long transferred, int speed_bytes_per_ms)
{
	// ProgressCore 196行: (Speed>0)? (Total-Transferred)/Speed : 0
	if (speed_bytes_per_ms <= 0) return 0;
	const long long rest = total - transferred;
	if (rest <= 0) return 0;
	return static_cast<int>(rest / speed_bytes_per_ms);
}

ThumbFit FitThumb(int w, int h, int max_size)
{
	// src/thumb_thread.cpp FitSize 73-92行: どちらかが上限超えのときだけ縮小
	if (max_size <= 0) return ThumbFit{w, h, false};
	if (w > max_size || h > max_size) {
		if (w > h) {
			// 横の方が長い: 幅を上限に合わせる
			const double r = 1.0 * static_cast<double>(h) / static_cast<double>(w);
			w = max_size;
			h = static_cast<int>(w * r);
		}
		else {
			// 縦の方が長い (正方形を含む): 高さを上限に合わせる
			const double r = 1.0 * static_cast<double>(w) / static_cast<double>(h);
			h = max_size;
			w = static_cast<int>(h * r);
		}
		return ThumbFit{w, h, true};
	}
	return ThumbFit{w, h, false};
}

std::vector<int> ThumbOrder(int count, int start)
{
	// src/thumb_thread.cpp Execute 240-271行の tag/n/p による交互走査。
	// VCL は ReqClear が来たら途中でやめるが、ここでは全件の順序だけを返す
	// (中断は ProcessBatched/CancelFlag の仕事)。
	if (count <= 0 || start < 0 || start >= count) return {};

	std::vector<int> order;
	order.reserve(static_cast<std::size_t>(count));

	int idx = start;
	int n = start;
	int p = start;
	int tag = 0;
	for (int i = 0; i < count; ++i) {
		switch (tag) {
		case 0:
			tag = 1;
			break;
		case 1:
			tag = (++n < count) ? 2 : 4;
			idx = (tag == 2) ? n : ((--p >= 0) ? p : -1);
			break;
		case 2:
			tag = ((--p >= 0) ? 1 : 3);
			idx = (tag == 1) ? p : ((++n < count) ? n : -1);
			break;
		case 3:
			idx = ((++n < count) ? n : -1);
			break;
		default:  // case 4
			idx = ((--p >= 0) ? p : -1);
			break;
		}
		if (idx == -1) break;
		order.push_back(idx);
	}
	return order;
}

int IconEvictCount(int count, int limit)
{
	// src/icon_thread.cpp Execute 44-48行: 上限を超えた先頭分を捨てる
	if (limit < 0) limit = 0;
	return (count > limit) ? (count - limit) : 0;
}

std::vector<int> IconPendingIndices(const std::vector<char> &has_icon)
{
	// src/icon_thread.cpp Execute 56-68行: Objects[i] が無いものだけ取得対象
	std::vector<int> pending;
	for (std::size_t i = 0; i < has_icon.size(); ++i) {
		if (!has_icon[i]) pending.push_back(static_cast<int>(i));
	}
	return pending;
}

bool ShouldNotify(int now_ms, int last_ms, int pending)
{
	// src/icon_thread.cpp Execute 84行: 200ms 超かつ取得ありで通知
	return pending > 0 && (now_ms - last_ms) > 200;
}

bool ShouldReportGrepProgress(int files_scanned)
{
	// gui/grep_dialog.cpp の Pulse 間引き (files % 20) と同じ規則。
	// worker_thread の進捗イベント間引きと共有する
	return files_scanned > 0 && (files_scanned % 20) == 0;
}

BatchResult ProcessBatched(int total, const BatchCancelCallback &cancel_cb,
                           const BatchProgressCallback &progress_cb)
{
	BatchResult r;
	if (total <= 0) return r;
	for (int i = 0; i < total; ++i) {
		// grep の WalkAndSearch・task の各ループと同様、1件の前に中断を見る
		if (cancel_cb && cancel_cb()) {
			r.cancelled = true;
			break;
		}
		++r.processed;
		if (progress_cb) progress_cb(r.processed, total);
	}
	return r;
}

}  // namespace workers
