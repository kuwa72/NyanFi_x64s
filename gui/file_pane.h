/**
 * @file gui/file_pane.h
 * @brief ファイル一覧ペイン (自前描画)
 *
 * NyanFi の中核である2画面のうちの片側。VCL 版は TDrawGrid のオーナードローで
 * 描いていたが、こちらは wxWindow に直接描く。
 *
 * 一覧の取得・整列・属性の解釈には移植済みのロジック層をそのまま使う
 * (FindFirst/FindNext、get_file_attr_str など)。並べ替え比較とマスク絞り込みの
 * 純粋ロジックは wx に依存しない gui/file_item.h/.cpp に分離してあり、
 * tests/core/ の doctest からも直接テストできる。
 * 色は wxSystemSettings から取るため、Windows のライト/ダークモードに追従する
 * (VCL Styles でライト/ダーク対応していた本フォークの存在理由を置き換える部分)。
 */
#ifndef NYANFI_GUI_FILE_PANE_H
#define NYANFI_GUI_FILE_PANE_H

#include <vector>

#include <wx/wx.h>

#include "gui/file_item.h"

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
	void CursorEnd() { MoveCursorTo(GetItemCount() - 1); }
	void PageMove(int direction);
	int GetCursor() const { return cursor_; }
	const FileItem *GetCurrentItem() const;

	//-- マーク ------------------------------------------------------------
	void ToggleMark();
	void MarkAll(bool marked);
	int GetMarkedCount() const;

	//-- 状態 --------------------------------------------------------------
	int GetItemCount() const { return static_cast<int>(order_.size()); }
	bool IsActive() const { return active_; }
	void SetActive(bool active);

	/// 親ディレクトリへ移動する。移動元のディレクトリ名にカーソルを合わせる
	bool GoParent();

	/// カーソル位置がディレクトリならそこへ入る
	bool EnterCurrent();

	/// ステータス表示用の要約 (件数とマーク数)
	UnicodeString GetSummary() const;

	//-- 並べ替え ------------------------------------------------------------
	SortKey GetSortKey() const { return sort_key_; }
	bool IsSortDescending() const { return sort_descending_; }
	bool IsDirsFirst() const { return dirs_first_; }

	/// 並べ替え設定を変えて再適用する (ディスクの再読み込みはしない)
	void SetSortSettings(SortKey key, bool descending, bool dirs_first);

	/// 現在の並べ替え設定を表す短い文字列 (ヘッダ表示用。例: "名前 昇順")
	UnicodeString GetSortSummary() const;

	//-- マスク絞り込み --------------------------------------------------------
	UnicodeString GetMask() const { return mask_; }
	bool HasMask() const { return !mask_.IsEmpty(); }

	/// マスクを設定する (空文字列で解除)。ApplyFilterAndSort() を呼び直す
	void SetMask(const UnicodeString &mask);

private:
	void OnPaint(wxPaintEvent &event);
	void OnSize(wxSizeEvent &event);
	void OnLeftDown(wxMouseEvent &event);
	void OnLeftDClick(wxMouseEvent &event);
	void OnMouseWheel(wxMouseEvent &event);
	void OnSetFocus(wxFocusEvent &event);

	void Collect();                //!< all_items_ をディスクから作り直す
	void ApplyFilterAndSort();     //!< all_items_ からマスク絞り込み + 並べ替えを行い order_ を作る
	void RestoreCursorByName(const UnicodeString &name);
	void EnsureVisible();
	int VisibleRows() const;       //!< 列見出し行を除いた、一覧が表示できる行数
	int RowHeight() const { return row_height_; }
	int HeaderHeight() const { return row_height_; }  //!< 列見出し1行分
	void UpdateMetrics();

	/// 表示上の index (order_ の添字) から実体 (all_items_) を引く
	FileItem &ItemAt(int index) { return all_items_[order_[static_cast<std::size_t>(index)]]; }
	const FileItem &ItemAt(int index) const { return all_items_[order_[static_cast<std::size_t>(index)]]; }

	UnicodeString path_;
	std::vector<FileItem> all_items_;   //!< ディスクから読み取った全件の実体 (マーク状態もここが正)
	std::vector<std::size_t> order_;    //!< マスク絞り込み + 並べ替え後に表示する all_items_ の添字列
	int cursor_ = 0;
	int top_ = 0;         //!< 先頭に表示している行 (order_ 内でのインデックス。見出し行は含まない)
	int row_height_ = 16;
	int char_width_ = 8;
	bool active_ = false;
	wxFont font_;

	SortKey sort_key_ = SortKey::Name;
	bool sort_descending_ = false;
	bool dirs_first_ = true;
	UnicodeString mask_;
};

#endif  // NYANFI_GUI_FILE_PANE_H
