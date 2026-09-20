/**
 * @file gui/text_viewer_core.cpp
 * @brief gui/text_viewer_core.h の実装 (wx 非依存)
 */
#include "gui/text_viewer_core.h"

#include <algorithm>
#include <memory>

#include "usr_file_ex.h"
#include "usr_file_inf.h"

namespace text_viewer_core {

namespace {

/// BOM のスキップ幅。src/usr_file_inf.cpp の get_top_line() と同じ判定
/// (UTF-16 系は2バイト、UTF-8 は3バイト、それ以外はBOMを持たない)
int BomSkipBytes(int code_page, bool has_bom)
{
	if (!has_bom) return 0;
	if (code_page == 1200 || code_page == 1201) return 2;
	if (code_page == CP_UTF8) return 3;
	return 0;
}

}  // namespace

//---------------------------------------------------------------------------
LoadResult LoadForView(const UnicodeString &path, Int64 max_bytes, int forced_code_page)
{
	LoadResult r;

	if (!file_exists(path)) {
		r.error = _T("ファイルが見つかりません: ") + path;
		return r;
	}

	try {
		std::unique_ptr<TFileStream> fs(new TFileStream(path, fmOpenRead | fmShareDenyNone));
		r.file_size = fs->Size;

		const Int64 read_size = std::min<Int64>(r.file_size, max_bytes);
		r.read_size = read_size;
		r.truncated = (read_size < r.file_size);

		std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
		// CopyFrom(fs, 0) は「count<=0 なら全体をコピーする」特別扱いがある
		// (compat/streams.h) ため、read_size==0 (空ファイル) のときは呼ばない
		if (read_size > 0) ms->CopyFrom(fs.get(), read_size);

		bool has_bom = false;
		int code_page = get_MemoryCodePage(ms.get(), &has_bom);
		r.has_bom = has_bom;

		if (forced_code_page > 0) {
			// ChangeCodePage による強制再読込。バイナリ判定を上書きして
			// テキストとしてデコードする (OpenTxtViewer 強制テキスト相当)
			r.is_binary = false;
			code_page = forced_code_page;
		}
		else if (code_page < 0) {
			// バイナリ判定。テキストとしては展開しない (呼び出し側が案内を出す)
			r.is_binary = true;
			r.code_page = code_page;
			r.ok = true;
			return r;
		}
		if (code_page == 0) code_page = 932;  // get_MemoryStrins() と同じフォールバック
		r.code_page = code_page;

		TBytes bytes;
		bytes.Length = static_cast<int>(ms->Size);
		if (bytes.Length > 0) {
			ms->Seek(0, soFromBeginning);
			ms->Read(bytes, bytes.Length);
		}

		const int skip = BomSkipBytes(code_page, has_bom);
		std::unique_ptr<TEncoding> enc(TEncoding::GetEncoding(code_page));
		const UnicodeString text = enc->GetString(bytes, skip, bytes.Length - skip);

		// 改行分割は移植済みの TStrings::SetText (compat/classes.cpp) に任せる。
		// CR/LF/CRLF のいずれも1区切りとして扱う Delphi 互換の挙動
		std::unique_ptr<TStringList> lst(new TStringList());
		lst->Text = text;
		r.lines.reserve(static_cast<std::size_t>(lst->Count));
		for (int i = 0; i < lst->Count; ++i) r.lines.push_back(lst->Strings[i]);

		r.ok = true;
	}
	catch (const Exception &e) {
		r.error = _T("読み込みに失敗しました: ") + UnicodeString(e.Message);
	}
	catch (...) {
		r.error = _T("読み込みに失敗しました (不明なエラー)");
	}

	return r;
}

//---------------------------------------------------------------------------
int CharDisplayWidth(wchar_t c)
{
	// 半角カナは全角の範囲に入るが幅1のため先に弾く
	if (c >= 0xFF61 && c <= 0xFF9F) return 1;

	const bool wide =
		(c >= 0x1100 && c <= 0x115F) ||    // ハングル字母
		(c >= 0x2E80 && c <= 0xA4CF) ||    // CJK部首補助・かな・カナ・ハングル互換字母・CJK統合漢字 等
		(c >= 0xAC00 && c <= 0xD7A3) ||    // ハングル音節
		(c >= 0xF900 && c <= 0xFAFF) ||    // CJK互換漢字
		(c >= 0xFF00 && c <= 0xFF60) ||    // 全角英数・記号
		(c >= 0xFFE0 && c <= 0xFFE6) ||    // 全角記号
		(c >= 0xD800 && c <= 0xDFFF);      // サロゲート (絵文字等の代用対象。幅2扱い)

	return wide ? 2 : 1;
}

//---------------------------------------------------------------------------
std::vector<UnicodeString> WrapLine(const UnicodeString &line, int width_cols, int tab_width)
{
	std::vector<UnicodeString> out;

	const int len = line.Length();
	if (width_cols <= 0 || len == 0) {
		out.push_back(line);
		return out;
	}
	if (tab_width <= 0) tab_width = 4;

	int col = 0;
	int seg_start = 1;  // UnicodeString は1始まり
	int i = 1;

	while (i <= len) {
		int chars = 1;
		int w;

		if (line[i] == L'\t') {
			w = tab_width - (col % tab_width);
		}
		else if (line.IsLeadSurrogate(i) && i < len) {
			w = 2;
			chars = 2;
		}
		else {
			w = CharDisplayWidth(line[i]);
		}

		// 行頭の1文字は幅が超えていても必ず入れる (無限ループ防止)
		if (col > 0 && col + w > width_cols) {
			out.push_back(line.SubString(seg_start, i - seg_start));
			seg_start = i;
			col = 0;
		}

		col += w;
		i += chars;
	}
	out.push_back(line.SubString(seg_start, len - seg_start + 1));
	return out;
}

//---------------------------------------------------------------------------
int StepLine(int cur, int delta, int count)
{
	if (count <= 0) return 0;
	return std::clamp(cur + delta, 0, count - 1);
}

//---------------------------------------------------------------------------
int PageStepLines(int visible_rows)
{
	return std::max(1, visible_rows - 1);
}

//---------------------------------------------------------------------------
int FindNextLine(const std::vector<UnicodeString> &lines, const UnicodeString &kwd,
                 int from_line, bool down)
{
	const int n = static_cast<int>(lines.size());
	if (n == 0 || kwd.IsEmpty()) return -1;
	for (int step = 1; step <= n; ++step) {
		const int i = down ? (from_line + step) % n
		                   : (from_line - step + n * 2) % n;
		if (ContainsText(lines[static_cast<std::size_t>(i)], kwd)) return i;
	}
	return -1;
}

//---------------------------------------------------------------------------
std::vector<int> ToggleMark(const std::vector<int> &marks, int line)
{
	std::vector<int> out = marks;
	const auto it = std::find(out.begin(), out.end(), line);
	if (it == out.end()) {
		out.push_back(line);
		std::sort(out.begin(), out.end());
	}
	else {
		out.erase(it);
	}
	return out;
}

//---------------------------------------------------------------------------
std::vector<int> ClearMarkList(const std::vector<int> & /*marks*/)
{
	return {};
}

//---------------------------------------------------------------------------
int FindMarkNext(const std::vector<int> &marks, int cur_line, bool down)
{
	int found = -1;
	for (int m : marks) {
		if (down) {
			if (m > cur_line && (found == -1 || m < found)) found = m;
		}
		else {
			if (m < cur_line && (found == -1 || m > found)) found = m;
		}
	}
	return found;
}

//---------------------------------------------------------------------------
int ParseJumpLine(const UnicodeString &param, int cur_0based, int count)
{
	if (count <= 0 || param.IsEmpty()) return -1;
	// 相対指定 "+n/-n" (バイナリ表示の ToAddrA と同じ考え方)
	if (param[1] == L'+' || param[1] == L'-') {
		const int rel = param.SubString(2).ToIntDef(-1);
		if (rel < 0) return -1;
		const int target = cur_0based + (param[1] == L'+' ? rel : -rel);
		if (target < 0 || target >= count) return -1;
		return target;
	}
	// 絶対指定 "n" (1ベース。テキスト表示の JumpLine と同じ)
	const int abs_no = param.ToIntDef(-1);
	if (abs_no <= 0 || abs_no > count) return -1;
	return abs_no - 1;
}

//---------------------------------------------------------------------------
int NextCodePage(int cur_code_page)
{
	static const int kCycle[] = {932, 50220, 20932, 1252, 65001, 1200};
	for (std::size_t i = 0; i < sizeof(kCycle) / sizeof(kCycle[0]); ++i) {
		if (kCycle[i] == cur_code_page) {
			return kCycle[(i + 1) % (sizeof(kCycle) / sizeof(kCycle[0]))];
		}
	}
	return 932;
}

//---------------------------------------------------------------------------
int ParseCodePageParam(const UnicodeString &param, int cur_code_page)
{
	if (param.IsEmpty()) return NextCodePage(cur_code_page);
	static const int kKnown[] = {932, 50220, 20932, 1252, 65001, 1200};
	const int cp = param.ToIntDef(0);
	for (int k : kKnown) {
		if (k == cp) return cp;
	}
	return 0;
}

//---------------------------------------------------------------------------
int ParseMoveCount(const UnicodeString &param, int /*visible_rows*/)
{
	if (param.IsEmpty()) return 1;
	const int n = param.ToIntDef(1);
	return n >= 1 ? n : 1;
}

}  // namespace text_viewer_core
