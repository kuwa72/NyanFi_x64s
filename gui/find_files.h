/**
 * @file gui/find_files.h
 * @brief ファイル名の検索 (wx 非依存)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の `FindFileDlg` / `FindDirDlg` /
 *          `FindFileDirDlg`。結果は「結果リスト」という**別のディレクトリの
 *          項目が混ざった一覧**として表示される。
 *
 *          歩く量に上限を設け、**超えたら結果に残す** (grep や
 *          ディレクトリ集計と同じ方針。報告書 §16.2)。
 */
#ifndef NYANFI_GUI_FIND_FILES_H
#define NYANFI_GUI_FIND_FILES_H

#include <vector>

#include "gui/file_item.h"

namespace find_files {

/// 走査するファイル数の上限
inline constexpr int kMaxScanFiles = 200000;
/// 結果の件数の上限
inline constexpr int kMaxResults = 5000;

/// 何を探すか
enum class Target { Files, Directories, Both };

/// 日付条件 (`src/Global.cpp` の `check_file_std` の `find_DT_mode` に対応。
/// 0=指定なし、1=同じ日、2=以前(当日を含む)、3=以後(当日を含む)。時刻は見ない)
enum class DateMode { None, Same, Before, After };

/// サイズ条件 (同 `find_SZ_mode`。0=指定なし、1=以下、2=以上。
/// VCL と同じくディレクトリは対象外)
enum class SizeMode { None, AtMost, AtLeast };

/// 属性条件 (同 `find_AT_mode`。0=指定なし、1=いずれかを含む、2=いずれも含まない)
enum class AttrMode { None, HasAny, HasNone };

/// 検索の条件
struct Query {
	UnicodeString mask;        //!< セミコロン区切りのマスク ("*.txt;*.dat")。空なら全部
	Target target = Target::Files;
	bool recursive = true;
	bool show_hidden = false;
	bool show_system = false;
	//--- ファイル名検索ダイアログ (src/FindDlg.cpp) の基本条件部 ---
	UnicodeString keyword;     //!< 検索語 (空なら条件なし。`find_Keywd` に対応)
	bool use_regex = false;    //!< true なら正規表現 (`find_RegEx`)
	bool case_sensitive = false;  //!< true なら大小文字を区別 (`find_Case`)
	bool match_all = false;    //!< true なら空白区切りの全語を含む (AND、`find_And`)
	DateMode date_mode = DateMode::None;  //!< 日付条件
	TDateTime date_value = 0;  //!< 日付条件の基準日
	SizeMode size_mode = SizeMode::None;  //!< サイズ条件
	Int64 size_value = 0;      //!< サイズ条件の基準値 (バイト)
	AttrMode attr_mode = AttrMode::None;  //!< 属性条件
	int attr_bits = 0;         //!< 属性条件のビット (`faReadOnly` 等の OR)
};

/// 検索の結果
struct Result {
	std::vector<FileItem> items;  //!< 見つかった項目 (full_path 付き)
	int scanned = 0;              //!< 走査した項目数
	bool truncated_scan = false;  //!< 走査の上限に達した
	bool truncated_hits = false;  //!< 結果の上限に達した
};

/**
 * @brief 名前がマスクに一致するか
 * @details `gui/file_item.h` の `MatchPathMask` はディレクトリ専用/除外指定まで
 *          扱うが、ここで要るのは**単純なワイルドカード照合**だけなので分けてある。
 *          `;` 区切りで複数指定でき、いずれかに一致すれば真。空なら常に真
 */
bool MatchesMask(const UnicodeString &name, const UnicodeString &mask);

/**
 * @brief ディレクトリを再帰的に歩いて名前で探す
 * @param root 起点
 * @param query 条件
 */
Result Search(const UnicodeString &root, const Query &query);

/**
 * @brief マスク以外の条件 (検索語・日付・サイズ・属性) に合うか
 * @details `src/Global.cpp` の `check_file_std()` の基本条件部だけを切り出した
 *          純関数。拡張条件部 (Exif・動画・テキスト内容等の `check_file_ex()`) は
 *          未移植のため扱わない (未実装扱い。検索自体は落とさない)。
 *          不正な正規表現は VCL ではダイアログ側の事前チェックで弾く
 *          (`TFindFileDlg::FindOkActionUpdate`)。ここでは念のため偽を返す
 * @param name パスを含まない名前
 * @param stamp 最終更新日時
 * @param size サイズ (ディレクトリは -1 等、何でもよい。サイズ条件の対象外)
 * @param attr 属性ビット (`faXXX`)
 * @param is_dir ディレクトリか
 */
bool MatchesQuery(const UnicodeString &name, TDateTime stamp, Int64 size, int attr,
                  bool is_dir, const Query &query);

/// 重複の判定基準
enum class DuplicateBy {
	NameSize,   //!< 名前とサイズ (速い。中身は見ない)
	Content,    //!< 内容 (サイズで絞ってからハッシュを取る)
};

/// 重複検索の条件 (`src/DuplDlg.cpp` の `TFindDuplDlg` の移植可能な部分。
/// ハッシュ算法の選択・最大サイズ・左右比較・シンボリックリンク除外は未移植)
struct DuplicateOptions {
	DuplicateBy how = DuplicateBy::Content;
	bool recursive = true;     //!< false なら直下だけ (`SubDirCheckBox` に対応)
	UnicodeString mask;        //!< 空なら全部。指定時は名前が一致するものだけ対象
};

/// 重複検索の結果
struct DuplicateResult {
	std::vector<FileItem> items;  //!< 重複しているファイル (グループごとに固まって並ぶ)
	int groups = 0;               //!< 重複グループの数
	int hashed = 0;               //!< ハッシュを計算したファイル数 (Content のときだけ)
	bool truncated_scan = false;
};

/**
 * @brief 重複しているファイルを探す (FindDuplDlg)
 * @param root 起点
 * @param how 判定基準
 * @param show_hidden 隠しファイルも見るか
 * @param show_system システムファイルも見るか
 * @details `Content` でも**まずサイズで束ねてから**ハッシュを取る。
 *          サイズが違えば内容も違うので、全ファイルのハッシュを計算する必要は無い
 *          (`ToOppSameHash` と同じ枝刈り)。
 *
 *          **サイズ 0 のファイルは対象外**。空ファイルは互いに「同じ内容」に
 *          なってしまい、重複として大量に並ぶだけで役に立たないため
 *          (こちらの判断)
 */
DuplicateResult FindDuplicates(const UnicodeString &root, const DuplicateOptions &opt,
                                bool show_hidden = false, bool show_system = false);

/**
 * @brief 重複しているファイルを探す (FindDuplDlg)。旧呼び出しとの互換用
 * @details 内容比較・再帰・マスク無しという既定の組み合わせ
 */
inline DuplicateResult FindDuplicates(const UnicodeString &root, DuplicateBy how,
                                      bool show_hidden, bool show_system)
{
	DuplicateOptions opt;
	opt.how = how;
	return FindDuplicates(root, opt, show_hidden, show_system);
}

}  // namespace find_files

#endif  // NYANFI_GUI_FIND_FILES_H
