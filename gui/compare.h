/**
 * @file gui/compare.h
 * @brief 左右のペインの比較とハッシュ (wx 非依存)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の `GetHash` / `CompareHash` /
 *          `SelOnlyCur` / `ToOppSameHash` / `DiffDir`。
 *          ハッシュの計算そのものは移植済みの `get_HashStr()`
 *          (src/usr_file_inf.h) が持つので、ここは**どう比べるか**だけを置く。
 */
#ifndef NYANFI_GUI_COMPARE_H
#define NYANFI_GUI_COMPARE_H

#include <functional>
#include <vector>

#include "gui/file_item.h"

namespace compare {

/// 比べ方
enum class MatchBy {
	Name,       //!< 名前だけ
	NameSize,   //!< 名前とサイズ
	NameTime,   //!< 名前と更新日時
	Content,    //!< 内容 (ハッシュ)
};

/**
 * @brief 反対側に「同じもの」が無い項目の添字を返す (SelOnlyCur)
 * @param items こちら側の一覧
 * @param others 反対側の一覧
 * @param how 比べ方 (Content はここでは扱わない。呼び出し側がハッシュを渡す)
 * @return こちら側だけにある項目の添字
 * @details ディレクトリと `..` は対象外 (VCL の SelOnlyCur も
 *          ファイルだけを選ぶ)
 */
std::vector<int> IndicesOnlyHere(const std::vector<FileItem> &items,
                                 const std::vector<FileItem> &others, MatchBy how);

/// 2件が「同じもの」か (Content 以外)
bool IsSameItem(const FileItem &a, const FileItem &b, MatchBy how);

/// ディレクトリ比較の1行
struct DiffRow {
	UnicodeString name;
	bool in_left = false;
	bool in_right = false;
	bool differs = false;   //!< 両方にあるが内容が違う (how の基準で)
};

/**
 * @brief 左右のディレクトリを突き合わせる (DiffDir)
 * @details 名前順に並べた突き合わせ結果を返す。ディレクトリと `..` は除く。
 *          **両方にあって同じものは含めない** (違いだけを見たいため)
 */
std::vector<DiffRow> DiffDirectories(const std::vector<FileItem> &left,
                                     const std::vector<FileItem> &right, MatchBy how);

//---------------------------------------------------------------------------
// ディレクトリ比較の条件 (TDiffDirDlg / src/DiffDlg.cpp)
//
// VCL 版の該当は `src/MainFrm.cpp` の `DiffDirActionExecute`
// (AL/DL/CS パラメータ分岐つき) と `src/DiffDlg.cpp` (`TDiffDirDlg`)。
// ダイアログ自体は `gui/diff_dialog.h` が持つ。ここではパラメータの
// 解決・マスクの正規化・一覧の絞り込みだけを置く (wx 非依存)。
//
// 未移植 (未実装扱い):
// - 再帰列挙 (`get_all_files_ex` の sub_sw/99階層・除外ディレクトリでの
//   走査)。こちらは表示中の一覧 (`VisibleItems`) だけを比べる
// - 内容比較。名前とサイズ (`MatchBy::NameSize`) で比べる
// - 履歴の永続化 (`DiffIncMaskHistory` 等)。現在値は ini に残す
//---------------------------------------------------------------------------

/// ディレクトリ比較の条件 (TDiffDirDlg の入力そのまま)
struct DiffDirOptions {
	UnicodeString inc_mask = _T("*.*");  //!< 対象マスク (`;` 区切り複数可)
	UnicodeString exc_mask;              //!< 除外マスク (`;` 区切り複数可)
	UnicodeString exc_dir;               //!< 除外ディレクトリマスク (sub_dir のときだけ有効)
	bool sub_dir = false;                //!< サブディレクトリも対象
	bool case_sensitive = false;         //!< CS パラメータ (表題の切り替え用)
};

/// 条件の出どころ (MainFrm.cpp の AL/DL 分岐と同じ)
enum class DiffDirSource {
	Dialog,        //!< ダイアログで入力
	AllPreset,     //!< AL: 全件 (`*.*`・サブディレクトリ込み、ダイアログ無し)
	DefaultPreset, //!< DL: 保存値 (ini) のまま、ダイアログ無し
};

/// `;` 区切りパラメータから出どころを決める (AL/DL があればそちらが優先)
DiffDirSource ResolveDiffDirSource(const UnicodeString &param);

/// 空の対象マスクは `*.*` (VCL `TDiffDirDlg::FormClose` と同じ)
UnicodeString NormalizeDiffIncMask(const UnicodeString &mask);

/// 除外ディレクトリ欄はサブディレクトリ対象のときだけ有効
/// (VCL `TDiffDirDlg::StartActionUpdate` と同じ)
inline bool IsDiffExcDirEnabled(bool sub_dir)
{
	return sub_dir;
}

/// AL プリセット (MainFrm.cpp の AL 分岐と同じ)
DiffDirOptions AllDiffPreset();

/// DL プリセット: 保存値 (ini) をそのまま使う。対象マスクだけ正規化する
DiffDirOptions DefaultDiffPreset(const UnicodeString &inc_mask, const UnicodeString &exc_mask,
                                 const UnicodeString &exc_dir, bool sub_dir);

/**
 * @brief 一覧をマスクで絞り込む
 * @details 対象マスク (`;` 区切り) のどれかに合い、除外マスクのどれにも
 *          合わないファイルだけを残す。`..` とディレクトリは落とす
 *          (VCL は再帰列挙のファイルだけを比べるため)
 */
std::vector<FileItem> FilterDiffItems(const std::vector<FileItem> &items,
                                      const UnicodeString &inc_mask,
                                      const UnicodeString &exc_mask);

//---------------------------------------------------------------------------
// 同名ファイルの比較 (TFileCompDlg / src/CompDlg.cpp、MainFrm.cpp:14464)
//
// VCL は FileCompDlg で「タイムスタンプ・サイズ・ハッシュ・同一性」の条件を
// 選んでから左右を突き合わせる。ダイアログは `gui/comp_dialog.h` が持つ。
// ここでは条件の有効判定・排他制御・1組の判定・一覧の突き合わせだけを置く。
//
// 未移植 (未実装扱い):
// - ハッシュの簡易チェック→全体計算の二段構え (src/MainFrm.cpp:14650付近)。
//   ここでは呼び出し側の `CompProbes::hash_equal` が 1 回だけ判定する
// - 処理中の進捗表示と ESC 中断。比較自体は落とさない
// - 選択マスクの実行 (VCL は実行時に `SelMask OP` を投げる)。呼び出し側の判断
// - 書庫内ファイルとディレクトリの突き合わせ (VCL の `b_name` 相当が
//   一覧に無いため、名前の完全一致で代用する)
//---------------------------------------------------------------------------

/// サイズ条件 (VCL の `s_mode` と同じ並び。0=無視)
enum class CompSizeMode { Ignore = 0, Unequal = 1, Equal = 2, Greater = 3, Less = 4 };
/// タイムスタンプ条件 (VCL の `t_mode` と同じ並び。0=無視)
enum class CompTimeMode { Ignore = 0, Unequal = 1, Equal = 2, Newer = 3, Older = 4 };
/// ハッシュ条件 (VCL の `h_mode` と同じ並び。0=無視)
enum class CompHashMode { Ignore = 0, Unequal = 1, Equal = 2 };
/// 同一性条件 (VCL の `i_mode` と同じ並び。0=無視)
enum class CompIdMode { Ignore = 0, Unequal = 1, Equal = 2 };

/// 同名ファイル比較の条件 (TFileCompDlg の入力)
struct CompOptions {
	CompTimeMode time_mode = CompTimeMode::Ignore;
	CompSizeMode size_mode = CompSizeMode::Ignore;
	CompHashMode hash_mode = CompHashMode::Ignore;
	CompIdMode id_mode = CompIdMode::Ignore;
	int alg_index = 0;              //!< ハッシュ算法 (HASH_ID_STR の添字。0=MD5)
	bool cmp_dir = false;           //!< ディレクトリも比較する
	bool cmp_arc = false;           //!< ディレクトリ比較のときだけ有効 (VCL と同じ)
	bool sel_opp = false;           //!< 結果と反対側も選択する
	bool sel_rev = false;           //!< 選択を反転する
	bool sel_msk = false;           //!< 選択項目だけ残す (実行は未移植)
	bool case_sensitive = false;    //!< CS パラメータ (名前の照合にだけ使う)
};

/// 条件の有効・無効 (VCL は `TFileCompDlg::OkActionUpdate` で決める)
struct CompEnable {
	bool size = false;
	bool hash = false;
	bool alg = false;     //!< ハッシュ算法コンボ
	bool id = false;
	bool cmp_arc = false;
	bool sel_mask = false;
};

/**
 * @brief 各条件を使えるかを決める
 * @param o 現在の条件
 * @param all_dir_has_size 比較対象の全ディレクトリのサイズが揃っているか
 *        (VCL は `AllDirHasSize`。揃っていなければサイズ条件ごと無効)
 * @param ftp_either どちらかが FTP か (内容比較ができない)
 * @param arc_either どちらかが書庫か (同一性比較ができない)
 * @param sel_mask_available 「ファイル一覧 or 書庫」か (選択マスクの実行条件)
 */
CompEnable ResolveCompEnabled(const CompOptions &o, bool all_dir_has_size, bool ftp_either,
                              bool arc_either, bool sel_mask_available);

/**
 * @brief ハッシュと同一性の排他を保つ (VCL の `OptRadioGroupClick` 相当)
 * @param o 現在の条件
 * @param hash_clicked ハッシュ欄 clicked=true / 同一性欄 clicked=false
 * @return 排他化解いた条件 (相手側が「無視」でなければ 0 に戻す)
 */
CompOptions ApplyExclusiveOpt(CompOptions o, bool hash_clicked);

/// サイズ条件の判定
bool CompSizeMet(CompSizeMode mode, Int64 left, Int64 right);

/**
 * @brief タイムスタンプ条件の判定
 * @details 2秒の許容誤差つき (VCL の `TimeTolerance` 既定値 2000ms 相当。
 *          ini で変えられる点は移植していないため固定)
 */
bool CompTimeMet(CompTimeMode mode, double left_stamp, double right_stamp);

/// ハッシュ/同一性は実処理が必要なため、呼び出し側が判定を注入する
struct CompProbes {
	std::function<bool(const FileItem &, const FileItem &)> hash_equal;
	std::function<bool(const FileItem &, const FileItem &)> identity_equal;
};

/// 突き合わせの結果
struct CompResult {
	std::vector<int> left_hits;   //!< 選択する左側 (現在側) の添字
	std::vector<int> right_hits;  //!< sel_opp のときの反対側の添字
	int left_count = 0;           //!< 比較対象にした左側の件数 (ログの分母)
};

/**
 * @brief 同名のファイル同士を条件付きで突き合わせる
 * @details 条件は AND 結合 (VCL と同じ)。`CompProbes` は条件が「無視」で
 *          ないときだけ呼ばれる。`hash_equal` はサイズが同じ組でのみ呼ばれる
 *          (VCL の `cfp->f_size==ofp->f_size` と同じ前置き)
 */
CompResult CompareSameNames(const std::vector<FileItem> &left,
                            const std::vector<FileItem> &right, const CompOptions &o,
                            const CompProbes &probes);

/**
 * @brief 選択を反転する (VCL の sel_rev 相当)
 * @param items 対象一覧 (`..` とディレクトリの判定に使う)
 * @param selected 現在の選択状態
 * @param cmp_dir ディレクトリも比較対象か。親 (..) は常に対象外
 * @return 反転後の選択状態 (対象外の項目は元のまま)
 */
std::vector<bool> ReverseCompareSelection(const std::vector<FileItem> &items,
                                          std::vector<bool> selected, bool cmp_dir);

}  // namespace compare

#endif  // NYANFI_GUI_COMPARE_H
