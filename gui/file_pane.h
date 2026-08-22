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

#include <functional>
#include <vector>

#include <wx/wx.h>

#include "gui/file_item.h"
#include "gui/navigation.h"

/**
 * @brief ファイル一覧ペイン
 */
class FilePane : public wxWindow {
public:
	FilePane(wxWindow *parent, wxWindowID id);

	//-- 表示対象 ----------------------------------------------------------
	/// ディレクトリを開く
	/// @param record_history true (既定) ならディレクトリ履歴に記録する。
	///        履歴をたどる移動 (GoBackHistory 等) からは false で呼ばれる
	bool SetPath(const UnicodeString &path, bool record_history = true);
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
	//-- 表示の切り替え (gui/view_state.h の純関数が判断を持つ) ---------------
	/// 隠しファイルを出すか (ShowHideAtr)。変えたら Reload() が要る
	void SetShowHidden(bool v) { show_hidden_ = v; }
	bool GetShowHidden() const { return show_hidden_; }
	/// システムファイルを出すか (ShowSystemAtr)。同上
	void SetShowSystem(bool v) { show_system_ = v; }
	bool GetShowSystem() const { return show_system_; }
	/// サイズをバイト単位で出すか (ShowByteSize)。再描画で足りる
	void SetByteSize(bool v) { byte_size_ = v; Refresh(); }
	bool GetByteSize() const { return byte_size_; }
	/// サイズと更新日時を隠すか (HideSizeTime)。再描画で足りる
	void SetHideSizeTime(bool v) { hide_size_time_ = v; Refresh(); }
	bool GetHideSizeTime() const { return hide_size_time_; }

	void ToggleMark();
	/// カーソルを動かさずに選択を反転する (Shift+↑↓ / SelectUp 用。
	/// VCL は「反転してから移動」の順で、移動先は反転しない)
	void ToggleMarkNoMove();
	void MarkAll(bool marked);
	int GetMarkedCount() const;

	//-- インクリメンタルサーチ (表示のハイライトのみ。状態管理は MainFrame 側の
	//   IncrementalSearch が持つ。gui/navigation.h を参照) ---------------------
	/// 一覧の項目名を表示順で返す (FindIncrementalSearchMatch に渡す用)
	std::vector<UnicodeString> VisibleNames() const;

	/// 表示順の index (0 始まり) から項目を引く。範囲外は nullptr。
	/// 画像ビューアの次/前の移動で「ディレクトリを画像と誤認しない」ために必要
	const FileItem *ItemAtVisible(int index) const;
	/// keyword に一致する項目の FileItem::matched を更新する (再描画も行う)
	void ApplyIncSearchHighlight(const UnicodeString &keyword);
	/// ハイライトを消す (ApplyIncSearchHighlight(EmptyStr) と同じ)
	void ClearIncSearchHighlight() { ApplyIncSearchHighlight(EmptyStr); }
	/// ハイライト中 (matched) の項目数
	int GetMatchedCount() const;

	/// 表示用の対象名を返す。マーク済みがあればそれら (".." を除く)、
	/// 無ければカーソル位置の1件 (".." なら空)。
	/// **ファイル操作の対象には使わないこと** (結果リストでは名前だけでは
	/// 場所が決まらない)。確認ダイアログの表示にだけ使う
	std::vector<UnicodeString> GetSelectedNames() const;

	/// ファイル操作 (Copy/Move/Delete 等) の対象を**フルパス**で返す。
	/// 対象の選び方は GetSelectedNames() と同じ。
	/// 結果リストの項目は一覧のディレクトリとは別の場所にあるので、
	/// `GetPath() + 名前` で組み立てると別のファイルを指してしまう
	std::vector<UnicodeString> GetSelectedPaths() const;

	/// カーソル位置の項目のフルパス。項目が無いか ".." なら空
	UnicodeString CurrentFullPath() const;

	/**
	 * @brief 栞マークが付いているかの問い合わせ先を渡す
	 * @param fn フルパスを受け取って true/false を返すもの。空なら印を出さない
	 * @details 栞の保存先 (`UsrIniFile`) は `MainFrame` が持っているので、
	 *          ペインからは直接引けない。描画のたびに呼ばれるので
	 *          **重い処理を渡さないこと** (実体は ini のメモリ上の索引を引くだけ)
	 */
	void SetBookmarkTest(const std::function<bool(const UnicodeString &)> &fn)
	{
		is_bookmarked_ = fn;
		Refresh();
	}

	/// GetSelectedNames() と同じ対象選択 (マーク済み、無ければカーソル位置の
	/// 1件) を FileItem (name/is_dir を含む) で返す。一括リネーム
	/// (gui/rename_dialog.h) がディレクトリと拡張子付きファイルを区別するために使う
	std::vector<FileItem> GetSelectedItems() const;

	/// 表示中の項目を並び順のまま取り出す (選択操作の純粋ロジック
	/// `gui/selection.h` に渡すため。絞り込みで隠れている項目は含まない)
	std::vector<FileItem> VisibleItems() const;

	//-- 結果リスト (検索結果などの、別ディレクトリの項目が混ざった一覧) ------
	/**
	 * @brief 結果リストを表示する
	 * @param title 見出し (ヘッダに出す)
	 * @param items 表示する項目 (それぞれ full_path を持つこと)
	 * @details **このモードの間は Reload() でディスクを読み直さない。**
	 *          読み直すと結果が消えてしまうため。`ReturnToList()` で通常の
	 *          一覧に戻る (VCL の ReturnList 相当)
	 */
	void ShowResultList(const UnicodeString &title, const std::vector<FileItem> &items);

	/**
	 * @brief 並び順を保ったまま一覧を表示する (ワークリスト)
	 * @param title 見出し
	 * @param items 表示する項目 (この順に出す)
	 * @details `ShowResultList()` と同じく `Reload()` でディスクを読み直さないが、
	 *          **並べ替えを掛けない**。ワークリストは項目を1つずつ動かせる
	 *          (`WorkItemUP` / `WorkItemDown`) ので、並べ替えると操作の結果が
	 *          その場で消えてしまうため。VCL も `NotSortWorkList` か
	 *          区切り行があれば並べ替えない (`Global.cpp:4551`)
	 */
	void ShowOrderedList(const UnicodeString &title, const std::vector<FileItem> &items);

	/// 結果リストを表示中か (ShowOrderedList で出したワークリストも含む)
	bool IsResultList() const { return result_mode_; }
	/// 並び順を保つ一覧 (ワークリスト) を表示中か
	bool IsOrderKept() const { return keep_order_; }
	/// 結果リストの見出し
	UnicodeString ResultTitle() const { return result_title_; }
	/// 通常の一覧に戻る (ReturnList)
	void ReturnToList();

	/// 項目のフルパスを返す。結果リストなら full_path、通常なら path_ + name
	/// (判断は wx 非依存の FullPathOfItem が持つ。gui/file_item.h)
	UnicodeString FullPathOf(const FileItem &item) const { return FullPathOfItem(path_, item); }

	/// `VisibleItems()` で取り出したものの選択状態を書き戻す。
	/// **並び順が変わっていないことが前提**なので、取り出してから
	/// 書き戻すまでの間に再読み込みや並べ替えを挟まないこと
	void ApplyMarks(const std::vector<FileItem> &items);

	//-- 状態 --------------------------------------------------------------
	int GetItemCount() const { return static_cast<int>(order_.size()); }
	bool IsActive() const { return active_; }
	void SetActive(bool active);

	/// 親ディレクトリへ移動する。移動元のディレクトリ名にカーソルを合わせる
	bool GoParent();

	/// カーソル位置がディレクトリならそこへ入る
	bool EnterCurrent();

	//-- ディレクトリ履歴 (戻る/進む/一覧。gui/navigation.h の DirHistory) -------
	bool HasBackDirHistory() const { return history_.CanBack(); }
	bool HasForwardDirHistory() const { return history_.CanForward(); }
	bool GoBackDirHistory();     //!< 履歴を1つ戻る (B)
	bool GoForwardDirHistory();  //!< 履歴を1つ進む (Shift+B、推測のキー)
	/// 履歴一覧 (古い順) と現在位置 (履歴ダイアログの表示用)
	const std::vector<UnicodeString> &DirHistoryEntries() const { return history_.Entries(); }
	int DirHistoryCurrentIndex() const { return history_.CurrentIndex(); }
	/// 履歴一覧から index (DirHistoryEntries() の添字) の位置へ直接移動する
	bool GoDirHistoryIndex(int index);

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
	bool result_mode_ = false;      //!< 結果リストを表示中か
	bool keep_order_ = false;       //!< 並べ替えを掛けないか (ワークリスト)
	UnicodeString result_title_;    //!< 結果リストの見出し

	bool show_hidden_ = false;      //!< 隠しファイルを出すか (ShowHideAtr)
	bool show_system_ = false;      //!< システムファイルを出すか (ShowSystemAtr)
	bool byte_size_ = false;        //!< サイズをバイト単位で出すか (ShowByteSize)
	bool hide_size_time_ = false;   //!< サイズと更新日時を隠すか (HideSizeTime)

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

	DirHistory history_;  //!< このペインのディレクトリ履歴 (戻る/進む/一覧)

	/// 栞マークが付いているかの問い合わせ先 (gui/bookmarks.h を持つ MainFrame が渡す)
	std::function<bool(const UnicodeString &)> is_bookmarked_;
};

#endif  // NYANFI_GUI_FILE_PANE_H
