/**
 * @file gui/worker_thread.h
 * @brief grep/task/thumb/icon を wxThread で実行する共通層 (Issue #41)
 *
 * @details VCL の実測箇所を先に確認してから、GUI に依存しない判断を
 *          gui/workers.h へ移し、その wx 側の接続だけをこのファイルに置く。
 *          2026-09-24 実読した該当行は次のとおり。
 *          - task: src/task_thread.cpp ProgressCore (169-213行)、TaskStart
 *            (1863-1873行)、FinishTask (1878-1896行)、Execute (1899-1963行)
 *          - thumb: src/thumb_thread.cpp FitSize (73-92行)、MakeThumbnail
 *            (95-231行)、Execute (234-297行)
 *          - icon: src/icon_thread.cpp Execute (40-96行)
 *
 *          4クラスとも wxTHREAD_JOINABLE + QueueEvent とし、完了結果は
 *          vector などをイベントに載せず Wait() 後の Result() で読む。
 *          wxEVT_WORKER_PROGRESS は Int=処理済み、ExtraLong=全体件数、
 *          wxEVT_WORKER_DONE は Int=中断なら1。イベントは GUI スレッドで処理する。
 *
 *          実接続の例は MainFrame::CmdGetHash。
 *          未移植 (未実装扱い): FTP/Git 連携タスク、書庫の実コピー/移動スレッド、
 *          OS 依存の実アイコン抽出。既存の同期/簡略表示をこの PR で差し替えない。
 */
#ifndef NYANFI_GUI_WORKER_THREAD_H
#define NYANFI_GUI_WORKER_THREAD_H

#include <functional>
#include <vector>

#include <wx/event.h>
#include <wx/thread.h>

#include "gui/grep.h"
#include "gui/image_load.h"
#include "gui/workers.h"

wxDECLARE_EVENT(wxEVT_WORKER_PROGRESS, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_WORKER_DONE, wxThreadEvent);
// grep は既存 dialog との互換のため専用イベントも公開する
wxDECLARE_EVENT(wxEVT_GREP_PROGRESS, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_GREP_DONE, wxThreadEvent);

namespace worker_thread {

/**
 * @brief CancelAllTask などに渡すための cancellable wxThread 基底
 * @details 所有権と破棄は呼び出し側で行う。終了待ちは wxThread::Wait()。
 */
class CancelableWorkerThread : public wxThread {
public:
	explicit CancelableWorkerThread() : wxThread(wxTHREAD_JOINABLE) {}

	/// 実行中の処理を中断要求する
	virtual void RequestCancel() = 0;
};

//---------------------------------------------------------------------------
// 汎用バッチワーカー
//---------------------------------------------------------------------------

/**
 * @brief total 件の処理を逐次実行する wxThread
 * @details 1件ごとに CancelFlag を確認し、item_cb、進捗イベントの順に処理する。
 *          ProcessBatched の item_cb 引数を使い、中断境界を一箇所に固定する。
 */
class BatchedWorkerThread : public CancelableWorkerThread {
public:
	/**
	 * @param owner イベント送り先 (非所有)
	 * @param total 処理件数
	 * @param item_cb 0始まりの1件処理
	 * @param cancel 中断フラグ (非所有)
	 * @param progress_interval イベント通知間隔。1なら1件ずつ
	 */
	BatchedWorkerThread(wxEvtHandler *owner, int total,
	                    workers::BatchItemCallback item_cb,
	                    workers::CancelFlag *cancel = nullptr,
	                    int progress_interval = 1);

	virtual ExitCode Entry() override;
	void RequestCancel() override;

	/// Wait() 後に読む結果
	const workers::BatchResult &Result() const { return result_; }

private:
	void QueueProgress(int processed, int total) const;
	void QueueDone() const;

	wxEvtHandler *owner_;
	int total_;
	workers::BatchItemCallback item_cb_;
	workers::CancelFlag *cancel_;
	int progress_interval_;
	workers::BatchResult result_;
};

//---------------------------------------------------------------------------
// サムネイル候補ワーカー
//---------------------------------------------------------------------------

/** 1候補の読み込み結果。実画像は gui/image_load の RGB24 を保持する。 */
struct ThumbCandidate {
	int source_index = -1;
	UnicodeString path;
	image_load::LoadResult image;
	workers::ThumbFit fit;
};

/** ThumbWorkerThread の完了結果 */
struct ThumbWorkerResult {
	std::vector<ThumbCandidate> candidates;
	int processed = 0;
	bool cancelled = false;
};

/**
 * @brief ThumbOrder の順で画像候補を読み込む wxThread
 * @details VCL の FitSize/Execute の契約を image_load::LoadForView と
 *          workers::FitThumb に接続する。OS 依存の wxImage 構築や画面表示は
 *          呼び出し側の責務。
 */
class ThumbWorkerThread : public CancelableWorkerThread {
public:
	/**
	 * @param paths 候補ファイル
	 * @param start_index 現在位置 (VCL StartIndex)
	 * @param max_candidates 候補上限。負なら全件
	 * @param max_size サムネイル上限 (VCL ThumbnailSize の既定 120)
	 */
	ThumbWorkerThread(wxEvtHandler *owner, std::vector<UnicodeString> paths,
	                  int start_index = 0, int max_candidates = -1,
	                  int max_size = 120, workers::CancelFlag *cancel = nullptr);

	virtual ExitCode Entry() override;
	void RequestCancel() override;

	const ThumbWorkerResult &Result() const { return result_; }

private:
	void QueueProgress(int processed, int total) const;
	void QueueDone() const;

	wxEvtHandler *owner_;
	std::vector<UnicodeString> paths_;
	int start_index_;
	int max_candidates_;
	int max_size_;
	workers::CancelFlag *cancel_;
	ThumbWorkerResult result_;
};

//---------------------------------------------------------------------------
// アイコン取得ワーカー
//---------------------------------------------------------------------------

/**
 * @brief 実アイコン抽出は呼び出し側のコールバックに任せる
 * @details 戻り値は取得成功なら true。実装は wx/GDI に依存しないため、
 *          OS 依存の get_file_SmallIcon はこの PR に移植しない。
 */
using IconExtractCallback = std::function<bool(int index, const UnicodeString &path)>;
/// キャッシュ上限超過分を実際の側へ反映するコールバック (件数だけ渡す)
using IconEvictCallback = std::function<void(int count)>;

/** IconWorkerThread の完了結果 */
struct IconWorkerResult {
	std::vector<int> loaded_indices;
	int evicted = 0;
	int processed = 0;
	int notifications = 0;
	bool cancelled = false;
};

/**
 * @brief 未取得アイコンを Cache 制約付きで処理する wxThread
 * @details IconPendingIndices/IconEvictCount/ShouldNotify をそのまま使う。
 *          実抽出 (get_file_SmallIcon) は IconExtractCallback で外から渡す。
 */
class IconWorkerThread : public CancelableWorkerThread {
public:
	/**
	 * @param paths アイコン対象 (cache_count=-1 なら paths.size() を現在件数とする)
	 * @param has_icon 各添字の取得済みフラグ
	 * @param cache_limit キャッシュ上限
	 * @param extract 実抽出コールバック
	 * @param cancel 中断フラグ
	 * @param evict 破棄件数を受け取るコールバック
	 * @param cache_count 現在キャッシュ件数
	 */
	IconWorkerThread(wxEvtHandler *owner, std::vector<UnicodeString> paths,
	                 std::vector<char> has_icon, int cache_limit,
	                 IconExtractCallback extract, workers::CancelFlag *cancel = nullptr,
	                 IconEvictCallback evict = nullptr, int cache_count = -1);

	virtual ExitCode Entry() override;
	void RequestCancel() override;

	const IconWorkerResult &Result() const { return result_; }

private:
	void QueueProgress(int processed, int total) const;
	void QueueDone() const;

	wxEvtHandler *owner_;
	std::vector<UnicodeString> paths_;
	std::vector<char> has_icon_;
	int cache_limit_;
	IconExtractCallback extract_;
	workers::CancelFlag *cancel_;
	IconEvictCallback evict_;
	int cache_count_;
	IconWorkerResult result_;
};

//---------------------------------------------------------------------------
// grep (既存接続)
//---------------------------------------------------------------------------

/** grep の進捗・完了イベントと結果の契約は従来どおり。 */
class GrepWorkerThread : public CancelableWorkerThread {
public:
	GrepWorkerThread(wxEvtHandler *owner, const UnicodeString &dir,
	                 const grep_core::GrepOptions &opt, const grep_core::GrepLimits &limits,
	                 workers::CancelFlag *cancel);

	virtual ExitCode Entry() override;
	void RequestCancel() override;

	/// Wait() で終了確認後に読む
	const grep_core::GrepResult &Result() const { return result_; }

private:
	wxEvtHandler *owner_;
	UnicodeString dir_;
	grep_core::GrepOptions opt_;
	grep_core::GrepLimits limits_;
	workers::CancelFlag *cancel_;
	grep_core::GrepResult result_;
};

}  // namespace worker_thread

#endif  // NYANFI_GUI_WORKER_THREAD_H
