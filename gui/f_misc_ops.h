/**
 * @file gui/f_misc_ops.h
 * @brief Fモード残コマンド batch4 の判断ロジック (wx 非依存の純関数)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の以下 (src 必読済み):
 *          - Suspend (26516行): `SetToggleAction(RsvSuspended)` で反転し、
 *            解除時は `StartReserve()` で予約を再開する。Caption は
 *            保留中なら「解除」、そうでなければ「保留」
 *          - PauseAllTask (23570行): `paused = any(TaskPause)` を
 *            `SetToggleAction` に通す (既定は反転: 停止中があれば再開へ、
 *            無ければ停止へ)。Update の Caption は停止中があれば
 *            「すべて再開」、無ければ「すべて一旦停止」
 *          - CancelAllTask (14070行): 全スレッドの `TaskCancel=true` と
 *            予約の全消去。Update の Enabled は `get_BusyTaskCount()>0`
 *          - Backup (13645行): `IsCurFList/IsOppFList` 必須、
 *            `EqualDirLR` (同一) は不可。実コピーはタスク発行のため
 *            ここでは経路の検証だけを置く
 *          - CompressDir (14987行): 選択ディレクトリ優先、無ければ
 *            カーソル位置 (ディレクトリのみ)。`DCOMP` タスクを発行するが
 *            ここでは対象の選び方だけを置く
 *          - CompareDlg NC分岐 (14464行): `TestActionParam("CS")` で
 *            大小区別を切り替える名前突き合わせ。ダイアログ分岐
 *            (FileCompDlg) は GUI 層の仕事のため含めない
 *          - FindHardLink (18868行): 書庫/ADS/FTP/UNC不可、
 *            ディレクトリ不可、リンク数2以上必須
 *          - LinkToOpp (20622行): sym→l_name、lnk→リンク先、
 *            hard→反対側の一覧にあるものを優先、無ければ先頭
 *          - SetFolderIcon (25594行): RD/SD/RS/ND の分岐。
 *            実設定 (set_FolderIcon) は GUI 層のため分岐だけを置く
 *          - FindFolderIcon (18152行): アイコン未選択は不可
 *          - JumpTo (20433行): `def_if_empty(ActionParam, GetCurFileName())`。
 *            空は不可。書庫内ジャンプ (JumpToArcR) は GUI 層のため含めない
 *          - DriveGraph (16832行): パラメータあれば `X:` 形式、
 *            無ければカレントのドライブ。UNC は不可
 *          - TaskMan (26801行): ダイアログ表示のみ。件数表示だけを置く
 *
 *          いずれも wx を含まず、nyanfi_gui_core でビルド・テストする (規約8)。
 */
#ifndef NYANFI_GUI_F_MISC_OPS_H
#define NYANFI_GUI_F_MISC_OPS_H

#include <vector>

#include "usr_str.h"

namespace f_misc_ops {

//---------------------------------------------------------------------------
// SetToggleAction 汎用 (MainFrm.cpp:12651)
//   sw = TestActionParam("ON")? true : TestActionParam("OFF")? false : !sw
//---------------------------------------------------------------------------

/// ';' 区切りトークンの大小無視の完全一致 (VCL TestActionParam 相当)
bool HasParamToken(const UnicodeString &param, const UnicodeString &token);

/**
 * @brief ON/OFF/反転を解決する (Suspend・PauseAllTask の共通部)
 * @param cur 現在値 (Suspend なら RsvSuspended、PauseAll なら any(TaskPause))
 * @param param ActionParam
 */
bool ToggleFlagValue(bool cur, const UnicodeString &param);

/**
 * @brief PauseAllTask の新しい全体状態 (23570行)
 * @details VCL は `paused = any(TaskPause)` を SetToggleAction に通す。
 *          既定 (パラメータ無し) は反転: 停止中があれば全再開へ、
 *          無ければ全停止へ。ON/OFF 指定はそのまま。
 */
inline bool DecidePauseAllTarget(bool any_paused, const UnicodeString &param)
{
	return ToggleFlagValue(any_paused, param);
}

/// 一覧に1つでも停止中があれば true
bool AnyPaused(const std::vector<bool> &pauses);

/// PauseAllTask Update の Caption 分岐
UnicodeString PauseAllCaption(int paused_count);

/// Suspend Update の Caption 分岐
UnicodeString SuspendCaption(bool suspended);

/// CancelAllTask Update の Enabled 分岐 (`get_BusyTaskCount()>0`)
inline bool CanCancelTasks(int busy_count)
{
	return busy_count > 0;
}

//---------------------------------------------------------------------------
// Backup (13645行): 経路の検証
//---------------------------------------------------------------------------

/**
 * @brief バックアップ元/先として使えるか
 * @details VCL は IsCurFList/IsOppFList 必須・EqualDirLR 不可。
 *          ここでは両方非空かつ同一でない (大小無視) ことを見る。
 *          空・同一の場合は error_out に理由を入れる
 */
bool ValidateBackupPaths(const UnicodeString &cur_path, const UnicodeString &opp_path,
                         UnicodeString &error_out);

//---------------------------------------------------------------------------
// CompressDir (14987行): 対象の選び方
//---------------------------------------------------------------------------

/**
 * @brief 選択中のディレクトリの添字を返す
 * @param selected 選択状態 / @param is_dir ディレクトリか
 * @details VCL は選択があれば選択ディレクトリだけ (ファイルは飛ばす)。
 *          空なら呼び出し元がカーソル位置分岐へ進む
 */
std::vector<int> CompressTargetIndices(const std::vector<bool> &selected,
                                       const std::vector<bool> &is_dir);

/// カーソル位置が対象になるか (ディレクトリのみ)
inline bool CursorCompressTarget(bool cursor_is_dir)
{
	return cursor_is_dir;
}

//---------------------------------------------------------------------------
// CompareDlg NC分岐 (14464行): 名前突き合わせ
//---------------------------------------------------------------------------

/// CS指定なら区別、既定は大小無視 (VCL の cs_sw 分岐と同じ)
bool MatchFileNames(const UnicodeString &a, const UnicodeString &b, bool case_sensitive);

//---------------------------------------------------------------------------
// FindHardLink (18868行) / LinkToOpp (20622行)
//---------------------------------------------------------------------------

/**
 * @brief ハードリンク検索に入れるか
 * @details 書庫/ADS/FTP/UNC不可、ディレクトリ不可、リンク数2以上必須。
 *          不可の場合は error_out に理由を入れる
 */
bool CanFindHardLink(bool is_arc, bool is_ads, bool is_ftp, bool is_unc, bool is_dir,
                     int link_count, UnicodeString &error_out);

/**
 * @brief ハードリンクのうち反対側に開くものを選ぶ (20622行)
 * @param cur_file カーソル位置のフルパス (自分自身は除く)
 * @param candidates ハードリンクの一覧
 * @param opp_files 反対側の一覧にあるフルパス
 * @details 反対側の一覧にあるものを優先、無ければ先頭。無ければ空
 */
UnicodeString PickOppHardLink(const UnicodeString &cur_file,
                              const std::vector<UnicodeString> &candidates,
                              const std::vector<UnicodeString> &opp_files);

//---------------------------------------------------------------------------
// SetFolderIcon (25594行) / FindFolderIcon (18152行)
//---------------------------------------------------------------------------

/// SetFolderIcon のパラメータ分岐
enum class FolderIconCmd {
	ChooseFile,     //!< 空・その他: ファイル選択ダイアログ
	SetFile,        //!< パス指定: そのアイコンを設定 (VCL は ActionParam を直接使う)
	ResetToDefault, //!< "RS": デフォルトに戻す
	ClearDefault,   //!< "RD": デフォルトを空に
	SelectDefault,  //!< "SD": デフォルトを選択
	Menu,           //!< "ND": ポップアップメニュー
};

FolderIconCmd ParseFolderIconParam(const UnicodeString &param);

/// FindFolderIcon に入れるか (アイコン未選択は不可)
bool ValidateFolderIconSearch(const UnicodeString &path, bool icons_empty,
                              UnicodeString &error_out);

//---------------------------------------------------------------------------
// JumpTo (20433行) / DriveGraph (16832行)
//---------------------------------------------------------------------------

/**
 * @brief ジャンプ先を解決する
 * @details `def_if_empty(ActionParam, GetCurFileName())`。
 *          相対なら cur_dir 基準で絶対化する。両方空なら空を返す
 */
UnicodeString ResolveJumpTarget(const UnicodeString &action_param,
                                const UnicodeString &cur_file, const UnicodeString &cur_dir,
                                UnicodeString &error_out);

/**
 * @brief グラフ対象のドライブを解決する
 * @details パラメータあれば先頭文字を `X:` 形式に、無ければ cur_path の
 *          ドライブ (`C:` 等) を取り出す。UNC・空なら空を返す
 */
UnicodeString ResolveDriveName(const UnicodeString &action_param, const UnicodeString &cur_path);

//---------------------------------------------------------------------------
// TaskMan (26801行): 件数表示
//---------------------------------------------------------------------------

/// タスク一覧ダイアログの要約行。0件なら「実行中のタスクはありません」
UnicodeString FormatTaskSummary(int busy_count, int paused_count);

}  // namespace f_misc_ops

#endif  // NYANFI_GUI_F_MISC_OPS_H
