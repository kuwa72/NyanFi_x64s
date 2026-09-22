/**
 * @file gui/f_batch6_ops.h
 * @brief Fモード残コマンド batch6 の判断ロジック (wx 非依存の純関数)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の以下 (src 必読済み):
 *          - BgImgMode (13784行): 先頭 `^` を除去。`OFF`→0、`1`/`2`/`3` は
 *            同値指定+`^` なら反転で 0、そうでなければその値。
 *            それ以外は abort。適用は UpdateBgImage
 *          - Library (20595行): `SD` なら共有ダイアログ (isLibrary)。
 *            非空パラメータなら PopSelLibrary、空なら LibraryPath へ
 *          - JsonViewer (20266行): `CB` ならクリップボード、そうでなければ
 *            カーソル位置 (dummy/dir/無効は Abort、virtual は一時展開)。
 *            非テキストは不可
 *          - XmlViewer (27874行): JsonViewer と同じ (CB 無し)
 *          - LoadFindSet (21402行): `*` なら共有ダイアログ (isFindSet)、
 *            非空なら絶対パス化、空なら開くダイアログ。FindFileCore で適用
 *          - SaveAsFindSet (24523行): find_DUPL 中は不可。
 *            保存ダイアログ→ save_FindSettings
 *          - LockTextPreview (21697行): SetToggleAction で反転/ON/OFF
 *          - ShowIndent (33735行): TVIEW 中はビューアへ委譲、そうでなければ
 *            SetToggleAction で反転/ON/OFF
 *          - FindTagName (19142行): `EJ` なら EDIT、既定は VIEW。
 *            TVIEW 中かつ `CO` なら当該ファイルを対象に。tags 必須
 *          - Grep2 (19407行): 書庫/FTP、検索中ディレクトリは不可。
 *            grep.exe 未設定は不可。実体は GrepAction へ
 *          - ExPopupMenu (17223行): `MN` ならメニューのみ、`TL` なら
 *            ツールのみ、無指定は両方。ShowExPopupMenu で表示
 *          - WebMap (34302行): `IN` 取り除き後 `;` 区切りで `M<n>`/`Z<n>`
 *            (先頭 MZ 以外・空・0 は abort)。入力/TVIEW は緯度経度を入力、
 *            そうでなければ画像の位置情報。既定 zoom=16
 *          - PlayList (23604行): `NX`/`PR`/`PS`/`RS`/`PP`/`CA`/`FI`、
 *            無指定は設定 (`LS`/`RP`/`SF`/`SR` 削除)。MCI で再生
 *
 *          いずれも wx を含まず、nyanfi_gui_core でビルド・テストする (規約8)。
 */
#ifndef NYANFI_GUI_F_BATCH6_OPS_H
#define NYANFI_GUI_F_BATCH6_OPS_H

#include "usr_str.h"

namespace f_batch6_ops {

//---------------------------------------------------------------------------
// ';' 区切りトークンの大小無視の完全一致 (VCL TestActionParam 相当)
//---------------------------------------------------------------------------

bool HasParamToken(const UnicodeString &param, const UnicodeString &token);

//---------------------------------------------------------------------------
// BgImgMode (13784行)
//---------------------------------------------------------------------------

struct BgImgModePlan {
	bool abort = false; //!< 対応する分岐が無ければ true
	int mode = 0;       //!< 適用すべき BgImgMode (0〜3)
};

/**
 * @brief BgImgMode の分岐を解決する
 * @details 先頭 `^` は反転指定 (remove_top_s)。`OFF`→0、`1`〜`3` は
 *          反転指定かつ現在値と同値なら 0、そうでなければその値。
 *          どれにも当たらなければ abort (VCL の SetActionAbort と同じ)
 */
BgImgModePlan ResolveBgImgMode(int cur_mode, const UnicodeString &param);

//---------------------------------------------------------------------------
// Library (20595行)
//---------------------------------------------------------------------------

enum class LibraryMode {
	ShareDlg,   //!< SD 指定: 共有ダイアログ (isLibrary)
	OpenNamed,  //!< 非空: PopSelLibrary で指定ライブラリを開く
	OpenDefault,//!< 空: LibraryPath (既定のライブラリ) を開く
};

LibraryMode ResolveLibraryMode(const UnicodeString &param);

//---------------------------------------------------------------------------
// JsonViewer (20266行) / XmlViewer (27874行)
//---------------------------------------------------------------------------

/// CB 指定ならクリップボード、そうでなければカーソル位置のファイル
inline bool IsClipboardViewer(const UnicodeString &param)
{
	return HasParamToken(param, _T("CB"));
}

/**
 * @brief ビューア対象として有効か (VCL の Abort 条件の裏返し)
 * @details カーソル無し・dummy・ディレクトリ・属性無効は不可。
 *          virtual の一時展開・テキスト判定は実行側で行う
 */
inline bool CanViewFile(bool has_item, bool is_dummy, bool is_dir, bool attr_invalid)
{
	return has_item && !is_dummy && !is_dir && !attr_invalid;
}

//---------------------------------------------------------------------------
// LoadFindSet (21402行) / SaveAsFindSet (24523行)
//---------------------------------------------------------------------------

enum class LoadFindSetSrc {
	Star,  //!< "*" 指定: 共有ダイアログ (isFindSet)
	Named, //!< 非空: to_absolute_name で絶対パス化
	Dialog,//!< 空: 開くダイアログ
};

LoadFindSetSrc ResolveLoadFindSetSrc(const UnicodeString &param);

/// find_DUPL 中は保存不可 (VCL USTR_OpeNotSuported と同じ)
inline bool CanSaveFindSet(bool find_dupl)
{
	return !find_dupl;
}

//---------------------------------------------------------------------------
// LockTextPreview (21697行) / ShowIndent (33735行)
//---------------------------------------------------------------------------

/**
 * @brief SetToggleAction (MainFrm.cpp:12651) 相当
 * @details `ON` なら true、`OFF` なら false、無指定は反転
 */
inline bool ResolveToggle(bool cur, const UnicodeString &param)
{
	if (HasParamToken(param, _T("ON"))) return true;
	if (HasParamToken(param, _T("OFF"))) return false;
	return !cur;
}

struct ShowIndentPlan {
	bool delegate_to_viewer = false; //!< TVIEW 中はビューアへ委譲
	bool new_value = false;          //!< 非委譲時の反転後/指定値
};

ShowIndentPlan ResolveShowIndent(bool is_tview, bool cur, const UnicodeString &param);

//---------------------------------------------------------------------------
// FindTagName (19142行)
//---------------------------------------------------------------------------

struct FindTagPlan {
	UnicodeString tag_cmd;      //!< EJ 指定なら "EDIT"、既定は "VIEW"
	bool use_current_file = false; //!< TVIEW 中かつ CO 指定なら true
};

FindTagPlan ResolveFindTagPlan(const UnicodeString &param, bool is_tview);

//---------------------------------------------------------------------------
// Grep2 (19407行)
//---------------------------------------------------------------------------

/**
 * @brief Grep2 の事前条件を確認する
 * @details 書庫/FTP、検索中ディレクトリ (is_Find && find_Dir)、
 *          grep.exe 未設定はいずれも不可。空・不存在は error_out に理由を入れて false
 */
bool ValidateGrep2(bool is_arc, bool is_ftp, bool is_find, bool find_dir,
                   bool grep_exists, UnicodeString &error_out);

//---------------------------------------------------------------------------
// ExPopupMenu (17223行)
//---------------------------------------------------------------------------

struct ExPopupTarget {
	bool menu = true; //!< 追加メニュー (ExtMenuList) を含めるか
	bool tool = true; //!< 外部ツール (ExtToolList) を含めるか
};

/**
 * @brief ExPopupMenu の対象を解決する
 * @details `MN` ならメニューのみ、`TL` ならツールのみ、無指定は両方
 */
ExPopupTarget ResolveExPopupTarget(const UnicodeString &param);

//---------------------------------------------------------------------------
// WebMap (34302行)
//---------------------------------------------------------------------------

struct WebMapPlan {
	bool ok = false;        //!< 構文が正しければ true
	int map_idx = 0;        //!< M<n> の地図番号 (既定 0)
	int zoom = 16;          //!< Z<n> のズーム (既定 16)
	UnicodeString error;    //!< ok==false 時の理由 (USTR_IllegalParam 相当)
};

/**
 * @brief WebMap の `M<n>`/`Z<n>` パラメータを解釈する
 * @details `IN` は事前に取り除く (TestDelActionParam)。残りを `;` 区切りで見て、
 *          各要素は大文字化して先頭が M/Z、残りが 0 でない数値でなければ abort。
 *          空の ActionParam は ok (既定値のまま)
 */
WebMapPlan ParseWebMapParams(const UnicodeString &param);

/**
 * @brief 緯度経度の入力文字列を解釈する
 * @details 区切りは `,`・タブ・`;`・空白の順に探す (VCL と同じ優先順)。
 *          両方とも空でなく数値なら true で lat/lng に入れる
 */
bool ParseLatLng(const UnicodeString &text, double &lat, double &lng);

/// 地図 URL を組み立てる (配線側でブラウザ起動・ログに使う)
UnicodeString BuildMapUrl(double lat, double lng, int zoom);

//---------------------------------------------------------------------------
// PlayList (23604行)
//---------------------------------------------------------------------------

enum class PlayListSub {
	Next,     //!< NX: 次を再生
	Prev,     //!< PR: 前を再生
	Pause,    //!< PS: 一時停止
	Resume,   //!< RS: 再開
	PlayPause,//!< PP: 再生/一時停止の切替
	ClearAll, //!< CA: 停止・クリア
	FileInfo, //!< FI: ファイル情報
	Setup,    //!< 無指定: プレイリスト設定 (LS/RP/SF/SR 付き)
};

PlayListSub ResolvePlayListSub(const UnicodeString &param);

}  // namespace f_batch6_ops

#endif  // NYANFI_GUI_F_BATCH6_OPS_H
