/**
 * @file gui/view_settings.h
 * @brief 表示の切り替え (機能群22) の引数解釈と値の計算 (wx 非依存の純粋ロジック)
 *
 * @details 対象は `src/MainFrm.cpp` の SetFontSize / ZoomIn / ZoomOut / ZoomReset /
 *          WinPos / AlphaBlend / SetSubSize / SetSttBarFmt / ShowIcon / ShowToolBar /
 *          MenuBar / ShowFKeyBar / FileListOnly / ShowPreview / ShowFileInfo。
 *
 *          `gui/view_state.h` (境界比率・属性フィルタ) の隣に置く別モジュール。
 *          こちらは「引数の解釈」と「値の計算」だけを持つ。実際にウィンドウを
 *          動かす・部品を出す処理は wx 側 (呼び出し側) の仕事なので入れない
 *          (規約8)。DPI/PPI によるフォントサイズの実寸変換
 *          (MainFrm.cpp:5233 の `MulDiv(sz, CurrentPPI, lp->Font->PixelsPerInch)`)
 *          も実行時の画面情報が要るため、このモジュールの外側 (wx 側) の仕事とする。
 *
 *          VCL の実装から実測した点は各関数のコメントを参照。
 */
#ifndef NYANFI_GUI_VIEW_SETTINGS_H
#define NYANFI_GUI_VIEW_SETTINGS_H

#include <functional>

#include "gui/file_item.h"

namespace view_settings {

//===========================================================================
// トグル系 (ShowIcon / ShowToolBar / MenuBar / ShowFKeyBar / FileListOnly /
// ShowPreview / ShowFileInfo など)
//===========================================================================

/**
 * @brief トグル系アクションの指示
 * @details VCL の `SetToggleAction` (MainFrm.cpp:12651) が受け付ける形に合わせる。
 *          `sw = TestActionParam("ON")? true : TestActionParam("OFF")? false : !sw;`
 *          — 引数のトークンに "ON"/"OFF" があれば明示的にその値にし、
 *          どちらも無ければ現在値を反転する。常にトグルではない。
 */
enum class Toggle { Flip, On, Off };

/**
 * @brief 引数文字列を解釈する (SetToggleAction 相当)
 * @details `TestActionParam` (MainFrm.cpp:12515) は ActionParam を ';' で分割し、
 *          各トークンを `SameText` (大小文字を区別しない完全一致) で比較する。
 *          "ON" というトークンが1つでもあれば On、無くて "OFF" があれば Off、
 *          どちらも無ければ Flip を返す。"ON;OFF" のように両方あるときは
 *          "ON" が先に判定されるので On になる (VCL の三項式と同じ優先順位)。
 */
Toggle ParseToggle(const UnicodeString &param);

/// 現在値に Toggle を適用した結果 (MainFrm.cpp:12651 の代入そのもの)
bool ApplyToggle(bool current, Toggle how);

//===========================================================================
// フォントサイズ (SetFontSize / ZoomIn / ZoomOut / ZoomReset)
//===========================================================================

/// 最小フォントサイズ (`Global.h:137 MIN_FNTZOOM_SZ`)
inline constexpr int kMinFontSize = 2;
/// 最大フォントサイズ (`Global.h:136 MAX_FNTZOOM_SZ`)
inline constexpr int kMaxFontSize = 72;
/// 既定フォントサイズ。VCL 起動時の `defFont->Size` (MainFrm.cpp:521)。
/// 実際の「戻り先」は ini 設定 (`ListFont`) 次第なので、これは ini 未設定時の
/// 既定値でしかない。ParseFontSize の `base` 引数には実行時の ListFont サイズを渡すこと。
inline constexpr int kDefaultFontSize = 10;

/// [kMinFontSize, kMaxFontSize] に丸める。範囲外は端で止める (失敗しない)
int ClampFontSize(int size);

/**
 * @brief delta 段だけ動かす (ZoomIn/ZoomOut 相当)
 * @details MainFrm.cpp:5240-5243 の相対指定分岐:
 *          `int fsz = lp->Font->Size + std::clamp(sz, -12, 12);`
 *          `lp->Font->Size = std::clamp(fsz, MIN_FNTZOOM_SZ, MAX_FNTZOOM_SZ);`
 *          delta 自体もまず ±12 に丸めてから加算する。結果はさらに
 *          [kMinFontSize, kMaxFontSize] に丸める。
 */
int AdjustFontSize(int current, int delta);

/**
 * @brief SetFontSize の引数を解釈する
 * @param param   ActionParam。空なら false を返し `out` は変更しない
 *                (MainFrm.cpp:25695 `if (!ActionParam.IsEmpty())` のガードと同じ)
 * @param current 現在のフォントサイズ ('^' 判定の基準。MainFrm.cpp:5238 の
 *                `x_sw && lp->Font->Size==fsz_set`)
 * @param base    引数が数値として解釈できないとき、または '^' 指定で
 *                現在値と結果が一致したときの戻り先 (VCL の `ListFont->Size` 相当。
 *                MainFrm.cpp:5236 `AssignScaledFont(lp, ListFont)`)
 * @param out     適用後のフォントサイズ ([kMinFontSize, kMaxFontSize] に丸め済み)
 * @return 引数を適用したか (空文字列のときだけ false)
 * @details 先頭の '^' は「指定サイズにする。ただし既にそのサイズなら base に戻す」
 *          というトグル指定 (MainFrm.cpp:25693-25696 の `remove_top_s(ActionParam, '^')`、
 *          およびこの意味は MainFrm.cpp:5238 の分岐から実測した)。
 *          数値変換に失敗した場合は VCL の `ToIntDef` と同じく `base` にフォールバックする。
 */
bool ParseFontSize(const UnicodeString &param, int current, int base, int &out);

//===========================================================================
// 透過度 (AlphaBlend)
//===========================================================================

/// 透過度の最小値。MainFrm.cpp:13487 のコメント「64～255に制限」および
/// `std::max(64, a)`
inline constexpr int kMinAlpha = 64;
/// 透過度の最大値 (不透明)。`std::min(255, a)`
inline constexpr int kMaxAlpha = 255;

/// ParseAlpha の解釈結果の種類
enum class AlphaAction {
	Disable,	//!< 引数が空 (透過を解除して不透明に戻す。MainFrm.cpp:13460-13462)
	NeedsDialog,//!< 引数が "IN" (対話入力が要る。MainFrm.cpp:13469 の `inputbox_ex_n`
				//!< 呼び出しは wx 側の仕事なのでここでは解決しない)
	Apply,		//!< out に適用後の値が入っている
};

/**
 * @brief AlphaBlend の引数を解釈する (MainFrm.cpp:13458-13489)
 * @param param            ActionParam
 * @param current_value    現在の不透明度 ('+'/'-' 相対指定の基準。AlphaValue 相当)
 * @param current_enabled  現在 AlphaBlend が有効か ('^' 指定の判定に使う)
 * @param out              AlphaAction::Apply のときだけ有効。適用後の値
 *                          ([kMinAlpha, kMaxAlpha] に丸め済み)
 * @details 引数が空なら Disable。"IN" (トークン完全一致) なら NeedsDialog。
 *          それ以外は先頭1文字で分岐する (MainFrm.cpp:13471-13476):
 *          - '^': 「指定値にする。ただし現在有効なら 255 (不透明) に戻す」
 *            (`if (x_sw) { if (AlphaBlend) a = 255; }` — 現在無効なら '^' の後の
 *            数値がそのまま使われる点に注意。指定が無ければ 255)
 *          - '+': 現在値に加算 (`AlphaValue + a`)
 *          - '-': 現在値から減算 (`AlphaValue - a`)
 *          - それ以外 (数字など): 絶対値として解釈 (既定値 255)
 *          最後に必ず [kMinAlpha, kMaxAlpha] に丸める。
 */
AlphaAction ParseAlpha(const UnicodeString &param, int current_value, bool current_enabled, int &out);

//===========================================================================
// ウィンドウ位置 (WinPos)
//===========================================================================

/// 四辺のうち1つの指定 (絶対値 or 現在値からの符号付き相対量)
struct EdgeSpec {
	bool set = false;		//!< この辺の指定があったか
	bool relative = false;	//!< 相対指定 ('+'/'-') か
	int value = 0;			//!< 絶対値、または相対の符号付き差分
};

/// 四辺の指定。指定が無い辺は変えない
struct WindowEdges {
	EdgeSpec left;
	EdgeSpec top;
	EdgeSpec right;
	EdgeSpec bottom;
};

/**
 * @brief WinPos の引数を解釈する (MainFrm.cpp:27700-27738)
 * @details 書式は ';' 区切りの複数指定。各指定は `[LTRB の1文字]` +
 *          (`+`|`-` の省略可) + 整数、例: `"L100;T50;R+20;B-10"`。
 *          - 先頭文字は大小文字を区別しない (`prm_lst[i].UpperCase()`)
 *          - `+`/`-` が無ければ絶対値としてその辺を置き換える
 *          - `+`/`-` があれば現在値からの符号付き加算 (相対)
 *          - **`%` 指定は無い** (実測。ソースにその処理は存在しない)
 *          - 数値が読めない、あるいは L/T/R/B 以外の文字で始まる指定は
 *            不正 (この関数は false を返す)
 * @return 解釈できたか。空文字列は「起動時設定に戻す」の意味を持つが、
 *         その解決は ini (`IniWinMode`/`IniWinLeft` 等) と `IsPrimary` に依存する
 *         VCL 側の話なのでこの関数の範囲外。呼び出し側で `param.IsEmpty()` を
 *         別途判定すること (VCL にあるが入れなかったもの。報告を参照)
 */
bool ParseWindowPos(const UnicodeString &param, WindowEdges &out);

/**
 * @brief 現在の矩形に WindowEdges を適用する (MainFrm.cpp:27718-27737)
 * @param edges 適用する指定
 * @param cur_left, cur_top, cur_right, cur_bottom 現在のウィンドウ矩形
 *        (VCL の `BoundsRect`)
 * @param out_left, out_top, out_right, out_bottom 適用後の矩形
 * @return 適用後が有効な矩形 (`left<right && top<bottom`) なら true。
 *         無効なら false を返し、out_* は変更しない
 *         (MainFrm.cpp:27732 `if (l>=r || t>=b) UserAbort(...)`)
 */
bool ApplyWindowPos(const WindowEdges &edges,
	int cur_left, int cur_top, int cur_right, int cur_bottom,
	int &out_left, int &out_top, int &out_right, int &out_bottom);

//===========================================================================
// サブウィンドウのサイズ (SetSubSize)
//===========================================================================

/**
 * @brief SetSubSize の引数を解釈する (MainFrm.cpp:25729-25731)
 * @param param   ActionParam。"100" のような絶対値、または "+50"/"-50" のような相対値
 * @param out     解釈できた値 (符号付き。相対指定かどうかは `out_relative` を見る)
 * @param out_relative 相対指定 ('+' か '-' で始まる) だったか
 * @return 解釈できたか。空、または 0 (`ToIntDef(0)` の既定値と同じ = 数値でない)
 *         のときは false (VCL は `if (size==0) return;` で何もしない)
 */
bool ParseSubSize(const UnicodeString &param, int &out, bool &out_relative);

/**
 * @brief SubSize を適用した結果を返す (MainFrm.cpp:25732-25739)
 * @param requested   ParseSubSize で得た値
 * @param relative    相対指定なら true (current に加算)、false なら絶対値として置換
 * @param current     現在のサブパネルの大きさ
 * @param container_size 収まる側 (MainPanel) の大きさ
 * @param min_size    一覧側 (ListPanel) の最小許容サイズ
 *                    (`ListPanel->Constraints->MinHeight`/`MinWidth`)
 * @return 適用後のサイズ。`container_size - 新サイズ < min_size` になるなら
 *         **変更を拒否して current をそのまま返す** (クランプではなく丸ごと無視。
 *         MainFrm.cpp:25734/25738 の `if (...) return;`)
 */
int ResolveSubSize(int requested, bool relative, int current, int container_size, int min_size);

//===========================================================================
// ステータスバーの書式 (SetSttBarFmt)
//===========================================================================

/**
 * @brief `$PR(field,prefix,suffix)` の値を引くコールバック
 * @details VCL の `get_FileInfValue(fp, field)` (MainFrm.cpp:7852) 相当。
 *          プロパティの実際の一覧・取得方法は GUI 側 (ファイル情報まわり) の
 *          仕事なので、ここではコールバックとして受け取るだけにする。
 */
using StatusFieldLookup = std::function<UnicodeString(const UnicodeString &field)>;

/**
 * @brief ExpandStatusFormat に渡す値をまとめた構造体
 * @details 書式の展開そのものは純粋な文字列処理だが、埋め込む値
 *          (パス・サイズ文字列・タイムスタンプ文字列など) は他の未移植サブシステム
 *          (ファイル情報・アイコン・日時整形) が計算するので、ここでは
 *          「計算済みの値」として受け取る (規約8 と同じ考え方)。
 */
struct StatusFormatValues {
	/// 現在のカーソル項目があるか (VCL の `fp_ok` 相当。false なら
	/// $M/$Z/$Y/$T/$PR は何も出さない。MainFrm.cpp:7849)
	bool has_file = false;
	UnicodeString path;			//!< $P (VCL の `pnam`)
	UnicodeString base_name;		//!< $B (VCL の `bnam`。warn_filename_RLO 適用済み)
	/// $P2/$F2 で使う区切り文字置換後の文字列を作る区切り (VCL の `DirDelimiter`)。
	/// 既定は "\\" (無変換)
	UnicodeString dir_delimiter = "\\";
	int sort_mode = 0;			//!< $S/$S2 (VCL の `CurSortMode()`。0~6、範囲外は空文字)
	bool show_hidden = false;		//!< $HS の 'H'/'_' (ShowHideAtr)
	bool show_system = false;		//!< $HS の 'S'/'_' (ShowSystemAtr)
	UnicodeString mark_memo;		//!< $M (IniFile->GetMarkMemo)
	UnicodeString size_str;		//!< $Z (get_FileSizeStr)
	UnicodeString size_str_alt;	//!< $Y (get_size_str_B)
	UnicodeString time_str;		//!< $T (get_TimeStampStr)
};

/// $\ の区切り文字を置換する (VCL の `yen_to_delimiter`。Global.cpp:3490
/// `ReplaceStr(s, "\\", DirDelimiter)`)
UnicodeString ReplaceDirDelimiter(const UnicodeString &path, const UnicodeString &delimiter);

/**
 * @brief 書式文字列を展開する (MainFrm.cpp:7848-7887 `SetSttBarInf` の展開部分)
 * @param fmt 書式文字列 (SttBarFmt)
 * @param values 埋め込む値
 * @param field_lookup `$PR(...)` 用のコールバック。省略すると `$PR` は常に空を返す
 * @details 対応するディレクティブ (実測。前方一致で判定するので長い綴りを先に試す
 *          必要がある。例えば "P2" は "P" より先に判定しないと "P" にマッチしてしまう):
 *          - `$P`  現在のパス (`values.path`)
 *          - `$P2` パスの区切り文字を置換したもの
 *          - `$F`  パス+ファイル名
 *          - `$F2` `$F` の区切り文字を置換したもの
 *          - `$B`  ファイル名のみ
 *          - `$S`  ソート方法 ("名前|拡張子|日時|サイズ|属性|なし|場所" から
 *            `values.sort_mode` 番目)
 *          - `$S2` `$S` の1文字版 ("名|拡|時|サ|属|無|場")
 *          - `$HS` 隠し/システム属性の表示可否 ('H'/'_' + 'S'/'_')
 *          - `$DV` 以降を右寄せする位置のマーク (タブ文字 `\t` に変換する。
 *            複数回指定された場合は最後の指定だけが効く。MainFrm.cpp:7874
 *            `div_p = stt_str.Length() + 1` は代入のたびに上書きされるため)
 *          - `$M`/`$Z`/`$Y`/`$T` マークメモ/サイズ/サイズ(別形式)/タイムスタンプ
 *            (`values.has_file` が false なら何も出さない)
 *          - `$PR(field,prefix,suffix)` `field_lookup(field)` が空でなければ
 *            prefix + 値 + suffix を出す (`values.has_file` が false なら何も出さない)。
 *            区切りは **','** (セミコロンではない。VCL の `get_csv_array`
 *            (usr_str.cpp:1292) が `TStringList` の `Delimiter=','`/
 *            `QuoteChar='"'` を使うのをそのまま呼ぶ。引用符付き CSV も
 *            そのため対応済み)
 *          - 上記に一致しない `$` の並びは無視される (VCL は静かに何もしない)
 *          - `$` 以外の文字はそのまま出力する
 */
UnicodeString ExpandStatusFormat(const UnicodeString &fmt, const StatusFormatValues &values,
	const StatusFieldLookup &field_lookup = nullptr);

}  // namespace view_settings

#endif  // NYANFI_GUI_VIEW_SETTINGS_H
