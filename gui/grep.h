/**
 * @file gui/grep.h
 * @brief ファイル内容検索 (grep) のロジック層 (wx 非依存)
 *
 * @details
 * `src/grep_thread.cpp` (VCL 版の GREP スレッド) は `Global.h`
 * (`Application`/`WM_NYANFI_GREP_END`)、未移植の `load_text_ex`
 * (`Global.cpp`)、DFM 専用の展開処理 (`usr_xd2tx.h`) に依存しており、
 * そのままでは移植できない。
 *
 * `src/file_filter.cpp` (`FileFilter` クラス、移植済み) は Head/Tail/
 * HtmlHead/HtmlBody/HtmlRem/SubStr/DfmObj という「行範囲を絞り込むフィルタ
 * 文字列」を解釈するためのクラスで、今回の要件 (リテラル/正規表現 +
 * 大小文字無視 + ファイル名マスク) には無い機能ばかりのため、`FileFilter`
 * 自体は使わない (過剰)。
 *
 * 代わりに、`grep_thread.cpp` が実際にマッチ判定へ使っていたのと同じ
 * 移植済み関数 (`find_mlt` / `TRegEx::Match`、いずれも `src/usr_str.cpp`) を
 * そのまま使う。ファイルの読み込み・文字コード判定・バイナリ判定は
 * 自前実装せず、`gui/text_viewer_core.h` の `LoadForView`
 * (`get_MemoryCodePage` 経由) をそのまま流用する。
 * ファイル名マスクの照合は `gui/file_item.h` の `MatchPathMask` を使う
 * (ディレクトリ名にも適用するため、"!node_modules\\" のような
 * ディレクトリ専用の除外マスクも自然に効く)。
 */
#ifndef NYANFI_GUI_GREP_H
#define NYANFI_GUI_GREP_H

#include <functional>
#include <vector>

#include "gui/text_viewer_core.h"
#include "usr_str.h"

namespace grep_core {

/// 1件のマッチ (「ファイル名:行番号:行の内容」に相当)
struct GrepMatch {
	UnicodeString file;  //!< フルパス
	int line = 0;        //!< 行番号 (1始まり)
	UnicodeString text;  //!< 行の内容 (表示用)
};

/// 検索条件
struct GrepOptions {
	UnicodeString keyword;      //!< 検索文字列 (空白区切りで複数指定可。find_mlt の仕様のまま)
	UnicodeString mask;         //!< ファイル名マスク (";" 区切り。空/"*" で全件。MatchPathMask を参照)
	bool recursive = false;     //!< サブディレクトリを含むか
	bool use_regex = false;     //!< 正規表現として扱うか (false ならリテラル)
	bool case_sensitive = false;//!< 大小文字を区別するか
};

/// 走査・検索の上限 (固まらないための上限)
struct GrepLimits {
	/// 内容検索を行う最大ファイル数。超えたら以降のファイルは対象にせず打ち切る
	/// (推測・要検証。VCL 版に相当する明確な上限記載は見当たらなかった。
	/// 数万ファイル規模のディレクトリでも実用的な時間で終わるよう決めた値)
	int max_files = 20000;
	/// 保持する最大マッチ件数。超えたら以降の走査を打ち切る
	/// (結果一覧が巨大になって表示・選択が固まるのを防ぐ。推測・要検証)
	int max_matches = 5000;
	/// 1ファイルあたり検索する最大バイト数。超えた分は検索対象にしない
	/// (先頭の max_file_bytes バイトだけを検索する)。
	/// text_viewer_core::kMaxViewBytes (8MB、テキストビューアの上限) とそろえ、
	/// 大きなログファイル等を開いても固まらないようにする (推測・要検証)
	Int64 max_file_bytes = text_viewer_core::kMaxViewBytes;
};

/// 検索結果
struct GrepResult {
	std::vector<GrepMatch> matches;
	int files_scanned = 0;         //!< 実際に内容を読んだファイル数 (バイナリ除く)
	int files_skipped_binary = 0;  //!< バイナリ判定でスキップしたファイル数
	int files_truncated = 0;       //!< max_file_bytes を超えて先頭のみ検索したファイル数
	bool stopped_by_file_limit = false;   //!< max_files に達して打ち切った
	bool stopped_by_match_limit = false;  //!< max_matches に達して打ち切った
	bool cancelled = false;        //!< cancel_cb が true を返して中断した
	UnicodeString error;           //!< 検索を開始できなかった場合のエラー (不正な正規表現など)
};

/// 中断確認のコールバック。true を返すと以降の走査を打ち切る (呼び出しコストの
/// 低い処理にすること。ファイル単位で毎回呼ばれる)
using GrepCancelCallback = std::function<bool()>;
/// 進捗通知のコールバック (走査済みファイル数・現在のマッチ件数)
using GrepProgressCallback = std::function<void(int files_scanned, int matches_found)>;

/**
 * @brief dir 以下を検索する
 * @param dir 検索対象ディレクトリ (末尾の "\\" は有無どちらでもよい)
 * @param opt 検索条件
 * @param limits 走査・検索の上限
 * @param cancel_cb 中断確認 (省略可)
 * @param progress_cb 進捗通知 (省略可)
 * @return GrepResult
 */
GrepResult SearchDirectory(const UnicodeString &dir, const GrepOptions &opt,
                            const GrepLimits &limits = GrepLimits(),
                            const GrepCancelCallback &cancel_cb = nullptr,
                            const GrepProgressCallback &progress_cb = nullptr);

/**
 * @brief 1ファイルを検索する (単体テスト・単体呼び出し用)
 * @param path 対象ファイルのフルパス
 * @param opt 検索条件 (mask/recursive は無視される)
 * @param max_file_bytes 検索する最大バイト数
 * @param[out] is_binary バイナリ判定でスキップしたか
 * @param[out] truncated max_file_bytes を超えて先頭のみ検索したか
 * @return マッチした行のリスト
 */
std::vector<GrepMatch> SearchFile(const UnicodeString &path, const GrepOptions &opt,
                                   Int64 max_file_bytes, bool &is_binary, bool &truncated);

}  // namespace grep_core

#endif  // NYANFI_GUI_GREP_H
