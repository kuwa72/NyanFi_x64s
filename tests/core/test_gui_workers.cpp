/**
 * @file tests/core/test_gui_workers.cpp
 * @brief gui/workers.cpp (task/thumb/grep/icon の中断・進捗の純関数) の回帰テスト
 *
 * @details Issue #41 (port/phase3-workers)。VCL 版の該当は実測:
 *   - task: src/task_thread.cpp ProgressCore (169行) の比率・速度・残り時間と
 *     中断/一旦停止の判断、Execute/TaskStart/FinishTask (1863/1878/1899行) の
 *     状態遷移のうち GUI 非依存の部分
 *   - thumb: src/thumb_thread.cpp FitSize (73行) と Execute (234行) の
 *     現在位置から前後交互に取得する順序
 *   - icon: src/icon_thread.cpp Execute (40行) のキャッシュ数制限と
 *     200ms ごとの通知
 *   - grep: gui/grep.h の SearchDirectory は既に cancel/progress コールバックを
 *     持つため、ここではワーカー駆動の汎用バッチ実行 (ProcessBatched) として
 *     中断・進捗の契約を固定する
 *
 *   FTP/Git の外部連携は含まない (Issue #41 で分離と明記)。
 */
#include "doctest/doctest.h"

#include <vector>

#include "gui/workers.h"

using workers::CancelFlag;

//===========================================================================
// DecideStep: 中断/一旦停止の判断 (task_thread.cpp ProgressCore 171-178行)
//===========================================================================
TEST_CASE("DecideStep: 中断も停止もなければ続行")
{
	CHECK(workers::DecideStep(false, false) == workers::StepAction::Continue);
}

TEST_CASE("DecideStep: 中断が最優先 (停止中でも中断)")
{
	CHECK(workers::DecideStep(true, false) == workers::StepAction::Cancel);
	CHECK(workers::DecideStep(true, true) == workers::StepAction::Cancel);
}

TEST_CASE("DecideStep: 一旦停止中は待つ")
{
	CHECK(workers::DecideStep(false, true) == workers::StepAction::Wait);
}

//===========================================================================
// ProgressRatio / TransferSpeed / RemainingMs (ProgressCore 180-196行)
//===========================================================================
TEST_CASE("ProgressRatio: 転送量/全体量 (0.0〜1.0)")
{
	CHECK(workers::ProgressRatio(100, 0) == doctest::Approx(0.0));
	CHECK(workers::ProgressRatio(100, 50) == doctest::Approx(0.5));
	CHECK(workers::ProgressRatio(100, 100) == doctest::Approx(1.0));
}

TEST_CASE("ProgressRatio: 全体量が0以下なら-1 (VCL と同じ)")
{
	CHECK(workers::ProgressRatio(0, 0) == doctest::Approx(-1.0));
	CHECK(workers::ProgressRatio(-10, 5) == doctest::Approx(-1.0));
}

TEST_CASE("TransferSpeed: sampledバイト/elapsedミリ秒")
{
	CHECK(workers::TransferSpeed(2000, 1000) == 2);
	CHECK(workers::TransferSpeed(0, 1000) == 0);
}

TEST_CASE("TransferSpeed: 経過が0以下なら0")
{
	CHECK(workers::TransferSpeed(100, 0) == 0);
	CHECK(workers::TransferSpeed(100, -5) == 0);
}

TEST_CASE("RemainingMs: 残りバイト/速度")
{
	CHECK(workers::RemainingMs(1000, 600, 2) == 200);
	CHECK(workers::RemainingMs(1000, 1000, 5) == 0);
}

TEST_CASE("RemainingMs: 速度が0以下なら0 (0除算しない)")
{
	CHECK(workers::RemainingMs(1000, 100, 0) == 0);
	CHECK(workers::RemainingMs(1000, 100, -1) == 0);
}

//===========================================================================
// CancelFlag: スレッドセーフな中断フラグ
//===========================================================================
TEST_CASE("CancelFlag: 初期状態は中断なし、要求で中断、Resetで戻る")
{
	CancelFlag flag;
	CHECK(!flag.IsCancelled());
	flag.RequestCancel();
	CHECK(flag.IsCancelled());
	flag.Reset();
	CHECK(!flag.IsCancelled());
}

//===========================================================================
// FitThumb: サムネイルの収まりサイズ (thumb_thread.cpp FitSize 73行)
//===========================================================================
TEST_CASE("FitThumb: 横長は幅を上限に合わせる")
{
	const auto r = workers::FitThumb(200, 100, 120);
	CHECK(r.resized);
	CHECK(r.w == 120);
	CHECK(r.h == 60);
}

TEST_CASE("FitThumb: 縦長は高さを上限に合わせる")
{
	const auto r = workers::FitThumb(100, 200, 120);
	CHECK(r.resized);
	CHECK(r.w == 60);
	CHECK(r.h == 120);
}

TEST_CASE("FitThumb: 上限以下は変えない")
{
	const auto r = workers::FitThumb(100, 80, 120);
	CHECK(!r.resized);
	CHECK(r.w == 100);
	CHECK(r.h == 80);
}

TEST_CASE("FitThumb: 上限ちょうどは変えない (VCL は > のときだけ縮小)")
{
	const auto r = workers::FitThumb(120, 120, 120);
	CHECK(!r.resized);
	CHECK(r.w == 120);
	CHECK(r.h == 120);
}

//===========================================================================
// ThumbOrder: 現在位置から前後交互 (thumb_thread.cpp Execute 240-271行)
//===========================================================================
TEST_CASE("ThumbOrder: 中央開始は前後交互 (2,3,1,4,0)")
{
	const std::vector<int> order = workers::ThumbOrder(5, 2);
	REQUIRE(order.size() == 5);
	CHECK(order[0] == 2);
	CHECK(order[1] == 3);
	CHECK(order[2] == 1);
	CHECK(order[3] == 4);
	CHECK(order[4] == 0);
}

TEST_CASE("ThumbOrder: 先頭開始は順方向")
{
	const std::vector<int> order = workers::ThumbOrder(3, 0);
	REQUIRE(order.size() == 3);
	CHECK(order[0] == 0);
	CHECK(order[1] == 1);
	CHECK(order[2] == 2);
}

TEST_CASE("ThumbOrder: 末尾開始は逆方向")
{
	const std::vector<int> order = workers::ThumbOrder(3, 2);
	REQUIRE(order.size() == 3);
	CHECK(order[0] == 2);
	CHECK(order[1] == 1);
	CHECK(order[2] == 0);
}

TEST_CASE("ThumbOrder: 空・範囲外は空")
{
	CHECK(workers::ThumbOrder(0, 0).empty());
	CHECK(workers::ThumbOrder(3, -1).empty());
	CHECK(workers::ThumbOrder(3, 3).empty());
}

//===========================================================================
// IconEvictCount / IconPendingIndices / ShouldNotify (icon_thread.cpp 40行)
//===========================================================================
TEST_CASE("IconEvictCount: 上限超過分だけ捨てる")
{
	CHECK(workers::IconEvictCount(510, 500) == 10);
	CHECK(workers::IconEvictCount(500, 500) == 0);
	CHECK(workers::IconEvictCount(100, 500) == 0);
}

TEST_CASE("IconPendingIndices: 未取得 (false) の添字だけ返す")
{
	const std::vector<char> has = {1, 0, 1, 0, 0};
	const std::vector<int> pending = workers::IconPendingIndices(has);
	REQUIRE(pending.size() == 3);
	CHECK(pending[0] == 1);
	CHECK(pending[1] == 3);
	CHECK(pending[2] == 4);
}

TEST_CASE("ShouldNotify: 200ms超かつ取得ありで通知 (icon_thread.cpp 84行)")
{
	CHECK(workers::ShouldNotify(1000, 700, 1));
	CHECK(!workers::ShouldNotify(1000, 900, 1));
	CHECK(!workers::ShouldNotify(1000, 700, 0));
}

//===========================================================================
// ProcessBatched: wxWorker の1チャンク実行の契約 (中断・進捗)
//===========================================================================
TEST_CASE("ProcessBatched: 全件処理して進捗を毎回呼ぶ")
{
	std::vector<int> items = {1, 2, 3, 4, 5};
	int progress_calls = 0;
	int last_processed = 0;
	const auto r = workers::ProcessBatched(
		items.size(), nullptr,
		[&](int calls, int total) {
			++progress_calls;
			last_processed = calls;
			(void)total;
		});
	CHECK(!r.cancelled);
	CHECK(r.processed == 5);
	CHECK(progress_calls == 5);
	CHECK(last_processed == 5);
}

TEST_CASE("ProcessBatched: cancel_cb が true で打ち切る")
{
	std::vector<int> items = {1, 2, 3, 4, 5};
	int calls = 0;
	const auto r = workers::ProcessBatched(
		items.size(),
		[&calls]() {
			++calls;
			return calls > 2;
		},
		nullptr);
	CHECK(r.cancelled);
	CHECK(r.processed < 5);
}

TEST_CASE("ProcessBatched: CancelFlag と組み合わせて中断できる")
{
	CancelFlag flag;
	const auto r = workers::ProcessBatched(
		10,
		[&flag]() { return flag.IsCancelled(); },
		[&flag](int processed, int total) {
			(void)total;
			if (processed >= 3) flag.RequestCancel();
		});
	CHECK(r.cancelled);
	CHECK(r.processed == 3);
}

TEST_CASE("ProcessBatched: item callback is called before progress")
{
	std::vector<int> order;
	const auto r = workers::ProcessBatched(
		3, nullptr,
		[&order](int processed, int total) {
			(void)total;
			order.push_back(processed * 10);
		},
		[&order](int index) { order.push_back(index); });
	REQUIRE(order.size() == 6);
	CHECK(order[0] == 0);
	CHECK(order[1] == 10);
	CHECK(order[2] == 1);
	CHECK(order[3] == 20);
	CHECK(order[4] == 2);
	CHECK(order[5] == 30);
	CHECK(!r.cancelled);
	CHECK(r.processed == 3);
}

//===========================================================================
// worker_thread 契約 (Issue #41 batch2): wxThread 自体は単体テスト不可のため
// ここでは Entry が依存する純粋部の契約だけを固定する。スレッド起動/中断の
// 統合確認は MSYS2 CI (GUI ビルド) に委ねる
//===========================================================================
TEST_CASE("ProcessBatched: totalが0以下なら何もせず打ち切りなし")
{
	int progress_calls = 0;
	const auto r0 = workers::ProcessBatched(
		0, nullptr, [&](int, int) { ++progress_calls; });
	CHECK(!r0.cancelled);
	CHECK(r0.processed == 0);
	const auto rn = workers::ProcessBatched(
		-5, nullptr, [&](int, int) { ++progress_calls; });
	CHECK(!rn.cancelled);
	CHECK(rn.processed == 0);
	CHECK(progress_calls == 0);
}

TEST_CASE("ProcessBatched: 進捗は単調増加でtotalは一定 (worker進捗イベントの元)")
{
	std::vector<std::pair<int, int>> calls;
	const auto r = workers::ProcessBatched(
		4, nullptr,
		[&](int processed, int total) { calls.emplace_back(processed, total); });
	CHECK(!r.cancelled);
	CHECK(r.processed == 4);
	REQUIRE(calls.size() == 4);
	for (std::size_t i = 0; i < calls.size(); ++i) {
		CHECK(calls[i].first == static_cast<int>(i) + 1);
		CHECK(calls[i].second == 4);
	}
}

TEST_CASE("CancelFlag: Reset後は次の仕事を完走できる (worker再利用の契約)")
{
	CancelFlag flag;
	flag.RequestCancel();
	flag.Reset();
	int progress_calls = 0;
	const auto r = workers::ProcessBatched(
		3,
		[&flag]() { return flag.IsCancelled(); },
		[&](int, int) { ++progress_calls; });
	CHECK(!r.cancelled);
	CHECK(r.processed == 3);
	CHECK(progress_calls == 3);
}

TEST_CASE("ShouldReportGrepProgress: 20件ごとに通知 (dialog/worker共有)")
{
	CHECK(!workers::ShouldReportGrepProgress(0));
	CHECK(!workers::ShouldReportGrepProgress(1));
	CHECK(!workers::ShouldReportGrepProgress(19));
	CHECK(workers::ShouldReportGrepProgress(20));
	CHECK(!workers::ShouldReportGrepProgress(21));
	CHECK(workers::ShouldReportGrepProgress(40));
}

//===========================================================================
// GrepAsyncState: grep_dialog 非同期化の状態機械 (Issue #41 batch3)
//===========================================================================
// GrepWorkerThread が QueueEvent する wxEVT_GREP_PROGRESS (Int=files,
// ExtraLong=found) / wxEVT_GREP_DONE (Int=0/1) を grep_dialog がどう畳むかの
// 契約。wx イベント自体は単体テスト不可のため、畳み込みだけを純関数化する。
// スレッド起動/中断の統合確認は MSYS2 CI (GUI ビルド) に委ねる
TEST_CASE("GrepAsyncState: 初期状態は未完了・0件")
{
	workers::GrepAsyncState st;
	CHECK(st.files_scanned == 0);
	CHECK(st.matches_found == 0);
	CHECK(!st.done);
	CHECK(!st.cancelled);
}

TEST_CASE("GrepAsyncState: OnProgressで最新を保持 (未完了のまま)")
{
	workers::GrepAsyncState st;
	st.OnProgress(20, 3);
	CHECK(st.files_scanned == 20);
	CHECK(st.matches_found == 3);
	CHECK(!st.done);
	st.OnProgress(40, 5);
	CHECK(st.files_scanned == 40);
	CHECK(st.matches_found == 5);
	CHECK(!st.done);
}

TEST_CASE("GrepAsyncState: OnDone(false)で完了・中断なし")
{
	workers::GrepAsyncState st;
	st.OnProgress(40, 5);
	st.OnDone(false);
	CHECK(st.done);
	CHECK(!st.cancelled);
	CHECK(st.files_scanned == 40);
	CHECK(st.matches_found == 5);
}

TEST_CASE("GrepAsyncState: OnDone(true)で完了・中断あり")
{
	workers::GrepAsyncState st;
	st.OnProgress(20, 1);
	st.OnDone(true);
	CHECK(st.done);
	CHECK(st.cancelled);
}

//===========================================================================
// wxWorker 接続で使う合成純関数 (Issue #41 phase3-workers)
//===========================================================================
TEST_CASE("CalculateTaskProgress: 進捗率・速度・残り時間をまとめて計算")
{
	const auto p = workers::CalculateTaskProgress(1000, 400, 200, 100);
	CHECK(p.ratio == doctest::Approx(0.4));
	CHECK(p.speed_bytes_per_ms == 2);
	CHECK(p.remaining_ms == 300);
}

TEST_CASE("CalculateTaskProgress: 全体量不明は-1、速度なしは残り0")
{
	const auto unknown = workers::CalculateTaskProgress(0, 10, 100, 100);
	CHECK(unknown.ratio == doctest::Approx(-1.0));
	CHECK(unknown.remaining_ms == 0);
	const auto no_speed = workers::CalculateTaskProgress(1000, 400, 0, 100);
	CHECK(no_speed.speed_bytes_per_ms == 0);
	CHECK(no_speed.remaining_ms == 0);
}

TEST_CASE("ShouldReportProgress: 1件目は必ず、最終件は必ず通知")
{
	CHECK(workers::ShouldReportProgress(1, 10, 20));
	CHECK(!workers::ShouldReportProgress(2, 10, 20));
	CHECK(workers::ShouldReportProgress(10, 10, 20));
	CHECK(!workers::ShouldReportProgress(0, 10, 20));
}

TEST_CASE("ThumbCandidates: ThumbOrder の順序と上限を一緒に適用")
{
	const auto candidates = workers::ThumbCandidates(5, 2, 3);
	REQUIRE(candidates.size() == 3);
	CHECK(candidates[0] == 2);
	CHECK(candidates[1] == 3);
	CHECK(candidates[2] == 1);
	CHECK(workers::ThumbCandidates(5, 2, 0).empty());
	CHECK(workers::ThumbCandidates(5, 2, -1).size() == 5);
}

TEST_CASE("IconBatchPlan: 破棄・未取得添字・通知判定を合成")
{
	const std::vector<char> has = {1, 0, 1, 0};
	const auto plan = workers::PlanIconBatch(6, 4, has, 1000, 700);
	CHECK(plan.evict_count == 2);
	REQUIRE(plan.pending_indices.size() == 2);
	CHECK(plan.pending_indices[0] == 1);
	CHECK(plan.pending_indices[1] == 3);
	CHECK(plan.notify);
}

TEST_CASE("IconBatchPlan: 上限内・通知間隔内なら破棄/通知なし")
{
	const std::vector<char> has = {1, 1, 0};
	const auto plan = workers::PlanIconBatch(3, 4, has, 800, 700);
	CHECK(plan.evict_count == 0);
	REQUIRE(plan.pending_indices.size() == 1);
	CHECK(plan.pending_indices[0] == 2);
	CHECK(!plan.notify);
}
