/**
 * @file gui/file_pane.h
 * @brief ファイル一覧ペイン (自前描画)
 *
 * NyanFi の中核である2画面のうちの片側。VCL 版は TDrawGrid のオーナードローで
 * 描いていたが、こちらは wxWindow に直接描く。
 *
 * 一覧の取得・整列・属性の解釈には移植済みのロジック層をそのまま使う
 * (FindFirst/FindNext、comp_NaturalOrder、get_file_attr_str など)。
 * 色は wxSystemSettings から取るため、Windows のライト/ダークモードに追従する
 * (VCL Styles でライト/ダーク対応していた本フォークの存在理由を置き換える部分)。
 */
#ifndef NYANFI_GUI_FILE_PANE_H
#define NYANFI_GUI_FILE_PANE_H

#include <vector>

#include <wx/wx.h>

/// 一覧の1行
struct FileItem {
	UnicodeString name;    //!< ファイル名 (パスを含まない)
	Int64 size = 0;        //!< サイズ (ディレクトリは -1)
	TDateTime stamp;       //!< 最終更新日時
	int attr = 0;          //!< 属性 (faXXX)
	bool is_dir = false;   //!< ディレクトリか
	bool is_parent = false;//!< ".." か
	bool marked = false;   //!< マーク済みか
};

/**
 * @brief ファイル一覧ペイン
 */
class FilePane : public wxWindow {
public:
	FilePane(wxWindow *parent, wxWindowID id);

	//-- 表示対象 ----------------------------------------------------------
	bool SetPath(const UnicodeString &path);  //!< ディレクトリを開く
	void Reload();                            //!< 再読み込み (カーソル位置は名前で復元)
	UnicodeString GetPath() const { return path_; }

	//-- カーソル ----------------------------------------------------------
	void MoveCursor(int delta);
	void MoveCursorTo(int index);
	void CursorTop() { MoveCursorTo(0); }
	void CursorEnd() { MoveCursorTo(static_cast<int>(items_.size()) - 1); }
	void PageMove(int direction);
	int GetCursor() const { return cursor_; }
	const FileItem *GetCurrentItem() const;

	//-- マーク ------------------------------------------------------------
	void ToggleMark();
	void MarkAll(bool marked);
	int GetMarkedCount() const;

	//-- 状態 --------------------------------------------------------------
	int GetItemCount() const { return static_cast<int>(items_.size()); }
	bool IsActive() const { return active_; }
	void SetActive(bool active);

	/// 親ディレクトリへ移動する。移動元のディレクトリ名にカーソルを合わせる
	bool GoParent();

	/// カーソル位置がディレクトリならそこへ入る
	bool EnterCurrent();

	/// ステータス表示用の要約 (件数とマーク数)
	UnicodeString GetSummary() const;

private:
	void OnPaint(wxPaintEvent &event);
	void OnSize(wxSizeEvent &event);
	void OnLeftDown(wxMouseEvent &event);
	void OnLeftDClick(wxMouseEvent &event);
	void OnMouseWheel(wxMouseEvent &event);
	void OnSetFocus(wxFocusEvent &event);

	void Collect();                //!< items_ を作り直す
	void EnsureVisible();
	int VisibleRows() const;
	int RowHeight() const { return row_height_; }
	void UpdateMetrics();

	UnicodeString path_;
	std::vector<FileItem> items_;
	int cursor_ = 0;
	int top_ = 0;         //!< 先頭に表示している行
	int row_height_ = 16;
	int char_width_ = 8;
	bool active_ = false;
	wxFont font_;
};

#endif  // NYANFI_GUI_FILE_PANE_H
