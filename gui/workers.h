/**
 * @file gui/workers.h
 * @brief task/thumb/grep/icon の中断・進捗の純関数層 (wx 非依存, Issue #41)
 *
 * @details VCL 版の該当を実測して wx 非依存に切り出したもの (src 必読済み):
 *   - task: src/task_thread.cpp ProgressCore (169-213行) の比率・速度・残り時間と
 *     中断/一旦停止の判断、TaskStart (1863-1873行)/FinishTask (1878-1896行)/
 *     Execute (1899-1963行) の状態遷移のうち GUI 非依存の部分。コピー/移動の実体
 *     (CPY_core 等) は Global/UserFunc 依存のためここには含めない
 *   - thumb: src/thumb_thread.cpp FitSize (73-92行) と Execute (234-297行) の
 *     現在位置から前後交互に取得する順序。WIC デコード自体は
 *     gui/image_load.cpp の担当のため含めない
 *   - icon: src/icon_thread.cpp Execute (40-96行) のキャッシュ数制限と
 *     200ms ごとの通知。get_file_SmallIcon の実呼びは GUI 層の仕事のため含めない
 *   - grep: gui/grep.h の SearchDirectory は既に cancel/progress コールバックを
 *     持つため、ここではワーカー駆動の汎用バッチ実行 (ProcessBatched) として
 *     中断・進捗の契約だけを固定する
 *
 *   wx との接続は「wxThread::Entry が CancelFlag/DecideStep をポーリングし、
 *   ProcessBatched の単位で処理して CallAfter で進捗を投げる」想定。
 *   このヘッダ自体は wx を含まず、nyanfi_gui_core (ルート CMakeLists.txt) で
 *   ビルド・テストする (規約8)。
 *
 *   未移植 (未実装扱い): FTP/Git の外部連携タスク、書庫の実コピー/移動スレッド、
 *   OS 依存の実アイコン抽出。
 */
#ifndef NYANFI_GUI_WORKERS_H
#define NYANFI_GUI_WORKERS_H

#include <atomic>
#include <functional>
#include <vector>

namespace workers {

//-----------------------------------------------------------------------
// task: 中断/一旦停止の判断と進捗計算 (task_thread.cpp ProgressCore)
//-----------------------------------------------------------------------

/// 1ステップをどうするか (ProgressRoutine/ProgressCore の戻りに対応)
enum class StepAction {
	Continue,  ///< PROGRESS_CONTINUE。処理を続ける
	Cancel,    ///< PROGRESS_CANCEL。中断する
	Wait,      ///< 一旦停止中。WaitIfPause に対応 (呼び出し元が待つ)
};

/**
 * @brief 中断/一旦停止の判断 (ProgressCore 171-178行)
 * @param cancelled TaskCancel に対応
 * @param paused TaskPause に対応
 * @details VCL は pause 中に WaitIfPause してから cancel を見直すが、
 *          ここでは判断だけを純関数化する (待ち自体は呼び出し元の仕事)。
 *          中断が最優先なのは VCL と同じ。
 */
StepAction DecideStep(bool cancelled, bool paused);

/**
 * @brief 進捗率 (0.0〜1.0)。全体量が0以下なら -1 (VCL と同じ。ProgressCore 181行)
 */
double ProgressRatio(long long total, long long transferred);

/**
 * @brief 転送速度 (byte/ms)。経過が0以下なら 0 (ProgressCore 189-193行)
 * @param sampled_bytes 前回計算時からの増分バイト数 (SmplSize に対応)
 * @param elapsed_ms 前回計算時からの経過ミリ秒
 */
int TransferSpeed(long long sampled_bytes, int elapsed_ms);

/**
 * @brief 残り時間 (ms)。速度が0以下なら 0 (0除算しない。ProgressCore 196行)
 */
int RemainingMs(long long total, long long transferred, int speed_bytes_per_ms);

/**
 * @brief 1回のタスク進捗をまとめて計算する
 * @details ProgressRatio/TransferSpeed/RemainingMs を同じ契約で組み合わせる。
 *          wx 層はこの値を wxProgressDialog に渡せるが、GUI に依存しない。
 */
struct TaskProgressSnapshot {
	double ratio = -1.0;             //!< 0.0〜1.0。全体量が不明なら -1
	int speed_bytes_per_ms = 0;     //!< byte/ms
	int remaining_ms = 0;           //!< 残り時間 (ms)
};
TaskProgressSnapshot CalculateTaskProgress(long long total, long long transferred,
                                           long long sampled_bytes, int elapsed_ms);

//-----------------------------------------------------------------------
// スレッドセーフな中断フラグ (wxWorker がポーリングする想定)
//-----------------------------------------------------------------------

/// 原子的な中断フラグ。コピー不可 (std::atomic を含むため)
class CancelFlag {
public:
	CancelFlag() = default;
	CancelFlag(const CancelFlag &) = delete;
	CancelFlag &operator=(const CancelFlag &) = delete;

	/// 中断を要求する (複数スレッドから呼べる)
	void RequestCancel() { flag_.store(true); }
	/// 中断状態か
	bool IsCancelled() const { return flag_.load(); }
	/// 初期状態に戻す (次の仕事の前に呼ぶ)
	void Reset() { flag_.store(false); }

private:
	std::atomic<bool> flag_{false};
};

//-----------------------------------------------------------------------
// thumb: 収まりサイズと取得順序 (thumb_thread.cpp FitSize/Execute)
//-----------------------------------------------------------------------

/// FitThumb() の結果
struct ThumbFit {
	int w = 0;
	int h = 0;
	bool resized = false;  ///< 縮小したか
};

/**
 * @brief 上限サイズに収まるよう縮小する (FitSize 73-92行)
 * @param w,h 元のサイズ / @param max_size ThumbnailSize に対応 (既定 120)
 * @details VCL は w/h のどちらかが上限超えのときだけ縮小し、長い辺を上限に
 *          合わせる (float の比率→int への切り捨て)。上限ちょうどは変えない。
 */
ThumbFit FitThumb(int w, int h, int max_size);

/**
 * @brief 現在位置から前後交互に取得する順序 (Execute 240-271行)
 * @param count 一覧の件数 / @param start 開始位置 (StartIndex に対応)
 * @return 取得すべき添字の列。空・範囲外開始なら空
 * @details VCL の tag/n/p による交互走査をそのまま純関数化したもの。
 *          例: count=5, start=2 → [2,3,1,4,0]。
 */
std::vector<int> ThumbOrder(int count, int start);

/**
 * @brief ThumbOrder に候補数の上限を適用する
 * @param count 一覧の件数 / @param start 開始位置
 * @param max_count 候補数の上限。負なら全件、0なら候補なし
 * @return 交互取得順の添字列
 */
std::vector<int> ThumbCandidates(int count, int start, int max_count);

//-----------------------------------------------------------------------
// icon: キャッシュ制限と通知間隔 (icon_thread.cpp Execute)
//-----------------------------------------------------------------------

/**
 * @brief 捨てるべき先頭からの件数 (Execute 44-48行の数制限)
 * @param count 現在の件数 (CachedIcoList->Count) / @param limit IconCache に対応
 */
int IconEvictCount(int count, int limit);

/**
 * @brief 未取得 (アイコン未設定) の添字 (Execute 56-68行)
 * @param has_icon 件数分の取得済みフラグ (true=取得済み)
 */
std::vector<int> IconPendingIndices(const std::vector<char> &has_icon);

/**
 * @brief 通知すべきか (Execute 84行の 200ms ごと)
 * @param now_ms 現在の tick / @param last_ms 前回通知の tick
 * @param pending 未通知の取得件数 (cnt に対応)
 */
bool ShouldNotify(int now_ms, int last_ms, int pending);

/** icon の1バッチで必要な判断をまとめたもの */
struct IconBatchPlan {
	int evict_count = 0;             //!< キャッシュ上限超過分の破棄件数
	std::vector<int> pending_indices; //!< 未取得の添字
	bool notify = false;             //!< この時点で通知するか
};

/**
 * @brief icon のキャッシュ破棄・未取得列挙・通知判定を合成する
 * @param cache_count 現在キャッシュ件数 / @param cache_limit 上限
 * @param has_icon 取得済みフラグ (true=取得済み)
 * @param now_ms 現在時刻 / @param last_ms 前回通知時刻
 */
IconBatchPlan PlanIconBatch(int cache_count, int cache_limit,
                            const std::vector<char> &has_icon,
                            int now_ms, int last_ms);

/**
 * @brief 進捗通知を間引くか (最初・最終は常に通知)
 * @param processed 処理済み件数 / @param total 全体件数
 * @param interval 通知間隔。0以下は1件ごとに通知
 */
bool ShouldReportProgress(int processed, int total, int interval = 1);

/**
 * @brief grep 進捗を間引くか (20件ごと)。
 * @details gui/grep_dialog.cpp が repaint の重さ避けに `files % 20` で間引いて
 *          いたものを、gui/worker_thread.cpp (GrepWorkerThread::Entry) と共有する
 *          ために切り出した純関数。0件・端数は通知しない。
 */
bool ShouldReportGrepProgress(int files_scanned);

//-----------------------------------------------------------------------
// grep 非同期 (grep_dialog + GrepWorkerThread の接続、Issue #41 batch3)
//-----------------------------------------------------------------------

/**
 * @brief 非同期 grep の進捗・完了の畳み込み (wx 非依存の状態機械)
 * @details GrepWorkerThread が QueueEvent する2種のイベントの畳み方:
 *   - wxEVT_GREP_PROGRESS (SetInt=files_scanned、SetExtraLong=matches_found)
 *     → OnProgress で最新の件数を保持する (未完了のまま)
 *   - wxEVT_GREP_DONE (SetInt=中断なら1) → OnDone で完了・中断を記録する
 *   wx イベント自体は単体テスト不可のため、ここでは畳み込みだけを持ち、
 *   イベントの受け渡し (Bind/QueueEvent) は grep_dialog 側の仕事。
 */
struct GrepAsyncState {
	int files_scanned = 0;  ///< 最後に受けた進捗の走査ファイル数
	int matches_found = 0;  ///< 最後に受けた進捗の一致件数
	bool done = false;      ///< DONE を受けたか
	bool cancelled = false;  ///< 中断で終わったか (OnDone の引いを記録)

	/// 進捗を受けて最新の件数に更新する
	void OnProgress(int files, int found);
	/// 完了を受けて完了・中断を記録する
	void OnDone(bool was_cancelled);
};

//-----------------------------------------------------------------------
// grep/task/thumb/icon 共通: wxWorker の1チャンク実行の契約
//-----------------------------------------------------------------------

/// ProcessBatched() の結果
struct BatchResult {
	int processed = 0;   ///< 処理した件数
	bool cancelled = false;  ///< 途中で打ち切ったか
};

/// 中断確認。true で打ち切る (呼び出しコストの低い処理にすること)
using BatchCancelCallback = std::function<bool()>;
/// 1件処理する_CALLBACK (0始まりの添字を受け取る)
using BatchItemCallback = std::function<void(int index)>;
/// 進捗通知 (処理済み件数・全体件数)
using BatchProgressCallback = std::function<void(int processed, int total)>;

/**
 * @brief total 件を1件ずつ処理し、中断・進捗の契約を固定する
 * @details wxWorker 化の中核。VCL の各 Execute が `while (!Terminated)` と
 *          `Sleep(50)` で回していたポーリングを、「1件処理するたびに
 *          cancel_cb を見て、item_cb を呼び、progress_cb を呼ぶ」という形に
 *          純関数化したもの。item_cb を省略すると件数だけを進める。
 *          実際の副作用は呼び出し元 (wx 層) が担当する。
 */
BatchResult ProcessBatched(int total, const BatchCancelCallback &cancel_cb,
                           const BatchProgressCallback &progress_cb = nullptr,
                           const BatchItemCallback &item_cb = nullptr);

}  // namespace workers

#endif  // NYANFI_GUI_WORKERS_H
