/**
 * @file gui/f_batch5_ops.h
 * @brief Fモード残コマンド batch5 の判断ロジック (wx 非依存の純関数)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の以下 (src 必読済み):
 *          - ExeExtMenu (17125行): ActionParam を `get_IndexFromAccKey`
 *            で解決。見つからなければ中止。先頭項目が ">" なら
 *            サブメニュー表示、そうでなければ ExeExtMenuItem(idx) 実行
 *          - ExeExtTool (17150行): 同上 (ExtToolList 側)
 *          - RegDirPopup (24167行): `OP` 指定なら OppDir、既定は RegDir
 *          - PathMaskDlg (23553行): Find/Work 中は不可。`ND` なら
 *            ポップアップ、そうでなければダイアログ表示
 *          - BinaryEdit (13799行): 選択があれば選択ファイル、無ければ
 *            カーソル位置。BinaryEditor の実在が必須
 *          - EditHighlight (16892行): `FileEdit_"ハイライト定義"` を実行
 *          - FixedLen (33660行): TVIEW 中はビューアへ委譲。そうでなければ
 *            数値指定があれば上限化 (最低4) して ON、有無で反転
 *          - HtmlToText (33643行): TVIEW 中はビューアへ委譲。そうでなければ
 *            SetHtmlToText (ON/OFF/MD/^MD/TX の ';' 区切り)
 *          - SetColor (34202行): パラメータ有りならファイル指定、
 *            空なら配色ダイアログ
 *          - ShowRuby (33786行): TVIEW 中はビューアへ委譲。そうでなければ反転
 *          - ListDuration (20722行): 書庫/FTP 不可、選択必須。
 *            行書式は `名\t 長さ`、合計行付き
 *          - ListExpFunc (20837行): FTP 不可。SI/SR/SN でソート、
 *            .csv/.tsv で一覧形式
 *          - WatchTail (34217行): AC=全中止、ST=状態表示、CC=個別中止、
 *            無指定=監視開始 (キーワード=ActionParam)
 *
 *          いずれも wx を含まず、nyanfi_gui_core でビルド・テストする (規約8)。
 */
#ifndef NYANFI_GUI_F_BATCH5_OPS_H
#define NYANFI_GUI_F_BATCH5_OPS_H

#include <vector>

#include "usr_str.h"

namespace f_batch5_ops {

//---------------------------------------------------------------------------
// ';' 区切りトークンの大小無視の完全一致 (VCL TestActionParam 相当)
//---------------------------------------------------------------------------

bool HasParamToken(const UnicodeString &param, const UnicodeString &token);

//---------------------------------------------------------------------------
// ExeExtMenu / ExeExtTool (17125/17150行): アクセスキー解決
//---------------------------------------------------------------------------

/**
 * @brief 1文字のアクセスキーから項目添字を引く
 * @details VCL get_IndexFromAccKey (UserFunc.cpp:1196) 相当:
 *          1文字でなければ -1。キー先頭に "&" を付けて各項目の
 *          先頭列に大小無視で含まれるものを探す
 */
int ResolveAccKeyIndex(const std::vector<UnicodeString> &first_cols,
                       const UnicodeString &key);

/// 先頭列が ">" で始まればサブメニュー表示 (VCL StartsStr 分岐と同じ)
inline bool IsSubMenuHead(const UnicodeString &first_col)
{
	return StartsStr(_T(">"), first_col);
}

//---------------------------------------------------------------------------
// RegDirPopup (24167行) / PathMaskDlg (23553行)
//---------------------------------------------------------------------------

/// OP 指定なら OppDir、既定は RegDir
UnicodeString ResolveRegDirPopupTarget(const UnicodeString &param);

enum class PathMaskMode {
	Popup,  //!< ND 指定: 登録ポップアップメニュー
	Dialog, //!< 既定: パスマスクダイアログ
	Denied, //!< 検索中・ワークリスト中は不可
};

PathMaskMode ResolvePathMaskMode(const UnicodeString &param, bool is_find, bool is_work);

//---------------------------------------------------------------------------
// BinaryEdit (13799行) / EditHighlight (16892行)
//---------------------------------------------------------------------------

/// 選択があれば選択文字列、無ければカーソル位置 (VCL 三項と同じ)
UnicodeString ResolveBinaryEditTarget(bool has_selection, const UnicodeString &sel_str,
                                      const UnicodeString &cur_str);

/// BinaryEditor の実在確認。空・不存在は error_out に理由を入れて false
bool ValidateBinaryEditor(const UnicodeString &editor_path, UnicodeString &error_out);

/// `FileEdit_"定義ファイル"` の実行文字列を作る
UnicodeString BuildEditHighlightCommand(const UnicodeString &highlight_path);

//---------------------------------------------------------------------------
// FixedLen (33660行) / HtmlToText (33643行) / SetColor (34202行) / ShowRuby
//---------------------------------------------------------------------------

struct FixedLenPlan {
	bool delegate_to_viewer = false; //!< TVIEW 中はビューアへ委譲
	bool new_enabled = false;        //!< 非委譲時の反転後の値
	int limit = 0;                   //!< 数値指定があれば上限 (最低4)、無ければ 0
};

/**
 * @brief FixedLen の分岐を解決する
 * @details TVIEW 中は委譲。そうでなければ数値指定があれば上限化して ON、
 *          無ければ反転 (VCL の extract_int_def + SetToggleAction と同じ)
 */
FixedLenPlan ResolveFixedLen(bool is_tview, bool cur_enabled, const UnicodeString &param);

enum class MarkdownMode {
	Keep,       //!< 指定なし: そのまま
	ToMarkdown, //!< MD: Markdown 化する
	ToText,     //!< TX: 素のテキストに戻す
	Toggle,     //!< ^MD: 切り替える
};

struct HtmlToTextPlan {
	bool delegate_to_viewer = false; //!< TVIEW 中はビューアへ委譲
	UnicodeString toggle_param;      //!< ON/OFF 指定 (無ければ空)
	MarkdownMode markdown = MarkdownMode::Keep;
};

HtmlToTextPlan ParseHtmlToText(const UnicodeString &param, bool is_tview);

enum class SetColorMode {
	Dialog,  //!< 空: 配色ダイアログ
	FromFile,//!< 指定有り: その配色ファイルを使う
};

SetColorMode ResolveSetColorMode(const UnicodeString &param);

/// TVIEW 中はビューアへ委譲 (ShowRuby 33786行と同じ)
inline bool IsShowRubyViewerDelegate(bool is_tview)
{
	return is_tview;
}

/// ON/OFF/反転 (VCL SetToggleAction と同じ)
bool ToggleEnabled(bool cur, const UnicodeString &param);

//---------------------------------------------------------------------------
// ListDuration (20722行)
//---------------------------------------------------------------------------

/// 書庫/FTP 不可、選択必須。不可なら error_out に理由を入れて false
bool ValidateListDuration(bool is_arc, bool is_ftp, int sel_count, UnicodeString &error_out);

/// m秒を `HH:MM:SS.cc` (cs=false なら `HH:MM:SS`) にする (usr_str と同じ書式)
UnicodeString FormatDurationMs(unsigned int ms, bool with_cs);

/// 一覧の1行 `名\t 長さ` (VCL の sprintf 書式と同じ)
UnicodeString FormatDurationRow(const UnicodeString &name, unsigned int ms, bool with_cs);

/// 合計行 `合計 ... 件数 長さ`。ERR 件数があれば `  ERR:n` を付ける
UnicodeString FormatDurationTotal(unsigned int total_ms, bool with_cs, int file_count,
                                  int err_count);

//---------------------------------------------------------------------------
// ListExpFunc (20837行)
//---------------------------------------------------------------------------

/// SI=1/SR=2/SN=3、無指定=0 (VCL の連鎖三項と同じ優先順位)
int ParseExpFuncSort(const UnicodeString &param);

/// .csv=1/.tsv=2、無指定=0
int ParseExpFuncListMode(const UnicodeString &param);

//---------------------------------------------------------------------------
// WatchTail (34217行)
//---------------------------------------------------------------------------

enum class WatchTailSubCmd {
	Watch,    //!< 既定: 監視の開始/切替
	AllCancel,//!< AC: すべての監視を中止
	Status,   //!< ST: 監視内容の表示
	CancelOne,//!< CC: カーソル位置の監視を個別中止
};

WatchTailSubCmd ParseWatchTailSubCmd(const UnicodeString &param);

}  // namespace f_batch5_ops

#endif  // NYANFI_GUI_F_BATCH5_OPS_H
