/**
 * @file gui/main_frame.h
 * @brief メインウィンドウ (2画面 + ステータス)
 *
 * issue #1 の Phase 2 の骨格。VCL の MainFrm (38,502行) を置き換えるものでは
 * なく、「移植済みのロジック層だけでファイラとして最低限動く」ことを示す土台。
 *
 * 入力は MainFrame でまとめて拾い、キー名 → コマンド名 → 実行 の順に流す。
 * この流れは VCL 版と同じで、コマンド名の綴りも usr_cmdlist.cpp の表に合わせて
 * あるため、ini のキー割り当てをそのまま読み込める (gui/key_map.h の
 * LoadFromIni)。ウィンドウ位置・ペインのディレクトリは gui/settings.h の
 * Settings が起動時に復元し、終了時に保存する。
 *
 * タブ (複数ディレクトリの切り替え) は gui/tabs.h の TabManager が状態を持つ。
 * VCL 版の tab_info (実測。gui/tabs.h 冒頭のコメントを参照) が左右ペイン共有の
 * 1本のタブバーだったため、ここでもペインごとの独立タブにはしていない。
 */
#ifndef NYANFI_GUI_MAIN_FRAME_H
#define NYANFI_GUI_MAIN_FRAME_H

#include <functional>
#include <memory>
#include <vector>

#include <wx/wx.h>

#include "gui/file_ops.h"
#include "gui/file_pane.h"
#include "gui/image_viewer.h"
#include "gui/key_map.h"
#include "gui/navigation.h"
#include "gui/settings.h"
#include "gui/tabs.h"
#include "gui/text_viewer.h"

class TabBar;  // gui/main_frame.cpp の無名名前空間で定義する自前描画のタブバー

/**
 * @brief メインウィンドウ
 */
class MainFrame : public wxFrame {
public:
	MainFrame();

	/// コマンド名を実行する。未実装のコマンドなら false
	bool Execute(const UnicodeString &command);

	FilePane *ActivePane() { return panes_[active_]; }
	FilePane *OppositePane() { return panes_[1 - active_]; }

private:
	void OnCharHook(wxKeyEvent &event);
	void OnClose(wxCloseEvent &event);
	void OnSize(wxSizeEvent &event);

	void SetActivePane(int index);
	void UpdateStatus();
	void ShowKeyList();
	void ShowCmdList();
	void ShowSortDialog();  //!< ソートダイアログ (S)。並べ替えキー/昇降順/Dir集約を選ぶ
	void ShowMaskDialog();  //!< パスマスク入力 (Ctrl+M)。ファイル名マスクで一覧を絞り込む

	// インクリメンタルサーチ (gui/navigation.h の IncrementalSearch)。状態遷移は
	// OnCharHook が incsearch_.IsActive() を見て他のキー処理より先に横取りする
	void StartIncSearch();                       //!< サーチモードへ入る (F)
	void HandleIncSearchKey(wxKeyEvent &event);   //!< サーチ中の1キー分の処理
	void ExitIncSearch();                        //!< サーチモードを抜ける (Esc/Enter)
	void HandleIncSearchChar(wchar_t ch);         //!< 1文字追加。一致0件なら元に戻す
	void HandleIncSearchBackspace();              //!< 1文字削除 (BackSpace)
	void JumpToNearestIncSearchMatch();           //!< 現在位置から最も近い一致へ移動する

	// ディレクトリ履歴・ドライブ一覧・パス直接入力 (gui/navigation.h)
	void ShowDirHistoryDialog();  //!< ディレクトリ履歴の一覧から選ぶ (H)
	void ShowDriveListDialog();   //!< ドライブの一覧から選ぶ (L)
	void ShowInputDirDialog();    //!< パスを直接入力して移動する (Ctrl+G、推測のキー)

	// ファイル操作 (gui/file_ops.h)。いずれも確認ダイアログを出してから実行し、
	// 結果 (成功/スキップ/失敗の件数) を必ず表示する。詳細は main_frame.cpp を参照
	void CmdCopy();       //!< アクティブペインの選択項目を、反対側のペインへコピーする (C)
	void CmdMove();       //!< アクティブペインの選択項目を、反対側のペインへ移動する (M)
	void CmdDelete();     //!< アクティブペインの選択項目をゴミ箱へ送る (D)
	void CmdCreateDir();  //!< アクティブペインにディレクトリを作成する (K)
	void CmdRenameDlg();  //!< 選択項目 (マーク済み、無ければカーソル位置) の一括リネーム (R)

	// ファイルを開く (gui/file_open.h) とファイル情報 (gui/file_info_panel.h)
	void CmdOpenStandard();  //!< 関連付けで開く (ENTER)。ディレクトリなら入る
	void CmdOpenByApp();     //!< アプリケーションから開く (Ctrl+Enter)
	void CmdPropertyDlg();   //!< ファイル情報ダイアログ (Alt+Enter、推測のキー)

	// テキストビューア (gui/text_viewer.h)。"V" (src/Global.cpp の既定キー表
	// "F:V=TextViewer" と同じ) で開く。開いている間はキー入力を丸ごと
	// TextViewer::HandleKey に渡す (OnCharHook を参照)
	void CmdTextViewer();    //!< カーソル位置のファイルをビューアで開く (V)
	void ShowViewer(bool show);  //!< ビューアの表示/非表示を切り替える

	// 画像ビューア (gui/image_viewer.h)。"G" は src/Global.cpp の既定キー表
	// ("F:G=ImageViewer") と同じ。前後の画像への移動キー (Left/Right) は
	// 既定キー表に記載が無く推測 (gui/main_frame.cpp::CmdImageViewer 冒頭の
	// コメントを参照)
	void CmdImageViewer();      //!< カーソル位置の画像をビューアで開く (G)
	void ShowImageViewer(bool show);  //!< 画像ビューアの表示/非表示を切り替える
	/// アクティブペインの現在のディレクトリ内で、対応拡張子のファイルだけを
	/// 表示順に集めた一覧を作り直す (image_nav_list_/image_nav_index_ を更新)
	void BuildImageNavList(const UnicodeString &current_name);
	/// Left/Right (前後の画像へ) が押されたときに呼ばれる (ImageViewer::SetOnNavigate)
	void CmdImageNavigate(int direction);

	// 文字列検索 (gui/grep_dialog.h)。"FV:Grep" (usr_cmdlist.cpp のコマンド表)
	// に既定キーの記載が無かったため、キー割り当ては推測 (gui/key_map.cpp 参照)
	void CmdGrep();  //!< アクティブペインのディレクトリを対象に grep する

	// タブ (複数ディレクトリの切り替え。gui/tabs.h の TabManager)。VCL 版の
	// tab_info (実測。gui/tabs.h 冒頭のコメントを参照) と同じく、左右ペイン
	// 共有の1本のタブバーとして実装してある (ペインごとの独立タブではない)
	void CmdAddTab();   //!< タブを追加する (Ctrl+T、推測のキー)。現在のタブを複製して末尾に追加
	void CmdDelTab();   //!< 現在のタブを閉じる (Ctrl+W、推測のキー)。最後の1枚は閉じない
	void CmdNextTab();  //!< 次のタブへ (Ctrl+Tab、推測のキー)

	//-- 選択操作の受け渡し (判断は gui/selection.h の純関数が持つ。規約8) ----
	/// ステータスバーに一時的な警告を出す (次の UpdateStatus() で消える)
	void SetStatusWarning(const UnicodeString &text);
	/// 表示中の項目を取り出して fn を適用し、書き戻す。
	/// fn が選択を1件も作らなかったら false (呼び出し側が警告を出す)
	bool ApplySelection(FilePane *pane, const std::function<void(std::vector<FileItem> &)> &fn);
	/// カーソル位置の選択を反転してから delta だけ動かす (Shift+↑↓ / SelectUp)
	void MarkCurrentAndMove(FilePane *pane, int delta);
	/// from から to までを選択する (ページ移動・先頭末尾移動と組で使う)
	void MarkBetween(FilePane *pane, int from, int to);
	/// 指定文字列を含む項目を選択する (MatchSelect)
	void CmdMatchSelect();

	//-- 表示の切り替え (判断は gui/view_state.h の純関数が持つ) --------------
	/// 左ペインの取り分を変えて配置し直す
	void SetBorderRatio(double ratio);
	/// 両ペインに同じ表示切り替えを適用する (VCL も MAX_FILELIST 全部に効く)
	void ToggleBothPanes(const std::function<void(FilePane *)> &fn, bool reload);
	/// 左右のディレクトリとカーソルを入れ替える (SwapLR)
	void CmdSwapLR();

	//-- ディレクトリ移動 -----------------------------------------------------
	void CmdToRoot();               //!< ルートディレクトリへ (ToRoot)
	void CmdCopyPath(bool to_opp);  //!< カレント⇄反対のパスを揃える (CurrToOpp / CurrFromOpp)
	void CmdCsrDirToOpp();          //!< カーソル位置のディレクトリを反対側に開く
	void CmdToOppSameItem();        //!< 反対側の同名項目へカーソルを移す
	void CmdParentOn(int index);    //!< 指定ペインを親ディレクトリへ
	void CmdCycleDrive(bool forward);  //!< 次/前のドライブへ (NextDrive / PrevDrive)
	void CmdPushDir();
	void CmdPopDir();
	void CmdShowDirStack();

	//-- タブ操作 (機能群4) ---------------------------------------------------
	void CmdMoveTab(int direction);  //!< タブの位置を移動 (MoveTab)
	void CmdSoloTab();               //!< 他のタブをすべて閉じる (SoloTab)
	void CmdTabHome(bool all);       //!< タブをホームへ戻す (TabHome)
	void CmdToTab();                 //!< 番号/キャプションでタブを選ぶ (ToTab)
	void CmdSubDirList();            //!< サブディレクトリ一覧から選んで移動
	void CmdSpecialDirList();        //!< 特殊フォルダ一覧から選んで移動

	//-- ファイル操作 (機能群5) -----------------------------------------------
	/// 入力したディレクトリへコピー/移動する (CopyTo / MoveTo)
	void CmdCopyMoveTo(bool move);
	/// 名前の大文字/小文字を変換する (NameToUpper / NameToLower)
	void CmdChangeNameCase(file_ops::NameCase how);
	/// ファイル名をクリップボードへ (CopyFileName)
	void CmdCopyFileName(bool full_path);
	/// 空のファイルを作る (NewFile)
	void CmdNewFile();

	//-- クリップボード経由のファイル操作 (機能群5の続き) ---------------------
	void CmdFilesToClip(bool cut);  //!< CopyToClip / CutToClip
	void CmdPaste();                //!< Paste (破壊的。確認ダイアログを出す)

	DirStack dir_stack_;          //!< ディレクトリ・スタック (PushDir / PopDir)
	bool sync_lr_ = false;        //!< 左右のディレクトリを同期させるか (SyncLR)

	double border_ratio_ = 0.5;   //!< 左ペインの取り分 (gui/view_state.h)
	wxBoxSizer *columns_ = nullptr;  //!< 左右のペインを並べる sizer (比率を変えるため保持)
	void CmdPrevTab();  //!< 前のタブへ (Shift+Ctrl+Tab、推測のキー)
	void ShowTabListDialog();  //!< タブの一覧から選ぶ (Ctrl+E、推測のキー。F:PopupTab に相当)
	/// 現在のタブの記録 (tabs_.MutableCurrent()) を、いま実際に両ペインが
	/// 開いているディレクトリ・並べ替え設定で上書きする (VCL 版の
	/// StoreTabStt/SetCurTab 相当。タブを切り替える・追加する・保存する前に必ず呼ぶ)
	void StoreCurrentTabState();
	/// tabs_ の指定タブの記録を両ペインへ適用する (VCL 版の TabControl1Change 相当)。
	/// ディレクトリが現在のペインと同じなら SetPath を呼ばない (カーソル位置・
	/// 履歴の重複を避けるため)。呼ぶ前に必要なら StoreCurrentTabState() で
	/// 切り替え元のタブの記録を更新しておくこと
	void ApplyTabState(const TabState &state);
	void RefreshTabBar();  //!< tab_bar_ の表示 (キャプション一覧・現在位置) を更新する

	void LoadSettings();
	void SaveSettings();

	FilePane *panes_[2] = {nullptr, nullptr};
	wxStaticText *headers_[2] = {nullptr, nullptr};
	wxWindow *root_ = nullptr;      //!< 2ペインを収めた親パネル (ShowViewer でのサイズ調整用)
	TextViewer *viewer_ = nullptr;  //!< テキストビューア (root_ と同じ領域に重ねて表示)
	ImageViewer *image_viewer_ = nullptr;  //!< 画像ビューア (同じく root_ と同じ領域に重ねて表示)

	// CmdImageViewer で画像ビューアを開いた時点のディレクトリ内の対象ファイル
	// 一覧 (image_load::IsSupportedExt に一致するものだけ、".." は除く)。
	// Left/Right での前後移動はこの一覧の中だけを動く (ファイルペインの
	// カーソルとは独立。VCL 版の ViewFileList に相当する簡略版)
	UnicodeString image_nav_dir_;
	std::vector<UnicodeString> image_nav_list_;
	int image_nav_index_ = -1;

	int active_ = 0;
	KeyMap keymap_;
	Settings settings_{Settings::DefaultIniPath()};
	IncrementalSearch incsearch_;  //!< インクリメンタルサーチの状態 (gui/navigation.h)

	TabManager tabs_;       //!< タブの状態 (gui/tabs.h)。左右ペイン共有の1本のタブバー
	TabBar *tab_bar_ = nullptr;  //!< タブの見た目 (自前描画。gui/main_frame.cpp を参照)
};

#endif  // NYANFI_GUI_MAIN_FRAME_H
