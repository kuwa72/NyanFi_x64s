/**
 * @file gui/file_ext.h
 * @brief 拡張子別一覧ダイアログの集計・並べ替え・入出力 (wx 非依存)
 *
 * @details VCL は `src/FileExtDlg.cpp` の `TFileExtensionDlg`。再帰集計と
 *          ファイル走査は既存の gui/dir_info.h を拡張して使い、各項目の並べ替え、
 *          CP パラメーターの解決、一覧/CSV/TSV の書式だけをここへ分離した。
 *
 *          VCL 呼び出し位置 (grep 実測):
 *          - Dlg 生成・結果処理: src/MainFrm.cpp:17650-17679
 *          - ファイル情報表示:   src/FileExtDlg.cpp:950-953
 *          - 拡張子マスク検索:   src/FileExtDlg.cpp:904-910
 *
 *          未移植 (未実装扱い):
 *          - クラスタ単位の占有サイズとギャップ (移植済み core に
 *            get_ClusterSize の実体が無いため)
 *          - ライブラリ/書庫内ファイルの再帰
 *          - 拡張子別アイコンと Windows 標準プロパティ
 */
#ifndef NYANFI_GUI_FILE_EXT_H
#define NYANFI_GUI_FILE_EXT_H

#include <vector>

#include "usr_str.h"

namespace file_ext {

/// 拡張子一覧の並べ替えキー
enum class ExtensionSort { Extension, Count, Bytes, Average };

/// ファイル一覧の並べ替えキー
enum class FileSort { Name, Path };

/// 出力形式
enum class OutputFormat { Text, Csv, Tsv };

/// 拡張子1件の集計
struct Entry {
	UnicodeString extension;  //!< 小文字。拡張子なしは "(none)"
	int count = 0;
	Int64 bytes = 0;
	long double average = 0.0L;
	std::vector<UnicodeString> files;
};

/// ダイアログ全体の一覧
struct Summary {
	UnicodeString root;
	std::vector<Entry> extensions;
	int total_count = 0;
	Int64 total_bytes = 0;
	bool truncated = false;
};

/**
 * @brief ディレクトリを走査して拡張子別集計を作る
 * @details 実際の走査は dir_info::CalcExtStats。表示件数の上限は跨界で切らず、
 *          `truncated` を明示する (gui/dir_info.h の既存方針)。
 */
Summary Collect(const UnicodeString &path, bool recursive, bool show_hidden, bool show_system,
                bool &truncated_out);

/** @brief 拡張子一覧を並べ替える。同じ値の間は拡張子名の昇順で安定させる */
void SortExtensions(Summary &summary, ExtensionSort key, bool descending);

/** @brief 選択拡張子のファイル一覧を名前/場所順に並べ替える */
void SortFiles(std::vector<UnicodeString> &files, FileSort key, bool descending);

/** @brief 拡張子から MaskFind 用のマスクを作る */
UnicodeString BuildMask(const UnicodeString &extension);

/**
 * @brief FileExtList の CP パラメーターから対象ディレクトリを解決する
 * @param cursor_is_parent ".." かどうか
 * @details VCL MainFrm.cpp:17655-17663 と同じ。CP が無ければ現在ディレクトリ。
 */
UnicodeString ResolveTargetPath(const UnicodeString &param, const UnicodeString &current_path,
                                const UnicodeString &cursor_path, bool cursor_is_dir,
                                bool cursor_is_parent, UnicodeString &error_out);

/** @brief 集計を指定形式で整形する */
UnicodeString FormatReport(const Summary &summary, OutputFormat format, const UnicodeString &root);

}  // namespace file_ext

#endif  // NYANFI_GUI_FILE_EXT_H
