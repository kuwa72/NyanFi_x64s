/**
 * @file gui/text_viewer.h
 * @brief テキストビューア (自前描画、gui/file_pane.h と同じ作り)
 *
 * MainFrame の中に常駐し、開いていないときは Hide() しておく1面の
 * ビューア。VCL 版の TTxtViewer (src/TxtViewer.cpp、5457行) はテキスト/
 * バイナリ/CSV/JSON/画像プレビュー等の多数のモードと文字単位のカーソル・
 * 強調表示・折り返し禁則処理などを持つが、ここは issue #1 Phase 2 の
 * スコープに合わせて「行単位のカーソルを持つテキスト専用ビューア」に
 * 単純化してある (推測・要検証)。
 *
 * 文字コード判定・行分割・折り返し計算は wx に依存しない
 * gui/text_viewer_core.h に切り出してあり、そちらは
 * tests/core/test_gui_text_viewer.cpp から直接テストできる。
 *
 * 色は gui/file_pane.cpp と同じく wxSystemSettings から取り、ライト/ダーク
 * モードに自動追従する。
 */
#ifndef NYANFI_GUI_TEXT_VIEWER_H
#define NYANFI_GUI_TEXT_VIEWER_H

#include <functional>
#include <vector>

#include <wx/wx.h>

#include "gui/text_viewer_core.h"
#include "gui/find_txt.h"

/**
 * @brief テキストビューア
 */
class TextViewer : public wxWindow {
public:
	TextViewer(wxWindow *parent, wxWindowID id);

	/// ファイルを開く。失敗したら false を返し error にメッセージを入れる
	bool LoadFile(const UnicodeString &path, UnicodeString &error);

	/// 閉じるキー (既定 Q。ESC も受け付ける) が押されたときに呼ぶコールバック
	void SetOnClose(std::function<void()> fn) { on_close_ = std::move(fn); }

	/**
	 * @brief キー入力を処理する
	 * @return true 処理済み (MainFrame 側の通常のキー処理へは回さない)
	 */
	bool HandleKey(wxKeyEvent &event);

	/// ステータスバー表示用の要約 (ファイル名・コードページ・行数・折り返し等)
	UnicodeString GetStatusSummary() const;

	/// 指定行 (0ベース) へカーソルを移動する。範囲外は clamp する
	/// (grep 検索結果から「該当行にカーソルを合わせて開く」ための公開 API。
	/// gui/grep_dialog.cpp から呼ばれる)
	void GotoLine(int line);

	/**
	 * @brief Vモードコマンドを実行する
	 * @details src/TxtViewer.cpp::ExeCommand / src/MainFrm.cpp::ExeCommandV の
	 *          頻度上位コマンドを行単位ビューア向けに単純化したもの。
	 *          判断 (範囲計算・検索・マーク・ジャンプ先・コードページ) は
	 *          gui/text_viewer_core.h の純関数が持ち、ここは受け渡しだけにする
	 * @return true 処理済み (MainFrame::Execute は他のモードへ回さない)
	 */
	bool Execute(const UnicodeString &full_command);

	/// V:CursorUp / V:CursorDown (param 数値=行数、空=1)
	void CmdCursorUp(const UnicodeString &param);
	void CmdCursorDown(const UnicodeString &param);
	/// V:PageUp / V:PageDown
	void CmdPageUp();
	void CmdPageDown();
	/// V:TextTop / V:TextEnd (先頭/末尾ジャンプ)
	void CmdTextTop();
	void CmdTextEnd();
	/// V:LineTop / V:LineEnd (行単位ビューアでは水平スクロールの端へ。
	/// VCL 版は文字カーソルを行頭/行末へ動かすが、ここに桁カーソルは無い)
	void CmdLineTop();
	void CmdLineEnd();
	/// V:CursorLeft / V:CursorRight (param 数値=文字数、空=4)
	void CmdCursorLeft(const UnicodeString &param);
	void CmdCursorRight(const UnicodeString &param);
	/// V:FindText (param 空=ダイアログ表示、非空=その語で検索)
	void CmdFindText(const UnicodeString &param);
	/// V:FindDown / V:FindUp (param 非空=検索語を更新して検索)
	bool CmdFindDown(const UnicodeString &param);
	bool CmdFindUp(const UnicodeString &param);
	/// V:JumpLine (param 行番号。空=ダイアログ表示)
	bool CmdJumpLine(const UnicodeString &param);
	/// V:Mark (トグル) / V:ClearMark (全解除)
	void CmdMark();
	void CmdClearMark();
	/// V:FindMarkDown / V:FindMarkUp。移動したら true
	bool CmdFindMarkDown();
	bool CmdFindMarkUp();
	/// V:ChangeCodePage (param 空=循環切替、非空=指定)。無効値は無視する
	void CmdChangeCodePage(const UnicodeString &param);
	/// V:ReloadFile (現在行・マークを保って再読込)
	void CmdReload();
	/// V:Close (閉じる。SetOnClose 経由)
	void CmdClose();
	/**
	 * @brief FV:ShowLineNo / ShowRuler / ShowTAB / ShowCR (param 空=反転、ON/OFF)
	 * @details VCL 版 (MainFrm.cpp:33701〜) は TVIEW表示中は TxtViewer に委ね、
	 *          FLIST では既定フラグを反転する。ここは常駐1面なので状態を直接持つ。
	 *          ShowRuler/ShowTAB/ShowCR は状態の保持とステータス表示までで、
	 *          ルーラ行・タブ記号・改行記号の描画は未対応 (TODO)
	 */
	void CmdShowLineNo(const UnicodeString &param);
	void CmdShowRuler(const UnicodeString &param);
	void CmdShowTAB(const UnicodeString &param);
	void CmdShowCR(const UnicodeString &param);
	/**
	 * @brief FV:SetTab / SetWidth / SetMargin
	 * @details VCL 版 (SetTabActionExecute:34185、SetWidth/SetMargin) と同じ解釈
	 *          (gui/text_display.h)。空は入力ボックスを出すので無視する
	 */
	void CmdSetTab(const UnicodeString &param);
	void CmdSetWidth(const UnicodeString &param);
	void CmdSetMargin(const UnicodeString &param);

	/// 行数 (ViewTail の移動先計算用)
	int LineCount() const { return static_cast<int>(doc_.lines.size()); }
	/// 表示設定の現在値 (Fモード配線の状態表示・テスト用)
	bool ShowLineNo() const { return show_line_no_; }
	bool ShowRuler() const { return show_ruler_; }
	bool ShowTAB() const { return show_tab_; }
	bool ShowCR() const { return show_cr_; }
	int TabWidth() const { return tab_width_; }
	int FoldWidth() const { return fold_width_; }
	int LeftMargin() const { return left_margin_; }

	/// 現在の栞マーク (0ベース、昇順)。ステータス表示・テスト用
	const std::vector<int> &Marks() const { return marks_; }
	/// 直前の検索語 (FindDown/FindUp が使う)
	const UnicodeString &LastSearch() const { return last_search_; }

private:
	void OnPaint(wxPaintEvent &event);
	void OnSize(wxSizeEvent &event);
	void OnMouseWheel(wxMouseEvent &event);

	void UpdateMetrics();       //!< フォント計測 (char_width_/row_height_)
	void RebuildWrap();         //!< wrap_ と表示幅から wrap_rows_/prefix_rows_ を作り直す
	void UpdateLineNoCols();    //!< 行番号欄の桁数を行数から決める

	int HeaderHeight() const { return row_height_ + 4; }
	int VisibleRows() const;                 //!< 本文の表示行数
	int GutterWidth() const;                 //!< 行番号欄の幅(px)
	int TextAreaCols() const;                //!< 折り返し計算用の表示幅(半角換算)

	Int64 TotalDisplayRows() const { return prefix_rows_.empty() ? 0 : prefix_rows_.back(); }
	Int64 DisplayRowOfLine(int line) const;
	int LineOfDisplayRow(Int64 row) const;   //!< 表示行番号→元行番号 (二分探索)

	void MoveCursor(int delta_lines);
	void PageMove(int direction);
	void GotoTop();
	void GotoEnd();
	void ScrollHorizontal(int delta_chars);
	void ToggleWrap();
	void EnsureCursorVisible();

	void PromptSearch();
	bool SearchForward(const UnicodeString &kwd, int from_line,
	                   find_txt::Direction direction = find_txt::Direction::Down);
	bool SearchBackward(const UnicodeString &kwd, int from_line,
	                    find_txt::Direction direction = find_txt::Direction::Up);

	text_viewer_core::LoadResult doc_;
	UnicodeString path_;

	std::vector<int> wrap_rows_;      //!< 各行の折り返し後の表示行数 (折り返し無効なら全て1)
	std::vector<Int64> prefix_rows_;  //!< 表示行の累積和 (size = 行数+1)

	bool wrap_ = false;               //!< 折り返し表示
	int current_line_ = 0;            //!< カーソル行 (0ベース、行単位)
	Int64 top_row_ = 0;                //!< 先頭に表示する表示行番号
	int h_offset_chars_ = 0;           //!< 折り返し無効時の水平スクロール(文字単位)

	//-- テキスト表示設定 (F:ShowLineNo/SetTab/SetWidth/SetMargin 等) -----------
	// VCL の既定値 (src/Global.cpp のオプション表) と同じ初期値
	bool show_line_no_ = true;        //!< 行番号を表示する (ShowLineNo)
	bool show_ruler_ = true;          //!< ルーラ情報を保持する (ShowTextRuler。描画はTODO)
	bool show_tab_ = true;            //!< タブ記号の表示設定を保持する (描画はTODO)
	bool show_cr_ = true;             //!< 改行記号の表示設定を保持する (描画はTODO)
	int tab_width_ = 8;               //!< タブ幅 (SetTab。特殊拡張子は2固定だったが単純化)
	int fold_width_ = 0;              //!< 折り返し幅(半角換算)。0はウィンドウ幅追従 (SetWidth)
	int left_margin_ = 10;            //!< 左余白(px。ViewLeftMargin。SetMargin)

	UnicodeString last_search_;        //!< 直前の検索語 (次回のダイアログ初期値)
	find_txt::Options find_options_;   //!< VCL FindTextDlg の選択状態
	UnicodeString last_error_;         //!< 直前の Execute 系エラーメッセージ (無ければ空)
	std::vector<int> marks_;           //!< 栞マーク (0ベース、昇順。V:Mark 系)
	int forced_code_page_ = 0;         //!< ChangeCodePage による強制コードページ (0=自動判定)

	wxFont font_;
	int row_height_ = 16;
	int char_width_ = 8;
	int line_no_cols_ = 5;

	std::function<void()> on_close_;
};

#endif  // NYANFI_GUI_TEXT_VIEWER_H
