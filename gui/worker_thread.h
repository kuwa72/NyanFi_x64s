/**
 * @file gui/worker_thread.h
 * @brief grep 検索を別スレッドで回す wxThread 派生クラス (Issue #41 batch2)
 *
 * @details batch1 (PR #48) で純関数化した gui/workers.h の wx 層。
 *   Entry 内で workers::CancelFlag をポーリングし、1ファイル単位の処理
 *   (ProcessBatched の「1件処理するたびに cancel_cb を見て progress_cb を
 *   呼ぶ」と同じ契約。SearchDirectory がファイルごとにその順序で回す) で
 *   進め、進捗は wxThreadEvent を QueueEvent する (CallAfter 相当。GUI
 *   スレッドで安全に受け取れる)。
 *
 *   接続先は grep_core::SearchDirectory の1件だけ。中断は CancelFlag、
 *   進捗はイベントで渡す。FTP/Git/コピー・移動・削除のタスク本体は
 *   Global/UserFunc 未移植のため対象外。
 *
 *   使い方 (joinable スレッド):
 *     auto *th = new worker_thread::GrepWorkerThread(owner, dir, opt, limits, &flag);
 *     th->Run();
 *     ... owner は wxEVT_GREP_PROGRESS / wxEVT_GREP_DONE を受け取る ...
 *     th->Wait();  // 終了待ちの後に th->Result() で結果を読む
 *     delete th;
 *   中断は flag.RequestCancel() (または th->RequestCancel())。
 *
 *   wxThread 自体の単体テストは不可のため、契約 (中断・進捗の順序、間引き、
 *   イベントの積み方) は tests/core/test_gui_workers.cpp の純粋部テストと
 *   このヘッダの文書で固定し、スレッド起動/中断の統合確認は MSYS2 CI
 *   (GUI ビルド) に委ねる。
 */
#ifndef NYANFI_GUI_WORKER_THREAD_H
#define NYANFI_GUI_WORKER_THREAD_H

#include <wx/event.h>
#include <wx/thread.h>

#include "gui/grep.h"
#include "gui/workers.h"

wxDECLARE_EVENT(wxEVT_GREP_PROGRESS, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_GREP_DONE, wxThreadEvent);

namespace worker_thread {

/**
 * @brief 進捗・完了イベントの積み方の契約
 * @details 進捗 (wxEVT_GREP_PROGRESS): SetInt(files_scanned)、
 *          SetExtraLong(matches_found)。完了 (wxEVT_GREP_DONE):
 *          SetInt(cancelled ? 1 : 0)。結果本体 (matches 等) はイベントに
 *          載せず Result() で読む (vector をイベント経由で渡さない)。
 */
class GrepWorkerThread : public wxThread {
public:
	/**
	 * @brief 検索スレッドを作る (開始は Run())
	 * @param owner 進捗・完了イベントの送り先 (QueueEvent する相手。非所有)
	 * @param dir 検索対象ディレクトリ / @param opt 検索条件
	 * @param limits 走査・検索の上限
	 * @param cancel 中断フラグ (非所有。nullptr 可。その場合は中断なし)
	 */
	GrepWorkerThread(wxEvtHandler *owner, const UnicodeString &dir,
	                 const grep_core::GrepOptions &opt, const grep_core::GrepLimits &limits,
	                 workers::CancelFlag *cancel);

	/// Entry (別スレッド) で SearchDirectory を回す
	virtual ExitCode Entry() override;

	/// 中断を要求する (cancel_ があれば RequestCancel する)
	void RequestCancel();

	/// 検索結果。Wait() で終了を確認してから読むこと
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
