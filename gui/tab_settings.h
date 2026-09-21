/**
 * @file gui/tab_settings.h
 * @brief タブの設定ダイアログの判断ロジック (wx 非依存の純粋ロジック)
 *
 * @details VCL 版の該当は `src/TabDlg.cpp` (`TTabSetDlg`)。タブ一覧
 *          (`TabList`) の1行は `TABLIST_CSVITMCNT = 9` の CSV
 *          (`path0,path1,caption,icon,home0,home1,nwl_mode,nwl,sync_lr`、
 *          `src/Global.h`) で、ダイアログはそのうち [2..7] (キャプション・
 *          アイコン・左右のホーム・ワークリスト指定) だけを読み書きする
 *          (`FormShow` / `OkButtonClick` を実測)。パス ([0],[1]) と
 *          同期フラグ ([8]) は触らない。
 *
 *          未移植 (未実装扱い。落とさない):
 *          - アイコンのプレビュー表示 (`usr_SH->draw_SmallIcon`)
 *          - 参照ボタンの中身はダイアログ側 (`gui/tab_dialog.h`) で
 *            wx のファイル/ディレクトリ選択に置き換える
 *          - 確定後のワークリストの読み込み (`SetWorkList`。`MainFrm.cpp::
 *            TabDlgActionExecute` 後半)。設定値の保持だけ行う
 */
#ifndef NYANFI_GUI_TAB_SETTINGS_H
#define NYANFI_GUI_TAB_SETTINGS_H

#include "usr_str.h"

namespace tab_settings {

// TabList の CSV 項目数 (src/Global.h の TABLIST_CSVITMCNT と同じ)。
// src/Global.h は VCL の巨大ヘッダのため直接は読まず値を複写する
constexpr int kCsvItemCount = 9;

// ワークリストの指定 (TTabSetDlg の Work*RadioBtn に対応)
constexpr int kWorkNone = 0;     //!< 使わない (Work0RadioBtn)
constexpr int kWorkCurrent = 1;  //!< 現在のワークリスト (Work1RadioBtn)
constexpr int kWorkNamed = 2;    //!< 指定のワークリスト (Work2RadioBtn)

//---------------------------------------------------------------------------
// ダイアログの入出力 (TTabSetDlg の各 Edit/RadioBtn に対応)
//---------------------------------------------------------------------------
struct TabSettings {
	UnicodeString caption;    //!< タブのキャプション (CaptionEdit)
	UnicodeString icon;       //!< タブのアイコン (IconEdit)
	UnicodeString home0;      //!< 左のホーム (HomeDir1Edit)
	UnicodeString home1;      //!< 右のホーム (HomeDir2Edit)
	int work_mode = kWorkNone;  //!< ワークリストの指定 (Work*RadioBtn)
	UnicodeString work_list;  //!< 指定のワークリスト (WorkListEdit。mode==2 のときだけ有効)
};

//---------------------------------------------------------------------------
// TabList の1行 (CSV) と TabSettings の変換
//---------------------------------------------------------------------------

/// CSV レコードからダイアログの初期値を読む (TTabSetDlg::FormShow に対応)
TabSettings FromCsvRecord(const UnicodeString &record);

/// CSV レコードへダイアログの確定値を書く (TTabSetDlg::OkButtonClick に対応)。
/// [2..7] だけを書き換え、[0],[1] (パス) と [8] (同期) はそのまま返す
UnicodeString ApplyToCsvRecord(const UnicodeString &base_record, const TabSettings &settings);

}  // namespace tab_settings

#endif  // NYANFI_GUI_TAB_SETTINGS_H
