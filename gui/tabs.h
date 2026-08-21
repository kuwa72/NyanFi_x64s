/**
 * @file gui/tabs.h
 * @brief タブ (複数ディレクトリの切り替え) の wx 非依存ロジック
 *
 * @details
 * VCL 版のタブは `src/Global.h` の `tab_info` 構造体 (実測) が示すとおり、
 * **左右のペインで共有する1本のタブバー**であり、ペインごとに独立したタブでは
 * ない。1つのタブが両ペイン分の状態をまとめて持つ:
 *
 * ```cpp
 * struct tab_info {
 *     TRect rc;
 *     TStringList *sel_list[MAX_FILELIST];    // 選択状態 (ペインごと)
 *     TStringList *dir_hist[MAX_FILELIST];    // ディレクトリ履歴 (ペインごと)
 *     int          dir_hist_p[MAX_FILELIST];  // 履歴位置 (ペインごと)
 *     int          sort_mode[MAX_FILELIST];   // 並べ替えモード (ペインごと)
 *     bool         sync_lr;                   // 階層同期
 * };
 * ```
 *
 * `TabList` (ini/タブグループの実体、1行が1タブ) の CSV 書式も実測した
 * (`TABLIST_CSVITMCNT = 9`、`src/Global.h`):
 * `path0,path1,caption,icon,home0,home1,nwl_mode,nwl,sync_lr`。
 * つまり1タブに左右両方のパスが入っており、`src/MainFrm.cpp::TabControl1Change`
 * も `TabControl1->TabIndex` という単一の添字で両ペインを同時に切り替えている。
 *
 * このため本ヘッダの `TabManager` も「ペインごとに独立したタブ」ではなく、
 * **両ペイン分の状態を1タブにまとめて持つ、共有のタブバー**として設計してある
 * (依頼文面の「ペインごとにタブを持つ (左右独立)」という前提は、実測の結果
 * VCL 版の実際の挙動と異なっていたため、実測に合わせた。報告に明記する)。
 *
 * 保持する項目も実測に合わせて絞った:
 *  - ディレクトリ (path0/path1 に相当) … 保持する
 *  - 並べ替え (sort_mode[2] に相当) … 保持する。`tab_info::sort_mode` が
 *    実在するため、タブごとに独立した並べ替え設定を持たせる
 *  - パスマスク … `tab_info` に対応するフィールドが無い (実測)。
 *    そのためタブをまたいでも共有のまま (`FilePane::mask_` はタブに紐付けない)
 *  - 選択状態 (`sel_list`)・階層同期 (`sync_lr`)・ホームパス (`home0/home1`)・
 *    カスタムキャプション (`caption`) … VCL 版は持つが、Phase 2 骨格では
 *    対応する機能自体が無い (SyncLR 未実装、TabDlg 未移植) ため保持しない。
 *    未対応として報告に明記する
 *
 * ini への永続化はこのクラス専用の新規セクション ("WxGuiTabs"、
 * gui/settings.h の "WxGuiWindow" と同じ考え方) を使う。VCL 版のタブグループ
 * ファイル書式 (TabList の CSV) とは互換性が無い新規設計 (推測ではなく
 * Phase 2 向けに決めたもの)。
 */
#ifndef NYANFI_GUI_TABS_H
#define NYANFI_GUI_TABS_H

#include <vector>

#include "gui/file_item.h"  // SortKey

class UsrIniFile;

//---------------------------------------------------------------------------
/// 1ペイン分のタブの状態 (ディレクトリ + 並べ替え設定)
struct PaneTabState {
	UnicodeString directory;             //!< このタブでのディレクトリ (末尾 "\\" 付き)
	UnicodeString home;                  //!< このタブのホーム (TabHome の戻り先。
	                                     //!< VCL の TabList CSV の home0/home1 に相当。
	                                     //!< 空ならタブを作ったときのディレクトリ)
	SortKey sort_key = SortKey::Name;    //!< 並べ替えキー (tab_info::sort_mode に相当)
	bool sort_descending = false;
	bool dirs_first = true;
};

//---------------------------------------------------------------------------
/// 1タブ分の状態 (左右両ペイン)
struct TabState {
	PaneTabState panes[2];  //!< [0]=左ペイン, [1]=右ペイン (MAX_FILELIST に相当)
};

//---------------------------------------------------------------------------
/**
 * @brief タブの追加・削除・切り替えを管理する (両ペイン共有の1本のタブバー)
 *
 * @details 「現在のタブ (CurTabIndex に相当) の状態を書き換える」操作
 * (`MutableCurrent`) と「タブを切り替える」操作 (`Next`/`Prev`/`SelectAt`) を
 * 分けてある。切り替えの前に、呼び出し側 (gui/main_frame.cpp) が現在の
 * ペインの実際の状態 (ディレクトリ・並べ替え) を `MutableCurrent()` へ
 * 書き戻してから切り替えること (VCL 版の `StoreTabStt` → `TabControl1->TabIndex`
 * 変更 → `TabControl1Change` という手順と同じ)。
 */
class TabManager {
public:
	/// 最初は必ず1本のタブを持つ (最後のタブを閉じられないのと対になる)。
	/// 内容は空 (呼び出し側が直後に MutableCurrent() へ書き込むこと)
	TabManager();

	int Count() const { return static_cast<int>(tabs_.size()); }
	int CurrentIndex() const { return current_; }

	const TabState &Current() const { return tabs_[static_cast<std::size_t>(current_)]; }
	TabState &MutableCurrent() { return tabs_[static_cast<std::size_t>(current_)]; }
	const TabState &At(int index) const { return tabs_[static_cast<std::size_t>(index)]; }

	/**
	 * @brief タブを追加する (F:AddTab)
	 * @details VCL 版 (`AddTabActionExecute`) の既定 (ActionParam 無し、
	 * ツールバー等からの呼び出しに相当) と同じく、**末尾に追加**しそこへ
	 * 切り替える。VCL 版が持つ "NX" (次に挿入) / "PR" (前に挿入)
	 * パラメータは、Phase 2 骨格にはパラメータ付きでコマンドを呼ぶ仕組みが
	 * 無い (メニュー/ツールバー限定の機能だったと見られる) ため対応しない
	 * (未対応。要検証)
	 * @param state 新規タブの初期状態 (通常は現在のタブの複製)
	 * @return 追加したタブの添字
	 */
	int AddTab(const TabState &state);

	/**
	 * @brief 現在のタブを閉じる (F:DelTab)
	 * @details **最後の1枚は閉じられない** (要件。VCL 版の
	 * `DelTabActionExecute` 自体には枚数の下限チェックが見当たらず、
	 * 空になった状態で `TabControl1Change` が呼ばれると
	 * `get_TabInfo` が NULL を返して `Abort()` する経路に見えるが、
	 * 実機の挙動は未検証。本実装は要件に従いガードする)
	 * @return 閉じられたら true。タブが1枚しか無ければ何もせず false
	 */
	bool CloseCurrentTab() { return CloseTabAt(current_); }

	/**
	 * @brief 指定したタブを閉じる (タブバーの "x" クリックなど、現在のタブ
	 * 以外を閉じる操作向け)
	 * @details 閉じるタブが現在のタブより前にあれば現在の添字を1つ詰めるだけで、
	 * 現在のタブそのものの選択状態は変えない (バックグラウンドのタブを閉じても
	 * 表示中のタブが切り替わらないようにするため)
	 * @return 閉じられたら true。タブが1枚しか無い/index が範囲外なら false
	 */
	bool CloseTabAt(int index);

	/**
	 * @brief 現在のタブを移動する (F:MoveTab)
	 * @param direction 負なら前へ、正なら次へ
	 * @return 移動したら true。タブが1枚以下なら false
	 * @details VCL 版 (MainFrm.cpp:37422) はパラメータ無しのとき
	 *          **末尾から先頭へ回る** (`(tab_idx0 < Count-1)? +1 : 0`)。
	 *          前へも同様に先頭から末尾へ回る。移動後もそのタブが選ばれたまま
	 */
	bool MoveCurrentTab(int direction);

	/**
	 * @brief 現在のタブ以外をすべて閉じる (F:SoloTab)
	 * @return 閉じたタブの枚数
	 * @details VCL 版 (MainFrm.cpp:37401) と同じ。1枚しか無ければ 0 を返す
	 */
	int CloseOtherTabs();

	/**
	 * @brief タブをホームのディレクトリへ戻す (F:TabHome)
	 * @param all true ならすべてのタブ、false なら現在のタブだけ
	 * @return 戻したタブの枚数
	 * @details VCL 版 (MainFrm.cpp:37590) は CSV の home0/home1 が**空でない
	 *          ときだけ**書き戻す。空のペインは触らない
	 */
	int GoHome(bool all);

	/**
	 * @brief 番号 (1 起点) またはキャプションでタブを選ぶ (F:ToTab)
	 * @return 選べたら true
	 * @details VCL 版 (MainFrm.cpp:37462) は **まず数値として解釈**し、
	 *          数値でなければキャプションの完全一致 (大文字小文字を区別しない)
	 *          で探す。番号は 1 起点
	 */
	bool SelectByParam(const UnicodeString &param);

	/// 次のタブへ切り替える (周回。VCL 版の NextTabActionExecute と同じ)
	void NextTab();
	/// 前のタブへ切り替える (周回。VCL 版の PrevTabActionExecute と同じ)
	void PrevTab();

	/// index 番目のタブへ切り替える (F:ToTab / F:PopupTab で選ばれた番号)
	/// @return index が範囲外なら false (何もしない)
	bool SelectAt(int index);

	/// タブ一覧のキャプション (表示用。ディレクトリの末尾要素名。
	/// VCL 版の `get_DirNwlName` 相当の簡易版。空なら "(無題)")
	UnicodeString CaptionAt(int index) const;
	std::vector<UnicodeString> Captions() const;

	/**
	 * @brief ini へ保存する (このクラス専用のセクション "WxGuiTabs")
	 * @details gui/settings.h と同じ理由で、VCL 版の本物の ini を書き換え
	 * ないよう `<exe名>_wx.ini` 専用の新規セクションにする
	 */
	void SaveToIni(UsrIniFile &ini) const;

	/**
	 * @brief ini から読み込む
	 * @details セクションが無い/1件も読めない場合は何もしない (呼び出し側が
	 * 既定の1タブのまま使い続けられるようにするため)
	 */
	void LoadFromIni(UsrIniFile &ini);

private:
	std::vector<TabState> tabs_;
	int current_ = 0;
};

#endif  // NYANFI_GUI_TABS_H
