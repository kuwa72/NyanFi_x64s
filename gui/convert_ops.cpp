/**
 * @file gui/convert_ops.cpp
 * @brief 抽出と変換の実装 (設計は gui/convert_ops.h)
 */
#include "gui/convert_ops.h"

#include <memory>
#include <vector>

#include <string>

#include "gui/text_viewer_core.h"
#include "htmconv.h"
#include "usr_exif.h"
#include "usr_file_ex.h"
#include "usr_file_inf.h"
#include "usr_id3.h"
#include "usr_str.h"
#include "usr_wic.h"
#include "usr_xd2tx.h"

namespace convert_ops {

namespace {

bool exists_any(const UnicodeString &path)
{
	return file_exists(path) || dir_exists(path);
}

/// 出力先に同名があればスキップとして数える。作ってよければ false
bool skip_if_taken(const UnicodeString &path, file_ops::FileOpResult &result)
{
	if (!exists_any(path)) return false;
	result.skipped_existing++;
	return true;
}

/**
 * @brief 行の並びを指定の文字コードで書き出す
 * @details VCL は `saveto_TextFile` (Global.cpp) を使うが、`Global.cpp` は
 *          ビルド対象外なので同じことを直接書く。改行は CRLF、
 *          **UTF-8 と UTF-16 のときは BOM を付ける** (VCL の
 *          `TStrings::SaveToFile(enc)` が付けるのに合わせた)
 */
bool save_lines(const UnicodeString &path, TStrings *lines, int code_page,
                UnicodeString &error_out)
{
	UnicodeString all;
	for (int i = 0; i < lines->Count; ++i) all += lines->Strings[i] + _T("\r\n");

	std::string bytes;
	if (code_page == 1200 || code_page == 1201) {
		// UTF-16。BOM + そのままのバイト列 (BE なら並べ替える)
		const bool big = (code_page == 1201);
		bytes += big? "\xFE\xFF" : "\xFF\xFE";
		for (int i = 1; i <= all.Length(); ++i) {
			const wchar_t c = all[i];
			const char lo = static_cast<char>(c & 0xFF);
			const char hi = static_cast<char>((c >> 8) & 0xFF);
			if (big) { bytes += hi; bytes += lo; } else { bytes += lo; bytes += hi; }
		}
	}
	else {
		if (code_page == CP_UTF8) bytes += "\xEF\xBB\xBF";
		if (!all.IsEmpty()) {
			const int n = ::WideCharToMultiByte(code_page, 0, all.c_str(), all.Length(),
			                                    NULL, 0, NULL, NULL);
			if (n <= 0) { error_out = _T("この文字コードに変換できません"); return false; }
			std::string body(static_cast<std::size_t>(n), '\0');
			::WideCharToMultiByte(code_page, 0, all.c_str(), all.Length(), &body[0], n, NULL, NULL);
			bytes += body;
		}
	}

	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW,
	                         FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) { error_out = _T("書き込めません"); return false; }
	DWORD written = 0;
	const bool ok = bytes.empty()
		|| ((::WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, NULL) != 0)
		    && written == bytes.size());
	::CloseHandle(h);
	if (!ok) {
		::DeleteFileW(path.c_str());  // 書きかけを残さない
		error_out = _T("書き込みに失敗しました");
	}
	return ok;
}

}  // namespace

//---------------------------------------------------------------------------
UnicodeString OutputPath(const UnicodeString &src, const UnicodeString &dst_dir,
                         const UnicodeString &new_ext)
{
	const UnicodeString base = IncludeTrailingPathDelimiter(dst_dir);
	if (new_ext.IsEmpty()) return base + ExtractFileName(src);
	return base + get_base_name(src) + new_ext;
}

//---------------------------------------------------------------------------
UnicodeString IndexedOutputPath(const UnicodeString &src, const UnicodeString &dst_dir,
                                int index, const UnicodeString &ext)
{
	UnicodeString out = IncludeTrailingPathDelimiter(dst_dir);
	// VCL の書式は "%s%s_%03u.ico" (MainFrm.cpp:17417)。3桁固定
	out.cat_sprintf(_T("%s_%03u%s"), get_base_name(src).c_str(), index, ext.c_str());
	return out;
}

//---------------------------------------------------------------------------
file_ops::FileOpResult SetExifTime(const std::vector<UnicodeString> &paths)
{
	file_ops::FileOpResult result;

	for (const UnicodeString &p : paths) {
		if (dir_exists(p) || !test_ExifExt(get_extension(p))) {
			result.skipped_existing++;
			continue;
		}
		if (EXIF_SetExifTime(p)) result.success_count++;
		else result.failures.push_back(ExtractFileName(p) + _T(": Exif の撮影日時を取得できません"));
	}
	return result;
}

//---------------------------------------------------------------------------
file_ops::FileOpResult DeleteJpegExif(const std::vector<UnicodeString> &paths,
                                      const UnicodeString &dst_dir, bool keep_time)
{
	file_ops::FileOpResult result;

	for (const UnicodeString &p : paths) {
		if (dir_exists(p) || !test_JpgExt(get_extension(p))) {
			result.skipped_existing++;
			continue;
		}
		// 元と同じ名前で出力先に作る (VCL の Task_DLEXIF, task_thread.cpp:1553)
		const UnicodeString out = OutputPath(p, dst_dir, EmptyStr);
		if (SameText(out, p)) {
			// 出力先が元と同じディレクトリだと元を壊してしまう。**やらせない**
			result.failures.push_back(ExtractFileName(p) + _T(": 出力先が元と同じです"));
			continue;
		}
		if (skip_if_taken(out, result)) continue;

		const int r = EXIF_DelJpgExif(p, out, keep_time);
		if (r == 0)       result.success_count++;
		else if (r == 1)  result.skipped_existing++;   // Exif が無い
		else              result.failures.push_back(ExtractFileName(p) + _T(": 削除に失敗しました"));
	}
	return result;
}

//---------------------------------------------------------------------------
file_ops::FileOpResult ExtractEmbeddedImages(const std::vector<UnicodeString> &paths,
                                             const UnicodeString &dst_dir)
{
	file_ops::FileOpResult result;
	const UnicodeString base = IncludeTrailingPathDelimiter(dst_dir);

	for (const UnicodeString &p : paths) {
		const UnicodeString ext = get_extension(p);
		const bool is_mp3 = test_Mp3Ext(ext);
		const bool is_flac = test_FlacExt(ext);
		if (dir_exists(p) || (!is_mp3 && !is_flac)) {
			result.skipped_existing++;
			continue;
		}

		// 拡張子は付けずに渡す。中身を見て決まったものが付く
		// (ID3_GetImage / get_FlacImage の仕様。MainFrm.cpp:17484 も同じ)
		const UnicodeString stem = base + get_base_name(p);
		const bool ok = is_mp3? ID3_GetImage(p, NULL, stem) : get_FlacImage(p, NULL, stem);
		if (ok) result.success_count++;
		else result.skipped_existing++;  // 画像を持っていないだけなので失敗にしない
	}
	return result;
}

//---------------------------------------------------------------------------
namespace {

/**
 * @brief HICON を .ico ファイルとして書き出す
 * @details `Graphics::TIcon::SaveToFile` が宣言のみのシム (報告書 §20) なので
 *          自分で書く。.ico は `ICONDIR` + `ICONDIRENTRY` の並び + 各画像の
 *          DIB (BITMAPINFOHEADER + カラー + XOR ビット + AND マスク) という構造。
 *          ここは 1 枚だけを収める形で書く
 */
bool save_hicon_as_ico(HICON icon, const UnicodeString &path)
{
	ICONINFO info = {};
	if (!::GetIconInfo(icon, &info)) return false;

	// RAII 代わり。どの経路を通ってもビットマップを手放す
	struct Cleanup {
		HBITMAP color, mask;
		~Cleanup() { if (color) ::DeleteObject(color); if (mask) ::DeleteObject(mask); }
	} cleanup{info.hbmColor, info.hbmMask};

	BITMAP bm_color = {};
	BITMAP bm_mask = {};
	if (info.hbmColor == NULL || ::GetObject(info.hbmColor, sizeof(bm_color), &bm_color) == 0) return false;
	if (info.hbmMask == NULL || ::GetObject(info.hbmMask, sizeof(bm_mask), &bm_mask) == 0) return false;

	const int width = bm_color.bmWidth;
	const int height = bm_color.bmHeight;
	if (width <= 0 || height <= 0) return false;

	HDC dc = ::GetDC(NULL);
	if (dc == NULL) return false;

	// 32bpp のカラー面
	BITMAPINFO bi = {};
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = width;
	bi.bmiHeader.biHeight = height;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;

	const std::size_t color_bytes = static_cast<std::size_t>(width) * height * 4;
	std::vector<BYTE> color(color_bytes);
	bool ok = (::GetDIBits(dc, info.hbmColor, 0, static_cast<UINT>(height), color.data(), &bi, DIB_RGB_COLORS) != 0);

	// 1bpp の AND マスク。行は 4 バイト境界に揃う
	const std::size_t mask_stride = static_cast<std::size_t>(((width + 31) / 32) * 4);
	const std::size_t mask_bytes = mask_stride * height;
	std::vector<BYTE> mask(mask_bytes);
	if (ok) {
		// 1bpp 用のパレット付きヘッダを作る
		struct { BITMAPINFOHEADER h; RGBQUAD p[2]; } mi = {};
		mi.h.biSize = sizeof(BITMAPINFOHEADER);
		mi.h.biWidth = width;
		mi.h.biHeight = height;
		mi.h.biPlanes = 1;
		mi.h.biBitCount = 1;
		mi.h.biCompression = BI_RGB;
		ok = (::GetDIBits(dc, info.hbmMask, 0, static_cast<UINT>(height), mask.data(),
		                  reinterpret_cast<BITMAPINFO *>(&mi), DIB_RGB_COLORS) != 0);
	}
	::ReleaseDC(NULL, dc);
	if (!ok) return false;

	// .ico の中身を組み立てる
	BITMAPINFOHEADER dib = {};
	dib.biSize = sizeof(BITMAPINFOHEADER);
	dib.biWidth = width;
	dib.biHeight = height * 2;  // .ico はカラー面 + マスク面の合計高を書く
	dib.biPlanes = 1;
	dib.biBitCount = 32;
	dib.biCompression = BI_RGB;
	dib.biSizeImage = static_cast<DWORD>(color_bytes + mask_bytes);

	// ICONDIR / ICONDIRENTRY は 2 バイト境界で詰まっているので手で並べる
	const DWORD image_bytes = static_cast<DWORD>(sizeof(dib) + color_bytes + mask_bytes);
	const DWORD offset = 6 + 16;  // ICONDIR(6) + ICONDIRENTRY(16)

	std::vector<BYTE> head;
	const auto put16 = [&head](WORD v) { head.push_back(LOBYTE(v)); head.push_back(HIBYTE(v)); };
	const auto put32 = [&head](DWORD v) {
		for (int i = 0; i < 4; ++i) head.push_back(static_cast<BYTE>((v >> (i * 8)) & 0xFF));
	};
	put16(0);  // reserved
	put16(1);  // type = icon
	put16(1);  // count
	head.push_back(static_cast<BYTE>(width >= 256? 0 : width));
	head.push_back(static_cast<BYTE>(height >= 256? 0 : height));
	head.push_back(0);  // color count (32bpp なので 0)
	head.push_back(0);  // reserved
	put16(1);   // planes
	put16(32);  // bit count
	put32(image_bytes);
	put32(offset);

	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW,
	                         FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return false;

	DWORD written = 0;
	ok = (::WriteFile(h, head.data(), static_cast<DWORD>(head.size()), &written, NULL) != 0)
	     && (::WriteFile(h, &dib, sizeof(dib), &written, NULL) != 0)
	     && (::WriteFile(h, color.data(), static_cast<DWORD>(color_bytes), &written, NULL) != 0)
	     && (::WriteFile(h, mask.data(), static_cast<DWORD>(mask_bytes), &written, NULL) != 0);
	::CloseHandle(h);

	// **書きかけを残さない**
	if (!ok) ::DeleteFileW(path.c_str());
	return ok;
}

}  // namespace

//---------------------------------------------------------------------------
file_ops::FileOpResult ExtractIcons(const std::vector<UnicodeString> &paths,
                                    const UnicodeString &dst_dir, int index)
{
	file_ops::FileOpResult result;

	for (const UnicodeString &p : paths) {
		if (dir_exists(p)) { result.skipped_existing++; continue; }

		// 番号に -1 を渡すと「入っている数」が返る (Win32 の仕様)
		const UINT count = ::ExtractIconExW(p.c_str(), -1, NULL, NULL, 0);
		if (count == 0) { result.skipped_existing++; continue; }

		const int from = (index >= 0)? index : 0;
		const int to = (index >= 0)? index + 1 : static_cast<int>(count);
		if (from >= static_cast<int>(count)) {
			result.failures.push_back(ExtractFileName(p) + _T(": その番号のアイコンはありません"));
			continue;
		}

		for (int i = from; i < to; ++i) {
			HICON large = NULL;
			if (::ExtractIconExW(p.c_str(), i, &large, NULL, 1) != 1 || large == NULL) {
				result.failures.push_back(ExtractFileName(p) + _T(": アイコンを取り出せません"));
				continue;
			}
			const UnicodeString out = IndexedOutputPath(p, dst_dir, i, _T(".ico"));
			if (exists_any(out)) {
				result.skipped_existing++;
			}
			else if (save_hicon_as_ico(large, out)) {
				result.success_count++;
			}
			else {
				result.failures.push_back(ExtractFileName(out) + _T(": 書き出せません"));
			}
			::DestroyIcon(large);
		}
	}
	return result;
}

//---------------------------------------------------------------------------
file_ops::FileOpResult ConvertDocToText(const std::vector<UnicodeString> &paths,
                                        const UnicodeString &dst_dir, int code_page,
                                        UnicodeString &error_out)
{
	file_ops::FileOpResult result;

	// xdoc2txt.dll が無ければ1件も触らない (VCL も冒頭で中止する)
	if (!xd2tx_Initialize()) {
		error_out = _T("xdoc2txt が利用できません (xdoc2txt.dll が要ります)");
		return result;
	}

	for (const UnicodeString &p : paths) {
		if (dir_exists(p) || !xd2tx_TestExt(get_extension(p), true)) {
			result.skipped_existing++;
			continue;
		}

		std::unique_ptr<TStringList> buf(new TStringList());
		if (!xd2tx_Extract(p, buf.get())) {
			result.failures.push_back(ExtractFileName(p) + _T(": 変換できません"));
			continue;
		}

		const UnicodeString out = OutputPath(p, dst_dir, _T(".txt"));
		if (skip_if_taken(out, result)) continue;

		UnicodeString error;
		if (save_lines(out, buf.get(), code_page, error)) result.success_count++;
		else result.failures.push_back(ExtractFileName(out) + _T(": ") + error);
	}
	return result;
}

//---------------------------------------------------------------------------
file_ops::FileOpResult ConvertHtmlToText(const std::vector<UnicodeString> &paths,
                                         const UnicodeString &dst_dir, bool to_markdown)
{
	file_ops::FileOpResult result;

	std::unique_ptr<HtmConv> conv(new HtmConv());
	conv->ToMarkdown = to_markdown;

	for (const UnicodeString &p : paths) {
		if (dir_exists(p) || !test_HtmlExt(get_extension(p))) {
			result.skipped_existing++;
			continue;
		}

		// 読み込みと文字コード判定は既にある経路に載せる (テキストビューアと同じ)
		const text_viewer_core::LoadResult r = text_viewer_core::LoadForView(p);
		if (!r.ok) {
			result.failures.push_back(ExtractFileName(p) + _T(": ") + r.error);
			continue;
		}
		const int code_page = (r.code_page != 0)? r.code_page : 932;

		std::unique_ptr<TStringList> buf(new TStringList());
		for (const UnicodeString &line : r.lines) buf->Add(line);

		conv->FileName = p;
		conv->HtmBuf->Assign(buf.get());
		conv->CodePage = code_page;
		conv->Convert();

		const UnicodeString out = OutputPath(p, dst_dir, to_markdown? _T(".md") : _T(".txt"));
		if (skip_if_taken(out, result)) continue;

		// **出力の文字コードは入力に合わせる** (VCL も判定結果をそのまま使う)
		UnicodeString error;
		if (save_lines(out, conv->TxtBuf, code_page, error)) result.success_count++;
		else result.failures.push_back(ExtractFileName(out) + _T(": ") + error);
	}
	return result;
}

//---------------------------------------------------------------------------
file_ops::FileOpResult ConvertImages(const std::vector<UnicodeString> &paths,
                                     const UnicodeString &dst_dir, const UnicodeString &ext,
                                     int jpeg_quality)
{
	file_ops::FileOpResult result;

	for (const UnicodeString &p : paths) {
		if (dir_exists(p)) { result.skipped_existing++; continue; }

		const UnicodeString out = OutputPath(p, dst_dir, ext);
		if (SameText(out, p)) {
			// 変換先が元と同じ。元を壊すので**やらせない**
			result.failures.push_back(ExtractFileName(p) + _T(": 変換先が元と同じです"));
			continue;
		}
		if (skip_if_taken(out, result)) continue;

		std::unique_ptr<Graphics::TBitmap> bmp(new Graphics::TBitmap());
		if (!WIC_load_image(p, bmp.get(), WICIMG_FRAME) || bmp->Empty) {
			result.failures.push_back(ExtractFileName(p) + _T(": 画像として読めません"));
			continue;
		}
		if (WIC_save_image(out, bmp.get(), jpeg_quality)) result.success_count++;
		else result.failures.push_back(ExtractFileName(out) + _T(": 保存できません"));
	}
	return result;
}

}  // namespace convert_ops
