/**
 * @file gui/text_ops.cpp
 * @brief テキスト操作の実装 (設計は gui/text_ops.h)
 */
#include "gui/text_ops.h"

#include <string>
#include <utility>

#include "gui/text_viewer_core.h"
#include "usr_file_ex.h"
#include "usr_str.h"

namespace text_ops {

namespace {

/// 指定コードページのバイト列にする。UTF-16 は WideCharToMultiByte では
/// 変換できないので VCL の TEncoding と同じ BOM / エンディアンで書く。
std::string encode_bytes(const UnicodeString &s, int code_page, bool with_bom,
                         UnicodeString &error_out)
{
	std::string out;
	if (code_page == 1200 || code_page == 1201) {
		if (with_bom) out += code_page == 1201? "\xFE\xFF" : "\xFF\xFE";
		const bool big_endian = code_page == 1201;
		for (int i = 1; i <= s.Length(); ++i) {
			const unsigned int c = static_cast<unsigned int>(s[i]);
			const char lo = static_cast<char>(c & 0xFF);
			const char hi = static_cast<char>((c >> 8) & 0xFF);
			if (big_endian) { out += hi; out += lo; }
			else           { out += lo; out += hi; }
		}
		return out;
	}

	if (with_bom && code_page == CP_UTF8) out += "\xEF\xBB\xBF";
	if (s.IsEmpty()) return out;
	const int n = ::WideCharToMultiByte(code_page, 0, s.c_str(), s.Length(), nullptr, 0,
	                                    nullptr, nullptr);
	if (n <= 0) {
		error_out = _T("この文字コードに変換できません");
		return std::string();
	}
	const std::size_t old_size = out.size();
	out.resize(old_size + static_cast<std::size_t>(n));
	const int written = ::WideCharToMultiByte(code_page, 0, s.c_str(), s.Length(),
	                                           out.data() + old_size, n, nullptr, nullptr);
	if (written != n) {
		error_out = _T("この文字コードに変換できません");
		return std::string();
	}
	return out;
}

/// バイト列をファイルへ書く (既存は上書きする。呼び出し側が存在を確認済みのこと)
bool write_all(const UnicodeString &path, const std::string &bytes, UnicodeString &error_out)
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
	                         FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		error_out = _T("書き込めません");
		return false;
	}
	bool ok = true;
	if (!bytes.empty()) {
		DWORD written = 0;
		ok = (::WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, NULL) != 0)
		     && (written == bytes.size());
		if (!ok) error_out = _T("書き込みに失敗しました");
	}
	::CloseHandle(h);
	return ok;
}

/// 先頭を読んで NUL バイトが含まれるか見る。
///
/// **バイナリ判定をヒューリスティックだけに頼らない**ための保険。
/// `LoadForView()` の `is_binary` は `get_MemoryCodePage()` の判定で、
/// **数バイトしかない小さなバイナリを取りこぼす** (実測: NUL を含む6バイトの
/// ファイルがテキストと判定された)。書き換える前にここでも見る。
///
/// UTF-16 のテキストは NUL を含むが、`LoadForView()` がそちらは
/// コードページ付きで返すので、この関数は**その前段では使わない**
/// (変換元が UTF-16 のときは code_page で分かる)。
bool has_nul_byte(const UnicodeString &path, Int64 limit = 64 * 1024)
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
	                         OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (h == INVALID_HANDLE_VALUE) return false;

	bool found = false;
	char buf[8192];
	DWORD n = 0;
	Int64 total = 0;
	while (!found && total < limit && ::ReadFile(h, buf, sizeof(buf), &n, NULL) && n > 0) {
		for (DWORD i = 0; i < n; ++i) {
			if (buf[i] == '\0') { found = true; break; }
		}
		total += n;
	}
	::CloseHandle(h);
	return found;
}

}  // namespace

//---------------------------------------------------------------------------
LineStats CountLines(const std::vector<UnicodeString> &lines)
{
	LineStats st;
	st.total = static_cast<int>(lines.size());
	for (const UnicodeString &l : lines) {
		if (Trim(l).IsEmpty()) st.blank++;
	}
	st.non_blank = st.total - st.blank;
	return st;
}

//---------------------------------------------------------------------------
JoinResult JoinTextFiles(const std::vector<UnicodeString> &paths, const UnicodeString &out_path)
{
	return JoinTextFiles(paths, out_path, CP_UTF8, false, _T("\r\n"));
}

//---------------------------------------------------------------------------
JoinResult JoinTextFiles(const std::vector<UnicodeString> &paths, const UnicodeString &out_path,
                         int target_code_page, bool with_bom, const UnicodeString &line_break)
{
	JoinResult result;

	if (file_exists(out_path) || dir_exists(out_path)) {
		result.failures.push_back(out_path + _T(": 出力先が既に存在します"));
		return result;
	}

	struct Loaded {
		text_viewer_core::LoadResult text;
	};
	std::vector<Loaded> loaded;
	loaded.reserve(paths.size());
	for (const UnicodeString &p : paths) {
		Loaded item;
		item.text = text_viewer_core::LoadForView(p);
		if (!item.text.ok) {
			result.failures.push_back(p + _T(": ") + item.text.error);
			continue;
		}
		if (item.text.truncated) {
			result.failures.push_back(p + _T(": ファイルが大きすぎて全体を読めません"));
			continue;
		}
		const bool utf16 = item.text.code_page == 1200 || item.text.code_page == 1201;
		if (item.text.is_binary || (!utf16 && has_nul_byte(p))) {
			result.failures.push_back(p + _T(": バイナリなので結合しません"));
			continue;
		}
		loaded.push_back(std::move(item));
	}

	if (loaded.empty()) return result;

	int output_code_page = target_code_page;
	bool output_bom = with_bom;
	if (output_code_page == 0) {
		output_code_page = loaded.front().text.code_page;
		if (output_code_page == 0) output_code_page = 932;
		output_bom = loaded.front().text.has_bom;
	}

	UnicodeString all;
	for (const Loaded &item : loaded) {
		for (const UnicodeString &line : item.text.lines) all += line + line_break;
		++result.joined;
	}

	UnicodeString error;
	const std::string bytes = encode_bytes(all, output_code_page, output_bom, error);
	if (!error.IsEmpty() || !write_all(out_path, bytes, error)) {
		result.joined = 0;
		result.failures.push_back(out_path + _T(": ")
		                          + (error.IsEmpty()? _T("書き込めません") : error));
		::DeleteFileW(out_path.c_str());
	}
	return result;
}

//---------------------------------------------------------------------------
bool ConvertEncoding(const UnicodeString &path, int target_code_page, bool with_bom,
                     UnicodeString &error_out)
{
	return ConvertEncoding(path, target_code_page, with_bom, _T("\r\n"), error_out);
}

//---------------------------------------------------------------------------
bool ConvertEncoding(const UnicodeString &path, int target_code_page, bool with_bom,
                     const UnicodeString &line_break, UnicodeString &error_out)
{
	const text_viewer_core::LoadResult r = text_viewer_core::LoadForView(path);
	if (!r.ok) {
		error_out = r.error;
		return false;
	}
	// バイナリは触らない (壊すため)。**判定を2つ通す**:
	// LoadForView のヒューリスティックは小さなバイナリを取りこぼすので、
	// NUL バイトの有無も見る (has_nul_byte のコメント参照)
	const bool utf16 = r.code_page == 1200 || r.code_page == 1201;
	if (r.is_binary || (!utf16 && has_nul_byte(path))) {
		error_out = _T("バイナリなので変換しません");
		return false;
	}
	if (r.truncated) {
		// 途中までしか読めていないものを書き戻すと**残りが消える**
		error_out = _T("ファイルが大きすぎて全体を読めません");
		return false;
	}

	UnicodeString all;
	for (std::size_t i = 0; i < r.lines.size(); ++i) {
		if (i > 0) all += line_break;
		all += r.lines[i];
	}

	const std::string bytes = encode_bytes(all, target_code_page, with_bom, error_out);
	if (!error_out.IsEmpty()) return false;
	return write_all(path, bytes, error_out);
}

}  // namespace text_ops
