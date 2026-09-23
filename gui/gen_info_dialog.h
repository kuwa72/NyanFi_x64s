/**
 * @file gui/gen_info_dialog.h
 * @brief 汎用一覧ダイアログ (wx 依存)
 *
 * @details VCL 版の該当は `src/GenInfDlg.cpp` の `TGeneralInfoDlg` と、
 *          `src/GenInfDlg.h` / `src/GenInfDlg.dfm`。wx 版は変数・ログ・
 *          ファイル一覧・ツリー等の選択画面として入力を画面へ再構成する。
 *          一覧の判定・フィルタ・並べ替え・重複除去・文字列化は wx 非依存の
 *          `gui/gen_info.h` に置いている。
 *
 *          VCL 呼び出し元を `grep -n GeneralInfoDlg src/*.cpp` で実測した
 *          行番号は `gui/gen_info.h` 冒頭に記録した。要点は:
 *          - `src/MainFrm.cpp:14440-14443` が `CmdHistory` から開く
 *          - `src/MainFrm.cpp:20880-20886` が `ListClipboard` から開く
 *          - `src/MainFrm.cpp:20936-20941` が `ListLog` から開く
 *          - `src/usr_excmd.cpp:1778-1782` が変数一覧を開く
 *
 *          移植済み:
 *          - 複数選択可能な一覧、フィルタ欄、AND/OR、大小文字指定
 *          - 昇順/降順/元順、重複行の削除、リストの再構築
 *          - 選択行/値/コマンドのコピー、表示中の全件を UTF-8 テキスト保存
 *          - ログのエラー箇所のみ表示、表題・件数・選択数・カーソル位置
 *          - ファイル一覧/ツリーのタブ前後、変数一覧の値、Git の先頭行判定
 *
 *          未移植 (未実装扱い):
 *          - Migemo辞書検索、TSV/ツリー専用検索式、マッチ語の強調描画
 *          - Gitグラフ、ログ色、拡張子色、変数名列幅揃え等の専用描画
 *          - ファイル情報/プロパティ/外部エディタ・URL/前後ファイル切替
 *          - プレイリスト再生・監視、末尾表示の自動更新、FTP/HEAD 切替
 *          - FindTxtDlg 検索ダイアログ、2ストロークキー、独自スクロールバー
 *          - `.nbt` 形式の選択コマンド保存、保存文字コード選択 (UTF-8固定)、
 *            ini の位置/表示状態永続化
 */
#ifndef NYANFI_GUI_GEN_INFO_DIALOG_H
#define NYANFI_GUI_GEN_INFO_DIALOG_H

#include <functional>
#include <vector>

#include <wx/wx.h>

#include "gui/gen_info.h"

namespace gen_info_dialog {

/// wx 側の表示状態と元データ。
struct Input {
	UnicodeString title = _T("一覧");
	std::vector<UnicodeString> lines;
	gen_info::Kind kind = gen_info::Kind::Generic;
	bool focus_filter = false;
	/// 履歴を空にする Owner 側の操作。空なら「履歴を消去」は出さない。
	std::function<void()> clear_source;
};

/// OK で閉じたときの選択結果。
struct Result {
	UnicodeString primary;                 //!< 最初に選択された行 (実行対象など)
	std::vector<UnicodeString> selected;   //!< 選択された元行
};

/**
 * @brief 汎用一覧ダイアログを表示する。
 * @return OK で閉じたなら true。キャンセルなら false。
 */
bool Run(wxWindow *parent, const Input &input, Result &result_out);

}  // namespace gen_info_dialog

#endif
