/**
 * @file gui/app_list.h
 * @brief アプリケーション一覧・ランチャーの判断ロジック (wx 非依存の純粋ロジック)
 *
 * @details VCL 版の該当は `src/AppDlg.cpp` (`TAppListDlg::UpdateAppList`、
 *          `UpdateLaunchList` の通常/検索モード、`CloseSameItemClick`) と
 *          `src/MainFrm.cpp:13499` (`AppListActionExecute` の AO/LO/LI/FA/FL/
 *          FI/FZ/AS。パラメータ解決自体は `gui/f_batch7_ops.h` の
 *          `ParseAppListOpts` が既に対象コマンドを実測済みのため、ここでは
 *          その結果からの表示構成の解決だけ扱う)。
 *            - 列挙フィルタは `UpdateAppList` の `do{...}while(false)` 連鎖を
 *              実測 (非表示・自己ダイアログ・Cloaked・ツールウィンドウ・
 *              親持ち非APPWINDOW・空矩形・空テキストは落とす。
 *              `WS_EX_LAYERED` かつ `WS_EX_WINDOWEDGE` 無しも落とす)。
 *              除外テキストは `ExcAppText` の `;` 区切り
 *              (`split_strings_semicolon` + `ContainsText`) を実測
 *            - モニタ番号は重なり面積が半分以上で一致 (実測)
 *            - ランチャー通常ソートは `SortComp_Launch` を実測
 *              (上へ・ディレクトリ優先・同拡張子は名前の論理比較。
 *              論理比較 `StrCmpLogicalW` は移植層に無いため単純な
 *              大小無視比較で代替する簡略版)
 *            - まとめて閉じるは `CloseSameItemClick` を実測
 *              (同一実行ファイル名を大小無視で一致)
 *            - 全件表示条件は `contained_wd_i("*|?| ", IncSeaWord)` を実測
 *            - ファジー照合は `get_fuzzy_ptn` の正規表現化の代わりに
 *              部分列照合の簡略版 (Migemo/正規表現は未移植のため扱わない)
 *
 *          未移植 (未実装扱い。落とさない):
 *          - Win32 `EnumWindows`/`GetWindow` による実列挙・プロセス情報取得
 *            (実機APIのため。薄い層は `gui/app_dialog.cpp` 側に置く)
 *          - DWM サムネイル (`DwmRegisterThumbnail` 系)
 *          - UWP パッケージ解決・コマンドライン取得 (PEB 読み)
 *          - ウィンドウ実操作 (最小化/最大化/閉じる/強制終了) は確認+警告の
 *            簡略版 (`gui/app_dialog.h` を参照)
 *          - OptDlg (4729行・設定全体のため対象外)、FtpDlg/GitView
 *            (外部連携のため対象外)
 */
#ifndef NYANFI_GUI_APP_LIST_H
#define NYANFI_GUI_APP_LIST_H

#include <vector>

#include "usr_str.h"

namespace app_list {

//---------------------------------------------------------------------------
// ウィンドウ列挙のフィルタ条件 (UpdateAppList の do{}while(false) 連鎖を実測)
//---------------------------------------------------------------------------
struct WindowAttrs {
	bool visible = true;             //!< IsWindowVisible
	bool is_self_dialog = false;     //!< 自プロセスかつ TAppListDlg/HH Parent
	bool cloaked = false;            //!< DWMWA_CLOAKED
	bool tool_window = false;        //!< WS_EX_TOOLWINDOW
	bool layered_no_edge = false;    //!< WS_EX_LAYERED かつ WINDOWEDGE 無し
	bool has_parent_no_appwindow = false;  //!< GetParent かつ APPWINDOW 無し
	bool rect_empty = false;         //!< ウィンドウ矩形が空
	bool text_empty = false;         //!< ウィンドウテキストが空
};

//---------------------------------------------------------------------------
// 矩形 (モニタ番号解決用。wx 非依存のため自前定義)
//---------------------------------------------------------------------------
struct Rect {
	int left = 0;
	int top = 0;
	int width = 0;
	int height = 0;
};

//---------------------------------------------------------------------------
// 一覧表示用のアプリ情報 (AppWinInf のうち表示・操作に要るものだけ抜粋)
//---------------------------------------------------------------------------
struct AppEntry {
	UnicodeString caption;     //!< 実行ファイルの基底名
	UnicodeString win_text;    //!< ウィンドウテキスト
	UnicodeString file_name;   //!< 実行ファイルのフルパス
	UnicodeString cmd_param;   //!< コマンドラインパラメータ
	unsigned pid = 0;          //!< プロセスID
	int mon_no = -1;           //!< 表示モニタ (不明は -1)
	unsigned long mem_ws = 0;  //!< WorkingSetSize
	unsigned long mem_pws = 0; //!< PeakWorkingSetSize
	int win_wd = 0;            //!< ウィンドウ幅
	int win_hi = 0;            //!< ウィンドウ高さ
	bool minimized = false;    //!< 最小化中
	bool no_response = false;  //!< 無応答
	bool top_most = false;     //!< 最前面
	bool wow64 = false;        //!< 32-bit プロセス
	bool elevated = false;     //!< 昇格
	bool to_close = false;     //!< 終了要求済み
};

//---------------------------------------------------------------------------
// ランチャー項目 (file_rec のうち一覧・ソートに要るものだけ抜粋)
//---------------------------------------------------------------------------
struct LaunchEntry {
	UnicodeString base_name;  //!< 基本名 (拡張子なし)
	UnicodeString ext;        //!< 拡張子 (.lnk/.url 等)
	bool is_dir = false;      //!< ディレクトリか
	bool is_up = false;       //!< 上へ (..) か
	UnicodeString alias;      //!< コメント (ソート用)
};

//---------------------------------------------------------------------------
// 表示構成 (AppListActionExecute の AO/LO/LI を実測)
//---------------------------------------------------------------------------
struct AppView {
	bool show_app = true;      //!< アプリ一覧を表示
	bool show_launcher = true; //!< ランチャーを表示
};

//---------------------------------------------------------------------------
// 除外テキスト (ExcAppText の ; 区切りを実測)
//---------------------------------------------------------------------------

/// `;` 区切りを分割し空要素を落とす
std::vector<UnicodeString> SplitExcList(const UnicodeString &exc_text);

/// 除外テキストを含むか (ContainsText を実測。大小無視)
bool MatchesExc(const UnicodeString &win_text, const std::vector<UnicodeString> &exc_list);

//---------------------------------------------------------------------------
// 列挙フィルタ (UpdateAppList を実測)
//---------------------------------------------------------------------------

/// 一覧に載せるべきウィンドウか
bool ShouldListWindow(const WindowAttrs &attrs, const UnicodeString &win_text,
                      const std::vector<UnicodeString> &exc_list);

//---------------------------------------------------------------------------
// モニタ番号 (重なり面積が半分以上で一致を実測)
//---------------------------------------------------------------------------

/// ウィンドウが載るモニタ番号。無ければ -1
int ResolveMonNo(const Rect &win, const std::vector<Rect> &monitors);

//---------------------------------------------------------------------------
// ランチャー検索 (UpdateLaunchList の検索モードを実測・簡略化)
//---------------------------------------------------------------------------

/// `*|?|空白` を含めば全件表示 (`contained_wd_i("*|?| ", ...)` を実測)
bool IsLaunchAllPattern(const UnicodeString &word);

/// 名前照合。fuzzy のとき部分列照合。ignore_case のとき大小無視
bool MatchesLaunchName(const UnicodeString &base_name, const UnicodeString &word,
                       bool fuzzy, bool ignore_case);

//---------------------------------------------------------------------------
// ランチャー通常ソート (SortComp_Launch を実測・簡略版)
//---------------------------------------------------------------------------

/// 小さいほど前。0 は等価
int CompareLaunchNormal(const LaunchEntry &a, const LaunchEntry &b);

//---------------------------------------------------------------------------
// まとめて閉じる (CloseSameItemClick を実測: 同一実行ファイル名)
//---------------------------------------------------------------------------

/// idx と同一実行ファイル (大小無視) の添字一覧
std::vector<int> CloseTargetsWithSameFile(const std::vector<UnicodeString> &file_names,
                                          int index);

//---------------------------------------------------------------------------
// 表示構成 (AO/LO/LI の解決結果から。ParseAppListOpts は f_batch7_ops 側)
//---------------------------------------------------------------------------

/// only_app (AO) なら一覧のみ、only_launcher (LO/LI) ならランチャーのみ
AppView ResolveAppView(bool only_app, bool only_launcher);

//---------------------------------------------------------------------------
// ステータスバー (UpdateAppSttBar の PID/メモリ/サイズ部を実測・簡略版)
//---------------------------------------------------------------------------

/// `PID:1234 WS:... Win:800x600 ...` 形式
UnicodeString FormatStatus(const AppEntry &entry);

}  // namespace app_list

#endif  // NYANFI_GUI_APP_LIST_H
