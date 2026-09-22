/**
 * @file gui/f_batch7_ops.h
 * @brief Fモード残コマンド batch7 の判断ロジック (wx 非依存の純関数)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の以下 (src 必読済み):
 *          - AppList (13499行): `AO` のみ一覧、`LO`/`LI` はランチャーのみ、
 *            `FA`/`FL` は切替先、`FI`/`LI` はインクリメンタル検索へ、
 *            `FZ` はあいまい、`AS` は開始メニュー追加
 *          - DebugCmdFile (16308行): 非空パラメータなら先頭 `@` 除去・
 *            引用符除去・絶対パス化。空なら FLIST 中のカーソル位置で
 *            .nbt のもの。存在しなければ中止
 *          - DistributionDlg (16369行): 書庫/ADS/Work/FTP では不可。
 *            `XC` なら直ちにコピー、`XM` なら直ちに移動、`SN` なら
 *            カーソル名をマスク・反対パスを振分先に設定 (XC > XM > SN)。
 *            `.ini` 指定は登録ファイル名の保存
 *          - DotNyanDlg (16792行): FLIST 外は不可。`RS` なら再適用、
 *            そうでなければ設定ダイアログ
 *          - ExeCommands (17118行): ExeAliasOrCommands(ActionParam)。
 *            失敗したら中止
 *          - ExeMenuFile (24017行): load_MenuFile(ActionParam) できれば
 *            ExePopMenuList、できなければ中止
 *          - ExeToolBtn (17175行): ActionParam を 1 始まりの番号として
 *            ボタン一覧から引き、範囲外は中止
 *          - ExtractChmSrc (17254行): ADS/FTP・反対側非リストは不可。
 *            カーソル位置が非ディレクトリの .chm 必須。仮想は一時展開。
 *            空白を含む名前は未対応
 *          - ExtractGifBmp (17290行): 変換不可 (NotConvert) は不可。
 *            選択ありなら全て .gif 必須、無ければカーソル位置が .gif 必須
 *          - LockKeyMouse (21668行): 非空パラメータは英数字のみ
 *            (is_alnum_str)。空は ESC 解除
 *          - RegExChecker (24175行): 非表示のときだけ PatternStr に
 *            ActionParam を入れて開く
 *          - ToolBarDlg (36789行): 一覧系 (FLIST/TVIEW/IVIEW) かつ
 *            プライマリのときだけ有効。マウス位置のボタンを初期選択
 *          - UpdateFromArc (30205行): `UN` なら最新アーカイブ探索、
 *            そうでなければ選択ダイアログ
 *
 *          いずれも wx を含まず、nyanfi_gui_core でビルド・テストする (規約8)。
 */
#ifndef NYANFI_GUI_F_BATCH7_OPS_H
#define NYANFI_GUI_F_BATCH7_OPS_H

#include "usr_file_ex.h"
#include "usr_str.h"

namespace f_batch7_ops {

//---------------------------------------------------------------------------
// ';' 区切りトークンの大小無視の完全一致 (VCL TestActionParam 相当)
//---------------------------------------------------------------------------

bool HasParamToken(const UnicodeString &param, const UnicodeString &token);

//---------------------------------------------------------------------------
// AppList (13499行)
//---------------------------------------------------------------------------

struct AppListOpts {
	bool only_app = false;     //!< AO: アプリ一覧のみ
	bool only_launcher = false;//!< LO/LI: ランチャーのみ
	bool to_app = false;       //!< FA: アプリ一覧へ切替
	bool to_launcher = false;  //!< FL: ランチャーへ切替
	bool to_incsea = false;    //!< FI/LI: インクリメンタル検索へ
	bool fuzzy = false;        //!< FZ: あいまい検索
	bool add_start = false;    //!< AS: 開始メニュー追加
};

/**
 * @brief AppList の表示オプションを解決する
 */
AppListOpts ParseAppListOpts(const UnicodeString &param);

//---------------------------------------------------------------------------
// DebugCmdFile (16308行)
//---------------------------------------------------------------------------

enum class DebugCmdSrc {
	Param, //!< 非空パラメータ (@除去・絶対パス化して実行)
	Cursor,//!< FLIST 中のカーソル位置 (.nbt 必須)
	None,  //!< 対象なし (中止)
};

/**
 * @brief デバッグ実行の対象を解決する
 * @details パラメータ有無が最優先。空でも FLIST 中にカーソルがあり
 *          .nbt ならカーソル位置。どれにも当たらなければ None
 */
DebugCmdSrc ResolveDebugCmdSrc(bool has_param, bool is_flist, bool has_cursor,
                               bool cursor_is_nbt);

//---------------------------------------------------------------------------
// DistributionDlg (16369行)
//---------------------------------------------------------------------------

enum class DistributionMode {
	Dialog,       //!< 既定: ダイアログを開く
	ImmediateCopy,//!< XC: 直ちにコピー
	ImmediateMove,//!< XM: 直ちに移動
	SetMask,      //!< SN: カーソル名をマスクに設定
};

/**
 * @brief 振り分けの即時動作を解決する (XC > XM > SN > Dialog)
 */
DistributionMode ResolveDistributionMode(const UnicodeString &param);

/**
 * @brief 振り分け登録ファイルの指定か (.ini のみ保存対象)
 */
inline bool IsDistrIniFile(const UnicodeString &param)
{
	if (param.IsEmpty()) return false;
	return SameText(get_extension(param), _T(".ini"));
}

/**
 * @brief 振り分けダイアログが使える状態か (特殊リストでは不可)
 */
inline bool CanUseDistributionDlg(bool is_arc, bool is_ads, bool is_work,
                                  bool is_ftp)
{
	return !is_arc && !is_ads && !is_work && !is_ftp;
}

//---------------------------------------------------------------------------
// DotNyanDlg (16792行)
//---------------------------------------------------------------------------

enum class DotNyanMode {
	Reapply, //!< RS: 再適用
	Dialog,  //!< 設定ダイアログ
	Denied,  //!< FLIST 外は不可
};

DotNyanMode ResolveDotNyanMode(bool is_flist, const UnicodeString &param);

//---------------------------------------------------------------------------
// ExeCommands (17118行) / ExeMenuFile (24017行)
//---------------------------------------------------------------------------

/// 空パラメータは失敗 (VCL の SetActionAbort と同じ)
inline bool CanExeCommands(const UnicodeString &param)
{
	return !param.IsEmpty();
}

/// 開けなければ失敗 (VCL USTR_FileNotOpen と同じ)
inline bool CanExeMenuFile(const UnicodeString &param)
{
	return !param.IsEmpty();
}

//---------------------------------------------------------------------------
// ExeToolBtn (17175行)
//---------------------------------------------------------------------------

/**
 * @brief ツールボタンの番号 (1 始まり) を 0 始まりの添字に直す
 * @details 数値でない・1 未満・一覧数超過は -1 (VCL USTR_IllegalParam と同じ)
 */
int ResolveToolBtnIndex(const UnicodeString &param, int count);

//---------------------------------------------------------------------------
// ExtractChmSrc (17254行)
//---------------------------------------------------------------------------

/**
 * @brief CHM ソース抽出の事前条件を確認する
 * @details ADS/FTP・反対側非リスト・カーソル無し・ディレクトリ・非 .chm・
 *          仮想の一時展開失敗・空白を含む名前はいずれも不可。
 *          不可の理由は error_out に入れて false
 */
bool ValidateExtractChmSrc(bool is_ads, bool is_ftp, bool is_opp_flist,
                           bool has_cursor, bool is_dir, const UnicodeString &ext,
                           bool is_virtual, bool tmp_ok, bool fnam_has_space,
                           bool odir_has_space, UnicodeString &error_out);

//---------------------------------------------------------------------------
// ExtractGifBmp (17290行)
//---------------------------------------------------------------------------

/**
 * @brief GIF 抽出の対象を確認する
 * @details 選択ありなら全て .gif 必須 (all_sel_gif)、無ければ
 *          カーソル位置が .gif 必須 (cursor_is_gif)。不可なら false
 */
bool ValidateExtractGif(bool has_sel, bool all_sel_gif, bool cursor_is_gif,
                        UnicodeString &error_out);

//---------------------------------------------------------------------------
// LockKeyMouse (21668行)
//---------------------------------------------------------------------------

/**
 * @brief ロック解除ワードを確認する
 * @details 空 (ESC 解除) か英数字 (is_alnum_str) のみ可。それ以外は不可
 */
bool ValidateLockWord(const UnicodeString &param, UnicodeString &error_out);

//---------------------------------------------------------------------------
// RegExChecker (24175行) / ToolBarDlg (36789行)
//---------------------------------------------------------------------------

/// 非表示のときだけ開く
inline bool ShouldShowRegExChecker(bool visible)
{
	return !visible;
}

/**
 * @brief ツールバー設定が有効か
 * @details 一覧系 (FLIST/TVIEW/IVIEW) かつプライマリのときだけ有効
 *          (VCL ToolBarDlgActionUpdate と同じ)
 */
inline bool IsToolBarDlgEnabled(bool is_list_view, bool is_primary)
{
	return is_list_view && is_primary;
}

//---------------------------------------------------------------------------
// UpdateFromArc (30205行)
//---------------------------------------------------------------------------

enum class UpdateFromArcSrc {
	Newest, //!< UN 指定: 最新アーカイブを探索
	Dialog, //!< 空: 選択ダイアログ
};

UpdateFromArcSrc ResolveUpdateFromArcSrc(const UnicodeString &param);

}  // namespace f_batch7_ops

#endif  // NYANFI_GUI_F_BATCH7_OPS_H
