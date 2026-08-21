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
	bool SearchForward(const UnicodeString &kwd, int from_line);

	text_viewer_core::LoadResult doc_;
	UnicodeString path_;

	std::vector<int> wrap_rows_;      //!< 各行の折り返し後の表示行数 (折り返し無効なら全て1)
	std::vector<Int64> prefix_rows_;  //!< 表示行の累積和 (size = 行数+1)

	bool wrap_ = false;               //!< 折り返し表示
	int current_line_ = 0;            //!< カーソル行 (0ベース、行単位)
	Int64 top_row_ = 0;                //!< 先頭に表示する表示行番号
	int h_offset_chars_ = 0;           //!< 折り返し無効時の水平スクロール(文字単位)

	UnicodeString last_search_;        //!< 直前の検索語 (次回のダイアログ初期値)

	wxFont font_;
	int row_height_ = 16;
	int char_width_ = 8;
	int line_no_cols_ = 5;

	std::function<void()> on_close_;
};

#endif  // NYANFI_GUI_TEXT_VIEWER_H
