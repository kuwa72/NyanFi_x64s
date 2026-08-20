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
 */
#ifndef NYANFI_GUI_MAIN_FRAME_H
#define NYANFI_GUI_MAIN_FRAME_H

#include <memory>
#include <vector>

#include <wx/wx.h>

#include "gui/file_pane.h"
#include "gui/image_viewer.h"
#include "gui/key_map.h"
#include "gui/navigation.h"
#include "gui/settings.h"
#include "gui/text_viewer.h"

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
	void CmdRenameDlg();  //!< カーソル位置の項目の名前を変更する (R)

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
};

#endif  // NYANFI_GUI_MAIN_FRAME_H
