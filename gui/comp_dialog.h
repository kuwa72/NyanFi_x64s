/**
 * @file gui/comp_dialog.h
 * @brief 同名ファイルの比較ダイアログ (wx 依存)
 *
 * @details VCL 版の該当は `src/CompDlg.cpp` (`TFileCompDlg`。
 *          タイムスタンプ・サイズ・ハッシュ(+算法)・同一性の各条件と、
 *          ディレクトリ比較/書庫比較・結果と反対側も選択/選択を反転/
 *          選択項目だけ残す の各チェックを持つ)。このうち移植済みの
 *          判断 (`compare::CompOptions`・`compare::ResolveCompEnabled`・
 *          `compare::ApplyExclusiveOpt`) で表せる範囲を wx で再構成したもの
 *          (gui/diff_dialog.h と同じ作り)。
 *          条件の有効・無効・排他は `compare.h` 経由で決める。
 *          未移植 (未実装扱い):
 *          - 推定フォントの文章色による不使用条件のグレー表示
 *            (`get_PanelColor`。有効/無効と排他は行う)
 *          - 選択マスクの実行 (VCL は `SelMask OP` を投げる)
 *          - ハッシュの二段構え (簡易→全体) の途中表示
 */
#ifndef NYANFI_GUI_COMP_DIALOG_H
#define NYANFI_GUI_COMP_DIALOG_H

#include <wx/wx.h>

#include "gui/compare.h"

namespace comp_dialog {

/// ダイアログに出すのに必要な比較対象の状態 (VCL は MainFrm 側が判定していた)
struct Context {
	bool case_sensitive = false;    //!< CS パラメータ (表題の切り替えと名の照合)
	bool all_dir_has_size = false;  //!< 全ディレクトリがサイズ取得済みか
	bool ftp_either = false;        //!< どちらかが FTP か (内容比較不可)
	bool arc_either = false;        //!< どちらかが書庫か (同一性比較不可)
	bool sel_mask_available = false;//!< 選択マスクの実行条件 (ファイル一覧 or 書庫)
};

/**
 * @brief 同名ファイルの比較ダイアログを表示し、条件の入力を受け付ける
 * @param parent 親ウィンドウ
 * @param[in,out] opt_inout 条件 (初期表示に使い、OK 時に確定内容で置き換わる。
 *        `case_sensitive` は `Context` として渡すので変更しない)
 * @param ctx 比較対象の状態
 * @return true OK で閉じた (opt_inout が有効)。キャンセルなら false
 */
bool Run(wxWindow *parent, compare::CompOptions &opt_inout, const Context &ctx);

}  // namespace comp_dialog

#endif  // NYANFI_GUI_COMP_DIALOG_H
