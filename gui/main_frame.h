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

#include <wx/wx.h>

#include "gui/file_pane.h"
#include "gui/key_map.h"
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

	void LoadSettings();
	void SaveSettings();

	FilePane *panes_[2] = {nullptr, nullptr};
	wxStaticText *headers_[2] = {nullptr, nullptr};
	wxWindow *root_ = nullptr;      //!< 2ペインを収めた親パネル (ShowViewer でのサイズ調整用)
	TextViewer *viewer_ = nullptr;  //!< テキストビューア (root_ と同じ領域に重ねて表示)
	int active_ = 0;
	KeyMap keymap_;
	Settings settings_{Settings::DefaultIniPath()};
};

#endif  // NYANFI_GUI_MAIN_FRAME_H
