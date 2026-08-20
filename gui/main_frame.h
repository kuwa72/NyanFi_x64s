/**
 * @file gui/main_frame.h
 * @brief メインウィンドウ (2画面 + ステータス)
 *
 * issue #1 の Phase 2 の骨格。VCL の MainFrm (38,502行) を置き換えるものでは
 * なく、「移植済みのロジック層だけでファイラとして最低限動く」ことを示す土台。
 *
 * 入力は MainFrame でまとめて拾い、キー名 → コマンド名 → 実行 の順に流す。
 * この流れは VCL 版と同じで、コマンド名の綴りも usr_cmdlist.cpp の表に合わせて
 * あるため、後から ini のキー割り当てをそのまま読み込める。
 */
#ifndef NYANFI_GUI_MAIN_FRAME_H
#define NYANFI_GUI_MAIN_FRAME_H

#include <memory>

#include <wx/wx.h>

#include "gui/file_pane.h"
#include "gui/key_map.h"

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

	void SetActivePane(int index);
	void UpdateStatus();
	void ShowKeyList();
	void ShowCmdList();

	FilePane *panes_[2] = {nullptr, nullptr};
	wxStaticText *headers_[2] = {nullptr, nullptr};
	int active_ = 0;
	KeyMap keymap_;
};

#endif  // NYANFI_GUI_MAIN_FRAME_H
