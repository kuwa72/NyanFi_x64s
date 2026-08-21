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
LoadResult LoadForView(const UnicodeString &path, Int64 max_bytes)
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

		if (code_page < 0) {
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

}  // namespace text_viewer_core
