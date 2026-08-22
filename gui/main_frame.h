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

#include "gui/bookmarks.h"
#include "gui/convert_ops.h"
#include "gui/log_win.h"
#include "gui/dir_info.h"
#include "gui/external.h"
#include "gui/misc_ops.h"
#include "gui/file_ops.h"
#include "gui/file_ops2.h"
#include "gui/find_files.h"
#include "gui/links.h"
#include "gui/file_pane.h"
#include "gui/image_viewer.h"
#include "gui/key_map.h"
#include "gui/navigation.h"
#include "gui/settings.h"
#include "gui/tabs.h"
#include "gui/text_viewer.h"
#include "gui/work_list.h"
#include "usr_tag.h"

class TabBar;  // gui/main_frame.cpp の無名名前空間で定義する自前描画のタブバー

/**
 * @brief メインウィンドウ
 */
class MainFrame : public wxFrame {
public:
	MainFrame();

	/// コマンド名を実行する。未実装のコマンドなら false。
	/// `_` の後ろは引数として扱う (VCL と同じ。`WorkList_OP` など)
	bool Execute(const UnicodeString &full_command);

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

	//-- リンク・属性 (機能群6) -----------------------------------------------
	void CmdCreateLinks(links::LinkKind kind);  //!< 反対ペインにリンクを作る
	void CmdSetDirTime();                       //!< ディレクトリの日時を配下の最新に

	//-- 書庫 (機能群7) -------------------------------------------------------
	void CmdListArchive();          //!< 書庫の中身を一覧する
	void CmdTestArchive();          //!< 書庫の正当性を検査する
	void CmdUnPack(bool to_current);//!< 書庫を展開する (UnPack / UnPackToCurr)
	void CmdPack(bool to_current);  //!< 書庫を作る (Pack / PackToCurr)

	//-- 比較・ハッシュ (機能群8) ---------------------------------------------
	void CmdGetHash();        //!< 選択項目のハッシュ値を出す
	void CmdCompareHash();    //!< 反対側の同名ファイルとハッシュを比べる
	void CmdSelOnlyCur();     //!< カレント側だけにあるファイルを選択
	void CmdToOppSameHash();  //!< 反対側の同ハッシュ項目へ
	void CmdDiffDir();        //!< 左右のディレクトリを比較

	//-- テキスト操作 (機能群9) -----------------------------------------------
	void CmdCountLines();      //!< 行数を数える
	void CmdJoinText();        //!< テキストを1つに結合する
	void CmdConvertTextEnc();  //!< 文字コードを変換する (破壊的)
	void CmdListFileName();    //!< ファイル名の一覧を出す

	//-- 外部連携 (機能群10) --------------------------------------------------
	void CmdLaunchShell(external::ShellKind kind);  //!< cmd / PowerShell / wt を開く
	void CmdOpenByExplorer();  //!< エクスプローラで開く
	void CmdContextMenu();     //!< シェルのコンテキストメニューを出す
	void CmdOpenTrash();       //!< ごみ箱を開く
	void CmdFileRun();         //!< 「ファイル名を指定して実行」
	void CmdOpenCtrlPanel();   //!< コントロールパネルを開く

	//-- 情報系 (機能群11/12) -------------------------------------------------
	void CmdCalcDirSize(bool all);  //!< ディレクトリ容量を計算 (CalcDirSize / All)
	void CmdFileExtList();          //!< 拡張子別の一覧
	void CmdListTree();             //!< ディレクトリ構造のツリー
	void CmdAbout();                //!< バージョン情報

	//-- 設定・その他 (機能群13) ----------------------------------------------
	void CmdIniFile(bool edit);   //!< ini を編集 / 閲覧する
	void CmdNameFromClip();       //!< クリップボードの内容にファイル名を変える (破壊的)
	void CmdShareList();          //!< 共有フォルダ一覧
	void CmdNetConnect(bool disconnect);  //!< ネットワークドライブの割り当て / 切断
	void CmdListClipboard();      //!< クリップボードの内容を表示
	void CmdRestart();            //!< 再起動

	/**
	 * @brief 「このペインのディレクトリ」が要る操作を結果リスト上で断る
	 * @param verb 操作名 (メッセージに出す)
	 * @return true なら断った (呼び出し側は何もせず戻る)
	 * @details 結果リストの項目は**別々のディレクトリにある**ので、
	 *          `GetPath()` は最後に開いていたディレクトリを指すだけで
	 *          操作の宛先にならない。VCL も結果リストの上では
	 *          この種の操作を `USTR_CantOperate` で断る
	 */
	bool RejectOnResultList(const UnicodeString &verb);

	/**
	 * @brief 宛先が反対ペインになる操作を、反対側が結果リストのときに断る
	 * @param verb 操作名 (メッセージに出す)
	 * @return true なら断った
	 * @details 結果リストのペインの `GetPath()` は「結果リストに入る直前に
	 *          開いていたディレクトリ」でしかないので、宛先として使うと
	 *          **見えていない場所へ書き込む**ことになる
	 */
	bool RejectIfOppositeIsResultList(const UnicodeString &verb);

	//-- 検索と結果リスト -----------------------------------------------------
	void CmdFindFiles(find_files::Target target);  //!< 名前で探して結果リストに出す
	void CmdReturnList();                           //!< 通常の一覧に戻る (ReturnList)
	void CmdFindDuplicates();                       //!< 重複ファイルを探す (FindDuplDlg)
	void CmdSelSameDir();                           //!< 結果リストで同じディレクトリの項目を選択

	//-- ワークリスト (gui/work_list.h) ---------------------------------------
	//
	// 「別々のディレクトリにあるファイルを集めて持ち歩く一覧」。中身は
	// work_items_ が正で、ペインには ShowWorkListOn() で写す。並べ替えや
	// 追加・削除の判断はすべて wx 非依存の work_list:: に置いてある (規約8)。
	//
	// VCL 版と同じく**左右のペインで1つを共有する** (Global.cpp の WorkList は
	// グローバル1本)。表示中のペインは work_pane_ が持つ。
	void CmdWorkList(bool opposite);   //!< 表示/解除を切り替える (WorkList)
	void CmdHomeWorkList();            //!< ホームワークリストを開く (HomeWorkList)
	void CmdNewWorkList();             //!< 新規作成 (NewWorkList)
	void CmdLoadWorkList();            //!< ファイルから読み込む (LoadWorkList)
	bool CmdSaveWorkList();            //!< 上書き保存 (SaveWorkList)。無名なら名前を付けて保存
	bool CmdSaveAsWorkList(bool from_list);  //!< 名前を付けて保存 (SaveAsWorkList)。
	                                         //!< from_list なら現在の一覧の内容を書き出す
	void CmdSelWorkItem();             //!< 一覧のうちワークリストに登録済みの項目を選択 (SelWorkItem)
	void CmdSetAlias();                //!< 項目に別名を付ける (SetAlias)
	void CmdInsSeparator();            //!< 区切り行を挿入する (InsSeparator)
	void CmdWorkItemMove(int direction);  //!< -1=上へ / +1=下へ / 0=カーソル位置へ
	void CmdSendToWorkList();          //!< 選択項目をワークリストに登録する
	void CmdRemoveWorkItems();         //!< ワークリストから外す (ワークリスト上の Delete)
	void CmdDropMissingWorkItems();    //!< 実体の無い項目をまとめて外す (WorkList "DI")

	/// work_items_ を index のペインに出し直す。ペインの選択状態は
	/// 呼ぶ前に SyncWorkMarks() で取り込んでおくこと
	void ShowWorkListOn(int index);
	/// ペイン側の選択状態 (FileItem::marked) を work_items_ に取り込む。
	/// **絞り込み中は何もしない** (添字が合わないので取り違える)
	void SyncWorkMarks();
	/// 絞り込み中のワークリストは中身を変えられない。変えようとしたら断る。
	/// VCL も同じで、`WorkListFiltered` のとき `USTR_WorkFiltered` で中止する
	/// (MainFrm.cpp:11004 ほか)。**添字がずれた状態で並べ替えると
	/// 見えていない項目が動く**ため
	bool RejectWhenWorkFiltered();
	/// ワークリストを出しているペイン。出していなければ nullptr。
	/// **ペイン側で一覧に戻された** (ESC・ディレクトリ移動・項目に入る) 場合も
	/// ここで気付いて work_pane_ を落とす (取りこぼすと「もう出ていないのに
	/// 出ている扱い」になり、Delete がファイル削除ではなく登録解除になる)
	FilePane *WorkPane();
	/// アクティブペインがワークリストか
	bool IsWorkActive() { return WorkPane() != nullptr && work_pane_ == active_; }
	/// 変更があれば「保存しますか?」と聞く。中止を選んだら false
	bool ConfirmDiscardWorkList();
	/// ヘッダに出す見出し ("<ワーク> 名前  n 件")
	UnicodeString WorkListCaption() const;

	//-- ログ (機能群19。判断は gui/log_win.h) --------------------------------
	//
	// VCL のログウィンドウはまだ無いので、**溜めることと見せることだけ**を実装する。
	// スクロールやフォーカス移動 (ScrollUpLog / ToLog) はウィンドウが要るので
	// このPRでは入れていない (報告書 §28)
	void CmdClearLog();     //!< ログを消す (ClearLog)
	void CmdListLog();      //!< ログを一覧で見せる (ListLog / ShowLogWin)
	void CmdViewLog();      //!< ログをテキストビューアで開く (ViewLog)
	void CmdLogFileInfo();  //!< 選択項目のファイル情報をログへ (LogFileInfo)
	void CmdListNyanFi();   //!< NyanFi 自身の情報をログへ (ListNyanFi)

	/// 操作の結果をログに残す。**破壊的な操作の後は必ず呼ぶ**
	/// (画面のダイアログは閉じると消えるが、ログは残るので後から追える)
	void LogResult(const UnicodeString &verb, const file_ops::FileOpResult &result);

	log_win::LogBuffer log_;  //!< ログの中身

	//-- 抽出と変換 (機能群18。実処理は gui/convert_ops.h が移植済みコードへ委ねる) --
	void CmdSetExifTime();      //!< タイムスタンプを Exif 撮影日時に (SetExifTime)
	void CmdSetArcTime();       //!< 書庫のタイムスタンプを中身の最新に (SetArcTime)
	void CmdDelJpgExif();       //!< Jpeg の Exif を削除して反対側へ (DelJpgExif)
	void CmdExtractEmbedded();  //!< MP3/FLAC の埋め込み画像を抽出 (ExtractImage)
	void CmdExtractIcon();      //!< アイコンを抽出 (ExtractIcon)
	void CmdConvertDoc2Txt();   //!< バイナリ文書→テキスト (ConvertDoc2Txt)
	void CmdConvertHtm2Txt(bool to_markdown);  //!< HTML→テキスト/Markdown
	void CmdConvertImage();     //!< 画像形式の変換 (ConvertImage)

	/// 抽出・変換の宛先 (反対ペインのディレクトリ)。使えないなら空を返して警告する
	UnicodeString OutputDirOrWarn(const UnicodeString &verb);

	//-- ファイル操作の続き (機能群17。判断は gui/file_ops2.h) ------------------
	void CmdClone(bool to_current);  //!< クローンを作る (Clone / CloneToCurr)
	void CmdCopyDir();               //!< ディレクトリ構造だけを複製 (CopyDir)
	void CmdCreateDirsDlg();         //!< ディレクトリを一括作成 (CreateDirsDlg)
	void CmdSwapName();              //!< 選択2件の名前を入れ替える (SwapName)
	void CmdUndoRename();            //!< 直前の改名を元に戻す (UndoRename)
	void CmdCreateTestFile();        //!< テストファイルを作る (CreateTestFile)

	/// 一括リネームなどのあとに改名ログを残す (UndoRename で戻せるようにする)
	void RecordRenames(const std::vector<file_ops2::RenameRecord> &records);

	//-- 選択と絞り込みの拡張 (機能群16。判断は gui/selection.h) ----------------
	void CmdMaskSelect();      //!< マスクに一致するファイルを選択 (MaskSelect)
	void CmdSelByList();       //!< 名前を並べたファイルで選択 (SelByList)
	void CmdSelEmptyDir(bool no_file);  //!< 空のディレクトリを選択 (SelEmptyDir)
	void CmdDateSelect();      //!< 日付条件で選択 (DateSelect)
	void CmdNextSameName();    //!< 名前主部が同じ次のファイルへ (NextSameName)
	void CmdSelMask();         //!< 選択項目だけを残す (SelMask)
	void CmdDelSelMask();      //!< 選択項目を一覧から隠す (DelSelMask)
	void CmdMaskFind();        //!< マスクで配下を検索して結果リストへ (MaskFind)
	void CmdInputPathMask();   //!< パスマスクを入力 (InputPathMask)

	//-- 栞マークとタグ (機能群15。gui/bookmarks.h) ----------------------------
	//
	// 保存先は移植済みの UsrIniFile (栞) と TagManager (タグ) をそのまま使う。
	// ここに置くのは受け渡しだけで、「どこへ飛ぶか」は bookmarks:: の純関数が持つ
	void CmdMark();            //!< カーソル位置に栞を付ける/外す (Mark)
	void CmdMarkWithMemo();    //!< メモ付きで栞を付ける (Mark_IM)
	void CmdClearMark(bool all);  //!< 一覧の栞を外す / 全部外す (ClearMark, ClearMark_AC)
	void CmdJumpMark(int direction);  //!< 次 / 前の栞へ (NextMark / PrevMark)
	void CmdSelMark();         //!< 栞の付いた項目を選択 (SelMark)
	void CmdMarkMask();        //!< 栞の付いた項目だけを残す (MarkMask)
	void CmdMarkList();        //!< 栞の一覧から選んで飛ぶ (MarkList)
	void CmdFindMark();        //!< 配下の栞を集めて結果リストに出す (FindMark)

	void CmdSetTag(bool add);  //!< タグを設定 / 追加 (SetTag / AddTag)
	void CmdDelTag();          //!< タグを削除 (DelTag)
	void CmdTagSelect();       //!< 指定タグを含む項目を選択 (TagSelect)
	void CmdFindTag();         //!< 指定タグの項目を集めて結果リストに出す (FindTag)
	void CmdTrimTagData();     //!< 実体の無い項目のタグを整理 (TrimTagData)

	/// タグ管理。実体は移植済みの TagManager。初回に使うときだけ作る
	/// (起動のたびに TAGDATA.TXT を読むのを避けるため)
	TagManager *Tags();
	/// アクティブペインの対象項目のフルパス (マーク済み、無ければカーソル位置)
	std::vector<UnicodeString> TargetPaths();

	std::unique_ptr<TagManager> tags_;

	std::vector<work_list::WorkItem> work_items_;  //!< ワークリストの中身 (これが正)
	UnicodeString work_name_;      //!< 保存先の .nwl。空なら無名 (未保存)
	UnicodeString work_home_;      //!< ホームワークリスト (HomeWorkList)
	bool work_changed_ = false;    //!< 保存していない変更があるか
	int work_pane_ = -1;           //!< 表示中のペイン。-1 なら出していない

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
