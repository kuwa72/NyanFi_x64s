/**
 * @file gui/sync_dialog.h
 * @brief 同期コピー設定ダイアログ (wx 依存)
 *
 * @details VCL 版の該当は `src/SyncDlg.cpp` (`TRegSyncDlg`)。
 *          登録一覧 (有効チェック付き)・名前・上書き/同期削除オプション・
 *          対象ディレクトリ一覧の追加・変更・削除だけを wx で再構成したもの
 *          (gui/regdir_dialog.h と同じ作り)。
 *          判断部分は wx 非依存の `gui/sync_dirs.h` が持ち、ここでは表示と
 *          入力だけを受け持つ。確定時の正規化・不正行の破棄も `sync_dirs.h`
 *          経由で行う。
 *          未移植 (未実装扱い。`gui/sync_dirs.h` の説明を参照):
 *          - フォルダ参照 UI (`SelectDirEx`。パスは直接入力)
 *          - 登録のドラッグ並べ替え
 */
#ifndef NYANFI_GUI_SYNC_DIALOG_H
#define NYANFI_GUI_SYNC_DIALOG_H

#include <vector>
#include <wx/wx.h>

#include "gui/sync_dirs.h"

namespace sync_dialog {

/**
 * @brief 同期コピー設定ダイアログを表示し、設定一覧の編集を受け付ける
 * @param parent 親ウィンドウ
 * @param[in,out] entries 同期設定一覧 (OK 時に確定内容で置き換わる。
 *        戻り値が true のとき呼び出し側で ini へ保存すること)
 * @param current_dir 現在のディレクトリ (ディレクトリ追加時の初期値用)
 * @return true OK で閉じた (entries が有効)。キャンセルなら false
 */
bool Run(wxWindow *parent, std::vector<sync_dirs::SyncEntry> &entries,
         const UnicodeString &current_dir);

}  // namespace sync_dialog

#endif  // NYANFI_GUI_SYNC_DIALOG_H
