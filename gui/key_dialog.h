/**
 * @file gui/key_dialog.h
 * @brief キー割り当て一覧ダイアログ (wx 依存)
 *
 * @details VCL の `TKeyListDlg` (`src/KeyDlg.h:28-114`,
 *          `src/KeyDlg.dfm:1-267`) のタブ・フィルタ・ソート・一覧 UI を
 *          wx の listctrl/choice に置き換えたもの。行の生成/判定は
 *          `gui/key.h`、割り当ての入力は既存の `gui/key_map.*`。
 *
 *          未移植 (未実装扱い): VCL の独自描画、2 ストローク/SELECT+ 入力、
 *          Migemo辞書、S/V/I/L の ini 読み込み、コマンドヘルプ/設定画面。
 */
#ifndef NYANFI_GUI_KEY_DIALOG_H
#define NYANFI_GUI_KEY_DIALOG_H

#include <wx/wx.h>

#include "gui/key.h"

namespace key_dialog {

struct Result {
	UnicodeString command;
	bool accepted = false;
};

/// rows/state を表示し、選択した実在コマンドを result.command に返す。
bool Run(wxWindow *parent, key::StateStore &state,
         const std::vector<key::Entry> &rows, Result &result);

}  // namespace key_dialog

#endif  // NYANFI_GUI_KEY_DIALOG_H
