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

}  // namespace compare

#endif  // NYANFI_GUI_COMPARE_H
