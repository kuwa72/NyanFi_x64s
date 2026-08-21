/**
 * @file gui/grep.cpp
 * @brief gui/grep.h の実装 (wx 非依存)
 */
#include "gui/grep.h"

#include "gui/file_item.h"  // MatchPathMask
#include "usr_file_ex.h"
#include "usr_str.h"

namespace grep_core {

//---------------------------------------------------------------------------
std::vector<GrepMatch> SearchFile(const UnicodeString &path, const GrepOptions &opt,
                                   Int64 max_file_bytes, bool &is_binary, bool &truncated)
{
	std::vector<GrepMatch> result;
	is_binary = false;
	truncated = false;

	// 読み込み・文字コード判定・バイナリ判定は自前実装せず、テキストビューアと
	// 同じ text_viewer_core::LoadForView (get_MemoryCodePage 経由) を流用する
	text_viewer_core::LoadResult doc = text_viewer_core::LoadForView(path, max_file_bytes);
	if (!doc.ok) return result;  // 読めない (競合で削除された等) ファイルは黙って対象外にする
	if (doc.is_binary) {
		is_binary = true;
		return result;
	}
	truncated = doc.truncated;

	TRegExOptions re_opt;
	if (!opt.case_sensitive) re_opt << roIgnoreCase;

	for (std::size_t i = 0; i < doc.lines.size(); ++i) {
		const UnicodeString &line = doc.lines[i];

		bool found = false;
		try {
			if (opt.use_regex) {
				found = TRegEx::IsMatch(line, opt.keyword, re_opt);
			}
			else {
				// find_mlt は grep_thread.cpp の「通常」経路 (goAnd/goWord 無し) と同じ
				// 呼び出し方。キーワードを半角/全角空白で区切って OR 検索する仕様
				// (usr_str.h::find_mlt のコメント) を引き継ぐ
				found = find_mlt(opt.keyword, line, /*and_sw=*/false, /*not_sw=*/false,
				                  opt.case_sensitive, /*word_sw=*/false);
			}
		}
		catch (...) {
			// 不正な正規表現は呼び出し側 (SearchDirectory) で事前検証しているため
			// ここに来るのは想定外のケースのみ。1行だけ諦めて続行する
			continue;
		}

		if (found) {
			GrepMatch m;
			m.file = path;
			m.line = static_cast<int>(i) + 1;
			m.text = line;
			result.push_back(m);
		}
	}

	return result;
}

//---------------------------------------------------------------------------
namespace {

/// dir 以下を再帰的に走査しながら検索する (SearchDirectory の下請け)
void WalkAndSearch(const UnicodeString &dir, const GrepOptions &opt, const GrepLimits &limits,
                    GrepResult &result, const GrepCancelCallback &cancel_cb,
                    const GrepProgressCallback &progress_cb)
{
	if (result.cancelled || result.stopped_by_file_limit || result.stopped_by_match_limit) return;

	TSearchRec sr;
	if (FindFirst(dir + "*", faAnyFile, sr) != 0) return;

	do {
		if (SameStr(sr.Name, ".") || SameStr(sr.Name, "..")) continue;

		if (cancel_cb && cancel_cb()) {
			result.cancelled = true;
			break;
		}

		const bool is_dir = (sr.Attr & faDirectory) != 0;
		if (is_dir) {
			// ディレクトリにもマスクを適用する。MatchPathMask はディレクトリ専用の
			// 除外指定 ("!node_modules\\" 等) をここで初めて意味を持たせられる
			// (gui/file_item.h のコメント参照)。ファイル名マスクだけを指定した
			// 場合はディレクトリ側は常に "*" 扱いになるため、再帰は制限されない
			if (opt.recursive && MatchPathMask(opt.mask, sr.Name, true)) {
				WalkAndSearch(dir + sr.Name + "\\", opt, limits, result, cancel_cb, progress_cb);
			}
			continue;
		}

		if (!MatchPathMask(opt.mask, sr.Name, false)) continue;

		if (result.files_scanned >= limits.max_files) {
			result.stopped_by_file_limit = true;
			break;
		}

		const UnicodeString full = dir + sr.Name;
		bool is_binary = false;
		bool truncated = false;
		std::vector<GrepMatch> found = SearchFile(full, opt, limits.max_file_bytes, is_binary, truncated);

		result.files_scanned++;
		if (is_binary)  result.files_skipped_binary++;
		if (truncated)  result.files_truncated++;

		for (std::size_t i = 0; i < found.size(); ++i) {
			if (static_cast<int>(result.matches.size()) >= limits.max_matches) {
				result.stopped_by_match_limit = true;
				break;
			}
			result.matches.push_back(found[i]);
		}

		if (progress_cb) progress_cb(result.files_scanned, static_cast<int>(result.matches.size()));

		if (result.stopped_by_match_limit) break;
	} while (FindNext(sr) == 0
	         && !result.cancelled && !result.stopped_by_file_limit && !result.stopped_by_match_limit);

	FindClose(sr);
}

}  // namespace

//---------------------------------------------------------------------------
GrepResult SearchDirectory(const UnicodeString &dir, const GrepOptions &opt, const GrepLimits &limits,
                            const GrepCancelCallback &cancel_cb, const GrepProgressCallback &progress_cb)
{
	GrepResult result;

	// 正規表現の事前検証。無効なパターンだと1行ごとに例外を投げて拾うことに
	// なり無駄が大きいため、走査を始める前に1回だけ確認する
	if (opt.use_regex) {
		try {
			TRegExOptions re_opt;
			if (!opt.case_sensitive) re_opt << roIgnoreCase;
			TRegEx test(opt.keyword, re_opt);
		}
		catch (const Exception &e) {
			result.error = _T("正規表現が不正です: ") + UnicodeString(e.Message);
			return result;
		}
		catch (...) {
			result.error = _T("正規表現が不正です");
			return result;
		}
	}

	WalkAndSearch(IncludeTrailingPathDelimiter(dir), opt, limits, result, cancel_cb, progress_cb);
	return result;
}

}  // namespace grep_core
